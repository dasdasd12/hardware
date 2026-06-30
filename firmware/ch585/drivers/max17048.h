/*
 * MAX17048 single-cell fuel-gauge driver.
 */

#ifndef MAX17048_H__
#define MAX17048_H__

#include <stdint.h>

#include "ch585_i2c_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX17048_ADDR7              0x36U

#define MAX17048_REG_VCELL          0x02U
#define MAX17048_REG_SOC            0x04U
#define MAX17048_REG_MODE           0x06U
#define MAX17048_REG_VERSION        0x08U
#define MAX17048_REG_HIBRT          0x0AU
#define MAX17048_REG_CONFIG         0x0CU
#define MAX17048_REG_VALRT          0x14U
#define MAX17048_REG_CRATE          0x16U
#define MAX17048_REG_VRESET_ID      0x18U
#define MAX17048_REG_STATUS         0x1AU
#define MAX17048_REG_COMMAND        0xFEU

#define MAX17048_MODE_QUICK_START   0x4000U
#define MAX17048_COMMAND_POR        0x5400U

typedef enum
{
    MAX17048_STATUS_OK = 0,
    MAX17048_STATUS_PARAM = -1,
    MAX17048_STATUS_IO = -2
} max17048_status_t;

typedef struct
{
    const ch585_i2c_bus_t *bus;
    uint8_t addr7;
} max17048_config_t;

typedef struct
{
    uint16_t vcell_raw;
    uint16_t soc_raw;
    uint16_t version;
    uint16_t config;
    uint16_t status;
    uint32_t vcell_uv;
    uint16_t soc_q8_percent;
    uint8_t alert_pin_low;
} max17048_sample_t;

typedef struct
{
    max17048_config_t cfg;
    uint32_t reads;
    uint32_t io_errors;
    uint8_t initialized;
} max17048_t;

void max17048_default_config(max17048_config_t *cfg);
int max17048_init(max17048_t *dev, const max17048_config_t *cfg);
int max17048_probe(const max17048_t *dev);
int max17048_read_sample(max17048_t *dev,
                         uint8_t alert_pin_low,
                         max17048_sample_t *sample);
int max17048_quick_start(max17048_t *dev);
int max17048_power_on_reset(max17048_t *dev);
uint32_t max17048_vcell_raw_to_uv(uint16_t raw);
uint16_t max17048_soc_raw_to_q8_percent(uint16_t raw);

#ifdef __cplusplus
}
#endif

#endif /* MAX17048_H__ */
