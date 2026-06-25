/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_cdc_debug.c
 * Description        : USB FS CDC debug/control channel for CH585M
 *******************************************************************************/

#include "CONFIG.h"
#include "usb_cdc_debug.h"
#include "CH58x_usbdev.h"
#include "ble_hid.h"
#include "hiddev.h"
#include "kvm_control.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define USB_CDC_EP0_SIZE               64
#define USB_CDC_EP_DATA_SIZE           64
#define USB_CDC_NOTIFY_SIZE            10
#define USB_CDC_TX_BUF_SIZE            1024
#define USB_CDC_RX_BUF_SIZE            256
#define USB_CDC_CMD_BUF_SIZE           96
#define UART0_CMD_BUF_SIZE             96

#define DEBUG_CMD_OUTPUT_USB           0x01
#define DEBUG_CMD_OUTPUT_UART0         0x02

#define USB_CDC_SET_LINE_CODING        0x20
#define USB_CDC_GET_LINE_CODING        0x21
#define USB_CDC_SET_CONTROL_LINE_STATE 0x22
#define USB_CDC_SEND_BREAK             0x23
#define USB_CDC_SERIAL_STATE           0x20

#define USB_CDC_SERIAL_DTR             0x0001

static const uint8_t g_usb_dev_descr[] = {
    0x12,
    0x01,
    0x10, 0x01,
    0x02,
    0x00,
    0x00,
    USB_CDC_EP0_SIZE,
    0x86, 0x1A,
    0x40, 0x80,
    0x00, 0x01,
    0x01,
    0x02,
    0x03,
    0x01
};

static const uint8_t g_usb_cfg_descr[] = {
    0x09, 0x02, 0x43, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,

    0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
    0x05, 0x24, 0x00, 0x10, 0x01,
    0x04, 0x24, 0x02, 0x02,
    0x05, 0x24, 0x06, 0x00, 0x01,
    0x05, 0x24, 0x01, 0x01, 0x00,
    0x07, 0x05, 0x84, 0x03, 0x08, 0x00, 0x01,

    0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
    0x07, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00,
    0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00
};

static const uint8_t g_usb_lang_descr[] = {
    0x04, 0x03, 0x09, 0x04
};

static const uint8_t g_usb_manu_descr[] = {
    0x12, 0x03,
    'Q', 0x00, 'i', 0x00, 'n', 0x00, 'H', 0x00,
    'e', 0x00, 'n', 0x00, 'g', 0x00
};

static const uint8_t g_usb_prod_descr[] = {
    0x22, 0x03,
    'C', 0x00, 'H', 0x00, '5', 0x00, '8', 0x00, '5', 0x00,
    'M', 0x00, ' ', 0x00,
    'D', 0x00, 'e', 0x00, 'b', 0x00, 'u', 0x00, 'g', 0x00,
    ' ', 0x00,
    'C', 0x00, 'D', 0x00, 'C', 0x00
};

static const uint8_t g_usb_serial_descr[] = {
    0x16, 0x03,
    'C', 0x00, 'H', 0x00, '5', 0x00, '8', 0x00,
    '5', 0x00, 'M', 0x00, '0', 0x00, '0', 0x00,
    '0', 0x00, '1', 0x00
};

static uint8_t g_dev_config = 0;
static uint8_t g_setup_req_code = 0;
static uint16_t g_setup_req_len = 0;
static const uint8_t *g_setup_descr = NULL;
static uint16_t g_control_line_state = 0;
static uint8_t g_line_coding[7] = {0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08};
static uint8_t g_ready_banner_sent = FALSE;
static uint8_t g_serial_state_pending = FALSE;

static volatile uint16_t g_tx_head = 0;
static volatile uint16_t g_tx_tail = 0;
static volatile uint16_t g_rx_head = 0;
static volatile uint16_t g_rx_tail = 0;
static uint8_t g_cmd_len = 0;
static char g_cmd_buf[USB_CDC_CMD_BUF_SIZE];
static uint8_t g_uart0_cmd_len = 0;
static uint8_t g_uart0_banner_sent = FALSE;
static uint8_t g_cmd_output_mask = DEBUG_CMD_OUTPUT_USB;
static char g_uart0_cmd_buf[UART0_CMD_BUF_SIZE];

__attribute__((aligned(4))) static uint8_t g_ep0_databuf[64 + 64 + 64];
__attribute__((aligned(4))) static uint8_t g_ep1_databuf[64 + 64];
__attribute__((aligned(4))) static uint8_t g_ep2_databuf[64 + 64];
__attribute__((aligned(4))) static uint8_t g_ep3_databuf[64 + 64];

static uint8_t g_tx_ring[USB_CDC_TX_BUF_SIZE];
static uint8_t g_rx_ring[USB_CDC_RX_BUF_SIZE];

static uint16_t usbCdcRingNext(uint16_t value, uint16_t size);
static uint8_t usbCdcTxPush(uint8_t value);
static uint8_t usbCdcTxPop(uint8_t *value);
static uint8_t usbCdcRxPush(uint8_t value);
static uint8_t usbCdcRxPop(uint8_t *value);
static uint16_t usbCdcCopySetupData(const uint8_t *descr, uint16_t total_len);
static void usbCdcQueueSerialState(void);
static void usbCdcSendSerialState(void);
static void usbCdcFlushTx(void);
static void usbCdcProcessRx(void);
static void uart0DebugProcessRx(void);
static void uart0DebugWrite(const uint8_t *data, uint16_t len);
static void uart0DebugWriteString(const char *str);
static void usbCdcHandleCommand(char *line);
static void usbCdcPrintHelp(void);
static void usbCdcPrintStatus(void);
static void debugCmdWriteString(const char *str);
static void debugCmdLog(const char *fmt, ...);
static uint8_t usbCdcParseTapKey(const char *token, uint8_t *keycode);
static uint8_t usbCdcParseCombo(char *args, uint8_t *modifier, uint8_t *keycode);
static uint8_t usbCdcParseModifier(const char *token, uint8_t *modifier);
static uint8_t usbCdcParseRawReport(char *args, uint8_t *report8);
static uint8_t usbCdcParseHexByte(const char *token, uint8_t *value);
static uint8_t usbCdcHexValue(char ch, uint8_t *value);
static uint8_t usbCdcParseTarget(const char *token, uint8_t *target);
static void usbCdcToLower(char *str);
static void usbCdcTrim(char *str);
static uint8_t usbCdcStreq(const char *a, const char *b);

void USB_CDC_DebugInit(void)
{
    pEP0_RAM_Addr = g_ep0_databuf;
    pEP1_RAM_Addr = g_ep1_databuf;
    pEP2_RAM_Addr = g_ep2_databuf;
    pEP3_RAM_Addr = g_ep3_databuf;

    USB_DeviceInit();
    PFIC_EnableIRQ(USB_IRQn);

    USB_CDC_DebugWriteString("\r\nCH585M USB CDC boot\r\n");
    USB_CDC_DebugWriteString("Type 'help' for commands.\r\n");
    uart0DebugWriteString("\r\nCH585M UART0 debug ready\r\n");
    uart0DebugWriteString("Commands: help, status, tap <key>, combo <mods> <key>, type <text>, kvm switch <n>\r\n");
    g_uart0_banner_sent = TRUE;
}

void USB_CDC_DebugProcess(void)
{
    if(!g_uart0_banner_sent)
    {
        g_uart0_banner_sent = TRUE;
        uart0DebugWriteString("\r\nUART0 command channel ready\r\n");
    }

    if(!g_ready_banner_sent && USB_CDC_DebugIsReady())
    {
        g_ready_banner_sent = TRUE;
        USB_CDC_DebugWriteString("\r\nUSB CDC ready\r\n");
        USB_CDC_DebugWriteString("Commands: help, status, tap <key>, combo <mods> <key>, type <text>, kvm switch <n>\r\n");
    }

    uart0DebugProcessRx();
    usbCdcProcessRx();

    if(g_serial_state_pending && g_dev_config && EP4_GetINSta())
    {
        usbCdcSendSerialState();
    }

    if(USB_CDC_DebugIsReady() && EP1_GetINSta())
    {
        usbCdcFlushTx();
    }
}

uint8_t USB_CDC_DebugIsReady(void)
{
    return (g_dev_config != 0) && ((g_control_line_state & USB_CDC_SERIAL_DTR) != 0);
}

uint8_t USB_CDC_DebugWrite(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if(data == NULL)
    {
        return INVALIDPARAMETER;
    }

    for(i = 0; i < len; ++i)
    {
        if(!usbCdcTxPush(data[i]))
        {
            return FAILURE;
        }
    }

    return SUCCESS;
}

void USB_CDC_DebugWriteString(const char *str)
{
    if(str != NULL)
    {
        USB_CDC_DebugWrite((const uint8_t *)str, (uint16_t)strlen(str));
    }
}

void USB_CDC_DebugLog(const char *fmt, ...)
{
    char buffer[160];
    int len;
    va_list args;

    if(fmt == NULL)
    {
        return;
    }

    va_start(args, fmt);
    len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if(len <= 0)
    {
        return;
    }

    if((size_t)len >= sizeof(buffer))
    {
        len = sizeof(buffer) - 1;
    }

    USB_CDC_DebugWrite((const uint8_t *)buffer, (uint16_t)len);
    uart0DebugWrite((const uint8_t *)buffer, (uint16_t)len);
}

void DevEP1_OUT_Deal(uint8_t len)
{
    uint8_t i;

    for(i = 0; i < len; ++i)
    {
        usbCdcRxPush(pEP1_OUT_DataBuf[i]);
    }
}

void DevEP2_OUT_Deal(uint8_t len)
{
    (void)len;
}

void DevEP3_OUT_Deal(uint8_t len)
{
    (void)len;
}

void DevEP4_OUT_Deal(uint8_t len)
{
    (void)len;
}

void USB_DevTransProcess(void)
{
    uint8_t intflag = R8_USB_INT_FG;
    uint8_t token;
    uint8_t len = 0;
    uint8_t errflag = 0;
    uint8_t req_type;

    if(intflag & RB_UIF_TRANSFER)
    {
        if((R8_USB_INT_ST & MASK_UIS_TOKEN) != MASK_UIS_TOKEN)
        {
            token = R8_USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP);

            switch(token)
            {
                case UIS_TOKEN_IN:
                    if(g_setup_req_code == USB_GET_DESCRIPTOR || g_setup_req_code == USB_CDC_GET_LINE_CODING)
                    {
                        len = usbCdcCopySetupData(g_setup_descr, g_setup_req_len);
                        R8_UEP0_T_LEN = len;
                        R8_UEP0_CTRL ^= RB_UEP_T_TOG;
                    }
                    else if(g_setup_req_code == USB_SET_ADDRESS)
                    {
                        R8_USB_DEV_AD = (R8_USB_DEV_AD & RB_UDA_GP_BIT) | (g_setup_req_len & 0xFF);
                        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                    }
                    else
                    {
                        R8_UEP0_T_LEN = 0;
                        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                    }
                    break;

                case UIS_TOKEN_OUT:
                    len = R8_USB_RX_LEN;
                    if(g_setup_req_code == USB_CDC_SET_LINE_CODING && len == sizeof(g_line_coding))
                    {
                        memcpy(g_line_coding, pEP0_DataBuf, sizeof(g_line_coding));
                    }
                    R8_UEP0_T_LEN = 0;
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG |
                                   UEP_R_RES_NAK | UEP_T_RES_ACK;
                    break;

                case UIS_TOKEN_OUT | 1:
                    if(R8_USB_INT_ST & RB_UIS_TOG_OK)
                    {
                        DevEP1_OUT_Deal(R8_USB_RX_LEN);
                    }
                    break;

                case UIS_TOKEN_IN | 1:
                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    break;

                case UIS_TOKEN_IN | 4:
                    R8_UEP4_CTRL ^= RB_UEP_T_TOG;
                    R8_UEP4_CTRL = (R8_UEP4_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    break;

                default:
                    break;
            }

            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }

        if(R8_USB_INT_ST & RB_UIS_SETUP_ACT)
        {
            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;

            g_setup_req_code = pSetupReqPak->bRequest;
            g_setup_req_len = pSetupReqPak->wLength;
            g_setup_descr = NULL;
            req_type = pSetupReqPak->bRequestType;
            len = 0;
            errflag = 0;

            if((req_type & USB_REQ_TYP_MASK) == USB_REQ_TYP_STANDARD)
            {
                switch(g_setup_req_code)
                {
                    case USB_GET_DESCRIPTOR:
                        switch((uint8_t)(pSetupReqPak->wValue >> 8))
                        {
                            case USB_DESCR_TYP_DEVICE:
                                g_setup_descr = g_usb_dev_descr;
                                len = sizeof(g_usb_dev_descr);
                                break;

                            case USB_DESCR_TYP_CONFIG:
                                g_setup_descr = g_usb_cfg_descr;
                                len = sizeof(g_usb_cfg_descr);
                                break;

                            case USB_DESCR_TYP_STRING:
                                switch((uint8_t)pSetupReqPak->wValue)
                                {
                                    case 0:
                                        g_setup_descr = g_usb_lang_descr;
                                        len = sizeof(g_usb_lang_descr);
                                        break;

                                    case 1:
                                        g_setup_descr = g_usb_manu_descr;
                                        len = sizeof(g_usb_manu_descr);
                                        break;

                                    case 2:
                                        g_setup_descr = g_usb_prod_descr;
                                        len = sizeof(g_usb_prod_descr);
                                        break;

                                    case 3:
                                        g_setup_descr = g_usb_serial_descr;
                                        len = sizeof(g_usb_serial_descr);
                                        break;

                                    default:
                                        errflag = 0xFF;
                                        break;
                                }
                                break;

                            default:
                                errflag = 0xFF;
                                break;
                        }

                        if(!errflag)
                        {
                            len = usbCdcCopySetupData(g_setup_descr, len);
                        }
                        break;

                    case USB_SET_ADDRESS:
                        g_setup_req_len = pSetupReqPak->wValue & 0xFF;
                        break;

                    case USB_GET_CONFIGURATION:
                        pEP0_DataBuf[0] = g_dev_config;
                        g_setup_descr = pEP0_DataBuf;
                        len = usbCdcCopySetupData(g_setup_descr, 1);
                        break;

                    case USB_SET_CONFIGURATION:
                        g_dev_config = (uint8_t)pSetupReqPak->wValue;
                        g_ready_banner_sent = FALSE;
                        usbCdcQueueSerialState();
                        break;

                    case USB_CLEAR_FEATURE:
                        if((req_type & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP)
                        {
                            switch((uint8_t)pSetupReqPak->wIndex)
                            {
                                case 0x81:
                                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
                                    break;

                                case 0x01:
                                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK | RB_UEP_AUTO_TOG;
                                    break;

                                case 0x84:
                                    R8_UEP4_CTRL = (R8_UEP4_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
                                    break;

                                default:
                                    errflag = 0xFF;
                                    break;
                            }
                        }
                        else
                        {
                            errflag = 0xFF;
                        }
                        break;

                    case USB_GET_INTERFACE:
                        pEP0_DataBuf[0] = 0x00;
                        g_setup_descr = pEP0_DataBuf;
                        len = usbCdcCopySetupData(g_setup_descr, 1);
                        break;

                    case USB_SET_INTERFACE:
                        break;

                    case USB_GET_STATUS:
                        pEP0_DataBuf[0] = 0x00;
                        pEP0_DataBuf[1] = 0x00;
                        g_setup_descr = pEP0_DataBuf;
                        len = usbCdcCopySetupData(g_setup_descr, 2);
                        break;

                    default:
                        errflag = 0xFF;
                        break;
                }
            }
            else if((req_type & USB_REQ_TYP_MASK) == USB_REQ_TYP_CLASS)
            {
                switch(g_setup_req_code)
                {
                    case USB_CDC_SET_LINE_CODING:
                        break;

                    case USB_CDC_GET_LINE_CODING:
                        g_setup_descr = g_line_coding;
                        len = usbCdcCopySetupData(g_setup_descr, sizeof(g_line_coding));
                        break;

                    case USB_CDC_SET_CONTROL_LINE_STATE:
                        g_control_line_state = pSetupReqPak->wValue;
                        g_ready_banner_sent = FALSE;
                        usbCdcQueueSerialState();
                        break;

                    case USB_CDC_SEND_BREAK:
                        break;

                    default:
                        errflag = 0xFF;
                        break;
                }
            }
            else
            {
                errflag = 0xFF;
            }

            if(errflag == 0xFF)
            {
                R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG |
                               UEP_R_RES_STALL | UEP_T_RES_STALL;
            }
            else
            {
                if(req_type & 0x80)
                {
                    R8_UEP0_T_LEN = len;
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG |
                                   UEP_R_RES_ACK | UEP_T_RES_ACK;
                }
                else if(g_setup_req_code == USB_CDC_SET_LINE_CODING && pSetupReqPak->wLength != 0)
                {
                    R8_UEP0_T_LEN = 0;
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG |
                                   UEP_R_RES_ACK | UEP_T_RES_NAK;
                }
                else
                {
                    R8_UEP0_T_LEN = 0;
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG |
                                   UEP_R_RES_NAK | UEP_T_RES_ACK;
                }
            }

            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }
    }
    else if(intflag & RB_UIF_BUS_RST)
    {
        R8_USB_DEV_AD = 0;
        g_dev_config = 0;
        g_control_line_state = 0;
        g_ready_banner_sent = FALSE;
        g_serial_state_pending = FALSE;

        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
        R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
        R8_UEP3_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
        R8_UEP4_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_USB_INT_FG = RB_UIF_BUS_RST;
    }
    else if(intflag & RB_UIF_SUSPEND)
    {
        R8_USB_INT_FG = RB_UIF_SUSPEND;
    }
    else
    {
        R8_USB_INT_FG = intflag;
    }
}

__attribute__((interrupt("WCH-Interrupt-fast")))
__attribute__((section(".highcode")))
void USB_IRQHandler(void)
{
    USB_DevTransProcess();
}

static uint16_t usbCdcRingNext(uint16_t value, uint16_t size)
{
    value++;
    if(value >= size)
    {
        value = 0;
    }
    return value;
}

static uint8_t usbCdcTxPush(uint8_t value)
{
    uint16_t next = usbCdcRingNext(g_tx_head, USB_CDC_TX_BUF_SIZE);

    if(next == g_tx_tail)
    {
        return FALSE;
    }

    g_tx_ring[g_tx_head] = value;
    g_tx_head = next;
    return TRUE;
}

static uint8_t usbCdcTxPop(uint8_t *value)
{
    if(g_tx_tail == g_tx_head)
    {
        return FALSE;
    }

    *value = g_tx_ring[g_tx_tail];
    g_tx_tail = usbCdcRingNext(g_tx_tail, USB_CDC_TX_BUF_SIZE);
    return TRUE;
}

static uint8_t usbCdcRxPush(uint8_t value)
{
    uint16_t next = usbCdcRingNext(g_rx_head, USB_CDC_RX_BUF_SIZE);

    if(next == g_rx_tail)
    {
        return FALSE;
    }

    g_rx_ring[g_rx_head] = value;
    g_rx_head = next;
    return TRUE;
}

static uint8_t usbCdcRxPop(uint8_t *value)
{
    if(g_rx_tail == g_rx_head)
    {
        return FALSE;
    }

    *value = g_rx_ring[g_rx_tail];
    g_rx_tail = usbCdcRingNext(g_rx_tail, USB_CDC_RX_BUF_SIZE);
    return TRUE;
}

static uint16_t usbCdcCopySetupData(const uint8_t *descr, uint16_t total_len)
{
    uint16_t packet_len;

    if(descr == NULL)
    {
        return 0;
    }

    if(g_setup_req_len > total_len)
    {
        g_setup_req_len = total_len;
    }

    packet_len = (g_setup_req_len > USB_CDC_EP0_SIZE) ? USB_CDC_EP0_SIZE : g_setup_req_len;
    memcpy(pEP0_DataBuf, descr, packet_len);
    g_setup_req_len -= packet_len;
    g_setup_descr = descr + packet_len;
    return (uint16_t)packet_len;
}

static void usbCdcQueueSerialState(void)
{
    g_serial_state_pending = TRUE;
}

static void usbCdcSendSerialState(void)
{
    pEP4_IN_DataBuf[0] = 0xA1;
    pEP4_IN_DataBuf[1] = USB_CDC_SERIAL_STATE;
    pEP4_IN_DataBuf[2] = 0x00;
    pEP4_IN_DataBuf[3] = 0x00;
    pEP4_IN_DataBuf[4] = 0x00;
    pEP4_IN_DataBuf[5] = 0x00;
    pEP4_IN_DataBuf[6] = 0x02;
    pEP4_IN_DataBuf[7] = 0x00;
    pEP4_IN_DataBuf[8] = 0x00;
    pEP4_IN_DataBuf[9] = 0x00;

    DevEP4_IN_Deal(USB_CDC_NOTIFY_SIZE);
    g_serial_state_pending = FALSE;
}

static void usbCdcFlushTx(void)
{
    uint8_t len = 0;

    while(len < USB_CDC_EP_DATA_SIZE && usbCdcTxPop(&pEP1_IN_DataBuf[len]))
    {
        len++;
    }

    if(len > 0)
    {
        DevEP1_IN_Deal(len);
    }
}

static void usbCdcProcessRx(void)
{
    uint8_t ch;

    while(usbCdcRxPop(&ch))
    {
        if(ch == '\r' || ch == '\n')
        {
            if(g_cmd_len > 0)
            {
                g_cmd_buf[g_cmd_len] = '\0';
                g_cmd_output_mask = DEBUG_CMD_OUTPUT_USB;
                usbCdcHandleCommand(g_cmd_buf);
                g_cmd_len = 0;
            }
        }
        else if(g_cmd_len < (USB_CDC_CMD_BUF_SIZE - 1))
        {
            g_cmd_buf[g_cmd_len++] = (char)ch;
        }
    }
}

static void uart0DebugProcessRx(void)
{
    uint8_t ch;

    while(R8_UART0_RFC)
    {
        ch = R8_UART0_RBR;

        if(ch == '\r' || ch == '\n')
        {
            if(g_uart0_cmd_len > 0)
            {
                g_uart0_cmd_buf[g_uart0_cmd_len] = '\0';
                g_cmd_output_mask = DEBUG_CMD_OUTPUT_UART0;
                usbCdcHandleCommand(g_uart0_cmd_buf);
                g_uart0_cmd_len = 0;
            }
        }
        else if(g_uart0_cmd_len < (UART0_CMD_BUF_SIZE - 1))
        {
            g_uart0_cmd_buf[g_uart0_cmd_len++] = (char)ch;
        }
    }
}

static void uart0DebugWrite(const uint8_t *data, uint16_t len)
{
    if(data == NULL)
    {
        return;
    }

    while(len)
    {
        if(R8_UART0_TFC != UART_FIFO_SIZE)
        {
            R8_UART0_THR = *data++;
            len--;
        }
    }
}

static void uart0DebugWriteString(const char *str)
{
    if(str != NULL)
    {
        uart0DebugWrite((const uint8_t *)str, (uint16_t)strlen(str));
    }
}

static void usbCdcHandleCommand(char *line)
{
    char raw_line[USB_CDC_CMD_BUF_SIZE];
    char *arg;
    uint8_t keycode;
    uint8_t modifier;
    uint8_t target;
    uint8_t status;

    usbCdcTrim(line);

    strncpy(raw_line, line, sizeof(raw_line) - 1);
    raw_line[sizeof(raw_line) - 1] = '\0';

    usbCdcToLower(line);

    if(line[0] == '\0')
    {
        return;
    }

    if(usbCdcStreq(line, "help"))
    {
        usbCdcPrintHelp();
        return;
    }

    if(usbCdcStreq(line, "status"))
    {
        usbCdcPrintStatus();
        return;
    }

    if(strncmp(line, "tap ", 4) == 0)
    {
        arg = line + 4;
        usbCdcTrim(arg);

        if(usbCdcParseTapKey(arg, &keycode))
        {
            status = BLE_HID_TriggerKeyTap(keycode);
            debugCmdLog("tap %s -> status=%02x\r\n", arg, status);
        }
        else
        {
            debugCmdLog("unknown tap key: %s\r\n", arg);
        }
        return;
    }

    if(strncmp(line, "combo ", 6) == 0)
    {
        arg = line + 6;
        usbCdcTrim(arg);

        if(usbCdcParseCombo(arg, &modifier, &keycode))
        {
            status = KVM_SendCombo(modifier, keycode);
            debugCmdLog("combo -> status=%02x\r\n", status);
        }
        else
        {
            debugCmdWriteString("usage: combo ctrl c | combo ctrl alt del | combo shift a\r\n");
        }
        return;
    }

    if(strncmp(line, "type ", 5) == 0)
    {
        arg = raw_line + 5;
        usbCdcTrim(arg);

        status = KVM_TypeText(arg, (uint8_t)strlen(arg));
        debugCmdLog("type -> status=%02x\r\n", status);
        return;
    }

    if(strncmp(line, "kvm switch ", 11) == 0)
    {
        arg = line + 11;
        usbCdcTrim(arg);

        if(usbCdcParseTarget(arg, &target))
        {
            status = KVM_SwitchTarget(target);
            debugCmdLog("kvm switch %u -> status=%02x\r\n", target, status);
        }
        else
        {
            debugCmdWriteString("usage: kvm switch 1|2|3\r\n");
        }
        return;
    }

    if(strncmp(line, "report ", 7) == 0)
    {
        uint8_t report[BLE_HID_KBD_REPORT_LEN];

        arg = line + 7;
        if(usbCdcParseRawReport(arg, report))
        {
            status = BLE_HID_SendKeyboard(report);
            debugCmdLog("report -> status=%02x\r\n", status);
        }
        else
        {
            debugCmdWriteString("usage: report 00 00 04 00 00 00 00 00\r\n");
        }
        return;
    }

    if(usbCdcStreq(line, "release"))
    {
        uint8_t report[BLE_HID_KBD_REPORT_LEN] = {0};

        status = BLE_HID_SendKeyboard(report);
        debugCmdLog("release -> status=%02x\r\n", status);
        return;
    }

    if(strncmp(line, "adv ", 4) == 0)
    {
        arg = line + 4;
        usbCdcTrim(arg);

        if(usbCdcStreq(arg, "on"))
        {
            BLE_HID_StartAdvert();
            debugCmdWriteString("advertising on\r\n");
        }
        else if(usbCdcStreq(arg, "off"))
        {
            BLE_HID_StopAdvert();
            debugCmdWriteString("advertising off\r\n");
        }
        else
        {
            debugCmdLog("unknown adv arg: %s\r\n", arg);
        }
        return;
    }

    debugCmdLog("unknown command: %s\r\n", line);
}

static void usbCdcPrintHelp(void)
{
    debugCmdWriteString("Commands:\r\n");
    debugCmdWriteString("  help\r\n");
    debugCmdWriteString("  status\r\n");
    debugCmdWriteString("  tap a-z | tap 0-9\r\n");
    debugCmdWriteString("  tap enter | space | esc | tab | backspace\r\n");
    debugCmdWriteString("  tap f1-f12 | left | right | up | down\r\n");
    debugCmdWriteString("  combo ctrl c | combo ctrl alt del | combo shift a\r\n");
    debugCmdWriteString("  type hello123\r\n");
    debugCmdWriteString("  kvm switch 1 | kvm switch 2 | kvm switch 3\r\n");
    debugCmdWriteString("  report 00 00 04 00 00 00 00 00\r\n");
    debugCmdWriteString("  release\r\n");
    debugCmdWriteString("  adv on\r\n");
    debugCmdWriteString("  adv off\r\n");
}

static void usbCdcPrintStatus(void)
{
    uint32_t baud =
        ((uint32_t)g_line_coding[0]) |
        ((uint32_t)g_line_coding[1] << 8) |
        ((uint32_t)g_line_coding[2] << 16) |
        ((uint32_t)g_line_coding[3] << 24);

    debugCmdLog("status: usb_cfg=%u dtr=%u ready=%u ble_connected=%u tap_busy=%u tap_queue=%u kvm_target=%u baud=%lu\r\n",
                g_dev_config,
                (g_control_line_state & USB_CDC_SERIAL_DTR) ? 1 : 0,
                USB_CDC_DebugIsReady() ? 1 : 0,
                BLE_HID_IsConnected() ? 1 : 0,
                BLE_HID_IsKeyTapBusy() ? 1 : 0,
                BLE_HID_GetQueuedTapCount(),
                KVM_GetCurrentTarget(),
                (unsigned long)baud);
}

static void debugCmdWriteString(const char *str)
{
    if(str == NULL)
    {
        return;
    }

    if(g_cmd_output_mask & DEBUG_CMD_OUTPUT_USB)
    {
        USB_CDC_DebugWriteString(str);
    }

    if(g_cmd_output_mask & DEBUG_CMD_OUTPUT_UART0)
    {
        uart0DebugWriteString(str);
    }
}

static void debugCmdLog(const char *fmt, ...)
{
    char buffer[160];
    int len;
    va_list args;

    if(fmt == NULL)
    {
        return;
    }

    va_start(args, fmt);
    len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if(len <= 0)
    {
        return;
    }

    if((size_t)len >= sizeof(buffer))
    {
        len = sizeof(buffer) - 1;
    }

    if(g_cmd_output_mask & DEBUG_CMD_OUTPUT_USB)
    {
        USB_CDC_DebugWrite((const uint8_t *)buffer, (uint16_t)len);
    }

    if(g_cmd_output_mask & DEBUG_CMD_OUTPUT_UART0)
    {
        uart0DebugWrite((const uint8_t *)buffer, (uint16_t)len);
    }
}

static uint8_t usbCdcParseTapKey(const char *token, uint8_t *keycode)
{
    uint8_t fnum;

    if(token == NULL || keycode == NULL || token[0] == '\0')
    {
        return FALSE;
    }

    if(token[1] == '\0' && token[0] >= 'a' && token[0] <= 'z')
    {
        *keycode = (uint8_t)(HID_KEYBOARD_A + token[0] - 'a');
        return TRUE;
    }

    if(token[1] == '\0' && token[0] >= '1' && token[0] <= '9')
    {
        *keycode = (uint8_t)(HID_KEYBOARD_1 + token[0] - '1');
        return TRUE;
    }

    if(usbCdcStreq(token, "0"))
    {
        *keycode = HID_KEYBOARD_0;
        return TRUE;
    }

    if(token[0] == 'f' && token[1] >= '1' && token[1] <= '9')
    {
        fnum = (uint8_t)(token[1] - '0');
        if(token[2] >= '0' && token[2] <= '9' && token[3] == '\0')
        {
            fnum = (uint8_t)(fnum * 10 + token[2] - '0');
        }
        else if(token[2] != '\0')
        {
            fnum = 0;
        }

        if(fnum >= 1 && fnum <= 12)
        {
            *keycode = (uint8_t)(HID_KEYBOARD_F1 + fnum - 1);
            return TRUE;
        }
    }

    if(usbCdcStreq(token, "enter") || usbCdcStreq(token, "return"))
    {
        *keycode = HID_KEYBOARD_RETURN;
        return TRUE;
    }

    if(usbCdcStreq(token, "space"))
    {
        *keycode = HID_KEYBOARD_SPACEBAR;
        return TRUE;
    }

    if(usbCdcStreq(token, "esc") || usbCdcStreq(token, "escape"))
    {
        *keycode = HID_KEYBOARD_ESCAPE;
        return TRUE;
    }

    if(usbCdcStreq(token, "tab"))
    {
        *keycode = HID_KEYBOARD_TAB;
        return TRUE;
    }

    if(usbCdcStreq(token, "backspace") || usbCdcStreq(token, "bksp"))
    {
        *keycode = HID_KEYBOARD_DELETE;
        return TRUE;
    }

    if(usbCdcStreq(token, "delete") || usbCdcStreq(token, "del"))
    {
        *keycode = HID_KEYBOARD_DELETE_FWD;
        return TRUE;
    }

    if(usbCdcStreq(token, "minus") || usbCdcStreq(token, "-"))
    {
        *keycode = HID_KEYBOARD_MINUS;
        return TRUE;
    }

    if(usbCdcStreq(token, "equal") || usbCdcStreq(token, "="))
    {
        *keycode = HID_KEYBOARD_EQUAL;
        return TRUE;
    }

    if(usbCdcStreq(token, "lbracket") || usbCdcStreq(token, "leftbracket") || usbCdcStreq(token, "["))
    {
        *keycode = HID_KEYBOARD_LEFT_BRKT;
        return TRUE;
    }

    if(usbCdcStreq(token, "rbracket") || usbCdcStreq(token, "rightbracket") || usbCdcStreq(token, "]"))
    {
        *keycode = HID_KEYBOARD_RIGHT_BRKT;
        return TRUE;
    }

    if(usbCdcStreq(token, "backslash"))
    {
        *keycode = HID_KEYBOARD_BACK_SLASH;
        return TRUE;
    }

    if(usbCdcStreq(token, "semicolon") || usbCdcStreq(token, ";"))
    {
        *keycode = HID_KEYBOARD_SEMI_COLON;
        return TRUE;
    }

    if(usbCdcStreq(token, "quote"))
    {
        *keycode = HID_KEYBOARD_SGL_QUOTE;
        return TRUE;
    }

    if(usbCdcStreq(token, "grave") || usbCdcStreq(token, "`"))
    {
        *keycode = HID_KEYBOARD_GRV_ACCENT;
        return TRUE;
    }

    if(usbCdcStreq(token, "comma") || usbCdcStreq(token, ","))
    {
        *keycode = HID_KEYBOARD_COMMA;
        return TRUE;
    }

    if(usbCdcStreq(token, "dot") || usbCdcStreq(token, "period") || usbCdcStreq(token, "."))
    {
        *keycode = HID_KEYBOARD_DOT;
        return TRUE;
    }

    if(usbCdcStreq(token, "slash") || usbCdcStreq(token, "/"))
    {
        *keycode = HID_KEYBOARD_FWD_SLASH;
        return TRUE;
    }

    if(usbCdcStreq(token, "capslock") || usbCdcStreq(token, "caps"))
    {
        *keycode = HID_KEYBOARD_CAPS_LOCK;
        return TRUE;
    }

    if(usbCdcStreq(token, "insert") || usbCdcStreq(token, "ins"))
    {
        *keycode = HID_KEYBOARD_INSERT;
        return TRUE;
    }

    if(usbCdcStreq(token, "home"))
    {
        *keycode = HID_KEYBOARD_HOME;
        return TRUE;
    }

    if(usbCdcStreq(token, "pageup") || usbCdcStreq(token, "pgup"))
    {
        *keycode = HID_KEYBOARD_PAGE_UP;
        return TRUE;
    }

    if(usbCdcStreq(token, "end"))
    {
        *keycode = HID_KEYBOARD_END;
        return TRUE;
    }

    if(usbCdcStreq(token, "pagedown") || usbCdcStreq(token, "pgdn"))
    {
        *keycode = HID_KEYBOARD_PAGE_DOWN;
        return TRUE;
    }

    if(usbCdcStreq(token, "right"))
    {
        *keycode = HID_KEYBOARD_RIGHT_ARROW;
        return TRUE;
    }

    if(usbCdcStreq(token, "left"))
    {
        *keycode = HID_KEYBOARD_LEFT_ARROW;
        return TRUE;
    }

    if(usbCdcStreq(token, "down"))
    {
        *keycode = HID_KEYBOARD_DOWN_ARROW;
        return TRUE;
    }

    if(usbCdcStreq(token, "up"))
    {
        *keycode = HID_KEYBOARD_UP_ARROW;
        return TRUE;
    }

    return FALSE;
}

static uint8_t usbCdcParseCombo(char *args, uint8_t *modifier, uint8_t *keycode)
{
    char *token;
    char *next;
    uint8_t parsed_modifier;

    if(args == NULL || modifier == NULL || keycode == NULL)
    {
        return FALSE;
    }

    *modifier = 0;
    *keycode = HID_KEYBOARD_RESERVED;

    usbCdcTrim(args);
    token = strtok(args, " \t");
    if(token == NULL)
    {
        return FALSE;
    }

    while(token != NULL)
    {
        next = strtok(NULL, " \t");
        if(next == NULL)
        {
            return usbCdcParseTapKey(token, keycode);
        }

        if(!usbCdcParseModifier(token, &parsed_modifier))
        {
            return FALSE;
        }

        *modifier |= parsed_modifier;
        token = next;
    }

    return FALSE;
}

static uint8_t usbCdcParseModifier(const char *token, uint8_t *modifier)
{
    if(token == NULL || modifier == NULL)
    {
        return FALSE;
    }

    if(usbCdcStreq(token, "ctrl") || usbCdcStreq(token, "control") || usbCdcStreq(token, "lctrl"))
    {
        *modifier = KVM_MOD_LEFT_CTRL;
        return TRUE;
    }

    if(usbCdcStreq(token, "shift") || usbCdcStreq(token, "lshift"))
    {
        *modifier = KVM_MOD_LEFT_SHIFT;
        return TRUE;
    }

    if(usbCdcStreq(token, "alt") || usbCdcStreq(token, "lalt"))
    {
        *modifier = KVM_MOD_LEFT_ALT;
        return TRUE;
    }

    if(usbCdcStreq(token, "gui") || usbCdcStreq(token, "win") || usbCdcStreq(token, "cmd") || usbCdcStreq(token, "meta"))
    {
        *modifier = KVM_MOD_LEFT_GUI;
        return TRUE;
    }

    if(usbCdcStreq(token, "rctrl"))
    {
        *modifier = KVM_MOD_RIGHT_CTRL;
        return TRUE;
    }

    if(usbCdcStreq(token, "rshift"))
    {
        *modifier = KVM_MOD_RIGHT_SHIFT;
        return TRUE;
    }

    if(usbCdcStreq(token, "ralt"))
    {
        *modifier = KVM_MOD_RIGHT_ALT;
        return TRUE;
    }

    if(usbCdcStreq(token, "rgui") || usbCdcStreq(token, "rwin"))
    {
        *modifier = KVM_MOD_RIGHT_GUI;
        return TRUE;
    }

    return FALSE;
}

static uint8_t usbCdcParseRawReport(char *args, uint8_t *report8)
{
    char *token;
    uint8_t count = 0;

    if(args == NULL || report8 == NULL)
    {
        return FALSE;
    }

    usbCdcTrim(args);
    token = strtok(args, " \t");
    while(token != NULL)
    {
        if(count >= BLE_HID_KBD_REPORT_LEN)
        {
            return FALSE;
        }

        if(!usbCdcParseHexByte(token, &report8[count]))
        {
            return FALSE;
        }

        count++;
        token = strtok(NULL, " \t");
    }

    return (count == BLE_HID_KBD_REPORT_LEN) ? TRUE : FALSE;
}

static uint8_t usbCdcParseHexByte(const char *token, uint8_t *value)
{
    uint16_t acc = 0;
    uint8_t digits = 0;
    uint8_t nibble;

    if(token == NULL || value == NULL)
    {
        return FALSE;
    }

    if(token[0] == '0' && token[1] == 'x')
    {
        token += 2;
    }

    while(*token != '\0')
    {
        if(!usbCdcHexValue(*token, &nibble))
        {
            return FALSE;
        }

        acc = (uint16_t)((acc << 4) | nibble);
        digits++;
        if(digits > 2 || acc > 0xFF)
        {
            return FALSE;
        }

        token++;
    }

    if(digits == 0)
    {
        return FALSE;
    }

    *value = (uint8_t)acc;
    return TRUE;
}

static uint8_t usbCdcHexValue(char ch, uint8_t *value)
{
    if(value == NULL)
    {
        return FALSE;
    }

    if(ch >= '0' && ch <= '9')
    {
        *value = (uint8_t)(ch - '0');
        return TRUE;
    }

    if(ch >= 'a' && ch <= 'f')
    {
        *value = (uint8_t)(ch - 'a' + 10);
        return TRUE;
    }

    return FALSE;
}

static uint8_t usbCdcParseTarget(const char *token, uint8_t *target)
{
    if(token == NULL || target == NULL)
    {
        return FALSE;
    }

    if(token[0] >= '0' && token[0] <= '9' && token[1] == '\0')
    {
        *target = (uint8_t)(token[0] - '0');
        return TRUE;
    }

    return FALSE;
}

static void usbCdcToLower(char *str)
{
    while(*str != '\0')
    {
        if(*str >= 'A' && *str <= 'Z')
        {
            *str = (char)(*str - 'A' + 'a');
        }
        str++;
    }
}

static void usbCdcTrim(char *str)
{
    char *start = str;
    char *end;
    size_t len;

    while(*start == ' ' || *start == '\t')
    {
        start++;
    }

    if(start != str)
    {
        memmove(str, start, strlen(start) + 1);
    }

    len = strlen(str);
    if(len == 0)
    {
        return;
    }

    end = str + len - 1;
    while(end >= str && (*end == ' ' || *end == '\t'))
    {
        *end = '\0';
        if(end == str)
        {
            break;
        }
        end--;
    }
}

static uint8_t usbCdcStreq(const char *a, const char *b)
{
    return (strcmp(a, b) == 0) ? TRUE : FALSE;
}
