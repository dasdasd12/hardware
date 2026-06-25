#ifndef USB_HS_HID_KEYBOARD_H
#define USB_HS_HID_KEYBOARD_H

#include <rtthread.h>

int ch32h417_usbhs_hid_init(void);
void ch32h417_usbhs_hid_poll_keyboard(const rt_uint16_t *raw_adc,
                                      rt_uint16_t key_count);

rt_uint8_t ch32h417_usbhs_hid_configured(void);
rt_uint8_t ch32h417_usbhs_hid_keyboard_busy(void);
rt_uint8_t ch32h417_usbhs_hid_vendor_busy(void);
rt_uint32_t ch32h417_usbhs_hid_keyboard_reports(void);
rt_uint32_t ch32h417_usbhs_hid_vendor_rx_reports(void);
rt_uint32_t ch32h417_usbhs_hid_vendor_tx_reports(void);
rt_uint8_t ch32h417_usbhs_hid_last_vendor_cmd(void);

#endif
