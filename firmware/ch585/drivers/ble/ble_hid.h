/********************************** (C) COPYRIGHT *******************************
 * File Name          : ble_hid.h
 * Description        : Minimal connectable BLE HID wrapper for CH585M
 *******************************************************************************/

#ifndef __BLE_HID_H__
#define __BLE_HID_H__

#include "CONFIG.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_HID_KBD_REPORT_LEN 8

void BLE_HID_Init(void);
uint8_t BLE_HID_IsConnected(void);
uint8_t BLE_HID_IsKeyTapBusy(void);
uint8_t BLE_HID_GetQueuedTapCount(void);
void BLE_HID_StartAdvert(void);
void BLE_HID_StopAdvert(void);
uint8_t BLE_HID_SendKeyboard(const uint8_t *report8);
uint8_t BLE_HID_TriggerKeyTap(uint8_t keycode);
uint8_t BLE_HID_TriggerModifiedKeyTap(uint8_t modifier, uint8_t keycode);
uint16_t BLE_HID_ProcessEvent(uint8_t task_id, uint16_t events);

#ifdef __cplusplus
}
#endif

#endif /* __BLE_HID_H__ */
