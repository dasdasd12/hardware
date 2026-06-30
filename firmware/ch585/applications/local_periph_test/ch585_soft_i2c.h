#ifndef CH585_SOFT_I2C_H__
#define CH585_SOFT_I2C_H__

#include <stdint.h>

void ch585_soft_i2c_init(void);
int ch585_soft_i2c_probe_cb(uint8_t addr7, void *user);
int ch585_soft_i2c_write_cb(uint8_t addr7,
                            const uint8_t *data,
                            uint16_t len,
                            void *user);
int ch585_soft_i2c_read_cb(uint8_t addr7,
                           uint8_t *data,
                           uint16_t len,
                           void *user);
int ch585_soft_i2c_write_read_cb(uint8_t addr7,
                                 const uint8_t *wdata,
                                 uint16_t wlen,
                                 uint8_t *rdata,
                                 uint16_t rlen,
                                 void *user);
void ch585_soft_i2c_delay_ms_cb(uint32_t ms, void *user);

#endif /* CH585_SOFT_I2C_H__ */
