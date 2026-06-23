#ifndef CH585_SOFT_I2C_H
#define CH585_SOFT_I2C_H

#include <stdint.h>

void ch585_soft_i2c_init(void);
uint8_t ch585_soft_i2c_probe(uint8_t addr7);
uint8_t ch585_soft_i2c_read_u8(uint8_t addr7, uint8_t reg, uint8_t *value);
uint8_t ch585_soft_i2c_read_u16_be(uint8_t addr7, uint8_t reg, uint16_t *value);
uint8_t ch585_soft_i2c_write_u8(uint8_t addr7, uint8_t reg, uint8_t value);

#endif
