/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usb_dc_ch32h417_usbfs.h"

#include <rtthread.h>
#include <string.h>

#include "board.h"
#include "ch32h417.h"

#define USBFS_INIT_TIMEOUT     1000000U
#define USBFS_EP_BUFFER_SIZE   64U

#ifndef USBFS_ENABLE_SUSPEND_IRQ
#define USBFS_ENABLE_SUSPEND_IRQ 1
#endif

#define USBFS_UEP_DMA_BASE     ((uintptr_t)USBFS_BASE + 0x10U)
#define USBFS_UEP_LEN_BASE     ((uintptr_t)USBFS_BASE + 0x30U)
#define USBFS_UEP_CTL_BASE     ((uintptr_t)USBFS_BASE + 0x32U)
#define USBFS_IRQ_WORD(irqn)    ((uint32_t)(irqn) >> 5U)
#define USBFS_IRQ_MASK(irqn)    (1UL << ((uint32_t)(irqn) & 0x1FU))
#define USBFS_CFGR2_MASK        (RCC_USBFSSRC | RCC_USBFSDIV)
#define USBFS_CFGR2_48M         (RCC_USBFSSRC_USBHSPLL | RCC_USBFSDIV_DIV10)
#define USBFS_DMA_RAM_BASE      ((uintptr_t)0x20178200U)
#define USBFS_DMA_RAM_SIZE      (USB_CH32H417_FS_EP0_MPS + (USB_CH32H417_MAX_EP_NUM * USBFS_EP_BUFFER_SIZE * 2U))

struct usb_dc_ep_state {
    uint16_t ep_mps;
    uint8_t ep_type;
    uint8_t ep_stalled;
    uint8_t ep_enabled;
    uint8_t ep_toggle;
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
};

struct ch32h417_usbfs_udc {
    volatile uint8_t dev_addr;
    uint8_t addr_pending;
    uint8_t port_speed;
    struct usb_dc_ep_state in_ep[USB_CH32H417_MAX_EP_NUM];
    struct usb_dc_ep_state out_ep[USB_CH32H417_MAX_EP_NUM];
};

struct ch32h417_usbfs_diag {
    volatile uint32_t irq;
    volatile uint32_t bus_reset;
    volatile uint32_t suspend;
    volatile uint32_t resume;
    volatile uint32_t wakeup_irq;
    volatile uint32_t transfer;
    volatile uint32_t transfer_nak;
    volatile uint32_t transfer_bad_ep;
    volatile uint32_t setup;
    volatile uint32_t setup_recovered;
    volatile uint32_t ep0_prime_in;
    volatile uint32_t ep0_in;
    volatile uint32_t ep0_prime_out;
    volatile uint32_t ep0_out;
    volatile uint32_t set_addr;
    volatile uint32_t stall;
    volatile uint32_t last_setup0;
    volatile uint32_t last_setup1;
    volatile uint32_t last_resp0;
    volatile uint32_t last_intflag;
    volatile uint32_t last_intst;
    volatile uint32_t last_xfer_intflag;
    volatile uint32_t last_xfer_intst;
    volatile uint32_t last_xfer_ep;
    volatile uint32_t last_xfer_token;
    volatile uint32_t last_xfer_rx_len;
    volatile uint32_t last_xfer_buf0;
    volatile uint32_t last_xfer_buf1;
    volatile uint32_t last_tx_len;
    volatile uint32_t last_rx_len;
    volatile uint32_t last_addr;
};

static struct ch32h417_usbfs_udc g_ch32h417_usbfs_udc;
static struct ch32h417_usbfs_diag g_ch32h417_usbfs_diag;

static uint8_t *const usbfs_ep0_buffer = (uint8_t *)USBFS_DMA_RAM_BASE;
static uint8_t (*const usbfs_ep_tx_buffer)[USBFS_EP_BUFFER_SIZE] =
    (uint8_t (*)[USBFS_EP_BUFFER_SIZE])(USBFS_DMA_RAM_BASE + USB_CH32H417_FS_EP0_MPS);
static uint8_t (*const usbfs_ep_rx_buffer)[USBFS_EP_BUFFER_SIZE] =
    (uint8_t (*)[USBFS_EP_BUFFER_SIZE])(USBFS_DMA_RAM_BASE + USB_CH32H417_FS_EP0_MPS +
                                        (USB_CH32H417_MAX_EP_NUM * USBFS_EP_BUFFER_SIZE));

static inline __IO uint32_t *usbfs_ep_dma_reg(uint8_t ep_idx)
{
    return (__IO uint32_t *)(USBFS_UEP_DMA_BASE + ((uintptr_t)ep_idx * 4U));
}

static inline __IO uint16_t *usbfs_ep_tx_len_reg(uint8_t ep_idx)
{
    return (__IO uint16_t *)(USBFS_UEP_LEN_BASE + ((uintptr_t)ep_idx * 4U));
}

static inline __IO uint8_t *usbfs_ep_tx_ctrl_reg(uint8_t ep_idx)
{
    return (__IO uint8_t *)(USBFS_UEP_CTL_BASE + ((uintptr_t)ep_idx * 4U));
}

static inline __IO uint8_t *usbfs_ep_rx_ctrl_reg(uint8_t ep_idx)
{
    return (__IO uint8_t *)(USBFS_UEP_CTL_BASE + ((uintptr_t)ep_idx * 4U) + 1U);
}

static void usbfs_delay(uint32_t cycles)
{
    volatile uint32_t i;

    for (i = 0U; i < cycles; i++) {
        __NOP();
    }
}

static uint32_t usbfs_irq_is_enabled(IRQn_Type irq)
{
    uint32_t word = USBFS_IRQ_WORD(irq);

    if (word >= 8U) {
        return 0U;
    }

    return (NVIC->ISR[word] & USBFS_IRQ_MASK(irq)) != 0U;
}

static uint32_t usbfs_irq_is_pending(IRQn_Type irq)
{
    uint32_t word = USBFS_IRQ_WORD(irq);

    if (word >= 8U) {
        return 0U;
    }

    return (NVIC->IPR[word] & USBFS_IRQ_MASK(irq)) != 0U;
}

static uint32_t usbfs_irq_is_active(IRQn_Type irq)
{
    uint32_t word = USBFS_IRQ_WORD(irq);

    if (word >= 8U) {
        return 0U;
    }

    return (NVIC->IACTR[word] & USBFS_IRQ_MASK(irq)) != 0U;
}

static uint32_t usbfs_pack4(const uint8_t *data)
{
    if (data == RT_NULL) {
        return 0U;
    }

    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint8_t usbfs_ep0_buffer_has_setup(void)
{
    uint8_t request_type = usbfs_ep0_buffer[0];
    uint8_t request = usbfs_ep0_buffer[1];
    uint16_t length = (uint16_t)usbfs_ep0_buffer[6] | ((uint16_t)usbfs_ep0_buffer[7] << 8);

    if (((request_type & USB_REQUEST_TYPE_MASK) == USB_REQUEST_RESERVED) ||
        ((request_type & 0x1fU) > USB_REQUEST_RECIPIENT_OTHER)) {
        return 0U;
    }

    if (((request_type & USB_REQUEST_TYPE_MASK) == USB_REQUEST_STANDARD) &&
        (request > USB_REQUEST_SYNCH_FRAME)) {
        return 0U;
    }

    if ((request_type == 0U) && (request == 0U) &&
        (usbfs_ep0_buffer[2] == 0U) && (usbfs_ep0_buffer[3] == 0U) &&
        (usbfs_ep0_buffer[4] == 0U) && (usbfs_ep0_buffer[5] == 0U) &&
        (length == 0U)) {
        return 0U;
    }

    return 1U;
}

static void usbfs_ep_mod_update(uint8_t ep_idx, uint8_t is_in, uint8_t enable)
{
    __IO uint8_t *mod_reg = RT_NULL;
    uint8_t mask = 0U;

    switch (ep_idx) {
    case 1U:
        mod_reg = &USBFSD->UEP4_1_MOD;
        mask = is_in ? USBFS_UEP1_TX_EN : USBFS_UEP1_RX_EN;
        break;
    case 2U:
        mod_reg = &USBFSD->UEP2_3_MOD;
        mask = is_in ? USBFS_UEP2_TX_EN : USBFS_UEP2_RX_EN;
        break;
    case 3U:
        mod_reg = &USBFSD->UEP2_3_MOD;
        mask = is_in ? USBFS_UEP3_TX_EN : USBFS_UEP3_RX_EN;
        break;
    case 4U:
        mod_reg = &USBFSD->UEP4_1_MOD;
        mask = is_in ? USBFS_UEP4_TX_EN : USBFS_UEP4_RX_EN;
        break;
    case 5U:
        mod_reg = &USBFSD->UEP5_6_MOD;
        mask = is_in ? USBFS_UEP5_TX_EN : USBFS_UEP5_RX_EN;
        break;
    case 6U:
        mod_reg = &USBFSD->UEP5_6_MOD;
        mask = is_in ? USBFS_UEP6_TX_EN : USBFS_UEP6_RX_EN;
        break;
    case 7U:
        mod_reg = &USBFSD->UEP7_MOD;
        mask = is_in ? USBFS_UEP7_TX_EN : USBFS_UEP7_RX_EN;
        break;
    default:
        break;
    }

    if (mod_reg == RT_NULL) {
        return;
    }

    if (enable != 0U) {
        *mod_reg |= mask;
    } else {
        *mod_reg &= (uint8_t)~mask;
    }
}

static void usbfs_ep0_state_init(void)
{
    g_ch32h417_usbfs_udc.out_ep[0].ep_mps = USB_CH32H417_FS_EP0_MPS;
    g_ch32h417_usbfs_udc.out_ep[0].ep_type = USB_ENDPOINT_TYPE_CONTROL;
    g_ch32h417_usbfs_udc.out_ep[0].ep_enabled = 1U;
    g_ch32h417_usbfs_udc.out_ep[0].ep_toggle = 0U;
    g_ch32h417_usbfs_udc.out_ep[0].xfer_buf = RT_NULL;
    g_ch32h417_usbfs_udc.out_ep[0].xfer_len = 0U;
    g_ch32h417_usbfs_udc.out_ep[0].actual_xfer_len = 0U;
    g_ch32h417_usbfs_udc.out_ep[0].ep_stalled = 0U;

    g_ch32h417_usbfs_udc.in_ep[0].ep_mps = USB_CH32H417_FS_EP0_MPS;
    g_ch32h417_usbfs_udc.in_ep[0].ep_type = USB_ENDPOINT_TYPE_CONTROL;
    g_ch32h417_usbfs_udc.in_ep[0].ep_enabled = 1U;
    g_ch32h417_usbfs_udc.in_ep[0].ep_toggle = 0U;
    g_ch32h417_usbfs_udc.in_ep[0].xfer_buf = RT_NULL;
    g_ch32h417_usbfs_udc.in_ep[0].xfer_len = 0U;
    g_ch32h417_usbfs_udc.in_ep[0].actual_xfer_len = 0U;
    g_ch32h417_usbfs_udc.in_ep[0].ep_stalled = 0U;

    USBFSD->UEP0_DMA = (uint32_t)usbfs_ep0_buffer;
    USBFSD->UEP0_TX_LEN = 0U;
    USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
}

static void usbfs_clear_transfer_state(void)
{
    memset(&g_ch32h417_usbfs_udc, 0, sizeof(g_ch32h417_usbfs_udc));
    g_ch32h417_usbfs_udc.port_speed = USB_SPEED_FULL;
    usbfs_ep0_state_init();
}

static void usbfs_apply_pending_address(void)
{
    if (g_ch32h417_usbfs_udc.addr_pending != 0U) {
        USBFSD->DEV_ADDR = (uint8_t)((USBFSD->DEV_ADDR & USBFS_UDA_GP_BIT) |
                                     (g_ch32h417_usbfs_udc.dev_addr & USBFS_USB_ADDR_MASK));
        g_ch32h417_usbfs_udc.addr_pending = 0U;
    }
}

static void usbfs_ep_prime_in(uint8_t ep_idx, const uint8_t *data, uint32_t data_len)
{
    uint32_t mps = (uint32_t)g_ch32h417_usbfs_udc.in_ep[ep_idx].ep_mps;
    uint8_t ctrl;
    uint16_t xfer_len;

    if ((mps == 0U) || (mps > USBFS_EP_BUFFER_SIZE)) {
        mps = (ep_idx == 0U) ? USB_CH32H417_FS_EP0_MPS : USBFS_EP_BUFFER_SIZE;
    }
    if (data_len > mps) {
        data_len = mps;
    }
    xfer_len = (uint16_t)data_len;

    if (ep_idx == 0U) {
        ctrl = (g_ch32h417_usbfs_udc.in_ep[0].ep_toggle != 0U) ? USBFS_UEP_T_TOG : 0U;
        if ((data != RT_NULL) && (data_len != 0U)) {
            memcpy(usbfs_ep0_buffer, data, data_len);
        }
        USBFSD->UEP0_DMA = (uint32_t)usbfs_ep0_buffer;
        USBFSD->UEP0_TX_LEN = (uint8_t)xfer_len;
        USBFSD->UEP0_TX_CTRL = (uint8_t)(ctrl | USBFS_UEP_T_RES_ACK);
        g_ch32h417_usbfs_diag.ep0_prime_in++;
        g_ch32h417_usbfs_diag.last_tx_len = xfer_len;
        g_ch32h417_usbfs_diag.last_resp0 = usbfs_pack4(usbfs_ep0_buffer);
    } else {
        if ((data != RT_NULL) && (data_len != 0U)) {
            memcpy(usbfs_ep_tx_buffer[ep_idx], data, data_len);
        }
        *usbfs_ep_dma_reg(ep_idx) = (uint32_t)usbfs_ep_tx_buffer[ep_idx];
        *usbfs_ep_tx_len_reg(ep_idx) = xfer_len;
        *usbfs_ep_tx_ctrl_reg(ep_idx) =
            (uint8_t)((*usbfs_ep_tx_ctrl_reg(ep_idx) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_AUTO_TOG | USBFS_UEP_T_RES_ACK);
    }
}

static void usbfs_ep_prime_out(uint8_t ep_idx)
{
    uint8_t ctrl;

    if (ep_idx == 0U) {
        ctrl = (g_ch32h417_usbfs_udc.out_ep[0].ep_toggle != 0U) ? USBFS_UEP_R_TOG : 0U;
        USBFSD->UEP0_DMA = (uint32_t)usbfs_ep0_buffer;
        USBFSD->UEP0_RX_CTRL = (uint8_t)(ctrl | USBFS_UEP_R_RES_ACK);
        g_ch32h417_usbfs_diag.ep0_prime_out++;
    } else {
        *usbfs_ep_dma_reg(ep_idx) = (uint32_t)usbfs_ep_rx_buffer[ep_idx];
        *usbfs_ep_rx_ctrl_reg(ep_idx) =
            (uint8_t)((*usbfs_ep_rx_ctrl_reg(ep_idx) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_AUTO_TOG | USBFS_UEP_R_RES_ACK);
    }
}

static void usbfs_handle_setup_packet(uint8_t busid)
{
    g_ch32h417_usbfs_diag.setup++;
    g_ch32h417_usbfs_diag.last_setup0 = usbfs_pack4(usbfs_ep0_buffer);
    g_ch32h417_usbfs_diag.last_setup1 = usbfs_pack4(usbfs_ep0_buffer + 4);

    g_ch32h417_usbfs_udc.out_ep[0].ep_toggle = 1U;
    g_ch32h417_usbfs_udc.out_ep[0].actual_xfer_len = 0U;
    g_ch32h417_usbfs_udc.out_ep[0].xfer_len = 0U;
    g_ch32h417_usbfs_udc.out_ep[0].xfer_buf = RT_NULL;
    g_ch32h417_usbfs_udc.in_ep[0].ep_toggle = 1U;
    g_ch32h417_usbfs_udc.in_ep[0].actual_xfer_len = 0U;
    g_ch32h417_usbfs_udc.in_ep[0].xfer_len = 0U;
    g_ch32h417_usbfs_udc.in_ep[0].xfer_buf = RT_NULL;

    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_NAK;
    USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_NAK;
    usbd_event_ep0_setup_complete_handler(busid, usbfs_ep0_buffer);
}

static void usbfs_handle_ep_in_xfer_complete(uint8_t busid, uint8_t ep_idx)
{
    struct usb_dc_ep_state *ep = &g_ch32h417_usbfs_udc.in_ep[ep_idx];
    uint32_t sent_len = (ep_idx == 0U) ? USBFSD->UEP0_TX_LEN : *usbfs_ep_tx_len_reg(ep_idx);

    if (sent_len > ep->xfer_len) {
        sent_len = ep->xfer_len;
    }

    ep->actual_xfer_len += sent_len;
    ep->xfer_len -= sent_len;

    if (ep_idx == 0U) {
        g_ch32h417_usbfs_diag.ep0_in++;
        g_ch32h417_usbfs_diag.last_tx_len = sent_len;
        g_ch32h417_usbfs_udc.in_ep[0].ep_toggle ^= 1U;
    }

    if (ep->xfer_len == 0U) {
        if (ep_idx == 0U) {
            usbfs_apply_pending_address();
            usbd_event_ep_in_complete_handler(busid, USB_CONTROL_IN_EP0, ep->actual_xfer_len);
        } else {
            *usbfs_ep_tx_ctrl_reg(ep_idx) =
                (uint8_t)((*usbfs_ep_tx_ctrl_reg(ep_idx) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_AUTO_TOG | USBFS_UEP_T_RES_NAK);
            usbd_event_ep_in_complete_handler(busid, (uint8_t)(ep_idx | 0x80U), ep->actual_xfer_len);
        }
    } else {
        uint32_t next_len = (ep->xfer_len > ep->ep_mps) ? ep->ep_mps : ep->xfer_len;
        usbfs_ep_prime_in(ep_idx, ep->xfer_buf + ep->actual_xfer_len, next_len);
    }
}

static void usbfs_handle_ep_out_xfer_complete(uint8_t busid, uint8_t ep_idx)
{
    struct usb_dc_ep_state *ep = &g_ch32h417_usbfs_udc.out_ep[ep_idx];
    uint32_t rx_len = USBFSD->RX_LEN;

    if (ep_idx == 0U) {
        g_ch32h417_usbfs_diag.ep0_out++;
        g_ch32h417_usbfs_diag.last_rx_len = rx_len;
    }

    if ((ep_idx != 0U) && ((USBFSD->INT_ST & USBFS_UIS_TOG_OK) == 0U)) {
        usbfs_ep_prime_out(ep_idx);
        return;
    }

    if ((ep->xfer_buf != RT_NULL) && (rx_len != 0U)) {
        uint32_t room = (ep->xfer_len > ep->actual_xfer_len) ? (ep->xfer_len - ep->actual_xfer_len) : 0U;
        if (rx_len > room) {
            rx_len = room;
        }
        if (rx_len != 0U) {
            memcpy(ep->xfer_buf + ep->actual_xfer_len,
                   (ep_idx == 0U) ? usbfs_ep0_buffer : usbfs_ep_rx_buffer[ep_idx],
                   rx_len);
        }
    }

    ep->actual_xfer_len += rx_len;
    if (ep->xfer_len >= rx_len) {
        ep->xfer_len -= rx_len;
    } else {
        ep->xfer_len = 0U;
    }

    if (ep_idx == 0U) {
        ep->ep_toggle ^= 1U;
    }

    if ((ep->xfer_len == 0U) || (rx_len < ep->ep_mps)) {
        if (ep_idx == 0U) {
            usbd_event_ep_out_complete_handler(busid, USB_CONTROL_OUT_EP0, ep->actual_xfer_len);
        } else {
            *usbfs_ep_rx_ctrl_reg(ep_idx) =
                (uint8_t)((*usbfs_ep_rx_ctrl_reg(ep_idx) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_AUTO_TOG | USBFS_UEP_R_RES_NAK);
            usbd_event_ep_out_complete_handler(busid, ep_idx, ep->actual_xfer_len);
        }
    } else {
        usbfs_ep_prime_out(ep_idx);
    }
}

static int usbfs_clock_init(void)
{
    uint32_t timeout;

    if ((RCC->CTLR & RCC_USBHS_PLLRDY) == 0U) {
        RCC_USBHS_PLLCmd(DISABLE);
        RCC_USBHSPLLCLKConfig((RCC->CTLR & RCC_HSERDY) ? RCC_USBHSPLLSource_HSE : RCC_USBHSPLLSource_HSI);
        RCC_USBHSPLLReferConfig(RCC_USBHSPLLRefer_25M);
        RCC_USBHSPLLClockSourceDivConfig(RCC_USBHSPLL_IN_Div1);
        RCC_USBHS_PLLCmd(ENABLE);

        for (timeout = 0U; timeout < USBFS_INIT_TIMEOUT; timeout++) {
            if ((RCC->CTLR & RCC_USBHS_PLLRDY) != 0U) {
                break;
            }
        }
        if (timeout >= USBFS_INIT_TIMEOUT) {
            rt_kprintf("[USBFS] USBHS PLL ready timeout\r\n");
            return -1;
        }
    }

    RCC->CFGR2 = (RCC->CFGR2 & (uint32_t)~USBFS_CFGR2_MASK) | (uint32_t)USBFS_CFGR2_48M;
    if ((RCC->CFGR2 & USBFS_CFGR2_MASK) != USBFS_CFGR2_48M) {
        rt_kprintf("[USBFS] warning: USBFS 48M clock cfg readback failed cfgr2=0x%08x expected=0x%08x\r\n",
                   (unsigned int)RCC->CFGR2,
                   (unsigned int)USBFS_CFGR2_48M);
    }
    RCC_HBPeriphClockCmd(RCC_HBPeriph_OTG_FS, ENABLE);
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOA, ENABLE);
    return 0;
}

int usb_dc_ch32h417_usbfs_init(uint8_t busid)
{
    uint8_t int_en;

    (void)busid;

    if (usbfs_clock_init() != 0) {
        return -1;
    }

    NVIC_DisableIRQ(USBFS_IRQn);
    memset(&g_ch32h417_usbfs_udc, 0, sizeof(g_ch32h417_usbfs_udc));
    g_ch32h417_usbfs_udc.port_speed = USB_SPEED_FULL;
    memset((void *)USBFS_DMA_RAM_BASE, 0, USBFS_DMA_RAM_SIZE);

    USBFSD->OTG_CR = 0U;
    USBFSD->UDEV_CTRL = 0U;
    USBFSD->INT_EN = 0U;
    USBFSD->BASE_CTRL = 0U;
    rt_thread_mdelay(120);

    USBFSH->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
    usbfs_delay(8000U);
    USBFSH->BASE_CTRL = 0U;
    USBFSD->OTG_CR = 0U;

    USBFSD->DEV_ADDR = 0U;
    USBFSD->UEP4_1_MOD = 0U;
    USBFSD->UEP2_3_MOD = 0U;
    USBFSD->UEP5_6_MOD = 0U;
    USBFSD->UEP7_MOD = 0U;
    usbfs_ep0_state_init();

    USBFSD->INT_FG = 0xffU;
    int_en = USBFS_UIE_BUS_RST | USBFS_UIE_TRANSFER;
#if USBFS_ENABLE_SUSPEND_IRQ
    int_en |= USBFS_UIE_SUSPEND;
#endif
    USBFSD->INT_EN = int_en;
    USBFSD->BASE_CTRL = USBFS_UC_DEV_PU_EN | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
    USBFSD->UDEV_CTRL = USBFS_UD_PD_DIS | USBFS_UD_PORT_EN;

    NVIC_SetPriority(USBFS_IRQn, 1);
    NVIC_EnableIRQ(USBFS_IRQn);

    rt_kprintf("[USBFS] device init done bc=0x%02x uc=0x%02x ie=0x%02x fg=0x%02x ms=0x%02x otg=0x%08x/0x%08x dma=%08x/%08x rctl=0x%08x cfgr2=0x%08x pcfg=0x%08x irq=%u/%u\r\n",
               (unsigned int)USBFSD->BASE_CTRL,
               (unsigned int)USBFSD->UDEV_CTRL,
               (unsigned int)USBFSD->INT_EN,
               (unsigned int)USBFSD->INT_FG,
               (unsigned int)USBFSD->MIS_ST,
               (unsigned int)USBFSD->OTG_CR,
               (unsigned int)USBFSD->OTG_SR,
               (unsigned int)USBFS_DMA_RAM_BASE,
               (unsigned int)USBFSD->UEP0_DMA,
               (unsigned int)RCC->CTLR,
               (unsigned int)RCC->CFGR2,
               (unsigned int)RCC->PLLCFGR,
               (unsigned int)usbfs_irq_is_enabled(USBFS_IRQn),
               (unsigned int)usbfs_irq_is_pending(USBFS_IRQn));
    return 0;
}

int usb_dc_ch32h417_usbfs_deinit(uint8_t busid)
{
    (void)busid;

    NVIC_DisableIRQ(USBFS_IRQn);
    USBFSH->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
    usbfs_delay(1000U);
    USBFSD->OTG_CR = 0U;
    USBFSD->BASE_CTRL = 0U;
    RCC_HBPeriphClockCmd(RCC_HBPeriph_OTG_FS, DISABLE);
    return 0;
}

int usb_dc_ch32h417_usbfs_set_address(uint8_t busid, uint8_t addr)
{
    (void)busid;
    g_ch32h417_usbfs_udc.dev_addr = addr;
    g_ch32h417_usbfs_udc.addr_pending = 1U;
    g_ch32h417_usbfs_diag.set_addr++;
    g_ch32h417_usbfs_diag.last_addr = addr;
    return 0;
}

int usb_dc_ch32h417_usbfs_set_remote_wakeup(uint8_t busid)
{
    (void)busid;
    return -1;
}

uint8_t usb_dc_ch32h417_usbfs_get_port_speed(uint8_t busid)
{
    (void)busid;
    return USB_SPEED_FULL;
}

int usb_dc_ch32h417_usbfs_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep->bEndpointAddress);
    uint16_t ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
    uint8_t ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);

    (void)busid;

    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    if (ep_idx == 0U) {
        usbfs_ep0_state_init();
        return 0;
    }

    if ((ep_mps == 0U) || (ep_mps > USBFS_EP_BUFFER_SIZE)) {
        ep_mps = USBFS_EP_BUFFER_SIZE;
    }

    if (USB_EP_DIR_IS_OUT(ep->bEndpointAddress)) {
        g_ch32h417_usbfs_udc.out_ep[ep_idx].ep_mps = ep_mps;
        g_ch32h417_usbfs_udc.out_ep[ep_idx].ep_type = ep_type;
        g_ch32h417_usbfs_udc.out_ep[ep_idx].ep_enabled = 1U;
        g_ch32h417_usbfs_udc.out_ep[ep_idx].ep_stalled = 0U;
        *usbfs_ep_dma_reg(ep_idx) = (uint32_t)usbfs_ep_rx_buffer[ep_idx];
        *usbfs_ep_rx_ctrl_reg(ep_idx) = USBFS_UEP_R_AUTO_TOG | USBFS_UEP_R_RES_NAK;
        usbfs_ep_mod_update(ep_idx, 0U, 1U);
    } else {
        g_ch32h417_usbfs_udc.in_ep[ep_idx].ep_mps = ep_mps;
        g_ch32h417_usbfs_udc.in_ep[ep_idx].ep_type = ep_type;
        g_ch32h417_usbfs_udc.in_ep[ep_idx].ep_enabled = 1U;
        g_ch32h417_usbfs_udc.in_ep[ep_idx].ep_stalled = 0U;
        *usbfs_ep_dma_reg(ep_idx) = (uint32_t)usbfs_ep_tx_buffer[ep_idx];
        *usbfs_ep_tx_len_reg(ep_idx) = 0U;
        *usbfs_ep_tx_ctrl_reg(ep_idx) = USBFS_UEP_T_AUTO_TOG | USBFS_UEP_T_RES_NAK;
        usbfs_ep_mod_update(ep_idx, 1U, 1U);
    }

    return 0;
}

int usb_dc_ch32h417_usbfs_ep_close(uint8_t busid, uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    (void)busid;

    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    if (ep_idx == 0U) {
        g_ch32h417_usbfs_udc.in_ep[0].ep_enabled = 0U;
        g_ch32h417_usbfs_udc.out_ep[0].ep_enabled = 0U;
    } else if (USB_EP_DIR_IS_OUT(ep)) {
        g_ch32h417_usbfs_udc.out_ep[ep_idx].ep_enabled = 0U;
        usbfs_ep_mod_update(ep_idx, 0U, 0U);
    } else {
        g_ch32h417_usbfs_udc.in_ep[ep_idx].ep_enabled = 0U;
        usbfs_ep_mod_update(ep_idx, 1U, 0U);
    }

    return 0;
}

int usb_dc_ch32h417_usbfs_set_stall(uint8_t busid, uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    (void)busid;

    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    if (ep_idx == 0U) {
        USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_STALL;
        USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_STALL;
        g_ch32h417_usbfs_udc.in_ep[0].ep_stalled = 1U;
        g_ch32h417_usbfs_udc.out_ep[0].ep_stalled = 1U;
        g_ch32h417_usbfs_diag.stall++;
    } else if (USB_EP_DIR_IS_OUT(ep)) {
        *usbfs_ep_rx_ctrl_reg(ep_idx) =
            (uint8_t)((*usbfs_ep_rx_ctrl_reg(ep_idx) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_AUTO_TOG | USBFS_UEP_R_RES_STALL);
        g_ch32h417_usbfs_udc.out_ep[ep_idx].ep_stalled = 1U;
    } else {
        *usbfs_ep_tx_ctrl_reg(ep_idx) =
            (uint8_t)((*usbfs_ep_tx_ctrl_reg(ep_idx) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_AUTO_TOG | USBFS_UEP_T_RES_STALL);
        g_ch32h417_usbfs_udc.in_ep[ep_idx].ep_stalled = 1U;
    }

    return 0;
}

int usb_dc_ch32h417_usbfs_clear_stall(uint8_t busid, uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    (void)busid;

    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    if (ep_idx == 0U) {
        USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
        USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
        g_ch32h417_usbfs_udc.in_ep[0].ep_stalled = 0U;
        g_ch32h417_usbfs_udc.out_ep[0].ep_stalled = 0U;
        g_ch32h417_usbfs_udc.in_ep[0].ep_toggle = 0U;
        g_ch32h417_usbfs_udc.out_ep[0].ep_toggle = 0U;
    } else if (USB_EP_DIR_IS_OUT(ep)) {
        *usbfs_ep_rx_ctrl_reg(ep_idx) = USBFS_UEP_R_AUTO_TOG | USBFS_UEP_R_RES_ACK;
        g_ch32h417_usbfs_udc.out_ep[ep_idx].ep_stalled = 0U;
    } else {
        *usbfs_ep_tx_ctrl_reg(ep_idx) = USBFS_UEP_T_AUTO_TOG | USBFS_UEP_T_RES_NAK;
        g_ch32h417_usbfs_udc.in_ep[ep_idx].ep_stalled = 0U;
    }

    return 0;
}

int usb_dc_ch32h417_usbfs_is_stalled(uint8_t busid, uint8_t ep, uint8_t *stalled)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    (void)busid;

    if ((ep_idx >= USB_CH32H417_MAX_EP_NUM) || (stalled == RT_NULL)) {
        return -1;
    }

    *stalled = USB_EP_DIR_IS_OUT(ep) ?
               g_ch32h417_usbfs_udc.out_ep[ep_idx].ep_stalled :
               g_ch32h417_usbfs_udc.in_ep[ep_idx].ep_stalled;
    return 0;
}

int usb_dc_ch32h417_usbfs_start_write(uint8_t busid, uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t ep_mps;
    uint32_t send_len;

    (void)busid;

    if ((data == RT_NULL) && (data_len != 0U)) {
        return -1;
    }
    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    ep_mps = g_ch32h417_usbfs_udc.in_ep[ep_idx].ep_mps;
    if (ep_mps == 0U) {
        ep_mps = USB_CH32H417_FS_EP0_MPS;
    }

    g_ch32h417_usbfs_udc.in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_ch32h417_usbfs_udc.in_ep[ep_idx].xfer_len = data_len;
    g_ch32h417_usbfs_udc.in_ep[ep_idx].actual_xfer_len = 0U;

    send_len = (data_len > ep_mps) ? ep_mps : data_len;
    usbfs_ep_prime_in(ep_idx, data, send_len);
    return 0;
}

int usb_dc_ch32h417_usbfs_start_read(uint8_t busid, uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    (void)busid;

    if ((data == RT_NULL) && (data_len != 0U)) {
        return -1;
    }
    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    g_ch32h417_usbfs_udc.out_ep[ep_idx].xfer_buf = data;
    g_ch32h417_usbfs_udc.out_ep[ep_idx].xfer_len = data_len;
    g_ch32h417_usbfs_udc.out_ep[ep_idx].actual_xfer_len = 0U;
    usbfs_ep_prime_out(ep_idx);
    return 0;
}

static void usbfs_irq_handler(uint8_t busid)
{
    uint8_t intflag = USBFSD->INT_FG;
    uint8_t intst = USBFSD->INT_ST;
    uint8_t ep_idx;

    g_ch32h417_usbfs_diag.irq++;
    g_ch32h417_usbfs_diag.last_intflag = intflag;
    g_ch32h417_usbfs_diag.last_intst = intst;

    if ((intflag & USBFS_UIF_TRANSFER) != 0U) {
        g_ch32h417_usbfs_diag.transfer++;
        ep_idx = (uint8_t)(intst & USBFS_UIS_ENDP_MASK);
        g_ch32h417_usbfs_diag.last_xfer_intflag = intflag;
        g_ch32h417_usbfs_diag.last_xfer_intst = intst;
        g_ch32h417_usbfs_diag.last_xfer_ep = ep_idx;
        g_ch32h417_usbfs_diag.last_xfer_token = (uint8_t)(intst & USBFS_UIS_TOKEN_MASK);
        g_ch32h417_usbfs_diag.last_xfer_rx_len = USBFSD->RX_LEN;
        g_ch32h417_usbfs_diag.last_xfer_buf0 = usbfs_pack4(usbfs_ep0_buffer);
        g_ch32h417_usbfs_diag.last_xfer_buf1 = usbfs_pack4(usbfs_ep0_buffer + 4);
        if (((intst & USBFS_UIS_TOKEN_MASK) == USBFS_UIS_TOKEN_SETUP) &&
            (usbfs_ep0_buffer_has_setup() != 0U)) {
            if (((intst & USBFS_UIS_IS_NAK) != 0U) || (ep_idx != 0U)) {
                g_ch32h417_usbfs_diag.setup_recovered++;
            }
            usbfs_handle_setup_packet(busid);
        } else if ((intst & USBFS_UIS_IS_NAK) != 0U) {
            g_ch32h417_usbfs_diag.transfer_nak++;
        } else if (ep_idx < USB_CH32H417_MAX_EP_NUM) {
            switch (intst & USBFS_UIS_TOKEN_MASK) {
            case USBFS_UIS_TOKEN_SETUP:
                if (ep_idx == 0U) {
                    usbfs_handle_setup_packet(busid);
                }
                break;
            case USBFS_UIS_TOKEN_IN:
                usbfs_handle_ep_in_xfer_complete(busid, ep_idx);
                break;
            case USBFS_UIS_TOKEN_OUT:
                usbfs_handle_ep_out_xfer_complete(busid, ep_idx);
                break;
            default:
                break;
            }
        } else {
            g_ch32h417_usbfs_diag.transfer_bad_ep++;
        }
        USBFSD->INT_FG = USBFS_UIF_TRANSFER;
    } else if ((intflag & USBFS_UIF_BUS_RST) != 0U) {
        g_ch32h417_usbfs_diag.bus_reset++;
        USBFSD->DEV_ADDR = 0U;
        USBFSD->UEP4_1_MOD = 0U;
        USBFSD->UEP2_3_MOD = 0U;
        USBFSD->UEP5_6_MOD = 0U;
        USBFSD->UEP7_MOD = 0U;
        usbfs_clear_transfer_state();
        USBFSD->INT_FG = USBFS_UIF_BUS_RST;
        usbd_event_reset_handler(busid);
    } else if ((intflag & USBFS_UIF_SUSPEND) != 0U) {
        if ((USBFSD->MIS_ST & USBFS_UMS_SUSPEND) != 0U) {
            g_ch32h417_usbfs_diag.suspend++;
            usbd_event_suspend_handler(busid);
        } else {
            g_ch32h417_usbfs_diag.resume++;
            usbd_event_resume_handler(busid);
        }
        USBFSD->INT_FG = USBFS_UIF_SUSPEND;
    } else {
        USBFSD->INT_FG = intflag;
    }
}

void usb_dc_ch32h417_usbfs_dump_diag(void)
{
    uint32_t dma = USBFSD->UEP0_DMA;

    rt_kprintf("[USBFS diag] irq=%u rst=%u sus=%u res=%u wk=%u xfer=%u setup=%u rec=%u pIN0=%u in0=%u pOUT0=%u out0=%u stall=%u addr=%u\r\n",
               (unsigned int)g_ch32h417_usbfs_diag.irq,
               (unsigned int)g_ch32h417_usbfs_diag.bus_reset,
               (unsigned int)g_ch32h417_usbfs_diag.suspend,
               (unsigned int)g_ch32h417_usbfs_diag.resume,
               (unsigned int)g_ch32h417_usbfs_diag.wakeup_irq,
               (unsigned int)g_ch32h417_usbfs_diag.transfer,
               (unsigned int)g_ch32h417_usbfs_diag.setup,
               (unsigned int)g_ch32h417_usbfs_diag.setup_recovered,
               (unsigned int)g_ch32h417_usbfs_diag.ep0_prime_in,
               (unsigned int)g_ch32h417_usbfs_diag.ep0_in,
               (unsigned int)g_ch32h417_usbfs_diag.ep0_prime_out,
               (unsigned int)g_ch32h417_usbfs_diag.ep0_out,
               (unsigned int)g_ch32h417_usbfs_diag.stall,
               (unsigned int)g_ch32h417_usbfs_diag.last_addr);
    rt_kprintf("[USBFS diag] if=0x%02x st=0x%02x setup=%08x/%08x resp=%08x tx=%u rx=%u\r\n",
               (unsigned int)g_ch32h417_usbfs_diag.last_intflag,
               (unsigned int)g_ch32h417_usbfs_diag.last_intst,
               (unsigned int)g_ch32h417_usbfs_diag.last_setup0,
               (unsigned int)g_ch32h417_usbfs_diag.last_setup1,
               (unsigned int)g_ch32h417_usbfs_diag.last_resp0,
               (unsigned int)g_ch32h417_usbfs_diag.last_tx_len,
               (unsigned int)g_ch32h417_usbfs_diag.last_rx_len);
    rt_kprintf("[USBFS xfer] nak=%u bad=%u xif=0x%02x xst=0x%02x ep=%u tok=0x%02x xrx=%u buf=%08x/%08x\r\n",
               (unsigned int)g_ch32h417_usbfs_diag.transfer_nak,
               (unsigned int)g_ch32h417_usbfs_diag.transfer_bad_ep,
               (unsigned int)g_ch32h417_usbfs_diag.last_xfer_intflag,
               (unsigned int)g_ch32h417_usbfs_diag.last_xfer_intst,
               (unsigned int)g_ch32h417_usbfs_diag.last_xfer_ep,
               (unsigned int)g_ch32h417_usbfs_diag.last_xfer_token,
               (unsigned int)g_ch32h417_usbfs_diag.last_xfer_rx_len,
               (unsigned int)g_ch32h417_usbfs_diag.last_xfer_buf0,
               (unsigned int)g_ch32h417_usbfs_diag.last_xfer_buf1);
    rt_kprintf("[USBFS regs] bc=0x%02x uc=0x%02x ie=0x%02x fg=0x%02x ms=0x%02x da=0x%02x rxlen=%u e0t=0x%02x e0r=0x%02x dma=0x%08x\r\n",
               (unsigned int)USBFSD->BASE_CTRL,
               (unsigned int)USBFSD->UDEV_CTRL,
               (unsigned int)USBFSD->INT_EN,
               (unsigned int)USBFSD->INT_FG,
               (unsigned int)USBFSD->MIS_ST,
               (unsigned int)USBFSD->DEV_ADDR,
               (unsigned int)USBFSD->RX_LEN,
               (unsigned int)USBFSD->UEP0_TX_CTRL,
               (unsigned int)USBFSD->UEP0_RX_CTRL,
               (unsigned int)dma);
    rt_kprintf("[USBFS regs2] otg=0x%08x/0x%08x rctl=0x%08x cfgr2=0x%08x pll2=0x%08x hb=0x%08x irq=%u/%u/%u w=%u/%u/%u\r\n",
               (unsigned int)USBFSD->OTG_CR,
               (unsigned int)USBFSD->OTG_SR,
               (unsigned int)RCC->CTLR,
               (unsigned int)RCC->CFGR2,
               (unsigned int)RCC->PLLCFGR2,
               (unsigned int)RCC->HBPCENR,
               (unsigned int)usbfs_irq_is_enabled(USBFS_IRQn),
               (unsigned int)usbfs_irq_is_pending(USBFS_IRQn),
               (unsigned int)usbfs_irq_is_active(USBFS_IRQn),
               (unsigned int)usbfs_irq_is_enabled(USBFSWakeUp_IRQn),
               (unsigned int)usbfs_irq_is_pending(USBFSWakeUp_IRQn),
               (unsigned int)usbfs_irq_is_active(USBFSWakeUp_IRQn));
}

void USBFS_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USBFS_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    usbfs_irq_handler(USB_CH32H417_BUS_FS);
    rt_interrupt_leave();
    FREE_INT_SP();
}

void USBFSWakeup_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USBFSWakeup_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    g_ch32h417_usbfs_diag.wakeup_irq++;
    USBFSD->INT_FG = USBFS_UIF_SUSPEND;
    rt_interrupt_leave();
    FREE_INT_SP();
}
