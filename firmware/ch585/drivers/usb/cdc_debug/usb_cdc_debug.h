/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_cdc_debug.h
 * Description        : USB FS CDC debug/control channel for CH585M
 *******************************************************************************/

#ifndef __USB_CDC_DEBUG_H__
#define __USB_CDC_DEBUG_H__

#include "CONFIG.h"

#ifdef __cplusplus
extern "C" {
#endif

void USB_CDC_DebugInit(void);
void USB_CDC_DebugProcess(void);
uint8_t USB_CDC_DebugIsReady(void);
uint8_t USB_CDC_DebugWrite(const uint8_t *data, uint16_t len);
void USB_CDC_DebugWriteString(const char *str);
void USB_CDC_DebugLog(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __USB_CDC_DEBUG_H__ */
