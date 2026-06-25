/*******************************************************************************
 * usb_hid.c — CH585 USB HS HID 实现（有线模式）
 *
 * 使用 CH585 USBHS 控制器 + TMOS 任务调度（可选）。
 * 此文件提供对外封装的发送接口，底层描述符/中断由 ch585_usbhs_device.c 处理
 * （参考 CompositeKM/CompatibilityHID 例程）。
 *
 * 接口布局（在 usb_desc.c 中定义）：
 *   Interface 0 → Keyboard HID → EP1 IN  (bInterval = 1 = 125µs 高速 ≈ 8kHz)
 *   Interface 1 → Consumer HID → EP2 IN
 *   Interface 2 → Custom HID   → EP3 IN/OUT（64B 配置命令）
 ******************************************************************************/

#include "usb_hid.h"
#include "ch585_usbhs_kbd.h"
#include <string.h>

/* 对外可见变量（hid层上层代码使用）*/
uint8_t g_UsbKbdReport[KBD_REPORT_LEN];
uint8_t g_UsbConsumerReport[CONSUMER_REPORT_LEN];
volatile uint8_t g_UsbConfigCmdReceived  = 0;
uint8_t g_UsbConfigCmdBuf[CFG_EP_LEN];

/* ADC 监控缓冲区 */
static uint8_t s_adc_mon_buf[19];

/* EP3 复用保护：0=空闲, 1=配置响应发送中, 2=ADC监控发送中 */
static volatile uint8_t s_ep3_state = 0;
/* 待发送的ADC监控标记 */
static volatile uint8_t s_adc_mon_pending = 0;

/* -----------------------------------------------------------------------
 * USB_HID_Init
 * ----------------------------------------------------------------------- */
void USB_HID_Init(void)
{
    memset(g_UsbKbdReport, 0, sizeof(g_UsbKbdReport));
    memset(g_UsbConsumerReport, 0, sizeof(g_UsbConsumerReport));

    USBHS_KBD_Device_Init(ENABLE);
    PFIC_EnableIRQ(USB2_DEVICE_IRQn);
}

/* -----------------------------------------------------------------------
 * USB_HID_Enable / USB_HID_Disable
 * ----------------------------------------------------------------------- */
void USB_HID_Enable(void)
{
    USBHS_KBD_Device_Init(ENABLE);
    PFIC_EnableIRQ(USB2_DEVICE_IRQn);
}

void USB_HID_Disable(void)
{
    PFIC_DisableIRQ(USB2_DEVICE_IRQn);
    USBHS_KBD_Device_Init(DISABLE);
}

/* -----------------------------------------------------------------------
 * USB_HID_SendKeyboard
 * 参数 report：KBD_REPORT_LEN 字节的 NKRO 报告
 * ----------------------------------------------------------------------- */
void USB_HID_SendKeyboard(const uint8_t *report)
{
    if(!USBHS_DevEnumStatus) return;
    if(USBHS_Endp_Busy[DEF_UEP1] & DEF_UEP_BUSY) return;

    memcpy(g_UsbKbdReport, report, KBD_REPORT_LEN);
    USBHS_Endp_DataUp(DEF_UEP1, g_UsbKbdReport, KBD_REPORT_LEN, DEF_UEP_DMA_LOAD);
}

/* -----------------------------------------------------------------------
 * USB_HID_SendConsumer
 * usage：consumer control HID usage code（0=release）
 * ----------------------------------------------------------------------- */
void USB_HID_SendConsumer(uint16_t usage)
{
    if(!USBHS_DevEnumStatus) return;
    if(USBHS_Endp_Busy[DEF_UEP2] & DEF_UEP_BUSY) return;

    g_UsbConsumerReport[0] = (uint8_t)(usage & 0xFF);
    g_UsbConsumerReport[1] = (uint8_t)(usage >> 8);
    USBHS_Endp_DataUp(DEF_UEP2, g_UsbConsumerReport, CONSUMER_REPORT_LEN, DEF_UEP_DMA_LOAD);
}

/* -----------------------------------------------------------------------
 * USB_HID_SendConfigResp
 * 配置响应优先级高于ADC监控
 * ----------------------------------------------------------------------- */
void USB_HID_SendConfigResp(const uint8_t *resp)
{
    if(!USBHS_DevEnumStatus) return;

    /* 如果正在发送ADC监控，取消它，优先发送配置响应 */
    s_ep3_state = 1;

    if(USBHS_Endp_Busy[DEF_UEP3] & DEF_UEP_BUSY) {
        /* EP3忙，标记为挂起，由发送完成中断重试 */
        s_ep3_state = 0;
        return;
    }

    USBHS_Endp_DataUp(DEF_UEP3, (uint8_t*)resp, CFG_EP_LEN, DEF_UEP_CPY_LOAD);
}

/* -----------------------------------------------------------------------
 * USB_HID_SendADCMonitor
 * 格式：[0xAA][K1_lo][K1_hi]...[K9_lo][K9_hi]（19字节）
 * 配置响应优先级高于ADC监控
 * ----------------------------------------------------------------------- */
void USB_HID_SendADCMonitor(volatile uint16_t *adc9)
{
    uint8_t i;
    if(!USBHS_DevEnumStatus) return;

    /* 如果正在发送配置响应，跳过本次ADC监控 */
    if(s_ep3_state == 1) {
        s_adc_mon_pending = 1;  /* 标记有挂起的ADC监控 */
        return;
    }

    if(USBHS_Endp_Busy[DEF_UEP3] & DEF_UEP_BUSY) {
        s_adc_mon_pending = 1;
        return;
    }

    s_adc_mon_buf[0] = 0xAA;
    for(i = 0; i < 9; i++) {
        s_adc_mon_buf[1 + i*2]     = (uint8_t)(adc9[i] & 0xFF);
        s_adc_mon_buf[1 + i*2 + 1] = (uint8_t)(adc9[i] >> 8);
    }

    s_ep3_state = 2;  /* ADC监控发送中 */
    if(!USBHS_Endp_DataUp(DEF_UEP3, s_adc_mon_buf, 19, DEF_UEP_CPY_LOAD)) {
        s_ep3_state = 0;  /* 发送失败，重置状态 */
    }
}

/* -----------------------------------------------------------------------
 * USB_HID_EP3_Complete — EP3发送完成回调（由USB中断调用）
 * ----------------------------------------------------------------------- */
void USB_HID_EP3_Complete(void)
{
    s_ep3_state = 0;  /* EP3空闲 */
}
