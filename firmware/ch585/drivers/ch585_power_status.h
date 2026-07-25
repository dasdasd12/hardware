/*
 * U3 battery and charge-status normalization.
 *
 * MAX17048 is read over I2C. HE3342 charge pins are plain GPIOs and are
 * normalized here so the rest of the firmware does not depend on pin polarity.
 */

#ifndef CH585_POWER_STATUS_H__
#define CH585_POWER_STATUS_H__

#include <stdint.h>

#include "max17048.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    CH585_POWER_CHARGE_UNKNOWN = 0,
    CH585_POWER_CHARGE_DISCHARGING = 1,
    CH585_POWER_CHARGE_CHARGING = 2,
    CH585_POWER_CHARGE_STANDBY_FULL = 3,
    CH585_POWER_CHARGE_FAULT = 4
} ch585_power_charge_state_t;

#define CH585_POWER_FLAG_BAT_VALID       (1U << 0)
#define CH585_POWER_FLAG_BAT_ALERT_PIN   (1U << 1)
#define CH585_POWER_FLAG_BAT_LOW_SOC     (1U << 2)
#define CH585_POWER_FLAG_BAT_LOW_VOLTAGE (1U << 3)
#define CH585_POWER_FLAG_CHARGE_ACTIVE   (1U << 4)
#define CH585_POWER_FLAG_STANDBY_ACTIVE  (1U << 5)

typedef struct
{
    uint8_t charge_active_level;
    uint8_t standby_active_level;
    uint16_t low_soc_q8_percent;
    uint16_t low_voltage_mv;
} ch585_power_status_config_t;

typedef struct
{
    uint16_t vbat_mv;
    uint16_t soc_q8_percent;
    uint8_t charge_state;
    uint8_t flags;
    uint16_t max17048_status;
} ch585_power_status_t;

void ch585_power_status_default_config(ch585_power_status_config_t *cfg);
void ch585_power_status_from_sample(const ch585_power_status_config_t *cfg,
                                    const max17048_sample_t *sample,
                                    uint8_t charge_pin_level,
                                    uint8_t standby_pin_level,
                                    ch585_power_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* CH585_POWER_STATUS_H__ */
