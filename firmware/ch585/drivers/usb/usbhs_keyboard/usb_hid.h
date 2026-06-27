#ifndef __USB_HID_H
#define __USB_HID_H

/*******************************************************************************
 * usb_hid.h — CH585 USB HS HID（有线模式）
 *
 * 使用 USBHS 控制器（CH585 内置）。
 * 接口布局：
 *   Interface 0：HID Keyboard  EP1 IN  Boot+NKRO
 *   Interface 1：HID Consumer  EP2 IN  （旋钮音量）
 *   Interface 2：HID Custom    EP3 IN/OUT（配置命令）
 ******************************************************************************/

#include "CH58x_common.h"

/* 轮询间隔（bInterval，单位 125µs 高速 = 0.125ms） */
#define HID_KBD_INTERVAL    1    /* 1 × 125µs = 125µs ≈ 8kHz */
#define HID_CONSUMER_INTERVAL 4  /* 4 × 125µs = 500µs */

/* 键盘报告格式（NKRO，无 Report ID）：
 *   Byte 0 = Modifiers
 *   Byte 1 = Reserved
 *   Bytes 2-15 = 112-key NKRO 位图（0x04..0x6F）
 * 注：配置命令通过 EP3 传输，无 Report ID 区分 */
#define KBD_REPORT_LEN    16
#define CONSUMER_REPORT_LEN 2

/* EP3 配置命令包长度 */
#define CFG_EP_LEN        64

extern volatile uint8_t USBHS_DevEnumStatus;
extern uint8_t g_UsbKbdReport[KBD_REPORT_LEN];
extern uint8_t g_UsbConsumerReport[CONSUMER_REPORT_LEN];
extern volatile uint8_t g_UsbConfigCmdReceived;
extern uint8_t g_UsbConfigCmdBuf[CFG_EP_LEN];

void USB_HID_Init(void);
void USB_HID_Enable(void);                            /* 启动 USBHS 控制器 */
void USB_HID_Disable(void);                           /* 停用 USBHS 控制器 */
void USB_HID_SendKeyboard(const uint8_t *report);     /* report: KBD_REPORT_LEN 字节 */
void USB_HID_SendConsumer(uint16_t usage);
void USB_HID_SendConfigResp(const uint8_t *resp);
uint8_t USB_HID_SendCustom64(const uint8_t *report);
void USB_HID_SendADCMonitor(volatile uint16_t *adc9); /* 19 字节 ADC 监控包 */

/* EP3 发送完成回调（由 USB 中断处理调用） */
void USB_HID_EP3_Complete(void);

#endif /* __USB_HID_H */
