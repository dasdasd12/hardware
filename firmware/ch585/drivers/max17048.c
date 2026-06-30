#include "max17048.h"

#include <stddef.h>
#include <string.h>

void max17048_default_config(max17048_config_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->addr7 = MAX17048_ADDR7;
}

int max17048_init(max17048_t *dev, const max17048_config_t *cfg)
{
    if ((dev == NULL) || (cfg == NULL) || (cfg->bus == NULL) ||
        (cfg->addr7 > 0x7FU))
    {
        return MAX17048_STATUS_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->cfg = *cfg;
    dev->initialized = 1U;
    return MAX17048_STATUS_OK;
}

int max17048_probe(const max17048_t *dev)
{
    if ((dev == NULL) || (dev->initialized == 0U))
    {
        return MAX17048_STATUS_PARAM;
    }

    return (ch585_i2c_bus_probe(dev->cfg.bus, dev->cfg.addr7) ==
            CH585_I2C_STATUS_OK)
               ? MAX17048_STATUS_OK
               : MAX17048_STATUS_IO;
}

uint32_t max17048_vcell_raw_to_uv(uint16_t raw)
{
    /* Datasheet Table 2: VCELL LSb is 78.125 uV per cell. */
    return ((uint32_t)raw * 625UL) / 8UL;
}

uint16_t max17048_soc_raw_to_q8_percent(uint16_t raw)
{
    /* SOC is reported in 1/256 percent units. */
    return raw;
}

int max17048_read_sample(max17048_t *dev,
                         uint8_t alert_pin_low,
                         max17048_sample_t *sample)
{
    int rc;

    if ((dev == NULL) || (sample == NULL) || (dev->initialized == 0U))
    {
        return MAX17048_STATUS_PARAM;
    }

    memset(sample, 0, sizeof(*sample));

    rc = ch585_i2c_read_reg16_be(dev->cfg.bus, dev->cfg.addr7,
                                 MAX17048_REG_VCELL, &sample->vcell_raw);
    if (rc != CH585_I2C_STATUS_OK)
    {
        dev->io_errors++;
        return MAX17048_STATUS_IO;
    }
    rc = ch585_i2c_read_reg16_be(dev->cfg.bus, dev->cfg.addr7,
                                 MAX17048_REG_SOC, &sample->soc_raw);
    if (rc != CH585_I2C_STATUS_OK)
    {
        dev->io_errors++;
        return MAX17048_STATUS_IO;
    }
    rc = ch585_i2c_read_reg16_be(dev->cfg.bus, dev->cfg.addr7,
                                 MAX17048_REG_VERSION, &sample->version);
    if (rc != CH585_I2C_STATUS_OK)
    {
        dev->io_errors++;
        return MAX17048_STATUS_IO;
    }
    rc = ch585_i2c_read_reg16_be(dev->cfg.bus, dev->cfg.addr7,
                                 MAX17048_REG_CONFIG, &sample->config);
    if (rc != CH585_I2C_STATUS_OK)
    {
        dev->io_errors++;
        return MAX17048_STATUS_IO;
    }
    rc = ch585_i2c_read_reg16_be(dev->cfg.bus, dev->cfg.addr7,
                                 MAX17048_REG_STATUS, &sample->status);
    if (rc != CH585_I2C_STATUS_OK)
    {
        dev->io_errors++;
        return MAX17048_STATUS_IO;
    }

    sample->vcell_uv = max17048_vcell_raw_to_uv(sample->vcell_raw);
    sample->soc_q8_percent = max17048_soc_raw_to_q8_percent(sample->soc_raw);
    sample->alert_pin_low = alert_pin_low ? 1U : 0U;
    dev->reads++;
    return MAX17048_STATUS_OK;
}

int max17048_quick_start(max17048_t *dev)
{
    if ((dev == NULL) || (dev->initialized == 0U))
    {
        return MAX17048_STATUS_PARAM;
    }

    return (ch585_i2c_write_reg16_be(dev->cfg.bus, dev->cfg.addr7,
                                     MAX17048_REG_MODE,
                                     MAX17048_MODE_QUICK_START) ==
            CH585_I2C_STATUS_OK)
               ? MAX17048_STATUS_OK
               : MAX17048_STATUS_IO;
}

int max17048_power_on_reset(max17048_t *dev)
{
    if ((dev == NULL) || (dev->initialized == 0U))
    {
        return MAX17048_STATUS_PARAM;
    }

    return (ch585_i2c_write_reg16_be(dev->cfg.bus, dev->cfg.addr7,
                                     MAX17048_REG_COMMAND,
                                     MAX17048_COMMAND_POR) ==
            CH585_I2C_STATUS_OK)
               ? MAX17048_STATUS_OK
               : MAX17048_STATUS_IO;
}
