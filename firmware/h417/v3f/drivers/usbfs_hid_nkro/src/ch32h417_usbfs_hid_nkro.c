#include "ch32h417_usbfs_hid_nkro.h"

#include <string.h>

#include "ch32h417.h"
#include "ch32h417_usb.h"
#include "ch32h417_usbfs_device.h"
#include "usbd_compatibility_hid.h"

uint8_t HID_Report_Buffer[64];
volatile uint8_t HID_Set_Report_Flag = SET_REPORT_DEAL_OVER;

static uint32_t s_report_count;
static ch32h417_usbfs_hid_nkro_diag_t s_diag;

void Delay_Us(uint32_t n)
{
    while(n-- != 0U)
    {
        volatile uint32_t cycles = SystemCoreClock / 4000000U;

        while(cycles-- != 0U)
        {
        }
    }
}

void Delay_Ms(uint32_t n)
{
    while(n-- != 0U)
    {
        Delay_Us(1000U);
    }
}

static void refresh_diag(void)
{
    s_diag.clock_ready =
        ((RCC->CTLR & (uint32_t)RCC_USBHS_PLLRDY) != 0U) ? 1U : 0U;
    s_diag.clock_error = 0U;
    s_diag.rcc_ctlr = RCC->CTLR;
    s_diag.rcc_cfgr2 = RCC->CFGR2;
    s_diag.rcc_pllcfgr2 = RCC->PLLCFGR2;
    s_diag.usb_base_ctrl = USBFSD->BASE_CTRL;
    s_diag.usb_udev_ctrl = USBFSD->UDEV_CTRL;
    s_diag.usb_int_en = USBFSD->INT_EN;
    s_diag.uep0_dma = USBFSD->UEP0_DMA;
    s_diag.last_intflag = USBFSD->INT_FG;
    s_diag.last_intst = USBFSD->INT_ST;
    s_diag.last_tx_len = USBFSD_UEP_TLEN(DEF_UEP2);
    s_diag.last_rx_len = USBFSD->RX_LEN;
}

static uint8_t ep2_in_ready(void)
{
    return (uint8_t)(((USBFSD->UEP2_TX_CTRL & USBFS_UEP_T_RES_MASK) ==
                      USBFS_UEP_T_RES_NAK) ? 1U : 0U);
}

void ch32h417_usbfs_hid_nkro_init(void)
{
    NVIC_DisableIRQ(USBFS_IRQn);
    memset((void *)HID_Report_Buffer, 0, sizeof(HID_Report_Buffer));
    memset((void *)&RingBuffer_Comm, 0, sizeof(RingBuffer_Comm));
    memset((void *)Data_Buffer, 0, DEF_RING_BUFFER_SIZE);
    memset(&s_diag, 0, sizeof(s_diag));
    s_report_count = 0U;
    HID_Set_Report_Flag = SET_REPORT_DEAL_OVER;

    USBFS_RCC_Init();
    USBFS_Device_Init(ENABLE);
    refresh_diag();
}

void ch32h417_usbfs_hid_nkro_send(const uint8_t nkro16[AIK_NKRO_REPORT_BYTES])
{
    if((nkro16 == 0) || (USBFS_DevEnumStatus == 0U))
    {
        refresh_diag();
        return;
    }

    NVIC_DisableIRQ(USBFS_IRQn);
    if(ep2_in_ready() != 0U)
    {
        memset((void *)HID_Report_Buffer, 0, sizeof(HID_Report_Buffer));
        memcpy((void *)HID_Report_Buffer, nkro16, AIK_NKRO_REPORT_BYTES);
        memcpy((void *)USBFS_EP2_Buf, nkro16, AIK_NKRO_REPORT_BYTES);
        USBFSD->UEP2_DMA = (uint32_t)USBFS_EP2_Buf;
        USBFSD_UEP_TLEN(DEF_UEP2) = AIK_NKRO_REPORT_BYTES;
        USBFSD->UEP2_TX_CTRL =
            (USBFSD->UEP2_TX_CTRL & (uint8_t)(~USBFS_UEP_T_RES_MASK)) |
            USBFS_UEP_T_RES_ACK;
        s_report_count++;
    }
    refresh_diag();
    NVIC_EnableIRQ(USBFS_IRQn);
}

uint8_t ch32h417_usbfs_hid_nkro_pending_empty(void)
{
    if(USBFS_DevEnumStatus == 0U)
    {
        refresh_diag();
        return 0U;
    }

    refresh_diag();
    return ep2_in_ready();
}

uint8_t ch32h417_usbfs_hid_nkro_submit(const uint8_t nkro16[AIK_NKRO_REPORT_BYTES])
{
    uint32_t before;

    if(ch32h417_usbfs_hid_nkro_pending_empty() == 0U)
    {
        return 0U;
    }

    before = s_report_count;
    ch32h417_usbfs_hid_nkro_send(nkro16);
    return (uint8_t)((s_report_count != before) ? 1U : 0U);
}

uint32_t ch32h417_usbfs_hid_nkro_reports(void)
{
    return s_report_count;
}

uint8_t ch32h417_usbfs_hid_nkro_debug_write(const char *line)
{
#if V3F_ENABLE_USBFS_CDC_DEBUG
    uint8_t sent;
    uint16_t len;

    if(line == 0)
    {
        return 0U;
    }

    len = (uint16_t)strlen(line);
    NVIC_DisableIRQ(USBFS_IRQn);
    sent = USBFS_CDC_Debug_Send((const uint8_t *)line, len);
    NVIC_EnableIRQ(USBFS_IRQn);
    return sent;
#else
    (void)line;
    return 0U;
#endif
}

void ch32h417_usbfs_hid_nkro_diag_snapshot(ch32h417_usbfs_hid_nkro_diag_t *diag)
{
    if(diag != 0)
    {
        refresh_diag();
        *diag = s_diag;
    }
}
