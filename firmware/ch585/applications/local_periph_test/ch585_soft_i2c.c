#include "ch585_soft_i2c.h"

#include <stddef.h>

#include "CH58x_common.h"

#define CH585_SOFT_I2C_SDA_PIN GPIO_Pin_20
#define CH585_SOFT_I2C_SCL_PIN GPIO_Pin_21

static void i2c_wait(void)
{
    mDelayuS(2U);
}

static void sda_release(void)
{
    GPIOB_ModeCfg(CH585_SOFT_I2C_SDA_PIN, GPIO_ModeIN_PU);
}

static void scl_release(void)
{
    GPIOB_ModeCfg(CH585_SOFT_I2C_SCL_PIN, GPIO_ModeIN_PU);
}

static void sda_low(void)
{
    GPIOB_ResetBits(CH585_SOFT_I2C_SDA_PIN);
    GPIOB_ModeCfg(CH585_SOFT_I2C_SDA_PIN, GPIO_ModeOut_PP_5mA);
}

static void scl_low(void)
{
    GPIOB_ResetBits(CH585_SOFT_I2C_SCL_PIN);
    GPIOB_ModeCfg(CH585_SOFT_I2C_SCL_PIN, GPIO_ModeOut_PP_5mA);
}

static uint8_t sda_read(void)
{
    return GPIOB_ReadPortPin(CH585_SOFT_I2C_SDA_PIN) ? 1U : 0U;
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

    for (mask = 0x80U; mask != 0U; mask >>= 1U)
    {
        if ((value & mask) != 0U)
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
    ack = (sda_read() == 0U) ? 1U : 0U;
    scl_low();
    return ack;
}

static uint8_t i2c_read_byte(uint8_t ack)
{
    uint8_t value = 0U;
    uint8_t i;

    sda_release();
    for (i = 0U; i < 8U; i++)
    {
        value <<= 1U;
        scl_release();
        i2c_wait();
        if (sda_read() != 0U)
        {
            value |= 1U;
        }
        scl_low();
        i2c_wait();
    }

    if (ack != 0U)
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

int ch585_soft_i2c_probe_cb(uint8_t addr7, void *user)
{
    uint8_t ok;

    (void)user;
    i2c_start();
    ok = i2c_write_byte((uint8_t)(addr7 << 1U));
    i2c_stop();
    return (ok != 0U) ? 1 : 0;
}

int ch585_soft_i2c_write_cb(uint8_t addr7,
                            const uint8_t *data,
                            uint16_t len,
                            void *user)
{
    uint16_t i;

    (void)user;
    if ((data == NULL) && (len != 0U))
    {
        return 0;
    }

    i2c_start();
    if (i2c_write_byte((uint8_t)(addr7 << 1U)) == 0U)
    {
        i2c_stop();
        return 0;
    }

    for (i = 0U; i < len; i++)
    {
        if (i2c_write_byte(data[i]) == 0U)
        {
            i2c_stop();
            return 0;
        }
    }

    i2c_stop();
    return 1;
}

int ch585_soft_i2c_read_cb(uint8_t addr7,
                           uint8_t *data,
                           uint16_t len,
                           void *user)
{
    uint16_t i;

    (void)user;
    if ((data == NULL) && (len != 0U))
    {
        return 0;
    }

    i2c_start();
    if (i2c_write_byte((uint8_t)((addr7 << 1U) | 1U)) == 0U)
    {
        i2c_stop();
        return 0;
    }

    for (i = 0U; i < len; i++)
    {
        data[i] = i2c_read_byte((i + 1U < len) ? 1U : 0U);
    }

    i2c_stop();
    return 1;
}

int ch585_soft_i2c_write_read_cb(uint8_t addr7,
                                 const uint8_t *wdata,
                                 uint16_t wlen,
                                 uint8_t *rdata,
                                 uint16_t rlen,
                                 void *user)
{
    uint16_t i;

    (void)user;
    if (((wdata == NULL) && (wlen != 0U)) ||
        ((rdata == NULL) && (rlen != 0U)))
    {
        return 0;
    }

    i2c_start();
    if (i2c_write_byte((uint8_t)(addr7 << 1U)) == 0U)
    {
        i2c_stop();
        return 0;
    }
    for (i = 0U; i < wlen; i++)
    {
        if (i2c_write_byte(wdata[i]) == 0U)
        {
            i2c_stop();
            return 0;
        }
    }

    if (rlen == 0U)
    {
        i2c_stop();
        return 1;
    }

    i2c_start();
    if (i2c_write_byte((uint8_t)((addr7 << 1U) | 1U)) == 0U)
    {
        i2c_stop();
        return 0;
    }
    for (i = 0U; i < rlen; i++)
    {
        rdata[i] = i2c_read_byte((i + 1U < rlen) ? 1U : 0U);
    }

    i2c_stop();
    return 1;
}

void ch585_soft_i2c_delay_ms_cb(uint32_t ms, void *user)
{
    (void)user;
    while (ms != 0U)
    {
        uint16_t chunk = (ms > 60000UL) ? 60000U : (uint16_t)ms;
        mDelaymS(chunk);
        ms -= chunk;
    }
}
