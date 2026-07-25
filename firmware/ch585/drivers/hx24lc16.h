/*
 * HX24LC16B 16-Kbit EEPROM driver.
 *
 * The 24LC16 family uses the upper three memory-address bits in the 7-bit
 * I2C device address: 0x50 | A10:A8. The byte address on the wire is A7:A0.
 */

#ifndef HX24LC16_H__
#define HX24LC16_H__

#include <stdint.h>

#include "ch585_i2c_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HX24LC16_BASE_ADDR7          0x50U
#define HX24LC16_SIZE_BYTES          2048U
#define HX24LC16_PAGE_SIZE           16U
#define HX24LC16_WRITE_CYCLE_MS      5U
#define HX24LC16_DEFAULT_POLL_TRIES  8U

typedef enum
{
    HX24LC16_STATUS_OK = 0,
    HX24LC16_STATUS_PARAM = -1,
    HX24LC16_STATUS_IO = -2,
    HX24LC16_STATUS_RANGE = -3,
    HX24LC16_STATUS_TIMEOUT = -4
} hx24lc16_status_t;

typedef struct
{
    const ch585_i2c_bus_t *bus;
    uint8_t base_addr7;
    uint8_t poll_tries;
} hx24lc16_config_t;

typedef struct
{
    hx24lc16_config_t cfg;
    uint32_t reads;
    uint32_t writes;
    uint32_t io_errors;
    uint8_t initialized;
} hx24lc16_t;

void hx24lc16_default_config(hx24lc16_config_t *cfg);
int hx24lc16_init(hx24lc16_t *dev, const hx24lc16_config_t *cfg);
int hx24lc16_probe(const hx24lc16_t *dev);
int hx24lc16_read(hx24lc16_t *dev,
                  uint16_t mem_addr,
                  uint8_t *data,
                  uint16_t len);
int hx24lc16_write(hx24lc16_t *dev,
                   uint16_t mem_addr,
                   const uint8_t *data,
                   uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* HX24LC16_H__ */
