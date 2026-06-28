#include "ch585_power_status.h"

#include <stddef.h>
#include <string.h>

void ch585_power_status_default_config(ch585_power_status_config_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    /*
     * HE3342-style charge outputs are usually open-drain active-low. Confirm
     * the final polarity on PCB, then override if needed.
     */
    cfg->charge_active_level = 0U;
    cfg->standby_active_level = 0U;
    cfg->low_soc_q8_percent = (uint16_t)(10U * 256U);
    cfg->low_voltage_mv = 3300U;
}

void ch585_power_status_from_sample(const ch585_power_status_config_t *cfg,
                                    const max17048_sample_t *sample,
                                    uint8_t charge_pin_level,
                                    uint8_t standby_pin_level,
                                    ch585_power_status_t *status)
{
    ch585_power_status_config_t local_cfg;
    uint8_t charge_active;
    uint8_t standby_active;

    if (status == NULL)
    {
        return;
    }

    if (cfg == NULL)
    {
        ch585_power_status_default_config(&local_cfg);
        cfg = &local_cfg;
    }

    memset(status, 0, sizeof(*status));
    charge_active = ((charge_pin_level ? 1U : 0U) ==
                     (cfg->charge_active_level ? 1U : 0U))
                        ? 1U
                        : 0U;
    standby_active = ((standby_pin_level ? 1U : 0U) ==
                      (cfg->standby_active_level ? 1U : 0U))
                         ? 1U
                         : 0U;

    if (sample != NULL)
    {
        status->flags |= CH585_POWER_FLAG_BAT_VALID;
        status->vbat_mv = (uint16_t)((sample->vcell_uv + 500UL) / 1000UL);
        status->soc_q8_percent = sample->soc_q8_percent;
        status->max17048_status = sample->status;

        if (sample->alert_pin_low != 0U)
        {
            status->flags |= CH585_POWER_FLAG_BAT_ALERT_PIN;
        }
        if (sample->soc_q8_percent <= cfg->low_soc_q8_percent)
        {
            status->flags |= CH585_POWER_FLAG_BAT_LOW_SOC;
        }
        if (status->vbat_mv <= cfg->low_voltage_mv)
        {
            status->flags |= CH585_POWER_FLAG_BAT_LOW_VOLTAGE;
        }
    }

    if (charge_active != 0U)
    {
        status->flags |= CH585_POWER_FLAG_CHARGE_ACTIVE;
    }
    if (standby_active != 0U)
    {
        status->flags |= CH585_POWER_FLAG_STANDBY_ACTIVE;
    }

    if ((charge_active != 0U) && (standby_active != 0U))
    {
        status->charge_state = CH585_POWER_CHARGE_FAULT;
    }
    else if (charge_active != 0U)
    {
        status->charge_state = CH585_POWER_CHARGE_CHARGING;
    }
    else if (standby_active != 0U)
    {
        status->charge_state = CH585_POWER_CHARGE_STANDBY_FULL;
    }
    else
    {
        status->charge_state = CH585_POWER_CHARGE_DISCHARGING;
    }
}
