#include "ch585_soft_i2c_bus.h"

#include "CH58x_common.h"

#define CH585_SOFT_I2C_SDA_PIN GPIO_Pin_20
#define CH585_SOFT_I2C_SCL_PIN GPIO_Pin_21

static void i2c_wait(void)
{
    /* Match the MAX17048 bus timing already verified by hw_tests/ch585.
     * The production CH585 runs at 78 MHz, so the shorter delay used by the
     * first integration draft could push this software bus beyond Fast-mode. */
    volatile uint32_t count = 80U;

    while(count != 0U)
    {
        count--;
    }
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

    for(mask = 0x80U; mask != 0U; mask >>= 1)
    {
        if((value & mask) != 0U)
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
    ack = (uint8_t)(sda_read() == 0U);
    scl_low();
    return ack;
}

static uint8_t i2c_read_byte(uint8_t ack)
{
    uint8_t value = 0U;
    uint8_t i;

    sda_release();
    for(i = 0U; i < 8U; i++)
    {
        value = (uint8_t)(value << 1);
        scl_release();
        i2c_wait();
        if(sda_read() != 0U)
        {
            value |= 1U;
        }
        scl_low();
        i2c_wait();
    }

    if(ack != 0U)
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

static int soft_i2c_probe(uint8_t addr7, void *user)
{
    uint8_t ack;

    (void)user;
    i2c_start();
    ack = i2c_write_byte((uint8_t)(addr7 << 1));
    i2c_stop();
    return (int)ack;
}

static int soft_i2c_write(uint8_t addr7,
                          const uint8_t *data,
                          uint16_t len,
                          void *user)
{
    uint16_t i;

    (void)user;
    i2c_start();
    if(i2c_write_byte((uint8_t)(addr7 << 1)) == 0U)
    {
        i2c_stop();
        return 0;
    }
    for(i = 0U; i < len; i++)
    {
        if(i2c_write_byte(data[i]) == 0U)
        {
            i2c_stop();
            return 0;
        }
    }
    i2c_stop();
    return 1;
}

static int soft_i2c_read(uint8_t addr7,
                         uint8_t *data,
                         uint16_t len,
                         void *user)
{
    uint16_t i;

    (void)user;
    i2c_start();
    if(i2c_write_byte((uint8_t)((addr7 << 1) | 1U)) == 0U)
    {
        i2c_stop();
        return 0;
    }
    for(i = 0U; i < len; i++)
    {
        data[i] = i2c_read_byte((uint8_t)((i + 1U) < len));
    }
    i2c_stop();
    return 1;
}

static int soft_i2c_write_read(uint8_t addr7,
                               const uint8_t *wdata,
                               uint16_t wlen,
                               uint8_t *rdata,
                               uint16_t rlen,
                               void *user)
{
    uint16_t i;

    (void)user;
    i2c_start();
    if(i2c_write_byte((uint8_t)(addr7 << 1)) == 0U)
    {
        i2c_stop();
        return 0;
    }
    for(i = 0U; i < wlen; i++)
    {
        if(i2c_write_byte(wdata[i]) == 0U)
        {
            i2c_stop();
            return 0;
        }
    }

    i2c_start();
    if(i2c_write_byte((uint8_t)((addr7 << 1) | 1U)) == 0U)
    {
        i2c_stop();
        return 0;
    }
    for(i = 0U; i < rlen; i++)
    {
        rdata[i] = i2c_read_byte((uint8_t)((i + 1U) < rlen));
    }
    i2c_stop();
    return 1;
}

int ch585_soft_i2c_bus_init(ch585_i2c_bus_t *bus)
{
    GPIOBDigitalCfg(ENABLE,
                    CH585_SOFT_I2C_SDA_PIN | CH585_SOFT_I2C_SCL_PIN);
    sda_release();
    scl_release();
    i2c_wait();
    return ch585_i2c_bus_init(bus,
                              soft_i2c_probe,
                              soft_i2c_write,
                              soft_i2c_read,
                              soft_i2c_write_read,
                              0,
                              0);
}
