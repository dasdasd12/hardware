#include "ch585_common.h"
#include "ch585_soft_i2c.h"

#define I2C_SDA_PIN GPIO_Pin_20
#define I2C_SCL_PIN GPIO_Pin_21

static void i2c_wait(void)
{
    ch585_delay_cycles(80u);
}

static void sda_release(void)
{
    GPIOB_ModeCfg(I2C_SDA_PIN, GPIO_ModeIN_PU);
}

static void scl_release(void)
{
    GPIOB_ModeCfg(I2C_SCL_PIN, GPIO_ModeIN_PU);
}

static void sda_low(void)
{
    GPIOB_ResetBits(I2C_SDA_PIN);
    GPIOB_ModeCfg(I2C_SDA_PIN, GPIO_ModeOut_PP_5mA);
}

static void scl_low(void)
{
    GPIOB_ResetBits(I2C_SCL_PIN);
    GPIOB_ModeCfg(I2C_SCL_PIN, GPIO_ModeOut_PP_5mA);
}

static uint8_t sda_read(void)
{
    return GPIOB_ReadPortPin(I2C_SDA_PIN) ? 1u : 0u;
}

static void i2c_start(void)
{
    sda_release();
    scl_release();
    i2c_wait();
    sda_low();
    i2c_wait();
    scl_low();
}

static void i2c_stop(void)
{
    sda_low();
    i2c_wait();
    scl_release();
    i2c_wait();
    sda_release();
    i2c_wait();
}

static uint8_t i2c_write_byte(uint8_t value)
{
    uint8_t mask;
    uint8_t ack;

    for(mask = 0x80u; mask != 0u; mask >>= 1)
    {
        if(value & mask)
        {
            sda_release();
        }
        else
        {
            sda_low();
        }
        i2c_wait();
        scl_release();
        i2c_wait();
        scl_low();
    }

    sda_release();
    i2c_wait();
    scl_release();
    i2c_wait();
    ack = (uint8_t)(sda_read() == 0u);
    scl_low();
    return ack;
}

static uint8_t i2c_read_byte(uint8_t ack)
{
    uint8_t value = 0;
    uint8_t i;

    sda_release();
    for(i = 0; i < 8u; ++i)
    {
        value <<= 1;
        scl_release();
        i2c_wait();
        if(sda_read())
        {
            value |= 1u;
        }
        scl_low();
        i2c_wait();
    }

    if(ack)
    {
        sda_low();
    }
    else
    {
        sda_release();
    }
    i2c_wait();
    scl_release();
    i2c_wait();
    scl_low();
    sda_release();
    return value;
}

void ch585_soft_i2c_init(void)
{
    sda_release();
    scl_release();
    i2c_wait();
}

uint8_t ch585_soft_i2c_probe(uint8_t addr7)
{
    uint8_t ok;
    i2c_start();
    ok = i2c_write_byte((uint8_t)(addr7 << 1));
    i2c_stop();
    return ok;
}

uint8_t ch585_soft_i2c_read_u8(uint8_t addr7, uint8_t reg, uint8_t *value)
{
    i2c_start();
    if(!i2c_write_byte((uint8_t)(addr7 << 1)))
    {
        i2c_stop();
        return 0;
    }
    if(!i2c_write_byte(reg))
    {
        i2c_stop();
        return 0;
    }
    i2c_start();
    if(!i2c_write_byte((uint8_t)((addr7 << 1) | 1u)))
    {
        i2c_stop();
        return 0;
    }
    *value = i2c_read_byte(0);
    i2c_stop();
    return 1;
}

uint8_t ch585_soft_i2c_read_u16_be(uint8_t addr7, uint8_t reg, uint16_t *value)
{
    uint8_t hi;
    uint8_t lo;

    i2c_start();
    if(!i2c_write_byte((uint8_t)(addr7 << 1)))
    {
        i2c_stop();
        return 0;
    }
    if(!i2c_write_byte(reg))
    {
        i2c_stop();
        return 0;
    }
    i2c_start();
    if(!i2c_write_byte((uint8_t)((addr7 << 1) | 1u)))
    {
        i2c_stop();
        return 0;
    }
    hi = i2c_read_byte(1);
    lo = i2c_read_byte(0);
    i2c_stop();
    *value = (uint16_t)(((uint16_t)hi << 8) | lo);
    return 1;
}

uint8_t ch585_soft_i2c_write_u8(uint8_t addr7, uint8_t reg, uint8_t value)
{
    i2c_start();
    if(!i2c_write_byte((uint8_t)(addr7 << 1)))
    {
        i2c_stop();
        return 0;
    }
    if(!i2c_write_byte(reg))
    {
        i2c_stop();
        return 0;
    }
    if(!i2c_write_byte(value))
    {
        i2c_stop();
        return 0;
    }
    i2c_stop();
    return 1;
}
