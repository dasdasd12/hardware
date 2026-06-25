/*
 * Temporary H417 -> left CH585 BLE HID bridge.
 *
 * This is a bring-up adapter: H417 builds an 8-byte keyboard HID report from
 * the currently merged CH585 key state and sends it to the left CH585 over the
 * existing PCB SPI bus with the left CS line.
 */

#ifndef CH585_BLE_BRIDGE_H__
#define CH585_BLE_BRIDGE_H__

#include <stdint.h>

int ch585_ble_bridge_init(void);
void ch585_ble_bridge_poll_from_raw(const uint16_t *raw_adc, uint16_t key_count);
uint32_t ch585_ble_bridge_reports_sent(void);
uint32_t ch585_ble_bridge_send_errors(void);
uint8_t ch585_ble_bridge_last_seq(void);

#endif /* CH585_BLE_BRIDGE_H__ */
