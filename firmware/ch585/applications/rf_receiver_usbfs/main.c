/*******************************************************************************
 * rf_receiver_main.c — CH585 2.4G 键盘接收器主程序
 *
 * 独立项目，运行在接收器 CH585 板（与键盘主机相同硬件）。
 * 功能：
 *   1. 通过 CH585 RF IP 持续监听键盘主机发出的 2.4G RF 帧
 *   2. 解码 RF 帧为 HID 报告
 *   3. 通过 CH585 USB FS 设备（USB_LD Device）向 PC 发送 Boot Keyboard 报告
 *
 * USB FS HID 配置：
 *   EP1 IN  — 8B Boot Keyboard（Modifier + Reserved + 6 Keycodes）
 *   EP2 IN  — 2B Consumer（可选）
 *
 * 注意：本文件使用 CH585 内置 USB FS 控制器（非 USBHS）。
 *        仅用于测试功能验证，生产产品建议使用 USBHS。
 *
 * 编译方法：
 *   在 MounRiver Studio 中新建 CH585 项目，只包含：
 *     receiver/rf_receiver_main.c
 *     receiver/rf_receiver.c/.h
 *   并包含 CH58xBLE_LIB，不包含键盘主机的其他源文件。
 ******************************************************************************/

#include "CH58x_common.h"
#include "CH58x_usbdev.h"
#include "CONFIG.h"
#include "HAL.h"
#include "rf_receiver.h"
#include <string.h>

/* ─── TMOS 内存堆 ─── */
__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

/* ─── USB FS 端点缓冲区 ─── */
#define RX_EP0_SIZE   64
#define RX_EP1_SIZE   8    /* Boot Keyboard: 8B */
#define RX_EP2_SIZE   4    /* Consumer: 4B */
#define RX_USB_HW_EP_SIZE 64

__attribute__((aligned(4))) static uint8_t EP0_Buf[RX_EP0_SIZE * 3];
__attribute__((aligned(4))) static uint8_t EP1_Buf[RX_USB_HW_EP_SIZE * 2]; /* OUT + IN */
__attribute__((aligned(4))) static uint8_t EP2_Buf[RX_USB_HW_EP_SIZE * 2];
__attribute__((aligned(4))) static uint8_t EP3_Buf[RX_USB_HW_EP_SIZE * 2];

/* ─── USB 描述符（最小 Boot Keyboard）─── */
static const uint8_t RxDevDescr[] = {
    0x12, 0x01, 0x10, 0x01,         /* bLength, bDescriptorType, bcdUSB=1.1 */
    0x00, 0x00, 0x00, RX_EP0_SIZE,  /* class, subclass, protocol, MaxPacketSize0 */
    0x86, 0x1A, 0x07, 0xFE,         /* VID=0x1A86, PID=0xFE07 */
    0x01, 0x00,                     /* bcdDevice */
    0x01, 0x02, 0x03, 0x01          /* Manufacturer, Product, Serial, NumConfigurations */
};

static const uint8_t RxKbdRepDescr[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x75, 0x08, 0x95, 0x01, 0x81, 0x03,
    0x75, 0x08, 0x95, 0x06, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
    0xC0
};
#define RX_KBD_REP_LEN  sizeof(RxKbdRepDescr)

static const uint8_t RxCfgDescr[] = {
    /* Configuration */
    0x09, 0x02, 0x22, 0x00, 0x01, 0x01, 0x00, 0xA0, 0x32,
    /* Interface */
    0x09, 0x04, 0x00, 0x00, 0x01, 0x03, 0x01, 0x01, 0x00,
    /* HID */
    0x09, 0x21, 0x10, 0x01, 0x00, 0x01, 0x22,
    (uint8_t)RX_KBD_REP_LEN, 0x00,
    /* EP1 IN */
    0x07, 0x05, 0x81, 0x03, RX_EP1_SIZE, 0x00, 0x0A,  /* bInterval=10ms */
};
#define RX_CFG_LEN  sizeof(RxCfgDescr)

static const uint8_t RxLangDescr[]  = { 0x04, 0x03, 0x09, 0x04 };
static const uint8_t RxManuInfo[]   = { 0x0C, 0x03, 'C',0,'H',0,'5',0,'8',0,'5',0 };
static const uint8_t RxProdInfo[]   = { 0x14, 0x03, 'R',0,'F',0,'_',0,'R',0,'X',0,'_',0,'H',0,'I',0,'D',0 };
static const uint8_t RxSerialInfo[] = { 0x0A, 0x03, 'R',0,'X',0,'0',0,'1',0 };

/* ─── 枚举状态 ─── */
static uint8_t  g_dev_config    = 0;
static uint8_t  g_enum_done     = 0;
static uint8_t  g_setup_code    = 0;
static uint16_t g_setup_len     = 0;
static const uint8_t *g_descr   = NULL;
static uint8_t  g_hid_idle      = 0;
static uint8_t  g_hid_protocol  = 1;

/* ─── Boot Keyboard 报告缓冲区 ─── */
static uint8_t g_boot_report[8];   /* [modifier][reserved][k0..k5] */

/*******************************************************************************
 * nkro_to_boot — 将 16B NKRO 报告转换为 8B Boot 报告
 *   NKRO: [modifier][reserved][bitmap14B]
 *   Boot: [modifier][reserved][k0][k1][k2][k3][k4][k5]
 ******************************************************************************/
static void nkro_to_boot(const uint8_t *nkro16, uint8_t *boot8)
{
    boot8[0] = nkro16[0]; /* modifier */
    boot8[1] = 0x00;      /* reserved */
    uint8_t idx = 2;
    for (uint8_t byte = 0; byte < 14 && idx < 8; byte++) {
        uint8_t bm = nkro16[2 + byte];
        for (uint8_t bit = 0; bit < 8 && idx < 8; bit++) {
            if (bm & (1u << bit)) {
                boot8[idx++] = (uint8_t)(byte * 8 + bit + 0x04); /* usage = bit + 4 */
            }
        }
    }
    /* 填充多余 keycodes 为 0 */
    while (idx < 8) boot8[idx++] = 0;
}

/*******************************************************************************
 * usb_send_ep1 — 通过 EP1 IN 发送 Boot Keyboard 报告
 ******************************************************************************/
static void usb_send_ep1(const uint8_t *report8)
{
    if (!g_enum_done) return;
    memcpy(pEP1_IN_DataBuf, report8, RX_EP1_SIZE);
    R8_UEP1_T_LEN = RX_EP1_SIZE;
    R8_UEP1_CTRL  = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
}

/*******************************************************************************
 * USB_DevTransProcess — USB FS 中断处理（精简版，仅支持 HID 枚举）
 ******************************************************************************/
void USB_DevTransProcess(void)
{
    uint8_t len = 0;
    uint8_t errflag = 0;
    uint8_t req_type = 0;
    uint8_t intflag = R8_USB_INT_FG;

    if (intflag & RB_UIF_TRANSFER) {
        if ((R8_USB_INT_ST & MASK_UIS_TOKEN) != MASK_UIS_TOKEN) {
            switch (R8_USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP)) {
                /* ── EP0 IN ── */
                case UIS_TOKEN_IN:
                    switch (g_setup_code) {
                        case USB_GET_DESCRIPTOR:
                        case USB_GET_CONFIGURATION:
                        case USB_GET_STATUS:
                        case USB_GET_INTERFACE:
                        case DEF_USB_GET_IDLE:
                        case DEF_USB_GET_PROTOCOL:
                            len = (g_setup_len >= 64) ? 64 : g_setup_len;
                            if (len && g_descr) {
                                memcpy(pEP0_DataBuf, g_descr, len);
                                g_setup_len -= len;
                                g_descr += len;
                            }
                            R8_UEP0_T_LEN = len;
                            R8_UEP0_CTRL ^= RB_UEP_T_TOG;
                            break;
                        case USB_SET_ADDRESS:
                            R8_USB_DEV_AD = (R8_USB_DEV_AD & RB_UDA_GP_BIT) | g_setup_len;
                            R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                            break;
                        default:
                            R8_UEP0_T_LEN = 0;
                            R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                            break;
                    }
                    break;

                /* ── EP0 OUT ── */
                case UIS_TOKEN_OUT:
                    R8_UEP0_T_LEN = 0;
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
                    break;

                /* ── EP1 IN 完成 ── */
                case UIS_TOKEN_IN | 1:
                    R8_UEP1_T_LEN = 0;
                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    break;

                default:
                    break;
            }
        }

        if (R8_USB_INT_ST & RB_UIS_SETUP_ACT) {
            PUSB_SETUP_REQ pSetup = (PUSB_SETUP_REQ)pEP0_DataBuf;

            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;

            g_setup_code = pSetup->bRequest;
            g_setup_len = pSetup->wLength;
            g_descr = NULL;
            req_type = pSetup->bRequestType;
            errflag = 0;

            if ((req_type & USB_REQ_TYP_MASK) == USB_REQ_TYP_STANDARD) {
                switch (g_setup_code) {
                    case USB_GET_DESCRIPTOR:
                        switch ((uint8_t)(pSetup->wValue >> 8)) {
                            case USB_DESCR_TYP_DEVICE:
                                g_descr = RxDevDescr;
                                g_setup_len = sizeof(RxDevDescr);
                                break;
                            case USB_DESCR_TYP_CONFIG:
                                g_descr = RxCfgDescr;
                                g_setup_len = RX_CFG_LEN;
                                break;
                            case USB_DESCR_TYP_STRING:
                                switch ((uint8_t)(pSetup->wValue & 0xFF)) {
                                    case 0: g_descr = RxLangDescr;  g_setup_len = sizeof(RxLangDescr);  break;
                                    case 1: g_descr = RxManuInfo;   g_setup_len = sizeof(RxManuInfo);   break;
                                    case 2: g_descr = RxProdInfo;   g_setup_len = sizeof(RxProdInfo);   break;
                                    case 3: g_descr = RxSerialInfo; g_setup_len = sizeof(RxSerialInfo); break;
                                    default: errflag = 0xFF; break;
                                }
                                break;
                            case USB_DESCR_TYP_REPORT:
                                if (((uint8_t)pSetup->wIndex) == 0) {
                                    g_descr = RxKbdRepDescr;
                                    g_setup_len = RX_KBD_REP_LEN;
                                } else {
                                    errflag = 0xFF;
                                }
                                break;
                            case USB_DESCR_TYP_HID:
                                if (((uint8_t)pSetup->wIndex) == 0) {
                                    g_descr = &RxCfgDescr[18];
                                    g_setup_len = 9;
                                } else {
                                    errflag = 0xFF;
                                }
                                break;
                            default:
                                errflag = 0xFF;
                                break;
                        }
                        break;

                    case USB_SET_ADDRESS:
                        g_setup_len = (uint16_t)(pSetup->wValue & 0xFF);
                        break;

                    case USB_GET_CONFIGURATION:
                        pEP0_DataBuf[0] = g_dev_config;
                        g_descr = pEP0_DataBuf;
                        g_setup_len = 1;
                        break;

                    case USB_SET_CONFIGURATION:
                        g_dev_config = (uint8_t)(pSetup->wValue & 0xFF);
                        g_enum_done = (g_dev_config != 0);
                        g_setup_len = 0;
                        break;

                    case USB_GET_STATUS:
                        pEP0_DataBuf[0] = 0;
                        pEP0_DataBuf[1] = 0;
                        g_descr = pEP0_DataBuf;
                        g_setup_len = 2;
                        break;

                    case USB_GET_INTERFACE:
                        pEP0_DataBuf[0] = 0;
                        g_descr = pEP0_DataBuf;
                        g_setup_len = 1;
                        break;

                    case USB_SET_INTERFACE:
                        g_setup_len = 0;
                        break;

                    case USB_CLEAR_FEATURE:
                        if ((req_type & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) {
                            if (((uint8_t)pSetup->wIndex) == 0x81) {
                                R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) |
                                               UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
                            } else {
                                errflag = 0xFF;
                            }
                        } else {
                            errflag = 0xFF;
                        }
                        g_setup_len = 0;
                        break;

                    default:
                        errflag = 0xFF;
                        break;
                }
            } else if ((req_type & USB_REQ_TYP_MASK) == USB_REQ_TYP_CLASS) {
                switch (g_setup_code) {
                    case DEF_USB_SET_IDLE:
                        g_hid_idle = (uint8_t)(pSetup->wValue >> 8);
                        g_setup_len = 0;
                        break;
                    case DEF_USB_GET_IDLE:
                        pEP0_DataBuf[0] = g_hid_idle;
                        g_descr = pEP0_DataBuf;
                        g_setup_len = 1;
                        break;
                    case DEF_USB_SET_PROTOCOL:
                        g_hid_protocol = (uint8_t)(pSetup->wValue & 0xFF);
                        g_setup_len = 0;
                        break;
                    case DEF_USB_GET_PROTOCOL:
                        pEP0_DataBuf[0] = g_hid_protocol;
                        g_descr = pEP0_DataBuf;
                        g_setup_len = 1;
                        break;
                    default:
                        g_setup_len = 0;
                        break;
                }
            } else {
                errflag = 0xFF;
            }

            if (errflag == 0xFF) {
                R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL;
            } else {
                if (req_type & 0x80) {
                    if (pSetup->wLength < g_setup_len) {
                        g_setup_len = pSetup->wLength;
                    }
                    len = (g_setup_len >= 64) ? 64 : g_setup_len;
                    if (len && g_descr) {
                        memcpy(pEP0_DataBuf, g_descr, len);
                        g_setup_len -= len;
                        g_descr += len;
                    }
                    R8_UEP0_T_LEN = len;
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
                } else {
                    R8_UEP0_T_LEN = 0;
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_ACK;
                }
            }
        }

        R8_USB_INT_FG = RB_UIF_TRANSFER;
    }

    if (intflag & RB_UIF_BUS_RST) {
        g_dev_config = 0;
        g_enum_done  = 0;
        R8_USB_DEV_AD = 0;
        R8_UEP0_CTRL  = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP1_CTRL  = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
        R8_USB_INT_FG = RB_UIF_BUS_RST;
    }

    if (intflag & RB_UIF_SUSPEND) {
        R8_USB_INT_FG = RB_UIF_SUSPEND;
    }
}

/*******************************************************************************
 * USB_IRQHandler
 ******************************************************************************/
__attribute__((interrupt("WCH-Interrupt-fast")))
__attribute__((section(".highcode")))
void USB_IRQHandler(void)
{
    USB_DevTransProcess();
}

/*******************************************************************************
 * usb_fs_init — 初始化 CH585 USB FS 设备控制器
 ******************************************************************************/
static void usb_fs_init(void)
{
    pEP0_RAM_Addr = EP0_Buf;
    pEP1_RAM_Addr = EP1_Buf;
    pEP2_RAM_Addr = EP2_Buf;
    pEP3_RAM_Addr = EP3_Buf;

    USB_DeviceInit();
    PFIC_EnableIRQ(USB_IRQn);
}

/*******************************************************************************
 * Main_Circulation — TMOS 主循环
 ******************************************************************************/
__HIGH_CODE
__attribute__((noinline))
void Main_Circulation(void)
{
    uint8_t nkro16[16];
    uint8_t boot8[8];

    while (1) {
        TMOS_SystemProcess();

        /* 有新 RF 报告 */
        if (RF_Receiver_GetKbdReport(nkro16)) {
            nkro_to_boot(nkro16, boot8);
            usb_send_ep1(boot8);
        }
    }
}

/*******************************************************************************
 * main — 接收器入口
 ******************************************************************************/
int main(void)
{
    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(SYSCLK_FREQ);

    /* BLE 栈（RF IP 依赖 TMOS）*/
    CH58x_BLEInit();
    HAL_Init();

    /* USB FS 设备 */
    usb_fs_init();

    /* RF 接收器 */
    RF_Receiver_Init();

    /* 主循环 */
    Main_Circulation();

    return 0;
}
