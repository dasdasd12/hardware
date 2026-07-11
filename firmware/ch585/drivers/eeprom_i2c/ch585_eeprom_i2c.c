#include "ch585_eeprom_i2c.h"

#include "CH58x_common.h"

#define I2C_SDA_PIN GPIO_Pin_20
#define I2C_SCL_PIN GPIO_Pin_21

#define EEPROM_DEV_BASE 0x50U
#define ACK_POLL_MAX    400U

static void i2c_wait(void)
{
    volatile uint32_t n = 30U;

    while(n != 0U)
    {
        n--;
    }
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
    return GPIOB_ReadPortPin(I2C_SDA_PIN) ? 1U : 0U;
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

static uint8_t dev_addr_for(uint16_t addr)
{
    return (uint8_t)(EEPROM_DEV_BASE | ((addr >> 8) & 0x07U));
}

static uint8_t eeprom_select(uint16_t addr)
{
    i2c_start();
    if(i2c_write_byte((uint8_t)(dev_addr_for(addr) << 1)) == 0U)
    {
        i2c_stop();
        return 0U;
    }
    if(i2c_write_byte((uint8_t)(addr & 0xFFU)) == 0U)
    {
        i2c_stop();
        return 0U;
    }
    return 1U;
}

static uint8_t eeprom_ack_poll(uint16_t addr)
{
    uint16_t attempt;

    for(attempt = 0U; attempt < ACK_POLL_MAX; attempt++)
    {
        uint8_t ack;

        i2c_start();
        ack = i2c_write_byte((uint8_t)(dev_addr_for(addr) << 1));
        i2c_stop();
        if(ack != 0U)
        {
            return 1U;
        }
    }
    return 0U;
}

void ch585_eeprom_i2c_init(void)
{
    sda_release();
    scl_release();
    i2c_wait();
}

uint8_t ch585_eeprom_i2c_probe(void)
{
    uint8_t ack;

    i2c_start();
    ack = i2c_write_byte((uint8_t)(EEPROM_DEV_BASE << 1));
    i2c_stop();
    return ack;
}

uint8_t ch585_eeprom_i2c_read(uint16_t addr, uint8_t *buf, uint16_t len)
{
    if((buf == 0) || ((uint32_t)addr + len > CH585_EEPROM_I2C_SIZE))
    {
        return 0U;
    }

    while(len != 0U)
    {
        /* Restart at 256B block boundaries: the block index lives in
         * the device address bits. */
        uint16_t block_remain = (uint16_t)(0x100U - (addr & 0xFFU));
        uint16_t take = (len < block_remain) ? len : block_remain;
        uint16_t i;

        if(eeprom_select(addr) == 0U)
        {
            i2c_stop();
            return 0U;
        }
        i2c_start();
        if(i2c_write_byte((uint8_t)((dev_addr_for(addr) << 1) | 1U)) == 0U)
        {
            i2c_stop();
            return 0U;
        }
        for(i = 0U; i < take; i++)
        {
            buf[i] = i2c_read_byte((uint8_t)((i + 1U) < take));
        }
        i2c_stop();

        buf += take;
        addr = (uint16_t)(addr + take);
        len = (uint16_t)(len - take);
    }
    return 1U;
}

uint8_t ch585_eeprom_i2c_write(uint16_t addr, const uint8_t *buf,
                               uint16_t len)
{
    if((buf == 0) || ((uint32_t)addr + len > CH585_EEPROM_I2C_SIZE))
    {
        return 0U;
    }

    while(len != 0U)
    {
        uint16_t page_remain =
            (uint16_t)(CH585_EEPROM_I2C_PAGE_SIZE -
                       (addr % CH585_EEPROM_I2C_PAGE_SIZE));
        uint16_t take = (len < page_remain) ? len : page_remain;
        uint16_t i;

        if(eeprom_select(addr) == 0U)
        {
            i2c_stop();
            return 0U;
        }
        for(i = 0U; i < take; i++)
        {
            if(i2c_write_byte(buf[i]) == 0U)
            {
                i2c_stop();
                return 0U;
            }
        }
        i2c_stop();

        if(eeprom_ack_poll(addr) == 0U)
        {
            return 0U;
        }

        buf += take;
        addr = (uint16_t)(addr + take);
        len = (uint16_t)(len - take);
    }
    return 1U;
}
