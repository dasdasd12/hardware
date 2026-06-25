/*******************************************************************************
 * ch585_usbhs_kbd.h — CH585 键盘 USBHS 设备驱动头文件
 *
 * 基于 WCH 官方 CompositeKM 示例改编，支持三接口 HID：
 *   EP1 IN  — NKRO Keyboard（bInterval=1）
 *   EP2 IN  — Consumer Control（bInterval=4）
 *   EP3 IN+OUT — Custom Config / ADC Monitor（bInterval=1）
 ******************************************************************************/

#ifndef __CH585_USBHS_KBD_H__
#define __CH585_USBHS_KBD_H__

#include "CH58x_common.h"
#include "string.h"
#include "usb_desc_hid.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── 通用宏 ─── */
#define pUSBHS_SetupReqPak       ((PUSB_SETUP_REQ)USBHS_EP0_Buf)
#define DEF_UEP_IN               0x80
#define DEF_UEP_OUT              0x00
#define DEF_UEP_BUSY             0x01
#define DEF_UEP_FREE             0x00
#define DEF_UEP_NUM              16
#define DEF_UEP0                 0x00
#define DEF_UEP1                 0x01
#define DEF_UEP2                 0x02
#define DEF_UEP3                 0x03
#define DEF_USBD_UEP0_SIZE       64

/* ─── USBHS 端点 DMA/CTRL 访问宏 ─── */
#define USBHSD_UEP_RXDMA_BASE    0x40009024
#define USBHSD_UEP_TXDMA_BASE    0x40009040
#define USBHSD_UEP_TXLEN_BASE    0x404090A0
#define USBHSD_UEP_TXCTL_BASE    0x404090A2
#define USBHSD_UEP_TX_EN(N)      ((uint16_t)(0x01 << (N)))
#define USBHSD_UEP_RX_EN(N)      ((uint16_t)(0x01 << (N)))
#define DEF_UEP_DMA_LOAD         0
#define DEF_UEP_CPY_LOAD         1
#define USBHSD_UEP_RXDMA(N)     (*((volatile uint32_t *)(USBHSD_UEP_RXDMA_BASE + ((N)-1)*0x04)))
#define USBHSD_UEP_RXBUF(N)     ((uint8_t *)(*((volatile uint32_t *)(USBHSD_UEP_RXDMA_BASE + ((N)-1)*0x04))) + 0x20000000)
#define USBHSD_UEP_TXCTRL(N)    (*((volatile uint8_t *)(USBHSD_UEP_TXCTL_BASE + ((N)-1)*0x04)))
#define USBHSD_UEP_TXDMA(N)     (*((volatile uint32_t *)(USBHSD_UEP_TXDMA_BASE + ((N)-1)*0x04)))
#define USBHSD_UEP_TXBUF(N)     ((uint8_t *)(*((volatile uint32_t *)(USBHSD_UEP_TXDMA_BASE + ((N)-1)*0x04))) + 0x20000000)
#define USBHSD_UEP_TLEN(N)      (*((volatile uint16_t *)(USBHSD_UEP_TXLEN_BASE + ((N)-1)*0x04)))

/* ─── 端点尺寸 ─── */
#define DEF_USB_EP1_HS_SIZE      64   /* NKRO Keyboard */
#define DEF_USB_EP2_HS_SIZE      8    /* Consumer */
#define DEF_USB_EP3_HS_SIZE      64   /* Custom Config */

/* ─── 字符串索引（CH585SFR.h 已定义相同值，此处不重复定义）─── */
/* DEF_STRING_DESC_LANG=0, MANU=1, PROD=2, SERN=3 */

/* ─── HID 类命令 ─── */
#ifndef HID_SET_REPORT
#define HID_SET_REPORT           0x09
#define HID_SET_IDLE             0x0A
#define HID_SET_PROTOCOL         0x0B
#define HID_GET_IDLE             0x02
#define HID_GET_PROTOCOL         0x03
#endif

/* ─── 端点对齐缓冲区 ─── */
extern __attribute__((aligned(4))) uint8_t USBHS_EP0_Buf[];
extern __attribute__((aligned(4))) uint8_t USBHS_EP1_TX_Buf[];
extern __attribute__((aligned(4))) uint8_t USBHS_EP2_TX_Buf[];
extern __attribute__((aligned(4))) uint8_t USBHS_EP3_TX_Buf[];
extern __attribute__((aligned(4))) uint8_t USBHS_EP3_RX_Buf[];

/* ─── 设备状态 ─── */
extern volatile uint8_t  USBHS_DevConfig;
extern volatile uint8_t  USBHS_DevEnumStatus;
extern volatile uint8_t  USBHS_DevSleepStatus;
extern volatile uint8_t  USBHS_DevSpeed;

/* ─── 端点忙标志 ─── */
extern volatile uint8_t  USBHS_Endp_Busy[];

/* ─── EP3 OUT 接收完成回调（由 config_system.c 注册）─── */
extern void Config_ProcessUSBCommand(const uint8_t *cmd_buf, uint8_t *resp_buf);

/* ─── 端点编号上限 ─── */
#define DEF_UEP15   15

/* ─── 公共 API ─── */
void    USBHS_KBD_Device_Init(FunctionalState sta);
void    USBHS_KBD_Endp_Init(void);
uint8_t USBHS_Endp_DataUp(uint8_t endp, uint8_t *pbuf, uint16_t len, uint8_t mod);
void    USBHS_Send_Resume(void);

#ifdef __cplusplus
}
#endif

#endif /* __CH585_USBHS_KBD_H__ */
