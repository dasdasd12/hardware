#include "ch32h417_usbhs_hid_nkro.h"

#include <string.h>

#include "ch32h417.h"
#include "ch32h417_usb.h"
#include "ch32h417_usbhs_device.h"
#include "usbd_compatibility_hid.h"

uint8_t HID_Report_Buffer[DEF_USBD_HS_PACK_SIZE + 1];
volatile uint16_t Data_Pack_Max_Len;
volatile uint16_t Head_Pack_Len;
volatile uint8_t HID_Set_Report_Flag = SET_REPORT_DEAL_OVER;

static uint32_t s_report_count;
static uint8_t s_pending_report[AIK_NKRO_REPORT_BYTES];
static volatile uint8_t s_pending_full;
static volatile uint8_t s_in_flight;
static ch32h417_usbhs_hid_nkro_diag_t s_diag;

static void refresh_diag(void)
{
    s_diag.clock_ready =
        ((RCC->CTLR & (uint32_t)RCC_USBHS_PLLRDY) != 0U) ? 1U : 0U;
    s_diag.clock_error = 0U;
    s_diag.rcc_ctlr = RCC->CTLR;
    s_diag.rcc_cfgr2 = RCC->CFGR2;
    s_diag.rcc_pllcfgr2 = RCC->PLLCFGR2;
    s_diag.usb_base_ctrl = USBHSD->BASE_MODE;
    s_diag.usb_udev_ctrl = USBHSD->CONTROL;
    s_diag.usb_int_en = USBHSD->INT_EN;
    s_diag.uep0_dma = USBHSD->UEP0_DMA;
    s_diag.last_intflag = USBHSD->INT_FG;
    s_diag.last_intst = USBHSD->INT_ST;
    s_diag.last_setup_type = USBHS_SetupReqType;
    s_diag.last_setup_request = USBHS_SetupReqCode;
    s_diag.last_setup_value = USBHS_SetupReqValue;
    s_diag.last_setup_index = USBHS_SetupReqIndex;
    s_diag.last_setup_length = USBHS_SetupReqLen;
    s_diag.last_xfer_buf0 = USBHSD->UEP2_TX_DMA;
    s_diag.last_xfer_buf1 = (uint32_t)USBHS_EP2_Tx_Buf;
    s_diag.last_resp0 = USBHSD->UEP2_TX_CTRL;
    s_diag.last_tx_len = USBHSD->UEP2_TX_LEN;
    s_diag.last_rx_len = USBHSD->UEP1_RX_LEN;
}

static uint8_t ep2_in_ready(void)
{
    return (uint8_t)(((USBHSD->UEP2_TX_CTRL & USBHS_UEP_T_RES_MASK) ==
                      USBHS_UEP_T_RES_NAK) ? 1U : 0U);
}

static void arm_pending_if_ready_locked(void)
{
    if((s_pending_full == 0U) || (s_in_flight != 0U) ||
       (USBHS_DevEnumStatus == 0U) || (ep2_in_ready() == 0U))
    {
        return;
    }

    memset((void *)HID_Report_Buffer, 0, sizeof(HID_Report_Buffer));
    memcpy((void *)HID_Report_Buffer, s_pending_report, AIK_NKRO_REPORT_BYTES);
    memcpy((void *)USBHS_EP2_Tx_Buf, s_pending_report, AIK_NKRO_REPORT_BYTES);
    USBHSD->UEP2_TX_DMA = (uint32_t)USBHS_EP2_Tx_Buf;
    USBHSD->UEP2_TX_LEN = AIK_NKRO_REPORT_BYTES;
    USBHSD->UEP2_TX_CTRL =
        (USBHSD->UEP2_TX_CTRL & (uint8_t)(~USBHS_UEP_T_RES_MASK)) |
        USBHS_UEP_T_RES_ACK;
    s_in_flight = 1U;
}

void ch32h417_usbhs_hid_nkro_init(void)
{
    NVIC_DisableIRQ(USBHS_IRQn);
    memset((void *)HID_Report_Buffer, 0, sizeof(HID_Report_Buffer));
    memset((void *)s_pending_report, 0, sizeof(s_pending_report));
    memset((void *)&RingBuffer_Comm, 0, sizeof(RingBuffer_Comm));
    memset((void *)Data_Buffer, 0, DEF_RING_BUFFER_SIZE);
    memset(&s_diag, 0, sizeof(s_diag));
    s_report_count = 0U;
    s_pending_full = 0U;
    s_in_flight = 0U;
    Data_Pack_Max_Len = 0U;
    Head_Pack_Len = 0U;
    HID_Set_Report_Flag = SET_REPORT_DEAL_OVER;

    USBHS_Device_Init(ENABLE);
    refresh_diag();
}

uint8_t ch32h417_usbhs_hid_nkro_pending_empty(void)
{
    if(USBHS_DevEnumStatus == 0U)
    {
        return 0U;
    }

    if((s_pending_full != 0U) && (s_in_flight == 0U) && (ep2_in_ready() != 0U))
    {
        NVIC_DisableIRQ(USBHS_IRQn);
        arm_pending_if_ready_locked();
        NVIC_EnableIRQ(USBHS_IRQn);
    }

    return (uint8_t)(((s_pending_full == 0U) && (s_in_flight == 0U)) ? 1U : 0U);
}

uint8_t ch32h417_usbhs_hid_nkro_submit(const uint8_t nkro16[AIK_NKRO_REPORT_BYTES])
{
    uint8_t accepted = 0U;

    if(nkro16 == 0)
    {
        refresh_diag();
        return 0U;
    }

    NVIC_DisableIRQ(USBHS_IRQn);
    if((USBHS_DevEnumStatus != 0U) && (s_pending_full == 0U) && (s_in_flight == 0U))
    {
        memcpy((void *)s_pending_report, nkro16, AIK_NKRO_REPORT_BYTES);
        s_pending_full = 1U;
        arm_pending_if_ready_locked();
        accepted = 1U;
    }
    refresh_diag();
    NVIC_EnableIRQ(USBHS_IRQn);
    return accepted;
}

void ch32h417_usbhs_hid_nkro_send(const uint8_t nkro16[AIK_NKRO_REPORT_BYTES])
{
    (void)ch32h417_usbhs_hid_nkro_submit(nkro16);
}

uint32_t ch32h417_usbhs_hid_nkro_reports(void)
{
    return s_report_count;
}

uint8_t ch32h417_usbhs_hid_nkro_debug_write(const char *line)
{
    (void)line;
    return 0U;
}

void ch32h417_usbhs_hid_nkro_diag_snapshot(ch32h417_usbhs_hid_nkro_diag_t *diag)
{
    if(diag != 0)
    {
        refresh_diag();
        *diag = s_diag;
    }
}

void ch32h417_usbhs_hid_nkro_on_in_complete(void)
{
    s_pending_full = 0U;
    s_in_flight = 0U;
    s_report_count++;
}

void ch32h417_usbhs_hid_nkro_on_bus_reset(void)
{
    s_pending_full = 0U;
    s_in_flight = 0U;
}
