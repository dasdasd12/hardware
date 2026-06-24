/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usb_dc_ch32h417_usbss.h"

#include <rtthread.h>
#include <string.h>

#include "board.h"
#include "ch32h417.h"

#define USBSS_READY_FLAG_ADDR  ((volatile uint32_t *)0x20178020u)
#define USBSS_READY_FLAG_VALUE 0xABCD1234u
#define USBSS_FAILED_FLAG_VALUE 0xDEADBEEFu
#define USBSS_READY_TIMEOUT_MS 60000U
#define USBSS_EP_BUFFER_SIZE   1024U
#define USBSS_V3F_TRACE_BASE   ((volatile uint32_t *)0x20178000u)
#define USBSS_V3F_STAGE_PLL_BEGIN 7U
#define USBSS_V3F_STAGE_WAIT_TIMEOUT 1000000U
#define USBSS_LINK_BUSY_TIMEOUT  400000U
#define USBSS_LINK_SWEEP_DELAY   12000000U
#define USBSS_LINK_SAMPLE_COUNT  1000000U
#define USBSS_USE_V3F_LINK_INIT  1U
#define USBSS_ENABLE_LINK_SWEEP  0U
#define USBSS_HS_FALLBACK_REASON_DISABLE  1U
#define USBSS_HS_FALLBACK_REASON_INACTIVE 2U

#define USBSS_PHY_CFG_CR       (*((__IO uint32_t *)0x400341F8U))
#define USBSS_PHY_CFG_DAT      (*((__IO uint32_t *)0x400341FCU))

#define LMP_HP                 0U
#define LMP_SUBTYPE_MASK       (0x0FU << 5)
#define LMP_SET_LINK_FUNC      (0x01U << 5)
#define LMP_U2_INACT_TOUT      (0x02U << 5)
#define LMP_PORT_CAP           (0x04U << 5)
#define LMP_PORT_CFG           (0x05U << 5)
#define LMP_PORT_CFG_RES       (0x06U << 5)
#define LMP_LINK_SPEED         (1U << 9)
#define NUM_HP_BUF             (4U << 0)
#define DOWN_STREAM            (1U << 16)
#define UP_STREAM              (2U << 16)

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

struct ch32h417_usbss_udc {
    volatile uint8_t dev_addr;
    uint8_t addr_pending;
    uint8_t port_speed;
    struct usb_dc_ep_state in_ep[USB_CH32H417_MAX_EP_NUM];
    struct usb_dc_ep_state out_ep[USB_CH32H417_MAX_EP_NUM];
};

struct ch32h417_usbss_diag {
    volatile uint32_t irq;
    volatile uint32_t link_irq;
    volatile uint32_t setup;
    volatile uint32_t status;
    volatile uint32_t transfer;
    volatile uint32_t ep0_prime_in;
    volatile uint32_t ep0_in;
    volatile uint32_t ep0_prime_out;
    volatile uint32_t ep0_out;
    volatile uint32_t set_addr;
    volatile uint32_t stall;
    volatile uint32_t reset;
    volatile uint32_t u0;
    volatile uint32_t rxdet;
    volatile uint32_t hotrst;
    volatile uint32_t warmrst;
    volatile uint32_t last_status;
    volatile uint32_t last_link_int;
    volatile uint32_t last_link_state;
    volatile uint32_t last_link_status;
    volatile uint32_t last_setup0;
    volatile uint32_t last_setup1;
    volatile uint32_t last_resp0;
    volatile uint32_t last_tx_len;
    volatile uint32_t last_rx_len;
    volatile uint32_t last_addr;
    volatile uint32_t rxdet_no_reset;
    volatile uint32_t rxdet_flag;
    volatile uint32_t disable_flag;
    volatile uint32_t disable;
    volatile uint32_t inactive;
    volatile uint32_t polling;
    volatile uint32_t recovery;
    volatile uint32_t u1;
    volatile uint32_t u2;
    volatile uint32_t u3;
    volatile uint32_t rx_detect_seen;
    volatile uint32_t rx_lfps_seen;
    volatile uint32_t term_seen;
    volatile uint32_t ready_seen;
    volatile uint32_t polling_enable;
    volatile uint32_t ready_flag;
    volatile uint32_t ux_fail_flag;
    volatile uint32_t ux_exit_fail_flag;
    volatile uint32_t go_u0_flag;
    volatile uint32_t recovery_flag;
    volatile uint32_t inactive_flag;
    volatile uint32_t txeq_flag;
    volatile uint32_t ux_rej_flag;
    volatile uint32_t rx_lmp_tout_flag;
    volatile uint32_t phy_r03;
    volatile uint32_t phy_r0d;
    volatile uint32_t phy_r11;
    volatile uint32_t phy_r12;
    volatile uint32_t phy_r13;
    volatile uint32_t phy_r15;
    volatile uint32_t phy_cfg_cr;
    volatile uint32_t phy_cfg_dat;
    volatile uint32_t phy_mod;
    volatile uint32_t rcc_ctlr;
    volatile uint32_t rcc_pllcfgr2;
    volatile uint32_t rcc_hbpcenr;
    volatile uint32_t link_busy_wait;
    volatile uint32_t post_busy_status;
    volatile uint32_t post_term_status;
    volatile uint32_t sample_count;
    volatile uint32_t sample_first;
    volatile uint32_t sample_last;
    volatile uint32_t sample_or;
    volatile uint32_t sample_and;
    volatile uint32_t sample_rxdet;
    volatile uint32_t sample_lfps;
    volatile uint32_t sample_term;
    volatile uint32_t sample_ready;
    volatile uint32_t sample_disable;
    volatile uint32_t sample_polling;
    volatile uint32_t sample_recovery;
    volatile uint32_t sample_u0;
};

static struct ch32h417_usbss_udc g_ch32h417_usbss_udc;
static struct ch32h417_usbss_diag g_ch32h417_usbss_diag;
static uint8_t g_ch32h417_usbss_sweep_done;
static volatile uint8_t g_ch32h417_usbss_hs_fallback_request;
static volatile uint32_t g_ch32h417_usbss_hs_fallback_reason;

static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t usbss_ep0_buffer[USB_CH32H417_SS_EP0_MPS];
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t usbss_ep_tx_buffer[USB_CH32H417_MAX_EP_NUM][USBSS_EP_BUFFER_SIZE];
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t usbss_ep_rx_buffer[USB_CH32H417_MAX_EP_NUM][USBSS_EP_BUFFER_SIZE];

static void usbss_delay(uint32_t cycles)
{
    volatile uint32_t i;

    for (i = 0U; i < cycles; i++) {
        __NOP();
    }
}

static uint32_t usbss_pack4(const uint8_t *data)
{
    if (data == RT_NULL) {
        return 0U;
    }

    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static USBSS_EP_TX_TypeDef *usbss_tx_ep(uint8_t ep_idx)
{
    switch (ep_idx) {
    case 1U:
        return (USBSS_EP_TX_TypeDef *)&USBSSD->EP1_TX;
    case 2U:
        return (USBSS_EP_TX_TypeDef *)&USBSSD->EP2_TX;
    case 3U:
        return (USBSS_EP_TX_TypeDef *)&USBSSD->EP3_TX;
    case 4U:
        return (USBSS_EP_TX_TypeDef *)&USBSSD->EP4_TX;
    case 5U:
        return (USBSS_EP_TX_TypeDef *)&USBSSD->EP5_TX;
    case 6U:
        return (USBSS_EP_TX_TypeDef *)&USBSSD->EP6_TX;
    case 7U:
        return (USBSS_EP_TX_TypeDef *)&USBSSD->EP7_TX;
    default:
        return RT_NULL;
    }
}

static USBSS_EP_RX_TypeDef *usbss_rx_ep(uint8_t ep_idx)
{
    switch (ep_idx) {
    case 1U:
        return (USBSS_EP_RX_TypeDef *)&USBSSD->EP1_RX;
    case 2U:
        return (USBSS_EP_RX_TypeDef *)&USBSSD->EP2_RX;
    case 3U:
        return (USBSS_EP_RX_TypeDef *)&USBSSD->EP3_RX;
    case 4U:
        return (USBSS_EP_RX_TypeDef *)&USBSSD->EP4_RX;
    case 5U:
        return (USBSS_EP_RX_TypeDef *)&USBSSD->EP5_RX;
    case 6U:
        return (USBSS_EP_RX_TypeDef *)&USBSSD->EP6_RX;
    case 7U:
        return (USBSS_EP_RX_TypeDef *)&USBSSD->EP7_RX;
    default:
        return RT_NULL;
    }
}

static uint32_t usbss_phy_cfg(uint8_t port_num, uint8_t addr, uint16_t data)
{
    if (port_num != 0U) {
        return 0U;
    }

    USBSS_PHY_CFG_CR = (1UL << 23) | ((uint32_t)addr << 16) | data;
    USBSS_PHY_CFG_DAT = 0x01U;
    return USBSS_PHY_CFG_DAT;
}

static uint32_t usbss_phy_read(uint8_t port_num, uint8_t addr)
{
    if (port_num != 0U) {
        return 0U;
    }

    USBSS_PHY_CFG_CR &= ~(0x1FU << 16);
    USBSS_PHY_CFG_CR = ((0x1FU << 16) | 0x0U) | (1U << 23);
    USBSS_PHY_CFG_DAT = 0x1U;
    USBSS_PHY_CFG_CR &= ~(0x1FU << 16);
    USBSS_PHY_CFG_CR = ((uint32_t)addr << 16);
    return USBSS_PHY_CFG_DAT;
}

static void __attribute__((unused)) usbss_cfg_mod(void)
{
    usbss_phy_cfg(0U, 0x03U, 0x7C12U);
    usbss_phy_cfg(0U, 0x0DU, 0x79AAU);
    usbss_phy_cfg(0U, 0x15U, 0x4430U);
    usbss_phy_cfg(0U, 0x13U, 0x0010U);
    *((__IO uint32_t *)0x5003C018U) = 0xB0054000U;
}

static void usbss_snapshot_phy_diag(void)
{
    g_ch32h417_usbss_diag.phy_r03 = usbss_phy_read(0U, 0x03U);
    g_ch32h417_usbss_diag.phy_r0d = usbss_phy_read(0U, 0x0DU);
    g_ch32h417_usbss_diag.phy_r11 = usbss_phy_read(0U, 0x11U);
    g_ch32h417_usbss_diag.phy_r12 = usbss_phy_read(0U, 0x12U);
    g_ch32h417_usbss_diag.phy_r13 = usbss_phy_read(0U, 0x13U);
    g_ch32h417_usbss_diag.phy_r15 = usbss_phy_read(0U, 0x15U);
    g_ch32h417_usbss_diag.phy_cfg_cr = USBSS_PHY_CFG_CR;
    g_ch32h417_usbss_diag.phy_cfg_dat = USBSS_PHY_CFG_DAT;
    g_ch32h417_usbss_diag.phy_mod = *((__IO uint32_t *)0x5003C018U);
    g_ch32h417_usbss_diag.rcc_ctlr = RCC->CTLR;
    g_ch32h417_usbss_diag.rcc_pllcfgr2 = RCC->PLLCFGR2;
    g_ch32h417_usbss_diag.rcc_hbpcenr = RCC->HBPCENR;
}

static void usbss_sample_link_window(void)
{
    static uint8_t sampled;
    uint32_t i;
    uint32_t first = 0U;
    uint32_t last = 0U;
    uint32_t sample_or = 0U;
    uint32_t sample_and = 0xFFFFFFFFU;
    uint32_t rxdet = 0U;
    uint32_t lfps = 0U;
    uint32_t term = 0U;
    uint32_t ready = 0U;
    uint32_t disable = 0U;
    uint32_t polling = 0U;
    uint32_t recovery = 0U;
    uint32_t u0 = 0U;

    if (sampled != 0U) {
        return;
    }
    sampled = 1U;

    for (i = 0U; i < USBSS_LINK_SAMPLE_COUNT; i++) {
        uint32_t status = USBSSD->LINK_STATUS;
        uint32_t state = status & LINK_STATE_MASK;

        if (i == 0U) {
            first = status;
        }
        last = status;
        sample_or |= status;
        sample_and &= status;

        if ((status & LINK_RX_DETECT) != 0U) {
            rxdet++;
        }
        if ((status & LINK_RX_LFPS) != 0U) {
            lfps++;
        }
        if ((status & LINK_RX_TERM_PRES) != 0U) {
            term++;
        }
        if ((status & LINK_READY) != 0U) {
            ready++;
        }
        if (state == LINK_STATE_DISABLE) {
            disable++;
        } else if (state == LINK_STATE_POLLING) {
            polling++;
        } else if (state == LINK_STATE_RECOVERY) {
            recovery++;
        } else if (state == LINK_STATE_U0) {
            u0++;
        }
    }

    g_ch32h417_usbss_diag.sample_count = USBSS_LINK_SAMPLE_COUNT;
    g_ch32h417_usbss_diag.sample_first = first;
    g_ch32h417_usbss_diag.sample_last = last;
    g_ch32h417_usbss_diag.sample_or = sample_or;
    g_ch32h417_usbss_diag.sample_and = sample_and;
    g_ch32h417_usbss_diag.sample_rxdet = rxdet;
    g_ch32h417_usbss_diag.sample_lfps = lfps;
    g_ch32h417_usbss_diag.sample_term = term;
    g_ch32h417_usbss_diag.sample_ready = ready;
    g_ch32h417_usbss_diag.sample_disable = disable;
    g_ch32h417_usbss_diag.sample_polling = polling;
    g_ch32h417_usbss_diag.sample_recovery = recovery;
    g_ch32h417_usbss_diag.sample_u0 = u0;
}

static int usbss_wait_ready_flag(void)
{
    uint32_t timeout;

    for (timeout = 0U; timeout < USBSS_READY_TIMEOUT_MS; timeout++) {
        uint32_t flag = *USBSS_READY_FLAG_ADDR;

        if (flag == USBSS_READY_FLAG_VALUE) {
            return 0;
        }
        if (flag == USBSS_FAILED_FLAG_VALUE) {
            rt_kprintf("[USBSS] V3F PLL/PHY init failed flag\r\n");
            return -1;
        }
        rt_thread_mdelay(1);
    }

    rt_kprintf("[USBSS] V3F ready flag timeout, flag=0x%08x stage=%u idle=%u cfg=0x%08x st0=0x%08x st1=0x%08x\r\n",
               (unsigned int)*USBSS_READY_FLAG_ADDR,
               (unsigned int)USBSS_V3F_TRACE_BASE[1],
               (unsigned int)USBSS_V3F_TRACE_BASE[4],
               (unsigned int)USBSS_V3F_TRACE_BASE[11],
               (unsigned int)USBSS_V3F_TRACE_BASE[13],
               (unsigned int)USBSS_V3F_TRACE_BASE[14]);
    return -1;
}

static uint32_t usbss_wait_v3f_post_ready(void)
{
    uint32_t timeout;

    for (timeout = 0U; timeout < USBSS_V3F_STAGE_WAIT_TIMEOUT; timeout++) {
        if (USBSS_V3F_TRACE_BASE[1] != USBSS_V3F_STAGE_PLL_BEGIN) {
            break;
        }
        __NOP();
    }

    return timeout;
}

static void usbss_enable_v5f_clock_gates(void)
{
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBSS, ENABLE);
    RCC_PIPECmd(ENABLE);
    RCC_UTMIcmd(ENABLE);
    RCC_USBSS_PLLCmd(ENABLE);
}

static void usbss_ep0_state_init(void)
{
    g_ch32h417_usbss_udc.out_ep[0].ep_mps = USB_CH32H417_SS_EP0_MPS;
    g_ch32h417_usbss_udc.out_ep[0].ep_type = USB_ENDPOINT_TYPE_CONTROL;
    g_ch32h417_usbss_udc.out_ep[0].ep_enabled = 1U;
    g_ch32h417_usbss_udc.out_ep[0].ep_stalled = 0U;
    g_ch32h417_usbss_udc.out_ep[0].ep_toggle = 0U;
    g_ch32h417_usbss_udc.out_ep[0].xfer_buf = RT_NULL;
    g_ch32h417_usbss_udc.out_ep[0].xfer_len = 0U;
    g_ch32h417_usbss_udc.out_ep[0].actual_xfer_len = 0U;

    g_ch32h417_usbss_udc.in_ep[0].ep_mps = USB_CH32H417_SS_EP0_MPS;
    g_ch32h417_usbss_udc.in_ep[0].ep_type = USB_ENDPOINT_TYPE_CONTROL;
    g_ch32h417_usbss_udc.in_ep[0].ep_enabled = 1U;
    g_ch32h417_usbss_udc.in_ep[0].ep_stalled = 0U;
    g_ch32h417_usbss_udc.in_ep[0].ep_toggle = 0U;
    g_ch32h417_usbss_udc.in_ep[0].xfer_buf = RT_NULL;
    g_ch32h417_usbss_udc.in_ep[0].xfer_len = 0U;
    g_ch32h417_usbss_udc.in_ep[0].actual_xfer_len = 0U;

    USBSSD->UEP0_TX_DMA = (uint32_t)usbss_ep0_buffer;
    USBSSD->UEP0_RX_DMA = (uint32_t)usbss_ep0_buffer;
    USBSSD->UEP0_TX_DMA_OFS = 0U;
    USBSSD->UEP0_RX_DMA_OFS = 0U;
    USBSSD->UEP0_TX_CTRL = 0U;
    USBSSD->UEP0_RX_CTRL = 0U;
}

static void usbss_clear_transfer_state(void)
{
    memset(&g_ch32h417_usbss_udc, 0, sizeof(g_ch32h417_usbss_udc));
    g_ch32h417_usbss_udc.port_speed = USB_SPEED_SUPER;
    USBSSD->UEP_TX_EN = 0U;
    USBSSD->UEP_RX_EN = 0U;
    USBSSD->USB_CONTROL |= USBSS_USB_CLR_ALL;
    USBSSD->USB_CONTROL &= ~USBSS_USB_CLR_ALL;
    usbss_ep0_state_init();
}

static void usbss_apply_pending_address(void)
{
    if (g_ch32h417_usbss_udc.addr_pending != 0U) {
        USBSSD->USB_CONTROL &= ~USBSS_DEV_ADDR_MASK;
        USBSSD->USB_CONTROL |= ((uint32_t)(g_ch32h417_usbss_udc.dev_addr & 0x7FU) << 24);
        g_ch32h417_usbss_udc.addr_pending = 0U;
    }
}

static void usbss_ep_prime_in(uint8_t ep_idx, const uint8_t *data, uint32_t data_len)
{
    uint32_t mps = g_ch32h417_usbss_udc.in_ep[ep_idx].ep_mps;

    if ((mps == 0U) || (mps > USBSS_EP_BUFFER_SIZE)) {
        mps = (ep_idx == 0U) ? USB_CH32H417_SS_EP0_MPS : USBSS_EP_BUFFER_SIZE;
    }
    if (data_len > mps) {
        data_len = mps;
    }

    if (ep_idx == 0U) {
        if ((data != RT_NULL) && (data_len != 0U)) {
            memcpy(usbss_ep0_buffer, data, data_len);
        }
        USBSSD->UEP0_TX_DMA = (uint32_t)usbss_ep0_buffer;
        USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_DPH |
                                ((uint32_t)(g_ch32h417_usbss_udc.in_ep[0].ep_toggle + 1U) << 16) |
                                (data_len & USBSS_EP0_TX_LEN_MASK);
        USBSSD->UEP0_TX_CTRL |= USBSS_EP0_TX_ERDY;
        g_ch32h417_usbss_diag.ep0_prime_in++;
        g_ch32h417_usbss_diag.last_tx_len = data_len;
        g_ch32h417_usbss_diag.last_resp0 = usbss_pack4(usbss_ep0_buffer);
        g_ch32h417_usbss_udc.in_ep[0].ep_toggle++;
    } else {
        USBSS_EP_TX_TypeDef *tx = usbss_tx_ep(ep_idx);

        if (tx == RT_NULL) {
            return;
        }

        if ((data != RT_NULL) && (data_len != 0U)) {
            memcpy(usbss_ep_tx_buffer[ep_idx], data, data_len);
        }
        tx->UEP_TX_DMA = (uint32_t)usbss_ep_tx_buffer[ep_idx];
        tx->UEP_TX_DMA_OFS = 0U;
        tx->UEP_TX_CHAIN_LEN = (uint16_t)data_len;
        tx->UEP_TX_CHAIN_EXP_NUMP = 1U;
        tx->UEP_TX_CR = USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR;
        tx->UEP_TX_CR = 1U;
    }
}

static void usbss_ep_prime_out(uint8_t ep_idx)
{
    if (ep_idx == 0U) {
        USBSSD->UEP0_RX_DMA = (uint32_t)usbss_ep0_buffer;
        USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_ACK;
        g_ch32h417_usbss_diag.ep0_prime_out++;
    } else {
        USBSS_EP_RX_TypeDef *rx = usbss_rx_ep(ep_idx);

        if (rx == RT_NULL) {
            return;
        }

        rx->UEP_RX_DMA = (uint32_t)usbss_ep_rx_buffer[ep_idx];
        rx->UEP_RX_DMA_OFS = 0U;
        rx->UEP_RX_CHAIN_MAX_NUMP = 1U;
        rx->UEP_RX_CR = USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR;
        rx->UEP_RX_CR = 1U;
    }
}

static void usbss_handle_setup_packet(uint8_t busid)
{
    g_ch32h417_usbss_diag.setup++;
    g_ch32h417_usbss_diag.last_setup0 = usbss_pack4(usbss_ep0_buffer);
    g_ch32h417_usbss_diag.last_setup1 = usbss_pack4(usbss_ep0_buffer + 4);

    g_ch32h417_usbss_udc.out_ep[0].ep_toggle = 0U;
    g_ch32h417_usbss_udc.out_ep[0].actual_xfer_len = 0U;
    g_ch32h417_usbss_udc.out_ep[0].xfer_len = 0U;
    g_ch32h417_usbss_udc.out_ep[0].xfer_buf = RT_NULL;
    g_ch32h417_usbss_udc.in_ep[0].ep_toggle = 0U;
    g_ch32h417_usbss_udc.in_ep[0].actual_xfer_len = 0U;
    g_ch32h417_usbss_udc.in_ep[0].xfer_len = 0U;
    g_ch32h417_usbss_udc.in_ep[0].xfer_buf = RT_NULL;

    usbd_event_ep0_setup_complete_handler(busid, usbss_ep0_buffer);
}

static void usbss_handle_ep_in_xfer_complete(uint8_t busid, uint8_t ep_idx)
{
    struct usb_dc_ep_state *ep = &g_ch32h417_usbss_udc.in_ep[ep_idx];
    uint32_t sent_len;

    if (ep_idx == 0U) {
        sent_len = USBSSD->UEP0_TX_CTRL & USBSS_EP0_TX_LEN_MASK;
        g_ch32h417_usbss_diag.ep0_in++;
        g_ch32h417_usbss_diag.last_tx_len = sent_len;
    } else {
        USBSS_EP_TX_TypeDef *tx = usbss_tx_ep(ep_idx);

        if (tx == RT_NULL) {
            return;
        }
        sent_len = tx->UEP_TX_CHAIN_LEN;
        tx->UEP_TX_CHAIN_ST |= USBSS_EP_TX_CHAIN_IF;
    }

    if (sent_len > ep->xfer_len) {
        sent_len = ep->xfer_len;
    }
    ep->actual_xfer_len += sent_len;
    ep->xfer_len -= sent_len;

    if (ep->xfer_len == 0U) {
        if (ep_idx == 0U) {
            usbd_event_ep_in_complete_handler(busid, USB_CONTROL_IN_EP0, ep->actual_xfer_len);
        } else {
            usbd_event_ep_in_complete_handler(busid, (uint8_t)(ep_idx | 0x80U), ep->actual_xfer_len);
        }
    } else {
        uint32_t next_len = (ep->xfer_len > ep->ep_mps) ? ep->ep_mps : ep->xfer_len;
        usbss_ep_prime_in(ep_idx, ep->xfer_buf + ep->actual_xfer_len, next_len);
    }
}

static void usbss_handle_ep_out_xfer_complete(uint8_t busid, uint8_t ep_idx)
{
    struct usb_dc_ep_state *ep = &g_ch32h417_usbss_udc.out_ep[ep_idx];
    uint32_t rx_len;

    if (ep_idx == 0U) {
        rx_len = USBSSD->UEP0_RX_CTRL & USBSS_EP0_RX_LEN_MASK;
        g_ch32h417_usbss_diag.ep0_out++;
        g_ch32h417_usbss_diag.last_rx_len = rx_len;
    } else {
        USBSS_EP_RX_TypeDef *rx = usbss_rx_ep(ep_idx);

        if (rx == RT_NULL) {
            return;
        }
        rx_len = rx->UEP_RX_CHAIN_LEN;
        rx->UEP_RX_CHAIN_ST |= USBSS_EP_RX_CHAIN_IF;
    }

    if ((ep->xfer_buf != RT_NULL) && (rx_len != 0U)) {
        uint32_t room = (ep->xfer_len > ep->actual_xfer_len) ? (ep->xfer_len - ep->actual_xfer_len) : 0U;
        if (rx_len > room) {
            rx_len = room;
        }
        if (rx_len != 0U) {
            memcpy(ep->xfer_buf + ep->actual_xfer_len,
                   (ep_idx == 0U) ? usbss_ep0_buffer : usbss_ep_rx_buffer[ep_idx],
                   rx_len);
        }
    }

    ep->actual_xfer_len += rx_len;
    if (ep->xfer_len >= rx_len) {
        ep->xfer_len -= rx_len;
    } else {
        ep->xfer_len = 0U;
    }

    if ((ep->xfer_len == 0U) || (rx_len < ep->ep_mps)) {
        if (ep_idx == 0U) {
            usbd_event_ep_out_complete_handler(busid, USB_CONTROL_OUT_EP0, ep->actual_xfer_len);
        } else {
            usbd_event_ep_out_complete_handler(busid, ep_idx, ep->actual_xfer_len);
        }
    } else {
        usbss_ep_prime_out(ep_idx);
    }
}

static void usbss_handle_status_stage(uint8_t busid)
{
    uint8_t next_state = usbd_get_ep0_next_state(busid);

    g_ch32h417_usbss_diag.status++;
    usbss_apply_pending_address();

    if (next_state == USBD_EP0_STATE_OUT_STATUS) {
        usbd_event_ep_out_complete_handler(busid, USB_CONTROL_OUT_EP0, 0U);
    } else if (next_state == USBD_EP0_STATE_IN_STATUS) {
        usbd_event_ep_in_complete_handler(busid, USB_CONTROL_IN_EP0, 0U);
    }

    USBSSD->UEP0_TX_CTRL = 0U;
    USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_ACK;
}

static void usbss_device_reset(uint8_t busid, uint8_t notify_core)
{
    g_ch32h417_usbss_diag.reset++;
    USBSSD->USB_CONTROL |= USBSS_FORCE_RST;
    USBSSD->USB_STATUS = USBSS_UIF_TRANSFER | USBSS_UDIF_SETUP | USBSS_UDIF_STATUS |
                         USBSS_UIF_FIFO_RXOV | USBSS_UIF_FIFO_TXOV | USBSS_UIF_RX_PING |
                         USBSS_UIF_ITP;
    USBSSD->USB_CONTROL = USBSS_UIE_TRANSFER | USBSS_UDIE_SETUP | USBSS_UDIE_STATUS |
                          USBSS_DMA_EN | USBSS_SETUP_FLOW;
    usbss_clear_transfer_state();
    if (notify_core != 0U) {
        usbd_event_reset_handler(busid);
    }
}

static void usbss_enable_link_irqs(void)
{
    USBSSD->LINK_INT_CTRL = LINK_IE_TX_LMP | LINK_IE_RX_LMP | LINK_IE_RX_LMP_TOUT |
                            LINK_IE_STATE_CHG | LINK_IE_WARM_RST | LINK_IE_TERM_PRES |
                            LINK_IE_READY | LINK_IE_UX_FAIL | LINK_IE_UX_EXIT_FAIL |
                            LINK_IE_GO_U0 | LINK_IE_RECOVERY | LINK_IE_INACTIVE |
                            LINK_IE_TXEQ | LINK_IE_UX_REJ;
}

static void usbss_link_program(uint32_t cfg_extra, uint32_t ctrl_extra)
{
    uint32_t timeout;

    USBSSD->LINK_CFG = LINK_RX_EQ_EN | LINK_TX_DEEMPH_MASK | LINK_PHY_RESET;
    USBSSD->LINK_CTRL = LINK_P2_MODE | LINK_GO_DISABLED;
    USBSSD->LINK_CFG = LINK_RX_EQ_EN | LINK_TX_DEEMPH_MASK | LINK_LTSSM_MODE |
                       LINK_TOUT_MODE | cfg_extra;
    USBSSD->LINK_LPM_CR |= LINK_LPM_EN;

    for (timeout = 0U; timeout < USBSS_LINK_BUSY_TIMEOUT; timeout++) {
        if ((USBSSD->LINK_STATUS & LINK_BUSY) == 0U) {
            break;
        }
        __NOP();
    }
    g_ch32h417_usbss_diag.link_busy_wait = timeout;
    g_ch32h417_usbss_diag.post_busy_status = USBSSD->LINK_STATUS;

    USBSSD->LINK_CFG |= LINK_RX_TERM_EN;
    g_ch32h417_usbss_diag.post_term_status = USBSSD->LINK_STATUS;
    usbss_enable_link_irqs();
    USBSSD->LINK_CTRL = LINK_P2_MODE;
    USBSSD->LINK_U1_WKUP_TMR = 120U;
    USBSSD->LINK_U1_WKUP_FILTER = 50U;
    USBSSD->LINK_U2_WKUP_FILTER = 0U;
    USBSSD->LINK_U3_WKUP_FILTER = 0U;
    USBSSD->LINK_CTRL = LINK_P2_MODE | ctrl_extra;
}

static void usbss_link_init(void)
{
    usbss_link_program(0U, 0U);
}

static void usbss_request_hs_fallback(uint32_t reason)
{
    if (g_ch32h417_usbss_udc.port_speed != USB_SPEED_SUPER) {
        return;
    }
    if (g_ch32h417_usbss_diag.u0 != 0U) {
        return;
    }
    g_ch32h417_usbss_hs_fallback_reason = reason;
    g_ch32h417_usbss_hs_fallback_request = 1U;
}

int usb_dc_ch32h417_usbss_init(uint8_t busid)
{
    if (busid != USB_CH32H417_BUS_SS) {
        return -1;
    }

    uint32_t v3f_wait;

    if (usbss_wait_ready_flag() != 0) {
        return -1;
    }
    v3f_wait = usbss_wait_v3f_post_ready();
    rt_kprintf("[USBSS] V3F trace stage=%u wait=%u idle=%u flag=0x%08x phy=0x%08x mod=0x%08x\r\n",
               (unsigned int)USBSS_V3F_TRACE_BASE[1],
               (unsigned int)v3f_wait,
               (unsigned int)USBSS_V3F_TRACE_BASE[4],
               (unsigned int)USBSS_V3F_TRACE_BASE[8],
               (unsigned int)USBSS_V3F_TRACE_BASE[9],
               (unsigned int)USBSS_V3F_TRACE_BASE[10]);

    usbss_enable_v5f_clock_gates();
    memset(&g_ch32h417_usbss_udc, 0, sizeof(g_ch32h417_usbss_udc));
    g_ch32h417_usbss_udc.port_speed = USB_SPEED_SUPER;

#if USBSS_USE_V3F_LINK_INIT
    g_ch32h417_usbss_sweep_done = 1U;
    usbss_ep0_state_init();
    rt_kprintf("[USBSS] keeping V3F link without V5F reset cfg=0x%08x ctrl=0x%08x st=0x%08x int=0x%08x ie=0x%08x\r\n",
               (unsigned int)USBSSD->LINK_CFG,
               (unsigned int)USBSSD->LINK_CTRL,
               (unsigned int)USBSSD->LINK_STATUS,
               (unsigned int)USBSSD->LINK_INT_FLAG,
               (unsigned int)USBSSD->LINK_INT_CTRL);
#else
    usbss_link_init();
    usbss_cfg_mod();
    usbss_device_reset(busid, 1U);
#endif
    usbss_snapshot_phy_diag();

    NVIC_SetPriority(USBSS_IRQn, 1);
    NVIC_SetPriority(USBSS_LINK_IRQn, 1);
    NVIC_EnableIRQ(USBSS_IRQn);
    NVIC_EnableIRQ(USBSS_LINK_IRQn);
    rt_kprintf("[USBSS] device init done\r\n");
    return 0;
}

int usb_dc_ch32h417_usbss_deinit(uint8_t busid)
{
    (void)busid;

    NVIC_DisableIRQ(USBSS_LINK_IRQn);
    NVIC_DisableIRQ(USBSS_IRQn);
    USBSSD->USB_CONTROL = USBSS_FORCE_RST;
    USBSSD->LINK_CFG |= LINK_PHY_RESET | U3_LINK_RESET;
    usbss_delay(1000U);
    USBSSD->USB_CONTROL &= ~USBSS_FORCE_RST;
    USBSSD->LINK_CFG &= ~(LINK_PHY_RESET | U3_LINK_RESET);
    return 0;
}

int usb_dc_ch32h417_usbss_set_address(uint8_t busid, uint8_t addr)
{
    (void)busid;
    g_ch32h417_usbss_udc.dev_addr = addr;
    g_ch32h417_usbss_udc.addr_pending = 1U;
    g_ch32h417_usbss_diag.set_addr++;
    g_ch32h417_usbss_diag.last_addr = addr;
    return 0;
}

int usb_dc_ch32h417_usbss_set_remote_wakeup(uint8_t busid)
{
    (void)busid;
    return -1;
}

uint8_t usb_dc_ch32h417_usbss_get_port_speed(uint8_t busid)
{
    (void)busid;
    return USB_SPEED_SUPER;
}

int usb_dc_ch32h417_usbss_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep->bEndpointAddress);
    uint16_t ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
    uint8_t ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);

    (void)busid;

    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    if (ep_idx == 0U) {
        usbss_ep0_state_init();
        return 0;
    }

    if ((ep_mps == 0U) || (ep_mps > USBSS_EP_BUFFER_SIZE)) {
        ep_mps = USBSS_EP_BUFFER_SIZE;
    }

    if (USB_EP_DIR_IS_OUT(ep->bEndpointAddress)) {
        USBSS_EP_RX_TypeDef *rx = usbss_rx_ep(ep_idx);

        if (rx == RT_NULL) {
            return -1;
        }
        g_ch32h417_usbss_udc.out_ep[ep_idx].ep_mps = ep_mps;
        g_ch32h417_usbss_udc.out_ep[ep_idx].ep_type = ep_type;
        g_ch32h417_usbss_udc.out_ep[ep_idx].ep_enabled = 1U;
        g_ch32h417_usbss_udc.out_ep[ep_idx].ep_stalled = 0U;
        rx->UEP_RX_CFG = USBSS_EP_RX_SEQ_AUTO;
        rx->UEP_RX_DMA = (uint32_t)usbss_ep_rx_buffer[ep_idx];
        rx->UEP_RX_DMA_OFS = 0U;
        rx->UEP_RX_CHAIN_MAX_NUMP = 1U;
        rx->UEP_RX_CR = USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR;
        USBSSD->UEP_RX_EN |= (uint16_t)(1U << ep_idx);
    } else {
        USBSS_EP_TX_TypeDef *tx = usbss_tx_ep(ep_idx);

        if (tx == RT_NULL) {
            return -1;
        }
        g_ch32h417_usbss_udc.in_ep[ep_idx].ep_mps = ep_mps;
        g_ch32h417_usbss_udc.in_ep[ep_idx].ep_type = ep_type;
        g_ch32h417_usbss_udc.in_ep[ep_idx].ep_enabled = 1U;
        g_ch32h417_usbss_udc.in_ep[ep_idx].ep_stalled = 0U;
        tx->UEP_TX_CFG = USBSS_EP_TX_SEQ_AUTO;
        tx->UEP_TX_DMA = (uint32_t)usbss_ep_tx_buffer[ep_idx];
        tx->UEP_TX_DMA_OFS = 0U;
        tx->UEP_TX_CHAIN_LEN = 0U;
        tx->UEP_TX_CHAIN_EXP_NUMP = 1U;
        tx->UEP_TX_CR = USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR;
        USBSSD->UEP_TX_EN |= (uint16_t)(1U << ep_idx);
    }

    return 0;
}

int usb_dc_ch32h417_usbss_ep_close(uint8_t busid, uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    (void)busid;

    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    if (ep_idx == 0U) {
        g_ch32h417_usbss_udc.in_ep[0].ep_enabled = 0U;
        g_ch32h417_usbss_udc.out_ep[0].ep_enabled = 0U;
    } else if (USB_EP_DIR_IS_OUT(ep)) {
        g_ch32h417_usbss_udc.out_ep[ep_idx].ep_enabled = 0U;
        USBSSD->UEP_RX_EN &= (uint16_t)~(1U << ep_idx);
    } else {
        g_ch32h417_usbss_udc.in_ep[ep_idx].ep_enabled = 0U;
        USBSSD->UEP_TX_EN &= (uint16_t)~(1U << ep_idx);
    }

    return 0;
}

int usb_dc_ch32h417_usbss_set_stall(uint8_t busid, uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    (void)busid;

    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    if (ep_idx == 0U) {
        USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_STALL;
        USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_STALL;
        g_ch32h417_usbss_udc.in_ep[0].ep_stalled = 1U;
        g_ch32h417_usbss_udc.out_ep[0].ep_stalled = 1U;
        g_ch32h417_usbss_diag.stall++;
    } else if (USB_EP_DIR_IS_OUT(ep)) {
        USBSS_EP_RX_TypeDef *rx = usbss_rx_ep(ep_idx);
        if (rx != RT_NULL) {
            rx->UEP_RX_CR |= USBSS_EP_RX_HALT;
        }
        g_ch32h417_usbss_udc.out_ep[ep_idx].ep_stalled = 1U;
    } else {
        USBSS_EP_TX_TypeDef *tx = usbss_tx_ep(ep_idx);
        if (tx != RT_NULL) {
            tx->UEP_TX_CR |= USBSS_EP_TX_HALT;
        }
        g_ch32h417_usbss_udc.in_ep[ep_idx].ep_stalled = 1U;
    }

    return 0;
}

int usb_dc_ch32h417_usbss_clear_stall(uint8_t busid, uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    (void)busid;

    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    if (ep_idx == 0U) {
        USBSSD->UEP0_TX_CTRL = 0U;
        USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_ACK;
        g_ch32h417_usbss_udc.in_ep[0].ep_stalled = 0U;
        g_ch32h417_usbss_udc.out_ep[0].ep_stalled = 0U;
    } else if (USB_EP_DIR_IS_OUT(ep)) {
        USBSS_EP_RX_TypeDef *rx = usbss_rx_ep(ep_idx);
        if (rx != RT_NULL) {
            rx->UEP_RX_CR = USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR;
        }
        g_ch32h417_usbss_udc.out_ep[ep_idx].ep_stalled = 0U;
    } else {
        USBSS_EP_TX_TypeDef *tx = usbss_tx_ep(ep_idx);
        if (tx != RT_NULL) {
            tx->UEP_TX_CR = USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR;
        }
        g_ch32h417_usbss_udc.in_ep[ep_idx].ep_stalled = 0U;
    }

    return 0;
}

int usb_dc_ch32h417_usbss_is_stalled(uint8_t busid, uint8_t ep, uint8_t *stalled)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    (void)busid;

    if ((ep_idx >= USB_CH32H417_MAX_EP_NUM) || (stalled == RT_NULL)) {
        return -1;
    }

    *stalled = USB_EP_DIR_IS_OUT(ep) ?
               g_ch32h417_usbss_udc.out_ep[ep_idx].ep_stalled :
               g_ch32h417_usbss_udc.in_ep[ep_idx].ep_stalled;
    return 0;
}

int usb_dc_ch32h417_usbss_start_write(uint8_t busid, uint8_t ep, const uint8_t *data, uint32_t data_len)
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

    ep_mps = g_ch32h417_usbss_udc.in_ep[ep_idx].ep_mps;
    if (ep_mps == 0U) {
        ep_mps = (ep_idx == 0U) ? USB_CH32H417_SS_EP0_MPS : USBSS_EP_BUFFER_SIZE;
    }

    g_ch32h417_usbss_udc.in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_ch32h417_usbss_udc.in_ep[ep_idx].xfer_len = data_len;
    g_ch32h417_usbss_udc.in_ep[ep_idx].actual_xfer_len = 0U;

    send_len = (data_len > ep_mps) ? ep_mps : data_len;
    usbss_ep_prime_in(ep_idx, data, send_len);
    return 0;
}

int usb_dc_ch32h417_usbss_start_read(uint8_t busid, uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    (void)busid;

    if ((data == RT_NULL) && (data_len != 0U)) {
        return -1;
    }
    if (ep_idx >= USB_CH32H417_MAX_EP_NUM) {
        return -1;
    }

    g_ch32h417_usbss_udc.out_ep[ep_idx].xfer_buf = data;
    g_ch32h417_usbss_udc.out_ep[ep_idx].xfer_len = data_len;
    g_ch32h417_usbss_udc.out_ep[ep_idx].actual_xfer_len = 0U;
    usbss_ep_prime_out(ep_idx);
    return 0;
}

static void usbss_irq_handler(uint8_t busid)
{
    uint32_t status = USBSSD->USB_STATUS;
    uint8_t ep_idx;
    uint32_t ep_dir;

    g_ch32h417_usbss_diag.irq++;
    g_ch32h417_usbss_diag.last_status = status;

    if ((status & USBSS_UDIF_SETUP) && ((status & USBSS_UDIF_STATUS) == 0U)) {
        usbss_handle_setup_packet(busid);
        USBSSD->USB_STATUS = USBSS_UDIF_SETUP;
        return;
    }

    if ((status & USBSS_UDIF_STATUS) != 0U) {
        USBSSD->USB_STATUS = USBSS_UDIF_STATUS;
        usbss_handle_status_stage(busid);
        return;
    }

    if ((status & USBSS_UIF_TRANSFER) != 0U) {
        g_ch32h417_usbss_diag.transfer++;
        ep_idx = (uint8_t)((status & USBSS_EP_ID_MASK) >> 8);
        ep_dir = status & USBSS_EP_DIR_MASK;
        if (ep_idx < USB_CH32H417_MAX_EP_NUM) {
            if (ep_dir != 0U) {
                usbss_handle_ep_in_xfer_complete(busid, ep_idx);
            } else {
                usbss_handle_ep_out_xfer_complete(busid, ep_idx);
            }
        }
        USBSSD->USB_STATUS = USBSS_UIF_TRANSFER;
    }
}

static void usbss_link_irq_handler(uint8_t busid)
{
    uint32_t link_int = USBSSD->LINK_INT_FLAG;
    uint32_t link_status = USBSSD->LINK_STATUS;
    uint32_t link_state = link_status & LINK_STATE_MASK;
    uint32_t link_lmp_data0;

    g_ch32h417_usbss_diag.link_irq++;
    g_ch32h417_usbss_diag.last_link_int = link_int;
    g_ch32h417_usbss_diag.last_link_state = link_state;
    g_ch32h417_usbss_diag.last_link_status = link_status;

    if ((link_status & LINK_RX_DETECT) != 0U) {
        g_ch32h417_usbss_diag.rx_detect_seen++;
    }
    if ((link_status & LINK_RX_LFPS) != 0U) {
        g_ch32h417_usbss_diag.rx_lfps_seen++;
    }
    if ((link_status & LINK_RX_TERM_PRES) != 0U) {
        g_ch32h417_usbss_diag.term_seen++;
    }
    if ((link_status & LINK_READY) != 0U) {
        g_ch32h417_usbss_diag.ready_seen++;
    }

    if (link_int == 0U) {
        return;
    }

    if ((link_int & LINK_IF_STATE_CHG) != 0U) {
        USBSSD->LINK_INT_FLAG = LINK_IF_STATE_CHG;

        if (link_state == LINK_STATE_DISABLE) {
            g_ch32h417_usbss_diag.disable++;
            USBSSD->LINK_CTRL &= ~LINK_GO_DISABLED;
            if ((g_ch32h417_usbss_diag.rxdet != 0U) ||
                (g_ch32h417_usbss_diag.rxdet_flag != 0U) ||
                (g_ch32h417_usbss_diag.rx_detect_seen != 0U)) {
                usbss_request_hs_fallback(USBSS_HS_FALLBACK_REASON_DISABLE);
            }
        } else if (link_state == LINK_STATE_RXDET) {
            g_ch32h417_usbss_diag.rxdet++;
            g_ch32h417_usbss_diag.rxdet_no_reset++;
        } else if (link_state == LINK_STATE_HOTRST) {
            g_ch32h417_usbss_diag.hotrst++;
            usbss_device_reset(busid, 1U);
            USBSSD->LINK_CTRL &= ~LINK_HOT_RESET;
        } else if (link_state == LINK_STATE_U0) {
            g_ch32h417_usbss_diag.u0++;
            usbd_event_connect_handler(busid);
        } else if (link_state == LINK_STATE_U1) {
            g_ch32h417_usbss_diag.u1++;
        } else if (link_state == LINK_STATE_U2) {
            g_ch32h417_usbss_diag.u2++;
        } else if (link_state == LINK_STATE_U3) {
            g_ch32h417_usbss_diag.u3++;
            usbd_event_suspend_handler(busid);
        } else if (link_state == LINK_STATE_RECOVERY) {
            g_ch32h417_usbss_diag.recovery++;
            usbss_phy_cfg(0U, 0x12U, 0x67C8U);
        } else if (link_state == LINK_STATE_INACTIVE) {
            g_ch32h417_usbss_diag.inactive++;
            usbss_request_hs_fallback(USBSS_HS_FALLBACK_REASON_INACTIVE);
        } else if (link_state == LINK_STATE_POLLING) {
            g_ch32h417_usbss_diag.polling++;
        }
    }

    if ((link_int & LINK_IF_TERM_PRES) != 0U) {
        USBSSD->LINK_INT_FLAG = LINK_IF_TERM_PRES;
    }

    if ((link_int & LINK_IF_RX_DET) != 0U) {
        USBSSD->LINK_INT_FLAG = LINK_IF_RX_DET;
        g_ch32h417_usbss_diag.rxdet_flag++;
        if (g_ch32h417_usbss_diag.disable != 0U) {
            usbss_request_hs_fallback(USBSS_HS_FALLBACK_REASON_DISABLE);
        }
    }

    if ((link_int & LINK_IF_DISABLE) != 0U) {
        USBSSD->LINK_INT_FLAG = LINK_IF_DISABLE;
        g_ch32h417_usbss_diag.disable_flag++;
    }

    if ((link_int & LINK_IF_READY) != 0U) {
        USBSSD->LINK_INT_FLAG = LINK_IF_READY;
        g_ch32h417_usbss_diag.ready_flag++;
    }

    if ((link_int & LINK_IF_UX_FAIL) != 0U) {
        USBSSD->LINK_INT_FLAG = LINK_IF_UX_FAIL;
        g_ch32h417_usbss_diag.ux_fail_flag++;
    }

    if ((link_int & LINK_IF_UX_EXIT_FAIL) != 0U) {
        USBSSD->LINK_INT_FLAG = LINK_IF_UX_EXIT_FAIL;
        g_ch32h417_usbss_diag.ux_exit_fail_flag++;
    }

    if ((link_int & LINK_IF_GO_U0) != 0U) {
        USBSSD->LINK_INT_FLAG = LINK_IF_GO_U0;
        g_ch32h417_usbss_diag.go_u0_flag++;
    }

    if ((link_int & LINK_IF_RECOVERY) != 0U) {
        USBSSD->LINK_INT_FLAG = LINK_IF_RECOVERY;
        g_ch32h417_usbss_diag.recovery_flag++;
    }

    if ((link_int & LINK_IF_INACTIVE) != 0U) {
        USBSSD->LINK_INT_FLAG = LINK_IF_INACTIVE;
        g_ch32h417_usbss_diag.inactive_flag++;
        usbss_request_hs_fallback(USBSS_HS_FALLBACK_REASON_INACTIVE);
    }

    if ((link_int & LINK_IF_TXEQ) != 0U) {
        USBSSD->LINK_INT_FLAG = LINK_IF_TXEQ;
        g_ch32h417_usbss_diag.txeq_flag++;
    }

    if ((link_int & LINK_IF_UX_REJ) != 0U) {
        USBSSD->LINK_INT_FLAG = LINK_IF_UX_REJ;
        g_ch32h417_usbss_diag.ux_rej_flag++;
    }

    if ((link_int & LINK_IF_RX_LMP_TOUT) != 0U) {
        USBSSD->LINK_INT_FLAG = LINK_IF_RX_LMP_TOUT;
        g_ch32h417_usbss_diag.rx_lmp_tout_flag++;
        USBSSD->LINK_CTRL |= LINK_GO_DISABLED;
        USBSSD->LINK_CTRL |= LINK_GO_RX_DET;
    }

    if ((link_int & LINK_IF_TX_LMP) != 0U) {
        USBSSD->LINK_INT_FLAG = LINK_IF_TX_LMP;
        USBSSD->LINK_LMP_TX_DATA0 = LMP_LINK_SPEED | LMP_PORT_CAP | LMP_HP;
        USBSSD->LINK_LMP_TX_DATA1 = (USBSSD->LINK_CFG & LINK_DOWN_MODE) ? (DOWN_STREAM | NUM_HP_BUF) : (UP_STREAM | NUM_HP_BUF);
        USBSSD->LINK_LMP_TX_DATA2 = 0U;
    }

    if ((link_int & LINK_IF_RX_LMP) != 0U) {
        USBSSD->LINK_INT_FLAG = LINK_IF_RX_LMP;
        link_lmp_data0 = USBSSD->LINK_LMP_RX_DATA0;

        if ((USBSSD->LINK_CFG & LINK_DOWN_MODE) == 0U) {
            if ((link_lmp_data0 & LMP_SUBTYPE_MASK) == LMP_PORT_CFG) {
                USBSSD->LINK_LMP_TX_DATA0 = LMP_LINK_SPEED | LMP_PORT_CFG_RES | LMP_HP;
                USBSSD->LINK_LMP_TX_DATA1 = 0U;
                USBSSD->LINK_LMP_TX_DATA2 = 0U;
                USBSSD->LINK_LMP_PORT_CAP |= LINK_LMP_TX_CAP_VLD;
            } else if ((link_lmp_data0 & LMP_SUBTYPE_MASK) == LMP_U2_INACT_TOUT) {
                USBSSD->LINK_U2_INACT_TIMER = (uint8_t)((link_lmp_data0 >> 9) & 0xFFU);
            } else if ((link_lmp_data0 & LMP_SUBTYPE_MASK) == LMP_SET_LINK_FUNC) {
                if ((link_lmp_data0 & (0x02U << 9)) != 0U) {
                    USBSSD->LINK_CFG |= LINK_U1_ALLOW | LINK_U2_ALLOW;
                } else {
                    USBSSD->LINK_CFG &= ~(LINK_U1_ALLOW | LINK_U2_ALLOW);
                }
            }
        }
    }

    if ((link_int & LINK_IF_WARM_RST) != 0U) {
        USBSSD->LINK_INT_FLAG = LINK_IF_WARM_RST;
        if ((USBSSD->LINK_STATUS & LINK_RX_WARM_RST) != 0U) {
            g_ch32h417_usbss_diag.warmrst++;
            usbss_device_reset(busid, 1U);
            USBSSD->LINK_CTRL |= LINK_GO_DISABLED;
            __NOP();
            __NOP();
            __NOP();
            __NOP();
            USBSSD->LINK_CTRL &= ~LINK_GO_DISABLED;
        }
    }
}

void ch32h417_usbss_isr(void)
{
    usbss_irq_handler(USB_CH32H417_BUS_SS);
}

void ch32h417_usbss_link_isr(void)
{
    usbss_link_irq_handler(USB_CH32H417_BUS_SS);
}

struct usbss_link_sweep_mode {
    const char *name;
    uint32_t cfg_extra;
    uint32_t ctrl_extra;
};

static void usbss_link_sweep_once(void)
{
    static const struct usbss_link_sweep_mode modes[] = {
        { "official", 0U, 0U },
        { "swap", LINK_SS_PLR_SWAP, 0U },
        { "force_term", LINK_FORCE_RXTERM, 0U },
        { "force_term_swap", LINK_FORCE_RXTERM | LINK_SS_PLR_SWAP, 0U },
        { "force_poll_swap", LINK_FORCE_RXTERM | LINK_FORCE_POLLING | LINK_SS_PLR_SWAP, LINK_POLLING_EN },
    };
    uint32_t i;
    uint32_t sample;
    int best = -1;

    if (USBSS_ENABLE_LINK_SWEEP == 0U) {
        return;
    }

    if (g_ch32h417_usbss_sweep_done != 0U) {
        return;
    }
    g_ch32h417_usbss_sweep_done = 1U;

    if ((*USBSS_READY_FLAG_ADDR != USBSS_READY_FLAG_VALUE) ||
        (g_ch32h417_usbss_diag.u0 != 0U) ||
        (g_ch32h417_usbss_diag.setup != 0U)) {
        return;
    }

    NVIC_DisableIRQ(USBSS_LINK_IRQn);
    NVIC_DisableIRQ(USBSS_IRQn);

    for (i = 0U; i < (sizeof(modes) / sizeof(modes[0])); i++) {
        uint32_t link_int = 0U;
        uint32_t last_status = 0U;
        uint32_t rxdet = 0U;
        uint32_t disable = 0U;
        uint32_t polling = 0U;
        uint32_t lfps = 0U;
        uint32_t term = 0U;
        uint32_t ready = 0U;
        uint32_t u0 = 0U;

        USBSSD->LINK_INT_CTRL = 0U;
        USBSSD->LINK_INT_FLAG = 0xFFFFFFFFU;
        usbss_link_program(modes[i].cfg_extra, modes[i].ctrl_extra);
        usbss_delay(1000U);

        for (sample = 0U; sample < USBSS_LINK_SWEEP_DELAY; sample++) {
            uint32_t status = USBSSD->LINK_STATUS;
            uint32_t state = status & LINK_STATE_MASK;

            last_status = status;
            link_int |= USBSSD->LINK_INT_FLAG;
            if (state == LINK_STATE_RXDET) {
                rxdet = 1U;
            } else if (state == LINK_STATE_DISABLE) {
                disable = 1U;
            } else if (state == LINK_STATE_POLLING) {
                polling = 1U;
            } else if (state == LINK_STATE_U0) {
                u0 = 1U;
            }
            if ((status & LINK_RX_LFPS) != 0U) {
                lfps = 1U;
            }
            if ((status & LINK_RX_TERM_PRES) != 0U) {
                term = 1U;
            }
            if ((status & LINK_READY) != 0U) {
                ready = 1U;
            }
            if ((u0 != 0U) || (ready != 0U)) {
                break;
            }
        }

        rt_kprintf("[USBSS sweep] mode=%s cfg=0x%08x ctrl=0x%08x int=0x%08x last=0x%08x rxdet=%u dis=%u poll=%u lfps=%u term=%u ready=%u u0=%u\r\n",
                   modes[i].name,
                   (unsigned int)(LINK_RX_EQ_EN | LINK_TX_DEEMPH_MASK | LINK_LTSSM_MODE | LINK_TOUT_MODE | LINK_RX_TERM_EN | modes[i].cfg_extra),
                   (unsigned int)(LINK_P2_MODE | modes[i].ctrl_extra),
                   (unsigned int)link_int,
                   (unsigned int)last_status,
                   (unsigned int)rxdet,
                   (unsigned int)disable,
                   (unsigned int)polling,
                   (unsigned int)lfps,
                   (unsigned int)term,
                   (unsigned int)ready,
                   (unsigned int)u0);

        if ((u0 != 0U) || (ready != 0U)) {
            best = (int)i;
            break;
        }
    }

    if (best >= 0) {
        usbss_link_program(modes[best].cfg_extra, modes[best].ctrl_extra);
    } else {
        usbss_link_init();
    }
    usbss_cfg_mod();
    usbss_device_reset(USB_CH32H417_BUS_SS, 0U);

    NVIC_EnableIRQ(USBSS_IRQn);
    NVIC_EnableIRQ(USBSS_LINK_IRQn);
}

uint8_t usb_dc_ch32h417_usbss_take_hs_fallback_request(uint32_t *reason)
{
    rt_base_t level;
    uint8_t pending;

    level = rt_hw_interrupt_disable();
    pending = g_ch32h417_usbss_hs_fallback_request;
    if (pending != 0U) {
        if (reason != RT_NULL) {
            *reason = g_ch32h417_usbss_hs_fallback_reason;
        }
        g_ch32h417_usbss_hs_fallback_request = 0U;
    }
    rt_hw_interrupt_enable(level);

    return pending;
}

void usb_dc_ch32h417_usbss_dump_diag(void)
{
    usbss_link_sweep_once();
    usbss_sample_link_window();

    rt_kprintf("[USBSS diag] irq=%u link=%u reset=%u u0=%u rxdet=%u nrst=%u hotrst=%u warmrst=%u setup=%u status=%u xfer=%u\r\n",
               (unsigned int)g_ch32h417_usbss_diag.irq,
               (unsigned int)g_ch32h417_usbss_diag.link_irq,
               (unsigned int)g_ch32h417_usbss_diag.reset,
               (unsigned int)g_ch32h417_usbss_diag.u0,
               (unsigned int)g_ch32h417_usbss_diag.rxdet,
               (unsigned int)g_ch32h417_usbss_diag.rxdet_no_reset,
               (unsigned int)g_ch32h417_usbss_diag.hotrst,
               (unsigned int)g_ch32h417_usbss_diag.warmrst,
               (unsigned int)g_ch32h417_usbss_diag.setup,
               (unsigned int)g_ch32h417_usbss_diag.status,
               (unsigned int)g_ch32h417_usbss_diag.transfer);
    rt_kprintf("[USBSS diag] pIN0=%u in0=%u pOUT0=%u out0=%u stall=%u addr=%u st=0x%08x lint=0x%08x lst=0x%08x\r\n",
               (unsigned int)g_ch32h417_usbss_diag.ep0_prime_in,
               (unsigned int)g_ch32h417_usbss_diag.ep0_in,
               (unsigned int)g_ch32h417_usbss_diag.ep0_prime_out,
               (unsigned int)g_ch32h417_usbss_diag.ep0_out,
               (unsigned int)g_ch32h417_usbss_diag.stall,
               (unsigned int)g_ch32h417_usbss_diag.last_addr,
               (unsigned int)g_ch32h417_usbss_diag.last_status,
               (unsigned int)g_ch32h417_usbss_diag.last_link_int,
               (unsigned int)g_ch32h417_usbss_diag.last_link_state);
    rt_kprintf("[USBSS diag] setup=%08x/%08x resp=%08x tx=%u rx=%u\r\n",
               (unsigned int)g_ch32h417_usbss_diag.last_setup0,
               (unsigned int)g_ch32h417_usbss_diag.last_setup1,
               (unsigned int)g_ch32h417_usbss_diag.last_resp0,
               (unsigned int)g_ch32h417_usbss_diag.last_tx_len,
               (unsigned int)g_ch32h417_usbss_diag.last_rx_len);
    rt_kprintf("[USBSS link] rxdet_if=%u disable_if=%u\r\n",
               (unsigned int)g_ch32h417_usbss_diag.rxdet_flag,
               (unsigned int)g_ch32h417_usbss_diag.disable_flag);
    rt_kprintf("[USBSS states] dis=%u inact=%u poll=%u rec=%u u1=%u u2=%u u3=%u rxd=%u lfps=%u term=%u ready=%u lstat=0x%08x\r\n",
               (unsigned int)g_ch32h417_usbss_diag.disable,
               (unsigned int)g_ch32h417_usbss_diag.inactive,
               (unsigned int)g_ch32h417_usbss_diag.polling,
               (unsigned int)g_ch32h417_usbss_diag.recovery,
               (unsigned int)g_ch32h417_usbss_diag.u1,
               (unsigned int)g_ch32h417_usbss_diag.u2,
               (unsigned int)g_ch32h417_usbss_diag.u3,
               (unsigned int)g_ch32h417_usbss_diag.rx_detect_seen,
               (unsigned int)g_ch32h417_usbss_diag.rx_lfps_seen,
               (unsigned int)g_ch32h417_usbss_diag.term_seen,
               (unsigned int)g_ch32h417_usbss_diag.ready_seen,
               (unsigned int)g_ch32h417_usbss_diag.last_link_status);
    rt_kprintf("[USBSS flags] pen=%u rdy=%u ux=%u uxe=%u gu0=%u rec=%u ina=%u txeq=%u rej=%u lmp_to=%u\r\n",
               (unsigned int)g_ch32h417_usbss_diag.polling_enable,
               (unsigned int)g_ch32h417_usbss_diag.ready_flag,
               (unsigned int)g_ch32h417_usbss_diag.ux_fail_flag,
               (unsigned int)g_ch32h417_usbss_diag.ux_exit_fail_flag,
               (unsigned int)g_ch32h417_usbss_diag.go_u0_flag,
               (unsigned int)g_ch32h417_usbss_diag.recovery_flag,
               (unsigned int)g_ch32h417_usbss_diag.inactive_flag,
               (unsigned int)g_ch32h417_usbss_diag.txeq_flag,
               (unsigned int)g_ch32h417_usbss_diag.ux_rej_flag,
               (unsigned int)g_ch32h417_usbss_diag.rx_lmp_tout_flag);
    rt_kprintf("[USBSS sample] n=%u first=0x%08x last=0x%08x or=0x%08x and=0x%08x rxdet=%u lfps=%u term=%u ready=%u dis=%u poll=%u rec=%u u0=%u\r\n",
               (unsigned int)g_ch32h417_usbss_diag.sample_count,
               (unsigned int)g_ch32h417_usbss_diag.sample_first,
               (unsigned int)g_ch32h417_usbss_diag.sample_last,
               (unsigned int)g_ch32h417_usbss_diag.sample_or,
               (unsigned int)g_ch32h417_usbss_diag.sample_and,
               (unsigned int)g_ch32h417_usbss_diag.sample_rxdet,
               (unsigned int)g_ch32h417_usbss_diag.sample_lfps,
               (unsigned int)g_ch32h417_usbss_diag.sample_term,
               (unsigned int)g_ch32h417_usbss_diag.sample_ready,
               (unsigned int)g_ch32h417_usbss_diag.sample_disable,
               (unsigned int)g_ch32h417_usbss_diag.sample_polling,
               (unsigned int)g_ch32h417_usbss_diag.sample_recovery,
               (unsigned int)g_ch32h417_usbss_diag.sample_u0);
    rt_kprintf("[USBSS regs] ctl=0x%08x sta=0x%08x lcfg=0x%08x lctl=0x%08x\r\n",
               (unsigned int)USBSSD->USB_CONTROL,
               (unsigned int)USBSSD->USB_STATUS,
               (unsigned int)USBSSD->LINK_CFG,
               (unsigned int)USBSSD->LINK_CTRL);
    rt_kprintf("[USBSS regs2] lint=0x%08x lstat=0x%08x lpm=0x%04x cap=0x%08x rx0=0x%08x tx0=0x%08x ep0=%08x/%08x\r\n",
               (unsigned int)USBSSD->LINK_INT_FLAG,
               (unsigned int)USBSSD->LINK_STATUS,
               (unsigned int)USBSSD->LINK_LPM_CR,
               (unsigned int)USBSSD->LINK_LMP_PORT_CAP,
               (unsigned int)USBSSD->LINK_LMP_RX_DATA0,
               (unsigned int)USBSSD->LINK_LMP_TX_DATA0,
               (unsigned int)USBSSD->UEP0_TX_CTRL,
               (unsigned int)USBSSD->UEP0_RX_CTRL);
    rt_kprintf("[USBSS v3f] stage=%u idle=%u flag=0x%08x phy=0x%08x mod=0x%08x\r\n",
               (unsigned int)USBSS_V3F_TRACE_BASE[1],
               (unsigned int)USBSS_V3F_TRACE_BASE[4],
               (unsigned int)USBSS_V3F_TRACE_BASE[8],
               (unsigned int)USBSS_V3F_TRACE_BASE[9],
               (unsigned int)USBSS_V3F_TRACE_BASE[10]);
    rt_kprintf("[USBSS v3fl] cfg=0x%08x ctl=0x%08x st0=0x%08x st1=0x%08x lint=0x%08x lpm=0x%08x uctl=0x%08x ust=0x%08x\r\n",
               (unsigned int)USBSS_V3F_TRACE_BASE[11],
               (unsigned int)USBSS_V3F_TRACE_BASE[12],
               (unsigned int)USBSS_V3F_TRACE_BASE[13],
               (unsigned int)USBSS_V3F_TRACE_BASE[14],
               (unsigned int)USBSS_V3F_TRACE_BASE[15],
               (unsigned int)USBSS_V3F_TRACE_BASE[16],
               (unsigned int)USBSS_V3F_TRACE_BASE[17],
               (unsigned int)USBSS_V3F_TRACE_BASE[18]);
    rt_kprintf("[USBSS tune] wait=%u pst=0x%08x termst=0x%08x v3wait=%u v3pst=0x%08x v3term=0x%08x\r\n",
               (unsigned int)g_ch32h417_usbss_diag.link_busy_wait,
               (unsigned int)g_ch32h417_usbss_diag.post_busy_status,
               (unsigned int)g_ch32h417_usbss_diag.post_term_status,
               (unsigned int)USBSS_V3F_TRACE_BASE[19],
               (unsigned int)USBSS_V3F_TRACE_BASE[20],
               (unsigned int)USBSS_V3F_TRACE_BASE[21]);
    rt_kprintf("[USBSS phy] r03=0x%08x r0d=0x%08x r11=0x%08x r12=0x%08x r13=0x%08x r15=0x%08x\r\n",
               (unsigned int)g_ch32h417_usbss_diag.phy_r03,
               (unsigned int)g_ch32h417_usbss_diag.phy_r0d,
               (unsigned int)g_ch32h417_usbss_diag.phy_r11,
               (unsigned int)g_ch32h417_usbss_diag.phy_r12,
               (unsigned int)g_ch32h417_usbss_diag.phy_r13,
               (unsigned int)g_ch32h417_usbss_diag.phy_r15);
    rt_kprintf("[USBSS phy2] cr=0x%08x dat=0x%08x mod=0x%08x rctl=0x%08x pll2=0x%08x hb=0x%08x\r\n",
               (unsigned int)g_ch32h417_usbss_diag.phy_cfg_cr,
               (unsigned int)g_ch32h417_usbss_diag.phy_cfg_dat,
               (unsigned int)g_ch32h417_usbss_diag.phy_mod,
               (unsigned int)g_ch32h417_usbss_diag.rcc_ctlr,
               (unsigned int)g_ch32h417_usbss_diag.rcc_pllcfgr2,
               (unsigned int)g_ch32h417_usbss_diag.rcc_hbpcenr);
}

void USBSS_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USBSS_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    usbss_irq_handler(USB_CH32H417_BUS_SS);
    rt_interrupt_leave();
    FREE_INT_SP();
}

void USBSS_LINK_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USBSS_LINK_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    usbss_link_irq_handler(USB_CH32H417_BUS_SS);
    rt_interrupt_leave();
    FREE_INT_SP();
}
