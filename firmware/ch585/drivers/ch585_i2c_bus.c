#include "ch585_i2c_bus.h"

#include <stddef.h>
#include <string.h>

int ch585_i2c_bus_init(ch585_i2c_bus_t *bus,
                       ch585_i2c_probe_fn probe,
                       ch585_i2c_write_fn write,
                       ch585_i2c_read_fn read,
                       ch585_i2c_write_read_fn write_read,
                       ch585_i2c_delay_ms_fn delay_ms,
                       void *user)
{
    if (bus == NULL)
    {
        return CH585_I2C_STATUS_PARAM;
    }

    memset(bus, 0, sizeof(*bus));
    bus->probe = probe;
    bus->write = write;
    bus->read = read;
    bus->write_read = write_read;
    bus->delay_ms = delay_ms;
    bus->user = user;

    if ((bus->write == NULL) && (bus->write_read == NULL) &&
        (bus->probe == NULL))
    {
        return CH585_I2C_STATUS_PARAM;
    }

    return CH585_I2C_STATUS_OK;
}

int ch585_i2c_bus_probe(const ch585_i2c_bus_t *bus, uint8_t addr7)
{
    if ((bus == NULL) || (addr7 > 0x7FU))
    {
        return CH585_I2C_STATUS_PARAM;
    }

    if (bus->probe != NULL)
    {
        return (bus->probe(addr7, bus->user) == 0) ? CH585_I2C_STATUS_IO
                                                   : CH585_I2C_STATUS_OK;
    }

    if (bus->write != NULL)
    {
        return (bus->write(addr7, NULL, 0U, bus->user) == 0)
                   ? CH585_I2C_STATUS_IO
                   : CH585_I2C_STATUS_OK;
    }

    return CH585_I2C_STATUS_PARAM;
}

int ch585_i2c_bus_write(const ch585_i2c_bus_t *bus,
                        uint8_t addr7,
                        const uint8_t *data,
                        uint16_t len)
{
    if ((bus == NULL) || (addr7 > 0x7FU) ||
        ((data == NULL) && (len != 0U)) || (bus->write == NULL))
    {
        return CH585_I2C_STATUS_PARAM;
    }

    return (bus->write(addr7, data, len, bus->user) == 0)
               ? CH585_I2C_STATUS_IO
               : CH585_I2C_STATUS_OK;
}

int ch585_i2c_bus_read(const ch585_i2c_bus_t *bus,
                       uint8_t addr7,
                       uint8_t *data,
                       uint16_t len)
{
    if ((bus == NULL) || (addr7 > 0x7FU) ||
        ((data == NULL) && (len != 0U)) || (bus->read == NULL))
    {
        return CH585_I2C_STATUS_PARAM;
    }

    return (bus->read(addr7, data, len, bus->user) == 0)
               ? CH585_I2C_STATUS_IO
               : CH585_I2C_STATUS_OK;
}

int ch585_i2c_bus_write_read(const ch585_i2c_bus_t *bus,
                             uint8_t addr7,
                             const uint8_t *wdata,
                             uint16_t wlen,
                             uint8_t *rdata,
                             uint16_t rlen)
{
    if ((bus == NULL) || (addr7 > 0x7FU) ||
        ((wdata == NULL) && (wlen != 0U)) ||
        ((rdata == NULL) && (rlen != 0U)) ||
        (bus->write_read == NULL))
    {
        return CH585_I2C_STATUS_PARAM;
    }

    return (bus->write_read(addr7, wdata, wlen, rdata, rlen, bus->user) == 0)
               ? CH585_I2C_STATUS_IO
               : CH585_I2C_STATUS_OK;
}

void ch585_i2c_bus_delay_ms(const ch585_i2c_bus_t *bus, uint32_t ms)
{
    if ((bus != NULL) && (bus->delay_ms != NULL) && (ms != 0U))
    {
        bus->delay_ms(ms, bus->user);
    }
}

int ch585_i2c_read_reg8(const ch585_i2c_bus_t *bus,
                        uint8_t addr7,
                        uint8_t reg,
                        uint8_t *value)
{
    if (value == NULL)
    {
        return CH585_I2C_STATUS_PARAM;
    }

    return ch585_i2c_bus_write_read(bus, addr7, &reg, 1U, value, 1U);
}

int ch585_i2c_write_reg8(const ch585_i2c_bus_t *bus,
                         uint8_t addr7,
                         uint8_t reg,
                         uint8_t value)
{
    uint8_t tx[2];

    tx[0] = reg;
    tx[1] = value;
    return ch585_i2c_bus_write(bus, addr7, tx, (uint16_t)sizeof(tx));
}

int ch585_i2c_read_reg16_be(const ch585_i2c_bus_t *bus,
                            uint8_t addr7,
                            uint8_t reg,
                            uint16_t *value)
{
    uint8_t rx[2];
    int rc;

    if (value == NULL)
    {
        return CH585_I2C_STATUS_PARAM;
    }

    rc = ch585_i2c_bus_write_read(bus, addr7, &reg, 1U, rx,
                                  (uint16_t)sizeof(rx));
    if (rc != CH585_I2C_STATUS_OK)
    {
        return rc;
    }

    *value = (uint16_t)(((uint16_t)rx[0] << 8U) | rx[1]);
    return CH585_I2C_STATUS_OK;
}

int ch585_i2c_write_reg16_be(const ch585_i2c_bus_t *bus,
                             uint8_t addr7,
                             uint8_t reg,
                             uint16_t value)
{
    uint8_t tx[3];

    tx[0] = reg;
    tx[1] = (uint8_t)(value >> 8U);
    tx[2] = (uint8_t)value;
    return ch585_i2c_bus_write(bus, addr7, tx, (uint16_t)sizeof(tx));
}
