/********************************** (C) COPYRIGHT *******************************
 * File Name          : kvm_control.h
 * Description        : KVM action layer built on BLE HID keyboard reports
 *******************************************************************************/

#ifndef __KVM_CONTROL_H__
#define __KVM_CONTROL_H__

#include "CONFIG.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KVM_TARGET_MIN          1
#define KVM_TARGET_MAX          3

#define KVM_MOD_LEFT_CTRL       0x01
#define KVM_MOD_LEFT_SHIFT      0x02
#define KVM_MOD_LEFT_ALT        0x04
#define KVM_MOD_LEFT_GUI        0x08
#define KVM_MOD_RIGHT_CTRL      0x10
#define KVM_MOD_RIGHT_SHIFT     0x20
#define KVM_MOD_RIGHT_ALT       0x40
#define KVM_MOD_RIGHT_GUI       0x80

void KVM_ControlInit(void);
uint8_t KVM_GetCurrentTarget(void);
uint8_t KVM_SwitchTarget(uint8_t target);
uint8_t KVM_SendCombo(uint8_t modifier, uint8_t keycode);
uint8_t KVM_TypeText(const char *text, uint8_t max_len);
uint8_t KVM_AsciiToKey(char ch, uint8_t *modifier, uint8_t *keycode);

#ifdef __cplusplus
}
#endif

#endif /* __KVM_CONTROL_H__ */
