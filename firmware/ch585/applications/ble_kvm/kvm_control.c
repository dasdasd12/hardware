/********************************** (C) COPYRIGHT *******************************
 * File Name          : kvm_control.c
 * Description        : KVM action layer built on BLE HID keyboard reports
 *******************************************************************************/

#include "kvm_control.h"
#include "ble_hid.h"
#include "hiddev.h"
#include "usb_cdc_debug.h"

#include <string.h>

#define KVM_LOG(...)                  \
    do                                \
    {                                 \
        USB_CDC_DebugLog(__VA_ARGS__); \
    } while(0)

static uint8_t g_kvm_current_target = KVM_TARGET_MIN;

static uint8_t kvmQueueAscii(char ch);

void KVM_ControlInit(void)
{
    g_kvm_current_target = KVM_TARGET_MIN;
    KVM_LOG("KVM: init target=%u\n", g_kvm_current_target);
}

uint8_t KVM_GetCurrentTarget(void)
{
    return g_kvm_current_target;
}

uint8_t KVM_SwitchTarget(uint8_t target)
{
    if(target < KVM_TARGET_MIN || target > KVM_TARGET_MAX)
    {
        return INVALIDPARAMETER;
    }

    g_kvm_current_target = target;
    KVM_LOG("KVM: target=%u\n", g_kvm_current_target);
    return SUCCESS;
}

uint8_t KVM_SendCombo(uint8_t modifier, uint8_t keycode)
{
    uint8_t status;

    status = BLE_HID_TriggerModifiedKeyTap(modifier, keycode);
    KVM_LOG("KVM: combo mod=%02x key=%02x status=%02x\n", modifier, keycode, status);
    return status;
}

uint8_t KVM_TypeText(const char *text, uint8_t max_len)
{
    uint8_t i;
    uint8_t status = SUCCESS;

    if(text == NULL)
    {
        return INVALIDPARAMETER;
    }

    for(i = 0; i < max_len && text[i] != '\0'; ++i)
    {
        status = kvmQueueAscii(text[i]);
        if(status != SUCCESS)
        {
            KVM_LOG("KVM: type stop index=%u status=%02x\n", i, status);
            return status;
        }
    }

    KVM_LOG("KVM: type queued len=%u\n", i);
    return SUCCESS;
}

uint8_t KVM_AsciiToKey(char ch, uint8_t *modifier, uint8_t *keycode)
{
    if(modifier == NULL || keycode == NULL)
    {
        return INVALIDPARAMETER;
    }

    *modifier = 0;
    *keycode = HID_KEYBOARD_RESERVED;

    if(ch >= 'a' && ch <= 'z')
    {
        *keycode = (uint8_t)(HID_KEYBOARD_A + ch - 'a');
        return SUCCESS;
    }

    if(ch >= 'A' && ch <= 'Z')
    {
        *modifier = KVM_MOD_LEFT_SHIFT;
        *keycode = (uint8_t)(HID_KEYBOARD_A + ch - 'A');
        return SUCCESS;
    }

    if(ch >= '1' && ch <= '9')
    {
        *keycode = (uint8_t)(HID_KEYBOARD_1 + ch - '1');
        return SUCCESS;
    }

    switch(ch)
    {
        case '0':
            *keycode = HID_KEYBOARD_0;
            return SUCCESS;

        case '\r':
        case '\n':
            *keycode = HID_KEYBOARD_RETURN;
            return SUCCESS;

        case ' ':
            *keycode = HID_KEYBOARD_SPACEBAR;
            return SUCCESS;

        case '\t':
            *keycode = HID_KEYBOARD_TAB;
            return SUCCESS;

        case '-':
            *keycode = HID_KEYBOARD_MINUS;
            return SUCCESS;

        case '_':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_MINUS;
            return SUCCESS;

        case '=':
            *keycode = HID_KEYBOARD_EQUAL;
            return SUCCESS;

        case '+':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_EQUAL;
            return SUCCESS;

        case '[':
            *keycode = HID_KEYBOARD_LEFT_BRKT;
            return SUCCESS;

        case '{':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_LEFT_BRKT;
            return SUCCESS;

        case ']':
            *keycode = HID_KEYBOARD_RIGHT_BRKT;
            return SUCCESS;

        case '}':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_RIGHT_BRKT;
            return SUCCESS;

        case '\\':
            *keycode = HID_KEYBOARD_BACK_SLASH;
            return SUCCESS;

        case '|':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_BACK_SLASH;
            return SUCCESS;

        case ';':
            *keycode = HID_KEYBOARD_SEMI_COLON;
            return SUCCESS;

        case ':':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_SEMI_COLON;
            return SUCCESS;

        case '\'':
            *keycode = HID_KEYBOARD_SGL_QUOTE;
            return SUCCESS;

        case '"':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_SGL_QUOTE;
            return SUCCESS;

        case '`':
            *keycode = HID_KEYBOARD_GRV_ACCENT;
            return SUCCESS;

        case '~':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_GRV_ACCENT;
            return SUCCESS;

        case ',':
            *keycode = HID_KEYBOARD_COMMA;
            return SUCCESS;

        case '<':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_COMMA;
            return SUCCESS;

        case '.':
            *keycode = HID_KEYBOARD_DOT;
            return SUCCESS;

        case '>':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_DOT;
            return SUCCESS;

        case '/':
            *keycode = HID_KEYBOARD_FWD_SLASH;
            return SUCCESS;

        case '?':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_FWD_SLASH;
            return SUCCESS;

        case '!':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_1;
            return SUCCESS;

        case '@':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_2;
            return SUCCESS;

        case '#':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_3;
            return SUCCESS;

        case '$':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_4;
            return SUCCESS;

        case '%':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_5;
            return SUCCESS;

        case '^':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_6;
            return SUCCESS;

        case '&':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_7;
            return SUCCESS;

        case '*':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_8;
            return SUCCESS;

        case '(':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_9;
            return SUCCESS;

        case ')':
            *modifier = KVM_MOD_LEFT_SHIFT;
            *keycode = HID_KEYBOARD_0;
            return SUCCESS;

        default:
            return INVALIDPARAMETER;
    }
}

static uint8_t kvmQueueAscii(char ch)
{
    uint8_t modifier;
    uint8_t keycode;
    uint8_t status;

    status = KVM_AsciiToKey(ch, &modifier, &keycode);
    if(status != SUCCESS)
    {
        return status;
    }

    return BLE_HID_TriggerModifiedKeyTap(modifier, keycode);
}
