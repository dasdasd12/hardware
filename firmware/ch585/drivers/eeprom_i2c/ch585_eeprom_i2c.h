#ifndef CH585_EEPROM_I2C_H
#define CH585_EEPROM_I2C_H

/*
 * HX24LC16B (24LC16-class, 2KB) external I2C EEPROM on the left-half
 * CH585 (U2), SDA=PB20 / SCL=PB21. Used to persist BLE pairing (SNV)
 * data off the internal Data-Flash.
 *
 * Bit-banged bus: the same GPIO timing already proven by
 * hw_tests/ch585/src/ch585_soft_i2c.c on this board. 24LC16 block
 * addressing puts A10..A8 in the device address (0x50 | block);
 * writes go in 16-byte pages followed by ack polling.
 */

#include <stdint.h>

#define CH585_EEPROM_I2C_SIZE      2048U
#define CH585_EEPROM_I2C_PAGE_SIZE 16U

void ch585_eeprom_i2c_init(void);
uint8_t ch585_eeprom_i2c_probe(void);
uint8_t ch585_eeprom_i2c_read(uint16_t addr, uint8_t *buf, uint16_t len);
uint8_t ch585_eeprom_i2c_write(uint16_t addr, const uint8_t *buf,
                               uint16_t len);

#endif /* CH585_EEPROM_I2C_H */
