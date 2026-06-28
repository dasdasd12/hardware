/*
 * Small I2C access layer for CH585-side local peripherals.
 *
 * This module intentionally keeps the MCU-specific I2C implementation behind
 * callbacks. It can be backed by WCH hardware I2C, by the existing soft-I2C
 * bring-up code, or by a test double on PC.
 */

#ifndef CH585_I2C_BUS_H__
#define CH585_I2C_BUS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    CH585_I2C_STATUS_OK = 0,
    CH585_I2C_STATUS_PARAM = -1,
    CH585_I2C_STATUS_IO = -2,
    CH585_I2C_STATUS_TIMEOUT = -3,
    CH585_I2C_STATUS_RANGE = -4
} ch585_i2c_status_t;

typedef int (*ch585_i2c_probe_fn)(uint8_t addr7, void *user);
typedef int (*ch585_i2c_write_fn)(uint8_t addr7,
                                  const uint8_t *data,
                                  uint16_t len,
                                  void *user);
typedef int (*ch585_i2c_read_fn)(uint8_t addr7,
                                 uint8_t *data,
                                 uint16_t len,
                                 void *user);
typedef int (*ch585_i2c_write_read_fn)(uint8_t addr7,
                                       const uint8_t *wdata,
                                       uint16_t wlen,
                                       uint8_t *rdata,
                                       uint16_t rlen,
                                       void *user);
typedef void (*ch585_i2c_delay_ms_fn)(uint32_t ms, void *user);

typedef struct
{
    ch585_i2c_probe_fn probe;
    ch585_i2c_write_fn write;
    ch585_i2c_read_fn read;
    ch585_i2c_write_read_fn write_read;
    ch585_i2c_delay_ms_fn delay_ms;
    void *user;
} ch585_i2c_bus_t;

int ch585_i2c_bus_init(ch585_i2c_bus_t *bus,
                       ch585_i2c_probe_fn probe,
                       ch585_i2c_write_fn write,
                       ch585_i2c_read_fn read,
                       ch585_i2c_write_read_fn write_read,
                       ch585_i2c_delay_ms_fn delay_ms,
                       void *user);

int ch585_i2c_bus_probe(const ch585_i2c_bus_t *bus, uint8_t addr7);
int ch585_i2c_bus_write(const ch585_i2c_bus_t *bus,
                        uint8_t addr7,
                        const uint8_t *data,
                        uint16_t len);
int ch585_i2c_bus_read(const ch585_i2c_bus_t *bus,
                       uint8_t addr7,
                       uint8_t *data,
                       uint16_t len);
int ch585_i2c_bus_write_read(const ch585_i2c_bus_t *bus,
                             uint8_t addr7,
                             const uint8_t *wdata,
                             uint16_t wlen,
                             uint8_t *rdata,
                             uint16_t rlen);
void ch585_i2c_bus_delay_ms(const ch585_i2c_bus_t *bus, uint32_t ms);

int ch585_i2c_read_reg8(const ch585_i2c_bus_t *bus,
                        uint8_t addr7,
                        uint8_t reg,
                        uint8_t *value);
int ch585_i2c_write_reg8(const ch585_i2c_bus_t *bus,
                         uint8_t addr7,
                         uint8_t reg,
                         uint8_t value);
int ch585_i2c_read_reg16_be(const ch585_i2c_bus_t *bus,
                            uint8_t addr7,
                            uint8_t reg,
                            uint16_t *value);
int ch585_i2c_write_reg16_be(const ch585_i2c_bus_t *bus,
                             uint8_t addr7,
                             uint8_t reg,
                             uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* CH585_I2C_BUS_H__ */
