#include "hx24lc16.h"

#include <stddef.h>
#include <string.h>

static uint8_t hx24lc16_addr7(const hx24lc16_t *dev, uint16_t mem_addr)
{
    return (uint8_t)(dev->cfg.base_addr7 | ((mem_addr >> 8U) & 0x07U));
}

static uint16_t min_u16(uint16_t a, uint16_t b)
{
    return (a < b) ? a : b;
}

void hx24lc16_default_config(hx24lc16_config_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->base_addr7 = HX24LC16_BASE_ADDR7;
    cfg->poll_tries = HX24LC16_DEFAULT_POLL_TRIES;
}

int hx24lc16_init(hx24lc16_t *dev, const hx24lc16_config_t *cfg)
{
    if ((dev == NULL) || (cfg == NULL) || (cfg->bus == NULL) ||
        (cfg->base_addr7 > 0x77U))
    {
        return HX24LC16_STATUS_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->cfg = *cfg;
    if (dev->cfg.poll_tries == 0U)
    {
        dev->cfg.poll_tries = HX24LC16_DEFAULT_POLL_TRIES;
    }
    dev->initialized = 1U;
    return HX24LC16_STATUS_OK;
}

int hx24lc16_probe(const hx24lc16_t *dev)
{
    if ((dev == NULL) || (dev->initialized == 0U))
    {
        return HX24LC16_STATUS_PARAM;
    }

    return (ch585_i2c_bus_probe(dev->cfg.bus, dev->cfg.base_addr7) ==
            CH585_I2C_STATUS_OK)
               ? HX24LC16_STATUS_OK
               : HX24LC16_STATUS_IO;
}

int hx24lc16_read(hx24lc16_t *dev,
                  uint16_t mem_addr,
                  uint8_t *data,
                  uint16_t len)
{
    uint16_t done = 0U;

    if ((dev == NULL) || (dev->initialized == 0U) ||
        ((data == NULL) && (len != 0U)))
    {
        return HX24LC16_STATUS_PARAM;
    }
    if (((uint32_t)mem_addr + len) > HX24LC16_SIZE_BYTES)
    {
        return HX24LC16_STATUS_RANGE;
    }

    while (done < len)
    {
        uint16_t addr = (uint16_t)(mem_addr + done);
        uint16_t block_left = (uint16_t)(256U - (addr & 0x00FFU));
        uint16_t chunk = min_u16((uint16_t)(len - done), block_left);
        uint8_t word_addr = (uint8_t)addr;
        int rc;

        rc = ch585_i2c_bus_write_read(dev->cfg.bus, hx24lc16_addr7(dev, addr),
                                      &word_addr, 1U, &data[done], chunk);
        if (rc != CH585_I2C_STATUS_OK)
        {
            dev->io_errors++;
            return HX24LC16_STATUS_IO;
        }

        done = (uint16_t)(done + chunk);
    }

    dev->reads++;
    return HX24LC16_STATUS_OK;
}

int hx24lc16_write(hx24lc16_t *dev,
                   uint16_t mem_addr,
                   const uint8_t *data,
                   uint16_t len)
{
    uint16_t done = 0U;

    if ((dev == NULL) || (dev->initialized == 0U) ||
        ((data == NULL) && (len != 0U)))
    {
        return HX24LC16_STATUS_PARAM;
    }
    if (((uint32_t)mem_addr + len) > HX24LC16_SIZE_BYTES)
    {
        return HX24LC16_STATUS_RANGE;
    }

    while (done < len)
    {
        uint8_t tx[1U + HX24LC16_PAGE_SIZE];
        uint16_t addr = (uint16_t)(mem_addr + done);
        uint16_t page_left = (uint16_t)(HX24LC16_PAGE_SIZE -
                                        (addr & (HX24LC16_PAGE_SIZE - 1U)));
        uint16_t block_left = (uint16_t)(256U - (addr & 0x00FFU));
        uint16_t chunk = min_u16((uint16_t)(len - done),
                                 min_u16(page_left, block_left));
        uint8_t try_count;
        int rc;

        tx[0] = (uint8_t)addr;
        memcpy(&tx[1], &data[done], chunk);

        rc = ch585_i2c_bus_write(dev->cfg.bus, hx24lc16_addr7(dev, addr), tx,
                                 (uint16_t)(chunk + 1U));
        if (rc != CH585_I2C_STATUS_OK)
        {
            dev->io_errors++;
            return HX24LC16_STATUS_IO;
        }

        for (try_count = 0U; try_count < dev->cfg.poll_tries; try_count++)
        {
            ch585_i2c_bus_delay_ms(dev->cfg.bus, 1U);
            if (ch585_i2c_bus_probe(dev->cfg.bus,
                                    hx24lc16_addr7(dev, addr)) ==
                CH585_I2C_STATUS_OK)
            {
                break;
            }
        }
        if (try_count == dev->cfg.poll_tries)
        {
            dev->io_errors++;
            return HX24LC16_STATUS_TIMEOUT;
        }

        done = (uint16_t)(done + chunk);
    }

    dev->writes++;
    return HX24LC16_STATUS_OK;
}
