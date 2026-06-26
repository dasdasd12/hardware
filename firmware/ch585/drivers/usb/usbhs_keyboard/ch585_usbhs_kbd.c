/*******************************************************************************
 * ch585_usbhs_kbd.c — CH585 键盘 USBHS 设备驱动
 *
 * 基于 WCH 官方 CompositeKM 示例改编，适配三接口 HID 键盘：
 *   EP1 IN  — NKRO Keyboard（64B，bInterval=1）
 *   EP2 IN  — Consumer Control（8B，bInterval=4）
 *   EP3 IN  — Config/ADCMonitor 响应（64B，bInterval=1）
 *   EP3 OUT — Config 命令接收（64B）
 ******************************************************************************/

#include "ch585_usbhs_kbd.h"
#include "usb_desc_hid.h"

/* ─── 端点缓冲区 ─── */
__attribute__((aligned(4))) uint8_t USBHS_EP0_Buf[DEF_USBD_UEP0_SIZE];
__attribute__((aligned(4))) uint8_t USBHS_EP1_TX_Buf[DEF_USB_EP1_HS_SIZE];
__attribute__((aligned(4))) uint8_t USBHS_EP2_TX_Buf[DEF_USB_EP2_HS_SIZE];
__attribute__((aligned(4))) uint8_t USBHS_EP3_TX_Buf[DEF_USB_EP3_HS_SIZE];
__attribute__((aligned(4))) uint8_t USBHS_EP3_RX_Buf[DEF_USB_EP3_HS_SIZE];

/* ─── 设备状态 ─── */
volatile uint8_t  USBHS_DevConfig;
volatile uint8_t  USBHS_DevAddr;
volatile uint8_t  USBHS_DevEnumStatus;
volatile uint8_t  USBHS_DevSleepStatus;
volatile uint8_t  USBHS_DevSpeed;
volatile uint16_t USBHS_DevMaxPackLen;

/* ─── Setup 请求暂存 ─── */
volatile uint8_t  USBHS_SetupReqCode;
volatile uint8_t  USBHS_SetupReqType;
volatile uint16_t USBHS_SetupReqValue;
volatile uint16_t USBHS_SetupReqIndex;
volatile uint16_t USBHS_SetupReqLen;

/* ─── HID Class 暂存 ─── */
volatile uint8_t  USBHS_HidIdle[3];        /* interface 0/1/2 */
volatile uint8_t  USBHS_HidProtocol[3];

/* ─── 端点忙标志 ─── */
volatile uint8_t  USBHS_Endp_Busy[DEF_UEP_NUM];

/* ─── 描述符指针（用于多包传输）─── */
static const uint8_t *pUSBHS_Descr;

/*******************************************************************************
 * USBHS_KBD_Endp_Init — 初始化端点
 ******************************************************************************/
void USBHS_KBD_Endp_Init(void)
{
    uint8_t i;

    /* 使能 TX: EP0/EP1/EP2/EP3，RX: EP0/EP3 */
    R16_U2EP_TX_EN = RB_EP0_EN | RB_EP1_EN | RB_EP2_EN | RB_EP3_EN;
    R16_U2EP_RX_EN = RB_EP0_EN | RB_EP3_EN;

    R32_U2EP0_MAX_LEN = DEF_USBD_UEP0_SIZE;
    R32_U2EP1_MAX_LEN = DEF_USB_EP1_HS_SIZE;
    R32_U2EP2_MAX_LEN = DEF_USB_EP2_HS_SIZE;
    R32_U2EP3_MAX_LEN = DEF_USB_EP3_HS_SIZE;

    R32_U2EP0_DMA    = (uint32_t)(uint8_t *)USBHS_EP0_Buf;
    R32_U2EP1_TX_DMA = (uint32_t)(uint8_t *)USBHS_EP1_TX_Buf;
    R32_U2EP2_TX_DMA = (uint32_t)(uint8_t *)USBHS_EP2_TX_Buf;
    R32_U2EP3_TX_DMA = (uint32_t)(uint8_t *)USBHS_EP3_TX_Buf;
    R32_U2EP3_RX_DMA = (uint32_t)(uint8_t *)USBHS_EP3_RX_Buf;

    R8_U2EP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
    R8_U2EP0_RX_CTRL = USBHS_UEP_R_RES_ACK;
    R8_U2EP1_TX_CTRL = USBHS_UEP_T_RES_NAK;
    R8_U2EP2_TX_CTRL = USBHS_UEP_T_RES_NAK;
    R8_U2EP3_TX_CTRL = USBHS_UEP_T_RES_NAK;
    R8_U2EP3_RX_CTRL = USBHS_UEP_R_RES_ACK;

    for (i = 0; i < DEF_UEP_NUM; i++)
        USBHS_Endp_Busy[i] = 0;
}

/*******************************************************************************
 * USBHS_KBD_Device_Init — 初始化 / 关闭 USBHS 控制器
 ******************************************************************************/
void USBHS_KBD_Device_Init(FunctionalState sta)
{
    if (sta) {
        R16_CLK_SYS_CFG |= (RB_CLK_SYS_MOD & 0x40) | RB_XROM_SCLK_SEL | RB_OSC32M_SEL;
        R8_USBHS_PLL_CTRL = USBHS_PLL_EN;
        R16_PIN_CONFIG   |= RB_PIN_USB2_EN;

        R8_USB2_CTRL = USBHS_UD_RST_LINK | USBHS_UD_PHY_SUSPENDM;
        R8_USB2_INT_EN = USBHS_UDIE_BUS_RST | USBHS_UDIE_SUSPEND |
                         USBHS_UDIE_BUS_SLEEP | USBHS_UDIE_LPM_ACT |
                         USBHS_UDIE_TRANSFER  | USBHS_UDIE_LINK_RDY;
        USBHS_KBD_Endp_Init();
        R8_USB2_BASE_MODE = USBHS_UD_SPEED_HIGH;
        R8_USB2_CTRL = USBHS_UD_DEV_EN | USBHS_UD_DMA_EN |
                       USBHS_UD_LPM_EN | USBHS_UD_PHY_SUSPENDM;
        PFIC_EnableIRQ(USB2_DEVICE_IRQn);
    } else {
        R16_CLK_SYS_CFG &= ~((RB_CLK_SYS_MOD & 0x40) | RB_XROM_SCLK_SEL | RB_OSC32M_SEL);
        R8_USBHS_PLL_CTRL &= ~USBHS_PLL_EN;
        R32_PIN_CONFIG    &= ~RB_PIN_USB2_EN;
        R8_USB2_CTRL |= USBHS_UD_RST_SIE;
        R8_USB2_CTRL &= ~USBHS_UD_RST_SIE;
        PFIC_DisableIRQ(USB2_DEVICE_IRQn);
    }
}

/*******************************************************************************
 * USBHS_Endp_DataUp — 向指定端点上传数据（非零端点）
 *   mod: DEF_UEP_DMA_LOAD(0) = 直接指向pbuf；DEF_UEP_CPY_LOAD(1) = memcpy
 * 返回 1=成功，0=端点忙/无效
 ******************************************************************************/
uint8_t USBHS_Endp_DataUp(uint8_t endp, uint8_t *pbuf, uint16_t len, uint8_t mod)
{
    if (endp < DEF_UEP1 || endp > DEF_UEP15)
        return 0;
    if (!(R16_U2EP_TX_EN & USBHSD_UEP_TX_EN(endp)))
        return 0;
    if (USBHS_Endp_Busy[endp] & DEF_UEP_BUSY)
        return 0;

    if (mod == DEF_UEP_DMA_LOAD) {
        USBHSD_UEP_TXDMA(endp) = (uint32_t)pbuf;
    } else if (mod == DEF_UEP_CPY_LOAD) {
        memcpy(USBHSD_UEP_TXBUF(endp), pbuf, len);
    } else {
        return 0;
    }

    USBHS_Endp_Busy[endp] |= DEF_UEP_BUSY;
    USBHSD_UEP_TLEN(endp)   = len;
    USBHSD_UEP_TXCTRL(endp) = (USBHSD_UEP_TXCTRL(endp) & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_ACK;
    return 1;
}

/*******************************************************************************
 * USBHS_Send_Resume — 发送远程唤醒信号
 ******************************************************************************/
void USBHS_Send_Resume(void)
{
    R8_USB2_WAKE_CTRL |= USBHS_UD_UD_REMOTE_WKUP;
}

/*******************************************************************************
 * USB2_DEVICE_IRQHandler — USBHS 高速设备中断处理
 ******************************************************************************/
__INTERRUPT
__HIGH_CODE
void USB2_DEVICE_IRQHandler(void)
{
    uint8_t  intflag, intst, errflag;
    uint16_t len;
    uint8_t  endp_num;

    intflag = R8_USB2_INT_FG;
    intst   = R8_USB2_INT_ST;

    if (intflag & USBHS_UDIF_TRANSFER) {
        endp_num = intst & USBHS_UDIS_EP_ID_MASK;

        if (!(intst & USBHS_UDIS_EP_DIR)) {
            /* ── SETUP / OUT 事务 ── */
            switch (endp_num) {
                case DEF_UEP0:
                    if (R8_U2EP0_RX_CTRL & USBHS_UEP_R_SETUP_IS) {
                        /* 暂存 Setup 包 */
                        USBHS_SetupReqType  = pUSBHS_SetupReqPak->bRequestType;
                        USBHS_SetupReqCode  = pUSBHS_SetupReqPak->bRequest;
                        USBHS_SetupReqLen   = pUSBHS_SetupReqPak->wLength;
                        USBHS_SetupReqValue = pUSBHS_SetupReqPak->wValue;
                        USBHS_SetupReqIndex = pUSBHS_SetupReqPak->wIndex;

                        len = 0;
                        errflag = 0;

                        if ((USBHS_SetupReqType & USB_REQ_TYP_MASK) != USB_REQ_TYP_STANDARD) {
                            /* ── 类请求 ── */
                            if ((USBHS_SetupReqType & USB_REQ_TYP_MASK) == USB_REQ_TYP_CLASS) {
                                switch (USBHS_SetupReqCode) {
                                    case HID_SET_REPORT:
                                        /* 通过 EP0 OUT 数据阶段处理 */
                                        break;
                                    case HID_SET_IDLE:
                                        if (USBHS_SetupReqIndex <= 2)
                                            USBHS_HidIdle[USBHS_SetupReqIndex] = (uint8_t)(USBHS_SetupReqValue >> 8);
                                        else
                                            errflag = 0xFF;
                                        break;
                                    case HID_SET_PROTOCOL:
                                        if (USBHS_SetupReqIndex <= 2)
                                            USBHS_HidProtocol[USBHS_SetupReqIndex] = (uint8_t)USBHS_SetupReqValue;
                                        else
                                            errflag = 0xFF;
                                        break;
                                    case HID_GET_IDLE:
                                        if (USBHS_SetupReqIndex <= 2) {
                                            USBHS_EP0_Buf[0] = USBHS_HidIdle[USBHS_SetupReqIndex];
                                            len = 1;
                                        } else {
                                            errflag = 0xFF;
                                        }
                                        break;
                                    case HID_GET_PROTOCOL:
                                        if (USBHS_SetupReqIndex <= 2) {
                                            USBHS_EP0_Buf[0] = USBHS_HidProtocol[USBHS_SetupReqIndex];
                                            len = 1;
                                        } else {
                                            errflag = 0xFF;
                                        }
                                        break;
                                    default:
                                        errflag = 0xFF;
                                        break;
                                }
                            } else {
                                errflag = 0xFF;
                            }
                        } else {
                            /* ── 标准请求 ── */
                            switch (USBHS_SetupReqCode) {
                                case USB_GET_DESCRIPTOR:
                                    switch ((uint8_t)(USBHS_SetupReqValue >> 8)) {
                                        case USB_DESCR_TYP_DEVICE:
                                            pUSBHS_Descr = MyDevDescr;
                                            len = DEF_USBD_DEVICE_DESC_LEN;
                                            break;
                                        case USB_DESCR_TYP_CONFIG:
                                            pUSBHS_Descr = MyCfgDescr;
                                            len = DEF_USBD_CONFIG_DESC_LEN;
                                            break;
                                        case USB_DESCR_TYP_STRING:
                                            switch ((uint8_t)(USBHS_SetupReqValue & 0xFF)) {
                                                case DEF_STRING_DESC_LANG:
                                                    pUSBHS_Descr = MyLangDescr;
                                                    len = DEF_USBD_LANG_DESC_LEN;
                                                    break;
                                                case DEF_STRING_DESC_MANU:
                                                    pUSBHS_Descr = MyManuInfo;
                                                    len = DEF_USBD_MANU_DESC_LEN;
                                                    break;
                                                case DEF_STRING_DESC_PROD:
                                                    pUSBHS_Descr = MyProdInfo;
                                                    len = DEF_USBD_PROD_DESC_LEN;
                                                    break;
                                                case DEF_STRING_DESC_SERN:
                                                    pUSBHS_Descr = MySerialInfo;
                                                    len = DEF_USBD_SN_DESC_LEN;
                                                    break;
                                                default:
                                                    errflag = 0xFF;
                                                    break;
                                            }
                                            break;
                                        case USB_DESCR_TYP_HID:
                                            /* HID 描述符嵌在配置描述符内 */
                                            if (USBHS_SetupReqIndex == 0x00) {
                                                pUSBHS_Descr = &MyCfgDescr[18]; /* IF0 HID desc offset */
                                                len = 9;
                                            } else if (USBHS_SetupReqIndex == 0x01) {
                                                pUSBHS_Descr = &MyCfgDescr[43]; /* IF1 HID desc offset */
                                                len = 9;
                                            } else if (USBHS_SetupReqIndex == 0x02) {
                                                pUSBHS_Descr = &MyCfgDescr[68]; /* IF2 HID desc offset */
                                                len = 9;
                                            } else {
                                                errflag = 0xFF;
                                            }
                                            break;
                                        case USB_DESCR_TYP_REPORT:
                                            if (USBHS_SetupReqIndex == 0x00) {
                                                pUSBHS_Descr = KeyRepDesc_NKRO;
                                                len = DEF_NKRO_REP_DESC_LEN;
                                            } else if (USBHS_SetupReqIndex == 0x01) {
                                                pUSBHS_Descr = KeyRepDesc_Consumer;
                                                len = DEF_CONSUMER_REP_DESC_LEN;
                                            } else if (USBHS_SetupReqIndex == 0x02) {
                                                /* Custom interface: 空报告描述符，直接 ZLP */
                                                len = 0;
                                                pUSBHS_Descr = KeyRepDesc_Custom;
                                                len = DEF_CUSTOM_REP_DESC_LEN;
                                            } else {
                                                errflag = 0xFF;
                                            }
                                            break;
                                        default:
                                            errflag = 0xFF;
                                            break;
                                    }
                                    if (errflag == 0) {
                                        if (USBHS_SetupReqLen > len)
                                            USBHS_SetupReqLen = len;
                                        len = (USBHS_SetupReqLen >= DEF_USBD_UEP0_SIZE)
                                              ? DEF_USBD_UEP0_SIZE : USBHS_SetupReqLen;
                                        memcpy(USBHS_EP0_Buf, pUSBHS_Descr, len);
                                        pUSBHS_Descr += len;
                                    }
                                    break;

                                case USB_SET_ADDRESS:
                                    USBHS_DevAddr = (uint8_t)(USBHS_SetupReqValue & 0xFF);
                                    break;

                                case USB_GET_CONFIGURATION:
                                    USBHS_EP0_Buf[0] = USBHS_DevConfig;
                                    if (USBHS_SetupReqLen > 1) USBHS_SetupReqLen = 1;
                                    break;

                                case USB_SET_CONFIGURATION:
                                    USBHS_DevConfig     = (uint8_t)(USBHS_SetupReqValue & 0xFF);
                                    USBHS_DevEnumStatus = 0x01;
                                    break;

                                case USB_CLEAR_FEATURE:
                                    if ((USBHS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE) {
                                        if ((uint8_t)(USBHS_SetupReqValue & 0xFF) == 0x01)
                                            USBHS_DevSleepStatus &= ~0x01;
                                        else
                                            errflag = 0xFF;
                                    } else if ((USBHS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) {
                                        if ((uint8_t)(USBHS_SetupReqValue & 0xFF) == USB_REQ_FEAT_ENDP_HALT) {
                                            switch ((uint8_t)(USBHS_SetupReqIndex & 0xFF)) {
                                                case (DEF_UEP_IN | DEF_UEP1):
                                                    R8_U2EP1_TX_CTRL = USBHS_UEP_T_TOG_DATA0 | USBHS_UEP_T_RES_NAK;
                                                    break;
                                                case (DEF_UEP_IN | DEF_UEP2):
                                                    R8_U2EP2_TX_CTRL = USBHS_UEP_T_TOG_DATA0 | USBHS_UEP_T_RES_NAK;
                                                    break;
                                                case (DEF_UEP_IN | DEF_UEP3):
                                                    R8_U2EP3_TX_CTRL = USBHS_UEP_T_TOG_DATA0 | USBHS_UEP_T_RES_NAK;
                                                    break;
                                                case DEF_UEP3:
                                                    R8_U2EP3_RX_CTRL = USBHS_UEP_R_TOG_DATA0 | USBHS_UEP_R_RES_ACK;
                                                    break;
                                                default:
                                                    errflag = 0xFF;
                                                    break;
                                            }
                                        } else {
                                            errflag = 0xFF;
                                        }
                                    } else {
                                        errflag = 0xFF;
                                    }
                                    break;

                                case USB_SET_FEATURE:
                                    if ((USBHS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE) {
                                        if ((uint8_t)(USBHS_SetupReqValue & 0xFF) == USB_REQ_FEAT_REMOTE_WAKEUP) {
                                            if (MyCfgDescr[7] & 0x20)
                                                USBHS_DevSleepStatus |= 0x01;
                                            else
                                                errflag = 0xFF;
                                        } else {
                                            errflag = 0xFF;
                                        }
                                    } else if ((USBHS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) {
                                        if ((uint8_t)(USBHS_SetupReqValue & 0xFF) == USB_REQ_FEAT_ENDP_HALT) {
                                            switch ((uint8_t)(USBHS_SetupReqIndex & 0xFF)) {
                                                case (DEF_UEP_IN | DEF_UEP1):
                                                    R8_U2EP1_TX_CTRL = (R8_U2EP1_TX_CTRL & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_STALL;
                                                    break;
                                                case (DEF_UEP_IN | DEF_UEP2):
                                                    R8_U2EP2_TX_CTRL = (R8_U2EP2_TX_CTRL & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_STALL;
                                                    break;
                                                case (DEF_UEP_IN | DEF_UEP3):
                                                    R8_U2EP3_TX_CTRL = (R8_U2EP3_TX_CTRL & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_STALL;
                                                    break;
                                                default:
                                                    errflag = 0xFF;
                                                    break;
                                            }
                                        }
                                    }
                                    break;

                                case USB_GET_INTERFACE:
                                    USBHS_EP0_Buf[0] = 0x00;
                                    if (USBHS_SetupReqLen > 1) USBHS_SetupReqLen = 1;
                                    break;

                                case USB_SET_INTERFACE:
                                    break;

                                case USB_GET_STATUS:
                                    USBHS_EP0_Buf[0] = 0x00;
                                    USBHS_EP0_Buf[1] = 0x00;
                                    if ((USBHS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE) {
                                        if (USBHS_DevSleepStatus & 0x01)
                                            USBHS_EP0_Buf[0] = 0x02;
                                    } else if ((USBHS_SetupReqType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP) {
                                        uint8_t ep = (uint8_t)(USBHS_SetupReqIndex & 0xFF);
                                        if (ep == (DEF_UEP_IN | DEF_UEP1)) {
                                            if ((R8_U2EP1_TX_CTRL & USBHS_UEP_T_RES_MASK) == USBHS_UEP_T_RES_STALL)
                                                USBHS_EP0_Buf[0] = 0x01;
                                        } else if (ep == (DEF_UEP_IN | DEF_UEP2)) {
                                            if ((R8_U2EP2_TX_CTRL & USBHS_UEP_T_RES_MASK) == USBHS_UEP_T_RES_STALL)
                                                USBHS_EP0_Buf[0] = 0x01;
                                        } else if (ep == (DEF_UEP_IN | DEF_UEP3)) {
                                            if ((R8_U2EP3_TX_CTRL & USBHS_UEP_T_RES_MASK) == USBHS_UEP_T_RES_STALL)
                                                USBHS_EP0_Buf[0] = 0x01;
                                        } else {
                                            errflag = 0xFF;
                                        }
                                    } else {
                                        errflag = 0xFF;
                                    }
                                    if (USBHS_SetupReqLen > 2) USBHS_SetupReqLen = 2;
                                    break;

                                default:
                                    errflag = 0xFF;
                                    break;
                            }
                        }

                        /* 错误：STALL；正常：开始数据阶段 */
                        if (errflag == 0xFF) {
                            R8_U2EP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_STALL;
                            R8_U2EP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_STALL;
                        } else {
                            if (USBHS_SetupReqType & DEF_UEP_IN) {
                                len = (USBHS_SetupReqLen > DEF_USBD_UEP0_SIZE)
                                      ? DEF_USBD_UEP0_SIZE : USBHS_SetupReqLen;
                                USBHS_SetupReqLen -= len;
                                R16_U2EP0_T_LEN  = len;
                                R8_U2EP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_ACK;
                            } else {
                                if (USBHS_SetupReqLen == 0) {
                                    R16_U2EP0_T_LEN  = 0;
                                    R8_U2EP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_ACK;
                                } else {
                                    R8_U2EP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_ACK;
                                }
                            }
                        }
                    } else {
                        /* EP0 OUT 数据阶段（类请求写入）*/
                        R8_U2EP0_RX_CTRL = USBHS_UEP_R_RES_NAK;
                        if ((USBHS_SetupReqType & USB_REQ_TYP_MASK) == USB_REQ_TYP_CLASS) {
                            /* HID SET_REPORT — 忽略 LED 输出报告（键盘无 LED 硬件）*/
                        }
                        if (USBHS_SetupReqLen == 0) {
                            R16_U2EP0_T_LEN  = 0;
                            R8_U2EP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_ACK;
                        }
                    }
                    R8_U2EP0_RX_CTRL &= ~USBHS_UEP_R_DONE;
                    break;

                case DEF_UEP3:
                    /* EP3 OUT — 接收配置命令包 */
                    {
                        uint16_t rx_len = R16_U2EP3_RX_LEN;
                        if (rx_len > 0 && rx_len <= DEF_USB_EP3_HS_SIZE) {
                            memset(USBHS_EP3_TX_Buf, 0, DEF_USB_EP3_HS_SIZE);
                            Config_ProcessUSBCommand(USBHS_EP3_RX_Buf, USBHS_EP3_TX_Buf);
                        }
                        R8_U2EP3_RX_CTRL = (R8_U2EP3_RX_CTRL & ~USBHS_UEP_R_RES_MASK) | USBHS_UEP_R_RES_ACK;
                        R8_U2EP3_RX_CTRL ^= USBHS_UEP_R_TOG_DATA1;
                        R8_U2EP3_RX_CTRL &= ~USBHS_UEP_R_DONE;
                    }
                    break;

                default:
                    break;
            }
        } else {
            /* ── IN 事务完成 ── */
            switch (endp_num) {
                case DEF_UEP0:
                    if (USBHS_SetupReqLen == 0) {
                        R8_U2EP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_ACK;
                    }
                    if ((USBHS_SetupReqType & USB_REQ_TYP_MASK) == USB_REQ_TYP_STANDARD) {
                        switch (USBHS_SetupReqCode) {
                            case USB_GET_DESCRIPTOR:
                                len = (USBHS_SetupReqLen >= DEF_USBD_UEP0_SIZE)
                                      ? DEF_USBD_UEP0_SIZE : USBHS_SetupReqLen;
                                memcpy(USBHS_EP0_Buf, pUSBHS_Descr, len);
                                USBHS_SetupReqLen -= len;
                                pUSBHS_Descr      += len;
                                R16_U2EP0_T_LEN   = len;
                                R8_U2EP0_TX_CTRL ^= USBHS_UEP_T_TOG_DATA1;
                                R8_U2EP0_TX_CTRL  = (R8_U2EP0_TX_CTRL & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_ACK;
                                break;
                            case USB_SET_ADDRESS:
                                R8_USB2_DEV_AD = USBHS_DevAddr;
                                break;
                            default:
                                R16_U2EP0_T_LEN = 0;
                                break;
                        }
                    }
                    R8_U2EP0_TX_CTRL &= ~USBHS_UEP_T_DONE;
                    break;

                case DEF_UEP1:
                    R8_U2EP1_TX_CTRL  = (R8_U2EP1_TX_CTRL & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_NAK;
                    R8_U2EP1_TX_CTRL ^= USBHS_UEP_T_TOG_DATA1;
                    USBHS_Endp_Busy[DEF_UEP1] &= ~DEF_UEP_BUSY;
                    R8_U2EP1_TX_CTRL &= ~USBHS_UEP_T_DONE;
                    break;

                case DEF_UEP2:
                    R8_U2EP2_TX_CTRL  = (R8_U2EP2_TX_CTRL & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_NAK;
                    R8_U2EP2_TX_CTRL ^= USBHS_UEP_T_TOG_DATA1;
                    USBHS_Endp_Busy[DEF_UEP2] &= ~DEF_UEP_BUSY;
                    R8_U2EP2_TX_CTRL &= ~USBHS_UEP_T_DONE;
                    break;

                case DEF_UEP3:
                    R8_U2EP3_TX_CTRL  = (R8_U2EP3_TX_CTRL & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_NAK;
                    R8_U2EP3_TX_CTRL ^= USBHS_UEP_T_TOG_DATA1;
                    USBHS_Endp_Busy[DEF_UEP3] &= ~DEF_UEP_BUSY;
                    R8_U2EP3_TX_CTRL &= ~USBHS_UEP_T_DONE;
                    /* 通知上层EP3发送完成 */
                    extern void USB_HID_EP3_Complete(void);
                    USB_HID_EP3_Complete();
                    break;

                default:
                    break;
            }
        }
    } else if (intflag & USBHS_UDIF_LINK_RDY) {
        R8_USB2_INT_FG = USBHS_UDIF_LINK_RDY;
    } else if (intflag & USBHS_UDIF_SUSPEND) {
        R8_USB2_INT_FG = USBHS_UDIF_SUSPEND;
        if (R8_USB2_MIS_ST & USBHS_UDMS_SUSPEND) {
            USBHS_DevSleepStatus |= 0x02;
        } else {
            USBHS_DevSleepStatus &= ~0x02;
        }
    } else if (intflag & USBHS_UDIF_BUS_RST) {
        USBHS_DevConfig     = 0;
        USBHS_DevAddr       = 0;
        USBHS_DevSleepStatus = 0;
        USBHS_DevEnumStatus  = 0;
        R8_USB2_DEV_AD = 0;
        USBHS_KBD_Endp_Init();
        R8_USB2_INT_FG = USBHS_UDIF_BUS_RST;
    } else {
        R8_USB2_INT_FG = intflag;
    }
}
