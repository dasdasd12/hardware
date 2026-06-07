/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usb_dc_ch32h417_usbhs.h"

#include <string.h>

#include "ch32h417.h"
#include "board.h"

#define USBHS_INIT_TIMEOUT 1000000U
#define USBHS_EP_BUFFER_SIZE 1024U

/* USBHS register offsets (from USBHS base address) -- verified against USBHSD_TypeDef */
#define USBHS_OFFSET_USB_CTRL       0x00U
#define USBHS_OFFSET_USB_BASE_MODE  0x01U
#define USBHS_OFFSET_USB_INT_EN     0x02U
#define USBHS_OFFSET_USB_DEV_AD     0x03U
#define USBHS_OFFSET_USB_INT_FG     0x08U
#define USBHS_OFFSET_USB_INT_ST     0x09U
#define USBHS_OFFSET_USB_MIS_ST     0x0AU
#define USBHS_OFFSET_UEP_TX_EN      0x10U
#define USBHS_OFFSET_UEP_RX_EN      0x12U
#define USBHS_OFFSET_UEP0_DMA       0x20U
#define USBHS_OFFSET_UEP1_RX_DMA    0x24U
#define USBHS_OFFSET_UEP1_TX_DMA    0x40U
#define USBHS_OFFSET_UEP0_MAX_LEN   0x5CU
#define USBHS_OFFSET_UEP1_MAX_LEN   0x60U
#define USBHS_OFFSET_UEP0_RX_LEN    0x7CU
#define USBHS_OFFSET_UEP1_RX_LEN    0x80U
#define USBHS_OFFSET_UEP0_TX_LEN    0x9CU
#define USBHS_OFFSET_UEP1_TX_LEN    0xA0U
#define USBHS_OFFSET_UEP0_TX_CTRL   0x9EU
#define USBHS_OFFSET_UEP0_RX_CTRL   0x9FU
#define USBHS_OFFSET_UEP1_TX_CTRL   0xA2U
#define USBHS_OFFSET_UEP1_RX_CTRL   0xA3U

/* The following macros are already defined in ch32h417_usb.h (included via ch32h417.h):
 * USBHS_UD_RST_LINK, USBHS_UD_RST_SIE, USBHS_UD_CLR_ALL, USBHS_UD_PHY_SUSPENDM,
 * USBHS_UD_DMA_EN, USBHS_UD_DEV_EN, USBHS_UD_LPM_EN,
 * USBHS_UD_SPEED_HIGH, USBHS_UD_SPEED_FULL,
 * USBHS_UDIE_BUS_RST, USBHS_UDIE_SUSPEND, USBHS_UDIE_BUS_SLEEP, USBHS_UDIE_LPM_ACT,
 * USBHS_UDIE_TRANSFER, USBHS_UDIE_SOF_ACT, USBHS_UDIE_LINK_RDY, USBHS_UDIE_FIFO_OVER,
 * USBHS_UDIF_BUS_RST, USBHS_UDIF_SUSPEND, USBHS_UDIF_BUS_SLEEP, USBHS_UDIF_LPM_ACT,
 * USBHS_UDIF_TRANSFER, USBHS_UDIF_RX_SOF, USBHS_UDIF_LINK_RDY, USBHS_UDIF_FIFO_OV
 */

#define USBHS_UD_INT_EP_ID_MASK 0x07U
#define USBHS_UD_INT_DIR_IN     (1U << 4)
#define USBHS_UD_INT_DIR_OUT    (0U)

/* Compatibility aliases for endpoint control macros (SDK uses T/R suffixes) */
#define USBHS_UEP_TX_RES_NAK    USBHS_UEP_T_RES_NAK
#define USBHS_UEP_TX_RES_STALL  USBHS_UEP_T_RES_STALL
#define USBHS_UEP_TX_RES_ACK    USBHS_UEP_T_RES_ACK
#define USBHS_UEP_TX_D0         USBHS_UEP_T_TOG_DATA0
#define USBHS_UEP_TX_D1         USBHS_UEP_T_TOG_DATA1
#define USBHS_UEP_TX_DONE       USBHS_UEP_T_DONE

#define USBHS_UEP_RX_RES_NAK    USBHS_UEP_R_RES_NAK
#define USBHS_UEP_RX_RES_STALL  USBHS_UEP_R_RES_STALL
#define USBHS_UEP_RX_RES_ACK    USBHS_UEP_R_RES_ACK
#define USBHS_UEP_RX_D0         USBHS_UEP_R_TOG_DATA0
#define USBHS_UEP_RX_D1         USBHS_UEP_R_TOG_DATA1
#define USBHS_UEP_RX_TOG_MATCH  USBHS_UEP_R_TOG_MATCH
#define USBHS_UEP_RX_SETUP_IS   USBHS_UEP_R_SETUP_IS
#define USBHS_UEP_RX_DONE       USBHS_UEP_R_DONE

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

struct ch32h417_usbhs_udc {
    volatile uint8_t dev_addr;
    uint8_t addr_pending;
    uint8_t port_speed;
    struct usb_dc_ep_state in_ep[USB_CH32H417_MAX_EP_NUM];
    struct usb_dc_ep_state out_ep[USB_CH32H417_MAX_EP_NUM];
};

static struct ch32h417_usbhs_udc g_ch32h417_usbhs_udc;

static __attribute__((aligned(4))) uint8_t ep0_buffer[USB_CH32H417_EP0_MPS];
static __attribute__((aligned(4))) uint8_t ep_tx_buffer[USB_CH32H417_MAX_EP_NUM][USBHS_EP_BUFFER_SIZE];
static __attribute__((aligned(4))) uint8_t ep_rx_buffer[USB_CH32H417_MAX_EP_NUM][USBHS_EP_BUFFER_SIZE];

static inline __IO uint8_t *usbhs_reg8(uint32_t offset)
{
    return (__IO uint8_t *)((uintptr_t)USBHS_BASE + offset);
}

static inline __IO uint16_t *usbhs_reg16(uint32_t offset)
{
    return (__IO uint16_t *)((uintptr_t)USBHS_BASE + offset);
}

static inline __IO uint32_t *usbhs_reg32(uint32_t offset)
{
    return (__IO uint32_t *)((uintptr_t)USBHS_BASE + offset);
}

static inline uint32_t usbhs_ep_rx_dma_offset(uint8_t ep_idx)
{
    return (ep_idx == 0U) ? USBHS_OFFSET_UEP0_DMA : (USBHS_OFFSET_UEP1_RX_DMA + ((uint32_t)(ep_idx - 1U) * 4U));
}

static inline uint32_t usbhs_ep_tx_dma_offset(uint8_t ep_idx)
{
    return (ep_idx == 0U) ? USBHS_OFFSET_UEP0_DMA : (USBHS_OFFSET_UEP1_TX_DMA + ((uint32_t)(ep_idx - 1U) * 4U));
}

static inline uint32_t usbhs_ep_max_len_offset(uint8_t ep_idx)
{
    return (ep_idx == 0U) ? USBHS_OFFSET_UEP0_MAX_LEN : (USBHS_OFFSET_UEP1_MAX_LEN + ((uint32_t)(ep_idx - 1U) * 4U));
}

static inline uint32_t usbhs_ep_rx_len_offset(uint8_t ep_idx)
{
    return (ep_idx == 0U) ? USBHS_OFFSET_UEP0_RX_LEN : (USBHS_OFFSET_UEP1_RX_LEN + ((uint32_t)(ep_idx - 1U) * 4U));
}

static inline uint32_t usbhs_ep_tx_len_offset(uint8_t ep_idx)
{
    return (ep_idx == 0U) ? USBHS_OFFSET_UEP0_TX_LEN : (USBHS_OFFSET_UEP1_TX_LEN + ((uint32_t)(ep_idx - 1U) * 4U));
}

static inline uint32_t usbhs_ep_rx_ctrl_offset(uint8_t ep_idx)
{
    return (ep_idx == 0U) ? USBHS_OFFSET_UEP0_RX_CTRL : (USBHS_OFFSET_UEP1_RX_CTRL + ((uint32_t)(ep_idx - 1U) * 4U));
}

static inline uint32_t usbhs_ep_tx_ctrl_offset(uint8_t ep_idx)
{
    return (ep_idx == 0U) ? USBHS_OFFSET_UEP0_TX_CTRL : (USBHS_OFFSET_UEP1_TX_CTRL + ((uint32_t)(ep_idx - 1U) * 4U));
}

static inline __IO uint16_t *usbhs_ep_max_len_reg(uint8_t ep_idx)
{
    return usbhs_reg16(usbhs_ep_max_len_offset(ep_idx));
}

static inline __IO uint32_t *usbhs_ep_rx_dma_reg(uint8_t ep_idx)
{
    return usbhs_reg32(usbhs_ep_rx_dma_offset(ep_idx));
}

static inline __IO uint32_t *usbhs_ep_tx_dma_reg(uint8_t ep_idx)
{
    return usbhs_reg32(usbhs_ep_tx_dma_offset(ep_idx));
}

static inline __IO uint16_t *usbhs_ep_rx_len_reg(uint8_t ep_idx)
{
    return usbhs_reg16(usbhs_ep_rx_len_offset(ep_idx));
}

static inline __IO uint16_t *usbhs_ep_tx_len_reg(uint8_t ep_idx)
{
    return usbhs_reg16(usbhs_ep_tx_len_offset(ep_idx));
}

static inline __IO uint8_t *usbhs_ep_rx_ctrl_reg(uint8_t ep_idx)
{
    return usbhs_reg8(usbhs_ep_rx_ctrl_offset(ep_idx));
}

static inline __IO uint8_t *usbhs_ep_tx_ctrl_reg(uint8_t ep_idx)
{
    return usbhs_reg8(usbhs_ep_tx_ctrl_offset(ep_idx));
}

__WEAK void usb_dc_low_level_init(void)
{
    (void)0;
}

__WEAK void usb_dc_low_level_deinit(void)
{
    (void)0;
}

static void usbhs_ep0_state_init(void)
{
    g_ch32h417_usbhs_udc.out_ep[0].ep_mps = USB_CH32H417_EP0_MPS;
    g_ch32h417_usbhs_udc.out_ep[0].ep_type = USB_ENDPOINT_TYPE_CONTROL;
    g_ch32h417_usbhs_udc.out_ep[0].ep_enabled = 1U;
    g_ch32h417_usbhs_udc.out_ep[0].ep_toggle = 0U;
    g_ch32h417_usbhs_udc.out_ep[0].xfer_buf = NULL;
    g_ch32h417_usbhs_udc.out_ep[0].xfer_len = 0U;
    g_ch32h417_usbhs_udc.out_ep[0].actual_xfer_len = 0U;
    g_ch32h417_usbhs_udc.out_ep[0].ep_stalled = 0U;

    g_ch32h417_usbhs_udc.in_ep[0].ep_mps = USB_CH32H417_EP0_MPS;
    g_ch32h417_usbhs_udc.in_ep[0].ep_type = USB_ENDPOINT_TYPE_CONTROL;
    g_ch32h417_usbhs_udc.in_ep[0].ep_enabled = 1U;
    g_ch32h417_usbhs_udc.in_ep[0].ep_toggle = 0U;
    g_ch32h417_usbhs_udc.in_ep[0].xfer_buf = NULL;
    g_ch32h417_usbhs_udc.in_ep[0].xfer_len = 0U;
    g_ch32h417_usbhs_udc.in_ep[0].actual_xfer_len = 0U;
    g_ch32h417_usbhs_udc.in_ep[0].ep_stalled = 0U;

    *usbhs_reg16(USBHS_OFFSET_UEP_TX_EN) |= 1U;
    *usbhs_reg16(USBHS_OFFSET_UEP_RX_EN) |= 1U;
    *usbhs_ep_max_len_reg(0U) = USB_CH32H417_EP0_MPS;
    *usbhs_ep_rx_dma_reg(0U) = (uint32_t)ep0_buffer;
    *usbhs_ep_tx_dma_reg(0U) = (uint32_t)ep0_buffer;
    *usbhs_ep_rx_len_reg(0U) = 0U;
    *usbhs_ep_tx_len_reg(0U) = 0U;
    *usbhs_ep_rx_ctrl_reg(0U) = USBHS_UEP_RX_RES_ACK | USBHS_UEP_RX_D0 | USBHS_UEP_RX_TOG_MATCH;
    *usbhs_ep_tx_ctrl_reg(0U) = USBHS_UEP_TX_RES_ACK | USBHS_UEP_TX_D0;
}

static void usbhs_clear_all_transfer_state(void)
{
    uint8_t ep_idx;

    memset(&g_ch32h417_usbhs_udc, 0, sizeof(g_ch32h417_usbhs_udc));
    for (ep_idx = 0U; ep_idx < USB_CH32H417_MAX_EP_NUM; ep_idx++) {
        g_ch32h417_usbhs_udc.out_ep[ep_idx].ep_mps = 0U;
        g_ch32h417_usbhs_udc.in_ep[ep_idx].ep_mps = 0U;
    }
    usbhs_ep0_state_init();
}

static void usbhs_apply_pending_address(void)
{
    if (g_ch32h417_usbhs_udc.addr_pending != 0U) {
        *usbhs_reg8(USBHS_OFFSET_USB_DEV_AD) = g_ch32h417_usbhs_udc.dev_addr;
        g_ch32h417_usbhs_udc.addr_pending = 0U;
    }
}

static void usbhs_ep_prime_in(uint8_t ep_idx, const uint8_t *data, uint32_t data_len)
{
    uint8_t toggle = (g_ch32h417_usbhs_udc.in_ep[ep_idx].ep_toggle == 0U) ? USBHS_UEP_TX_D0 : USBHS_UEP_TX_D1;
    uint8_t ctrl = (uint8_t)(toggle | USBHS_UEP_TX_RES_ACK);
    uint16_t xfer_len = (data_len > 0xFFFFU) ? 0xFFFFU : (uint16_t)data_len;

    if (ep_idx == 0U) {
        if (data && (data_len != 0U)) {
            memcpy(ep0_buffer, data, data_len);
        }
        *usbhs_ep_tx_dma_reg(0U) = (uint32_t)ep0_buffer;
        *usbhs_ep_tx_len_reg(0U) = xfer_len;
        *usbhs_ep_tx_ctrl_reg(0U) = ctrl;
    } else {
        if (data && (data_len != 0U)) {
            memcpy(ep_tx_buffer[ep_idx], data, data_len);
        }
        *usbhs_ep_tx_dma_reg(ep_idx) = (uint32_t)ep_tx_buffer[ep_idx];
        *usbhs_ep_tx_len_reg(ep_idx) = xfer_len;
        *usbhs_ep_tx_ctrl_reg(ep_idx) = ctrl;
    }
    g_ch32h417_usbhs_udc.in_ep[ep_idx].ep_toggle ^= 1U;
}

static void usbhs_ep_prime_out(uint8_t ep_idx)
{
    uint8_t toggle = (g_ch32h417_usbhs_udc.out_ep[ep_idx].ep_toggle == 0U) ? USBHS_UEP_RX_D0 : USBHS_UEP_RX_D1;
    uint8_t ctrl = (uint8_t)(toggle | USBHS_UEP_RX_RES_ACK | USBHS_UEP_RX_TOG_MATCH);

    if (ep_idx == 0U) {
        *usbhs_ep_rx_dma_reg(0U) = (uint32_t)ep0_buffer;
        *usbhs_ep_rx_len_reg(0U) = 0U;
        *usbhs_ep_rx_ctrl_reg(0U) = ctrl;
    } else {
        *usbhs_ep_rx_dma_reg(ep_idx) = (uint32_t)ep_rx_buffer[ep_idx];
        *usbhs_ep_rx_len_reg(ep_idx) = 0U;
        *usbhs_ep_rx_ctrl_reg(ep_idx) = ctrl;
    }
}

static void usbhs_handle_setup_packet(void)
{
    g_ch32h417_usbhs_udc.out_ep[0].ep_toggle = 0U;
    g_ch32h417_usbhs_udc.out_ep[0].actual_xfer_len = 0U;
    g_ch32h417_usbhs_udc.out_ep[0].xfer_len = 0U;
    g_ch32h417_usbhs_udc.out_ep[0].xfer_buf = NULL;
    g_ch32h417_usbhs_udc.in_ep[0].actual_xfer_len = 0U;
    g_ch32h417_usbhs_udc.in_ep[0].xfer_len = 0U;
    g_ch32h417_usbhs_udc.in_ep[0].xfer_buf = NULL;
    usbd_event_ep0_setup_complete_handler(0U, ep0_buffer);
    usbhs_ep_prime_out(0U);
}

static void usbhs_handle_ep0_in_xfer_complete(uint8_t busid)
{
    struct usb_dc_ep_state *ep0 = &g_ch32h417_usbhs_udc.in_ep[0];
    uint16_t tx_len = *usbhs_ep_tx_len_reg(0U);

    if (tx_len > ep0->xfer_len) {
        tx_len = (uint16_t)ep0->xfer_len;
    }
    ep0->actual_xfer_len += tx_len;
    if (ep0->xfer_len >= (uint32_t)tx_len) {
        ep0->xfer_len -= (uint32_t)tx_len;
    } else {
        ep0->xfer_len = 0U;
    }

    if (ep0->xfer_len == 0U) {
        usbhs_apply_pending_address();
        usbd_event_ep_in_complete_handler(busid, USB_CONTROL_IN_EP0, ep0->actual_xfer_len);
    } else {
        uint32_t next_len = (ep0->xfer_len > ep0->ep_mps) ? ep0->ep_mps : ep0->xfer_len;
        if (ep0->xfer_buf != NULL) {
            usbhs_ep_prime_in(0U, ep0->xfer_buf + ep0->actual_xfer_len, next_len);
        } else {
            usbhs_ep_prime_in(0U, NULL, 0U);
        }
    }
}

static void usbhs_handle_ep_in_xfer_complete(uint8_t busid, uint8_t ep_idx)
{
    struct usb_dc_ep_state *ep = &g_ch32h417_usbhs_udc.in_ep[ep_idx];
    uint16_t tx_len = *usbhs_ep_tx_len_reg(ep_idx);
    uint8_t last = 0U;

    if (tx_len > ep->xfer_len) {
        tx_len = (uint16_t)ep->xfer_len;
    }
    ep->actual_xfer_len += tx_len;
    if (ep->xfer_len >= (uint32_t)tx_len) {
        ep->xfer_len -= (uint32_t)tx_len;
    } else {
        ep->xfer_len = 0U;
    }

    if (ep->xfer_len == 0U) {
        last = 1U;
    } else if (ep->xfer_buf != NULL) {
        uint32_t next_len = (ep->xfer_len > ep->ep_mps) ? ep->ep_mps : ep->xfer_len;
        usbhs_ep_prime_in(ep_idx, ep->xfer_buf + ep->actual_xfer_len, next_len);
    } else {
        usbhs_ep_prime_in(ep_idx, NULL, 0U);
    }

    if (last != 0U) {
        usbd_event_ep_in_complete_handler(busid, (uint8_t)(ep_idx | 0x80U), ep->actual_xfer_len);
    }
}

static void usbhs_handle_ep0_out_xfer_complete(uint8_t busid)
{
    struct usb_dc_ep_state *ep0 = &g_ch32h417_usbhs_udc.out_ep[0];
    uint16_t rx_len = *usbhs_ep_rx_len_reg(0U);
    uint8_t done = 0U;

    if ((*usbhs_ep_rx_ctrl_reg(0U) & USBHS_UEP_RX_SETUP_IS) != 0U) {
        usbhs_handle_setup_packet();
        return;
    }

    if ((ep0->xfer_buf != NULL) && (rx_len != 0U)) {
        if (rx_len > USBHS_EP_BUFFER_SIZE) {
            rx_len = USBHS_EP_BUFFER_SIZE;
        }
        memcpy(ep0->xfer_buf + ep0->actual_xfer_len, ep0_buffer, rx_len);
    }

    ep0->actual_xfer_len += rx_len;
    if (ep0->xfer_len >= (uint32_t)rx_len) {
        ep0->xfer_len -= (uint32_t)rx_len;
    } else {
        ep0->xfer_len = 0U;
    }

    if ((ep0->xfer_len == 0U) || (rx_len < ep0->ep_mps)) {
        done = 1U;
    }

    if (done != 0U) {
        usbd_event_ep_out_complete_handler(busid, USB_CONTROL_OUT_EP0, ep0->actual_xfer_len);
    } else {
        ep0->ep_toggle ^= 1U;
        usbhs_ep_prime_out(0U);
    }
}

static void usbhs_handle_ep_out_xfer_complete(uint8_t busid, uint8_t ep_idx)
{
    struct usb_dc_ep_state *ep = &g_ch32h417_usbhs_udc.out_ep[ep_idx];
    uint16_t rx_len = *usbhs_ep_rx_len_reg(ep_idx);

    if (ep->xfer_buf != NULL && (rx_len != 0U)) {
        if ((ep->actual_xfer_len + (uint32_t)rx_len) > USBHS_EP_BUFFER_SIZE) {
            rx_len = (uint16_t)(USBHS_EP_BUFFER_SIZE - ep->actual_xfer_len);
        }
        if (rx_len != 0U) {
            memcpy(ep->xfer_buf + ep->actual_xfer_len, ep_rx_buffer[ep_idx], rx_len);
        }
    }

    ep->actual_xfer_len += rx_len;
    if (ep->xfer_len >= (uint32_t)rx_len) {
        ep->xfer_len -= (uint32_t)rx_len;
    } else {
        ep->xfer_len = 0U;
    }

    if ((ep->xfer_len == 0U) || (rx_len < ep->ep_mps)) {
        ep->xfer_len = 0U;
        usbd_event_ep_out_complete_handler(busid, ep_idx, ep->actual_xfer_len);
    } else {
        ep->ep_toggle ^= 1U;
        usbhs_ep_prime_out(ep_idx);
    }
}

int usb_dc_ch32h417_usbhs_init(uint8_t busid)
{
    (void)busid;
    usb_dc_low_level_init();
    memset(&g_ch32h417_usbhs_udc, 0, sizeof(g_ch32h417_usbhs_udc));
    g_ch32h417_usbhs_udc.port_speed = USB_SPEED_HIGH;

    rt_kprintf("[USBHS] init\r\n");

    RCC_USBHS_PLLCmd(DISABLE);
    RCC_USBHSPLLCLKConfig(RCC_USBHSPLLSource_HSE);
    RCC_USBHSPLLReferConfig(RCC_USBHSPLLRefer_25M);
    RCC_USBHSPLLClockSourceDivConfig(RCC_USBHSPLL_IN_Div1);
    RCC_USBHS_PLLCmd(ENABLE);
    RCC_UTMIcmd(ENABLE);
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBHS, ENABLE);

    *usbhs_reg8(USBHS_OFFSET_USB_CTRL) = (uint8_t)(USBHS_UD_RST_LINK | USBHS_UD_PHY_SUSPENDM);
    *usbhs_reg8(USBHS_OFFSET_USB_INT_EN) = (uint8_t)(USBHS_UDIE_BUS_RST | USBHS_UDIE_SUSPEND | USBHS_UDIE_BUS_SLEEP |
                                                        USBHS_UDIE_LPM_ACT | USBHS_UDIE_TRANSFER | USBHS_UDIE_LINK_RDY);
    *usbhs_reg8(USBHS_OFFSET_USB_INT_FG) = (uint8_t)0xFFU;
    *usbhs_reg8(USBHS_OFFSET_USB_BASE_MODE) = USBHS_UD_SPEED_HIGH;
    *usbhs_reg16(USBHS_OFFSET_UEP_TX_EN) = 0U;
    *usbhs_reg16(USBHS_OFFSET_UEP_RX_EN) = 0U;
    *usbhs_reg8(USBHS_OFFSET_USB_CTRL) = (uint8_t)(USBHS_UD_DEV_EN | USBHS_UD_DMA_EN | USBHS_UD_LPM_EN | USBHS_UD_PHY_SUSPENDM);

    usbhs_ep0_state_init();
    NVIC_EnableIRQ(USBHS_IRQn);

    (void)USBHS_INIT_TIMEOUT;
    return 0;
  }

int usb_dc_ch32h417_usbhs_deinit(uint8_t busid)
{
    (void)busid;

    NVIC_DisableIRQ(USBHS_IRQn);
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBHS, DISABLE);
    RCC_UTMIcmd(DISABLE);
    RCC_USBHS_PLLCmd(DISABLE);
    *usbhs_reg8(USBHS_OFFSET_USB_CTRL) = USBHS_UD_RST_LINK;
    *usbhs_reg16(USBHS_OFFSET_UEP_TX_EN) = 0U;
    *usbhs_reg16(USBHS_OFFSET_UEP_RX_EN) = 0U;
    usb_dc_low_level_deinit();
    return 0;
}

int usb_dc_ch32h417_usbhs_set_address(uint8_t busid, const uint8_t addr)
{
    (void)busid;
    g_ch32h417_usbhs_udc.dev_addr = addr;
    g_ch32h417_usbhs_udc.addr_pending = 1U;
    return 0;
}

int usb_dc_ch32h417_usbhs_set_remote_wakeup(uint8_t busid)
{
    (void)busid;
    return -1;
}

uint8_t usb_dc_ch32h417_usbhs_get_port_speed(uint8_t busid)
{
    (void)busid;
    return USB_SPEED_HIGH;
}

int usb_dc_ch32h417_usbhs_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep->bEndpointAddress);
    uint16_t ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
    uint8_t ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);

    (void)busid;

    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    if (ep_idx == 0U) {
        usbhs_ep0_state_init();
        return 0;
    }

    if (USB_EP_DIR_IS_OUT(ep->bEndpointAddress)) {
        g_ch32h417_usbhs_udc.out_ep[ep_idx].ep_mps = ep_mps;
        g_ch32h417_usbhs_udc.out_ep[ep_idx].ep_type = ep_type;
        g_ch32h417_usbhs_udc.out_ep[ep_idx].ep_enabled = 1U;
        g_ch32h417_usbhs_udc.out_ep[ep_idx].ep_toggle = 0U;
        *usbhs_ep_max_len_reg(ep_idx) = ep_mps;
        *usbhs_reg16(USBHS_OFFSET_UEP_RX_EN) |= (uint16_t)(1U << ep_idx);
        *usbhs_ep_rx_dma_reg(ep_idx) = (uint32_t)ep_rx_buffer[ep_idx];
        *usbhs_ep_rx_len_reg(ep_idx) = 0U;
        *usbhs_ep_rx_ctrl_reg(ep_idx) = USBHS_UEP_RX_RES_ACK | USBHS_UEP_RX_D0 | USBHS_UEP_RX_TOG_MATCH;
    } else {
        g_ch32h417_usbhs_udc.in_ep[ep_idx].ep_mps = ep_mps;
        g_ch32h417_usbhs_udc.in_ep[ep_idx].ep_type = ep_type;
        g_ch32h417_usbhs_udc.in_ep[ep_idx].ep_enabled = 1U;
        g_ch32h417_usbhs_udc.in_ep[ep_idx].ep_toggle = 0U;
        *usbhs_ep_max_len_reg(ep_idx) = ep_mps;
        *usbhs_reg16(USBHS_OFFSET_UEP_TX_EN) |= (uint16_t)(1U << ep_idx);
        *usbhs_ep_tx_dma_reg(ep_idx) = (uint32_t)ep_tx_buffer[ep_idx];
        *usbhs_ep_tx_len_reg(ep_idx) = 0U;
        *usbhs_ep_tx_ctrl_reg(ep_idx) = USBHS_UEP_TX_RES_NAK | USBHS_UEP_TX_D0;
    }

    return 0;
}

int usb_dc_ch32h417_usbhs_ep_close(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    (void)busid;

    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    if (ep_idx == 0U) {
        g_ch32h417_usbhs_udc.in_ep[0].ep_enabled = 0U;
        g_ch32h417_usbhs_udc.out_ep[0].ep_enabled = 0U;
    } else if (USB_EP_DIR_IS_OUT(ep)) {
        g_ch32h417_usbhs_udc.out_ep[ep_idx].ep_enabled = 0U;
        *usbhs_reg16(USBHS_OFFSET_UEP_RX_EN) &= (uint16_t)~(1U << ep_idx);
    } else {
        g_ch32h417_usbhs_udc.in_ep[ep_idx].ep_enabled = 0U;
        *usbhs_reg16(USBHS_OFFSET_UEP_TX_EN) &= (uint16_t)~(1U << ep_idx);
    }

    return 0;
}

int usb_dc_ch32h417_usbhs_set_stall(uint8_t busid, uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    (void)busid;

    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    if (ep_idx == 0U) {
        *usbhs_ep_tx_ctrl_reg(0U) = USBHS_UEP_TX_RES_STALL | USBHS_UEP_TX_D0;
        *usbhs_ep_rx_ctrl_reg(0U) = USBHS_UEP_RX_RES_STALL | USBHS_UEP_RX_D0 | USBHS_UEP_RX_TOG_MATCH;
        g_ch32h417_usbhs_udc.in_ep[0].ep_stalled = 1U;
        g_ch32h417_usbhs_udc.out_ep[0].ep_stalled = 1U;
    } else if (USB_EP_DIR_IS_OUT(ep)) {
        *usbhs_ep_rx_ctrl_reg(ep_idx) = USBHS_UEP_RX_RES_STALL | USBHS_UEP_RX_D0;
        g_ch32h417_usbhs_udc.out_ep[ep_idx].ep_stalled = 1U;
    } else {
        *usbhs_ep_tx_ctrl_reg(ep_idx) = USBHS_UEP_TX_RES_STALL | USBHS_UEP_TX_D0;
        g_ch32h417_usbhs_udc.in_ep[ep_idx].ep_stalled = 1U;
    }

    return 0;
}

int usb_dc_ch32h417_usbhs_clear_stall(uint8_t busid, uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t in_pid;
    uint8_t out_pid;

    (void)busid;

    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    in_pid = (g_ch32h417_usbhs_udc.in_ep[ep_idx].ep_toggle == 0U) ? USBHS_UEP_TX_D0 : USBHS_UEP_TX_D1;
    out_pid = (g_ch32h417_usbhs_udc.out_ep[ep_idx].ep_toggle == 0U) ? USBHS_UEP_RX_D0 : USBHS_UEP_RX_D1;
    g_ch32h417_usbhs_udc.in_ep[ep_idx].ep_stalled = 0U;
    g_ch32h417_usbhs_udc.out_ep[ep_idx].ep_stalled = 0U;
    g_ch32h417_usbhs_udc.in_ep[ep_idx].ep_toggle = 0U;
    g_ch32h417_usbhs_udc.out_ep[ep_idx].ep_toggle = 0U;

    if (ep_idx == 0U) {
        *usbhs_ep_tx_ctrl_reg(0U) = (uint8_t)(USBHS_UEP_TX_RES_ACK | USBHS_UEP_TX_D0);
        *usbhs_ep_rx_ctrl_reg(0U) = (uint8_t)(USBHS_UEP_RX_RES_ACK | USBHS_UEP_RX_D0 | USBHS_UEP_RX_TOG_MATCH);
    } else if (USB_EP_DIR_IS_OUT(ep)) {
        *usbhs_ep_rx_ctrl_reg(ep_idx) = (uint8_t)(out_pid | USBHS_UEP_RX_RES_ACK | USBHS_UEP_RX_TOG_MATCH);
    } else {
        *usbhs_ep_tx_ctrl_reg(ep_idx) = (uint8_t)(in_pid | USBHS_UEP_TX_RES_ACK);
    }

    return 0;
}

int usb_dc_ch32h417_usbhs_is_stalled(uint8_t busid, const uint8_t ep, uint8_t *stalled)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    (void)busid;

    if ((ep_idx >= USB_CH32H417_MAX_EP_NUM) || (stalled == NULL)) {
        return -1;
    }

    if (USB_EP_DIR_IS_OUT(ep)) {
        *stalled = g_ch32h417_usbhs_udc.out_ep[ep_idx].ep_stalled;
    } else {
        *stalled = g_ch32h417_usbhs_udc.in_ep[ep_idx].ep_stalled;
    }
    return 0;
}

int usb_dc_ch32h417_usbhs_start_write(uint8_t busid, uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t ep_mps;
    uint32_t send_len;

    (void)busid;

    if (!data && (data_len != 0U)) {
        return -1;
    }

    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    ep_mps = (uint32_t)g_ch32h417_usbhs_udc.in_ep[ep_idx].ep_mps;
    if ((ep_idx != 0U) && (ep_mps == 0U)) {
        ep_mps = USB_CH32H417_EP0_MPS;
    }
    g_ch32h417_usbhs_udc.in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_ch32h417_usbhs_udc.in_ep[ep_idx].xfer_len = data_len;
    g_ch32h417_usbhs_udc.in_ep[ep_idx].actual_xfer_len = 0U;
    g_ch32h417_usbhs_udc.in_ep[ep_idx].ep_toggle = 0U;

    send_len = (data_len > ep_mps) ? ep_mps : data_len;
    usbhs_ep_prime_in(ep_idx, data, send_len);

    return 0;
}

int usb_dc_ch32h417_usbhs_start_read(uint8_t busid, uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    (void)busid;

    if (!data && (data_len != 0U)) {
        return -1;
    }

    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    g_ch32h417_usbhs_udc.out_ep[ep_idx].xfer_buf = data;
    g_ch32h417_usbhs_udc.out_ep[ep_idx].xfer_len = data_len;
    g_ch32h417_usbhs_udc.out_ep[ep_idx].actual_xfer_len = 0U;
    g_ch32h417_usbhs_udc.out_ep[ep_idx].ep_toggle = 0U;
    usbhs_ep_prime_out(ep_idx);

    return 0;
}

void USBD_IRQHandler(uint8_t busid)
{
    uint8_t int_fg;
    uint8_t int_st;
    uint8_t ep_idx;

    int_fg = *usbhs_reg8(USBHS_OFFSET_USB_INT_FG);
    if (int_fg == 0U) {
        return;
    }

    if ((int_fg & USBHS_UDIF_BUS_RST) != 0U) {
        uint32_t reset_loop;
        for (reset_loop = 0U; reset_loop < USBHS_INIT_TIMEOUT; reset_loop++) {
            (void)reset_loop;
        }
        usbhs_clear_all_transfer_state();
        *usbhs_reg8(USBHS_OFFSET_USB_CTRL) = (uint8_t)(USBHS_UD_RST_LINK | USBHS_UD_PHY_SUSPENDM);
        *usbhs_reg8(USBHS_OFFSET_USB_CTRL) = (uint8_t)(USBHS_UD_DEV_EN | USBHS_UD_DMA_EN | USBHS_UD_LPM_EN | USBHS_UD_PHY_SUSPENDM);
        usbd_event_reset_handler(busid);
    }

    if ((int_fg & USBHS_UDIF_SUSPEND) != 0U) {
        usbd_event_suspend_handler(busid);
    }
    if ((int_fg & USBHS_UDIF_LINK_RDY) != 0U) {
        usbd_event_connect_handler(busid);
    }

    if ((int_fg & USBHS_UDIF_TRANSFER) != 0U) {
        int_st = *usbhs_reg8(USBHS_OFFSET_USB_INT_ST);
        ep_idx = (uint8_t)(int_st & USBHS_UD_INT_EP_ID_MASK);

        if (ep_idx < USB_CH32H417_MAX_EP_NUM) {
            if ((int_st & USBHS_UD_INT_DIR_IN) == USBHS_UD_INT_DIR_OUT) {
                if (ep_idx == 0U) {
                    usbhs_handle_ep0_out_xfer_complete(busid);
                } else {
                    usbhs_handle_ep_out_xfer_complete(busid, ep_idx);
                }
            } else {
                if (ep_idx == 0U) {
                    usbhs_handle_ep0_in_xfer_complete(busid);
                } else {
                    usbhs_handle_ep_in_xfer_complete(busid, ep_idx);
                }
            }
        }
    }

    *usbhs_reg8(USBHS_OFFSET_USB_INT_FG) = int_fg;
}

void USBHS_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USBHS_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    USBD_IRQHandler(0U);
    rt_interrupt_leave();
    FREE_INT_SP();
}
