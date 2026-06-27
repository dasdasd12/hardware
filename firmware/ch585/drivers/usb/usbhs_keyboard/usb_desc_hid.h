/*******************************************************************************
 * usb_desc_hid.h — CH585 键盘 HID USB 描述符声明
 *
 * 对应 usb_desc_hid.c 中定义的所有描述符数组。
 ******************************************************************************/

#ifndef __USB_DESC_HID_H__
#define __USB_DESC_HID_H__

#include "CH58x_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── 描述符长度 ─── */
#define DEF_USBD_DEVICE_DESC_LEN    18
#define DEF_USBD_CONFIG_DESC_LEN    91
#define DEF_USBD_LANG_DESC_LEN      4

/* ─── 描述符外部声明 ─── */
extern const uint8_t MyDevDescr[];
extern const uint8_t MyCfgDescr[];
extern const uint8_t MyLangDescr[];
extern const uint8_t MyManuInfo[];
extern const uint8_t MyProdInfo[];
extern const uint8_t MySerialInfo[];
extern const uint8_t KeyRepDesc_NKRO[];
extern const uint8_t KeyRepDesc_Consumer[];
extern const uint8_t KeyRepDesc_Custom[];

/* ─── Report 描述符字节长度（用于跨编译单元的 sizeof 替代）─── */
#define DEF_NKRO_REP_DESC_LEN     45   /* sizeof(KeyRepDesc_NKRO)     */
#define DEF_CONSUMER_REP_DESC_LEN 23   /* sizeof(KeyRepDesc_Consumer) */
#define DEF_CUSTOM_REP_DESC_LEN   27   /* sizeof(KeyRepDesc_Custom)   */

/* ─── 字符串描述符长度（sizeof 计算）─── */
#define DEF_USBD_MANU_DESC_LEN      14   /* sizeof(MyManuInfo)   */
#define DEF_USBD_PROD_DESC_LEN      26   /* sizeof(MyProdInfo)   */
#define DEF_USBD_SN_DESC_LEN        12   /* sizeof(MySerialInfo) */

#ifdef __cplusplus
}
#endif

#endif /* __USB_DESC_HID_H__ */
