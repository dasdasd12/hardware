#include "ch585_ads7948_mux_acq.h"

#include "CH585SFR.h"
#include "core_riscv.h"
#include "CH58x_gpio.h"
#include "CH58x_spi.h"

#define CH585_ADS7948_MUX_TARGET_SPS 2000000U
#define CH585_ADS7948_MUX_SYSCLK_HZ 78000000U
#define CH585_ADS7948_MUX_SPI_CLOCK_DIV 2U
#define CH585_ADS7948_MUX_FRAME_CLOCKS 16U
#define CH585_ADS7948_MUX_FRAME_SPS \
    ((CH585_ADS7948_MUX_SYSCLK_HZ / CH585_ADS7948_MUX_SPI_CLOCK_DIV) / \
     CH585_ADS7948_MUX_FRAME_CLOCKS)
#if CH585_ADS7948_MUX_FRAME_SPS < CH585_ADS7948_MUX_TARGET_SPS
#error CH585 ADS7948 MUX acquisition cannot reach the requested ADC frame rate.
#endif

#define CH585_ADC_MUX_SPI_SCK_PA0 GPIO_Pin_0
#define CH585_ADC_MUX_SPI_MOSI_PA1 GPIO_Pin_1
#define CH585_ADC_MUX_SPI_MISO_PA2 GPIO_Pin_2
#define CH585_ADC_MUX_SEL0_PB0 GPIO_Pin_0
#define CH585_ADC_MUX_SEL1_PB1 GPIO_Pin_1
#define CH585_ADC_MUX_SEL2_PB2 GPIO_Pin_2
#define CH585_ADC_MUX_SEL3_PB3 GPIO_Pin_3
#define CH585_ADS7948_MUX_CS_PB14 GPIO_Pin_14
#define CH585_ADS7948_MUX_CS_PB15 GPIO_Pin_15
#define CH585_ADC_MUX_CH_SEL_PB18 GPIO_Pin_18
#define CH585_ADC_MUX_PDEN_PB19 GPIO_Pin_19

#define CH585_ADC_MUX_SEL_MASK \
    (CH585_ADC_MUX_SEL0_PB0 | CH585_ADC_MUX_SEL1_PB1 | \
     CH585_ADC_MUX_SEL2_PB2 | CH585_ADC_MUX_SEL3_PB3)
#define CH585_ADC_MUX_GPIOA_DIGITAL_MASK \
    (CH585_ADC_MUX_SPI_SCK_PA0 | CH585_ADC_MUX_SPI_MOSI_PA1 | \
     CH585_ADC_MUX_SPI_MISO_PA2)
#define CH585_ADC_MUX_GPIOB_DIGITAL_MASK \
    (CH585_ADC_MUX_SEL_MASK | CH585_ADS7948_MUX_CS_PB14 | \
     CH585_ADS7948_MUX_CS_PB15 | CH585_ADC_MUX_CH_SEL_PB18 | \
     CH585_ADC_MUX_PDEN_PB19)

#define CH585_ADS7948_MUX_LANE(adc, channel, cs, cs_text, first, count) \
    {adc, channel, cs, cs_text, first, count}

static const ch585_ads7948_mux_profile_t g_ch585_ads7948_mux_profiles[2] = {
    {
        "left",
        "left all",
        {
            CH585_ADS7948_MUX_LANE(0U, 0U, CH585_ADS7948_MUX_CS_PB14, "CS1", 0U, 9U),
            CH585_ADS7948_MUX_LANE(0U, 1U, CH585_ADS7948_MUX_CS_PB14, "CS1", 0U, 9U),
            CH585_ADS7948_MUX_LANE(1U, 0U, CH585_ADS7948_MUX_CS_PB15, "CS2", 0U, 9U),
            CH585_ADS7948_MUX_LANE(1U, 1U, CH585_ADS7948_MUX_CS_PB15, "CS2", 0U, 9U),
        },
    },
    {
        "right",
        "right all",
        {
            CH585_ADS7948_MUX_LANE(0U, 0U, CH585_ADS7948_MUX_CS_PB15, "CS1'", 0U, 10U),
            CH585_ADS7948_MUX_LANE(0U, 1U, CH585_ADS7948_MUX_CS_PB15, "CS1'", 0U, 10U),
            CH585_ADS7948_MUX_LANE(1U, 0U, CH585_ADS7948_MUX_CS_PB14, "CS2'", 0U, 10U),
            CH585_ADS7948_MUX_LANE(1U, 1U, CH585_ADS7948_MUX_CS_PB14, "CS2'", 0U, 11U),
        },
    },
};

const ch585_ads7948_mux_profile_t *ch585_ads7948_mux_profile(
    ch585_ads7948_mux_side_t side)
{
    if(side == CH585_ADS7948_MUX_SIDE_RIGHT)
    {
        return &g_ch585_ads7948_mux_profiles[1];
    }

    return &g_ch585_ads7948_mux_profiles[0];
}

uint8_t ch585_ads7948_mux_profile_active_keys(
    const ch585_ads7948_mux_profile_t *profile)
{
    uint8_t lane;
    uint8_t total = 0U;

    if(profile == 0)
    {
        return 0U;
    }

    for(lane = 0U; lane < CH585_ADS7948_MUX_LANE_COUNT; lane++)
    {
        total = (uint8_t)(total + profile->lanes[lane].mux_count);
    }

    return total;
}

const char *ch585_ads7948_mux_cs_pin_name(uint32_t pin)
{
    if(pin == CH585_ADS7948_MUX_CS_PB14)
    {
        return "PB14";
    }
    if(pin == CH585_ADS7948_MUX_CS_PB15)
    {
        return "PB15";
    }
    return "?";
}

uint32_t ch585_ads7948_mux_target_sps(void)
{
    return CH585_ADS7948_MUX_TARGET_SPS;
}

uint32_t ch585_ads7948_mux_frame_sps(void)
{
    return CH585_ADS7948_MUX_FRAME_SPS;
}

uint32_t ch585_ads7948_mux_spi_clock_div(void)
{
    return CH585_ADS7948_MUX_SPI_CLOCK_DIV;
}

static uint8_t ch585_ads7948_mux_lane_has_mux(
    const ch585_ads7948_mux_lane_t *lane,
    uint8_t mux_channel)
{
    uint8_t mux_end;

    if(lane == 0)
    {
        return 0U;
    }

    mux_end = (uint8_t)(lane->mux_first + lane->mux_count);
    return (uint8_t)((lane->mux_count != 0U) &&
                     (mux_channel >= lane->mux_first) &&
                     (mux_channel < mux_end));
}

static uint8_t ch585_ads7948_mux_profile_lane_index_for_adc_channel(
    const ch585_ads7948_mux_profile_t *profile,
    uint8_t adc_index,
    uint8_t adc_channel,
    uint8_t *lane_out)
{
    uint8_t lane;

    if((profile == 0) || (lane_out == 0))
    {
        return 0U;
    }

    for(lane = 0U; lane < CH585_ADS7948_MUX_LANE_COUNT; lane++)
    {
        const ch585_ads7948_mux_lane_t *lane_cfg = &profile->lanes[lane];

        if((lane_cfg->adc_index == adc_index) &&
           (lane_cfg->adc_channel == adc_channel) &&
           (lane_cfg->mux_count != 0U))
        {
            *lane_out = lane;
            return 1U;
        }
    }

    return 0U;
}

static uint32_t ch585_ads7948_mux_profile_cs_pin_for_adc(
    const ch585_ads7948_mux_profile_t *profile,
    uint8_t adc_index)
{
    uint8_t lane;

    if(profile == 0)
    {
        return 0U;
    }

    for(lane = 0U; lane < CH585_ADS7948_MUX_LANE_COUNT; lane++)
    {
        const ch585_ads7948_mux_lane_t *lane_cfg = &profile->lanes[lane];

        if((lane_cfg->adc_index == adc_index) &&
           (lane_cfg->mux_count != 0U))
        {
            return lane_cfg->cs_pin;
        }
    }

    return 0U;
}

static void ch585_ads7948_mux_set_ch_sel(uint8_t level)
{
    if(level != 0U)
    {
        GPIOB_SetBits(CH585_ADC_MUX_CH_SEL_PB18);
    }
    else
    {
        GPIOB_ResetBits(CH585_ADC_MUX_CH_SEL_PB18);
    }
}

static void ch585_ads7948_mux_set_mux_addr(uint8_t mux_channel)
{
    uint32_t set_bits = 0U;

    if((mux_channel & 0x01U) != 0U)
    {
        set_bits |= CH585_ADC_MUX_SEL0_PB0;
    }
    if((mux_channel & 0x02U) != 0U)
    {
        set_bits |= CH585_ADC_MUX_SEL1_PB1;
    }
    if((mux_channel & 0x04U) != 0U)
    {
        set_bits |= CH585_ADC_MUX_SEL2_PB2;
    }
    if((mux_channel & 0x08U) != 0U)
    {
        set_bits |= CH585_ADC_MUX_SEL3_PB3;
    }

    GPIOB_ResetBits(CH585_ADC_MUX_SEL_MASK);
    GPIOB_SetBits(set_bits);
}

void ch585_ads7948_mux_gpio_init(void)
{
    GPIOADigitalCfg(ENABLE, CH585_ADC_MUX_GPIOA_DIGITAL_MASK);
    GPIOBDigitalCfg(ENABLE, CH585_ADC_MUX_GPIOB_DIGITAL_MASK);

    GPIOB_SetBits(CH585_ADS7948_MUX_CS_PB14 |
                  CH585_ADS7948_MUX_CS_PB15);
    GPIOB_ResetBits(CH585_ADC_MUX_SEL_MASK |
                    CH585_ADC_MUX_CH_SEL_PB18 |
                    CH585_ADC_MUX_PDEN_PB19);
    GPIOB_ModeCfg(CH585_ADC_MUX_SEL_MASK |
                      CH585_ADS7948_MUX_CS_PB14 |
                      CH585_ADS7948_MUX_CS_PB15 |
                      CH585_ADC_MUX_CH_SEL_PB18 |
                      CH585_ADC_MUX_PDEN_PB19,
                  GPIO_ModeOut_PP_5mA);

    GPIOA_ModeCfg(CH585_ADC_MUX_SPI_SCK_PA0, GPIO_ModeOut_PP_20mA);
    GPIOA_ModeCfg(CH585_ADC_MUX_SPI_MISO_PA2, GPIO_ModeIN_PU);

    SPI1_MasterDefInit();
    SPI1_DataMode(Mode0_HighBitINFront);
    SPI1_CLKCfg(CH585_ADS7948_MUX_SPI_CLOCK_DIV);

    R8_SPI1_CTRL_MOD &= (uint8_t)(~RB_SPI_MOSI_OE);
}

static uint8_t ch585_ads7948_mux_spi1_recv_byte(void)
{
    R8_SPI1_CTRL_MOD |= RB_SPI_FIFO_DIR;
    R8_SPI1_BUFFER = 0xFFU;
    while((R8_SPI1_INT_FLAG & RB_SPI_FREE) == 0U)
    {
    }
    return R8_SPI1_BUFFER;
}

static uint16_t ch585_ads7948_mux_decode_10bit(uint16_t rx_word)
{
    return (uint16_t)((rx_word >> 6U) & 1023U);
}

static uint16_t ch585_ads7948_mux_read_adc_code(
    ch585_ads7948_mux_acq_t *acq,
    uint8_t adc_index)
{
    uint8_t hi;
    uint8_t lo;
    uint16_t word;
    uint32_t cs_pin;

    if((acq == 0) || (acq->profile == 0) ||
       (adc_index >= CH585_ADS7948_MUX_ADC_COUNT))
    {
        return 0U;
    }

    cs_pin = ch585_ads7948_mux_profile_cs_pin_for_adc(acq->profile,
                                                      adc_index);
    if(cs_pin == 0U)
    {
        return 0U;
    }

    GPIOB_ResetBits(cs_pin);
    hi = ch585_ads7948_mux_spi1_recv_byte();
    lo = ch585_ads7948_mux_spi1_recv_byte();
    GPIOB_SetBits(cs_pin);

    word = (uint16_t)(((uint16_t)hi << 8U) | (uint16_t)lo);
    acq->spi_frames++;
    return ch585_ads7948_mux_decode_10bit(word);
}

static void ch585_ads7948_mux_store(ch585_ads7948_mux_acq_t *acq,
                                    uint8_t lane,
                                    uint8_t mux_channel,
                                    uint16_t code)
{
    uint16_t key = (uint16_t)lane *
                   CH585_ADS7948_MUX_MUX_CHANNEL_COUNT +
                   (uint16_t)mux_channel;

    if((acq != 0) && (key < CH585_ADS7948_MUX_KEY_COUNT))
    {
        acq->raw[key] = code;
    }
}

static uint8_t ch585_ads7948_mux_compact_key_to_lane_mux(
    const ch585_ads7948_mux_profile_t *profile,
    uint8_t compact_key,
    uint8_t *lane_out,
    uint8_t *mux_out)
{
    uint8_t lane;
    uint8_t offset = compact_key;

    if((profile == 0) || (lane_out == 0) || (mux_out == 0))
    {
        return 0U;
    }

    for(lane = 0U; lane < CH585_ADS7948_MUX_LANE_COUNT; lane++)
    {
        const ch585_ads7948_mux_lane_t *lane_cfg = &profile->lanes[lane];

        if(offset < lane_cfg->mux_count)
        {
            *lane_out = lane;
            *mux_out = (uint8_t)(lane_cfg->mux_first + offset);
            return 1U;
        }

        offset = (uint8_t)(offset - lane_cfg->mux_count);
    }

    return 0U;
}

static void ch585_ads7948_mux_acq_poll_channel(
    ch585_ads7948_mux_acq_t *acq,
    uint8_t adc_channel)
{
    uint8_t adc_index;
    uint8_t lane_index[CH585_ADS7948_MUX_ADC_COUNT] = {0U, 0U};
    uint8_t has_lane[CH585_ADS7948_MUX_ADC_COUNT] = {0U, 0U};
    uint8_t prev_valid[CH585_ADS7948_MUX_ADC_COUNT] = {0U, 0U};
    uint8_t prev_lane[CH585_ADS7948_MUX_ADC_COUNT] = {0U, 0U};
    uint8_t prev_mux[CH585_ADS7948_MUX_ADC_COUNT] = {0U, 0U};
    uint8_t min_mux = CH585_ADS7948_MUX_MUX_CHANNEL_COUNT;
    uint8_t max_mux_end = 0U;
    uint8_t mux_channel;

    if((acq == 0) || (acq->profile == 0))
    {
        return;
    }

    for(adc_index = 0U;
        adc_index < CH585_ADS7948_MUX_ADC_COUNT;
        adc_index++)
    {
        has_lane[adc_index] =
            ch585_ads7948_mux_profile_lane_index_for_adc_channel(
                acq->profile,
                adc_index,
                adc_channel,
                &lane_index[adc_index]);

        if(has_lane[adc_index] != 0U)
        {
            const ch585_ads7948_mux_lane_t *lane =
                &acq->profile->lanes[lane_index[adc_index]];
            uint8_t lane_end = (uint8_t)(lane->mux_first + lane->mux_count);

            if(lane->mux_first < min_mux)
            {
                min_mux = lane->mux_first;
            }
            if(lane_end > max_mux_end)
            {
                max_mux_end = lane_end;
            }
        }
    }

    if(max_mux_end == 0U)
    {
        return;
    }

    ch585_ads7948_mux_set_ch_sel(adc_channel);

    for(mux_channel = min_mux; mux_channel < max_mux_end; mux_channel++)
    {
        ch585_ads7948_mux_set_mux_addr(mux_channel);

        for(adc_index = 0U;
            adc_index < CH585_ADS7948_MUX_ADC_COUNT;
            adc_index++)
        {
            uint8_t current_valid = 0U;
            const ch585_ads7948_mux_lane_t *lane = 0;

            if(has_lane[adc_index] != 0U)
            {
                lane = &acq->profile->lanes[lane_index[adc_index]];
                current_valid =
                    ch585_ads7948_mux_lane_has_mux(lane, mux_channel);
            }

            if((current_valid != 0U) || (prev_valid[adc_index] != 0U))
            {
                uint16_t code =
                    ch585_ads7948_mux_read_adc_code(acq, adc_index);

                if(prev_valid[adc_index] != 0U)
                {
                    ch585_ads7948_mux_store(acq,
                                            prev_lane[adc_index],
                                            prev_mux[adc_index],
                                            code);
                }

                if(current_valid != 0U)
                {
                    prev_valid[adc_index] = 1U;
                    prev_lane[adc_index] = lane_index[adc_index];
                    prev_mux[adc_index] = mux_channel;
                }
                else
                {
                    prev_valid[adc_index] = 0U;
                }
            }
        }
    }

    for(adc_index = 0U;
        adc_index < CH585_ADS7948_MUX_ADC_COUNT;
        adc_index++)
    {
        if(prev_valid[adc_index] != 0U)
        {
            uint16_t code =
                ch585_ads7948_mux_read_adc_code(acq, adc_index);
            ch585_ads7948_mux_store(acq,
                                    prev_lane[adc_index],
                                    prev_mux[adc_index],
                                    code);
        }
    }
}

int ch585_ads7948_mux_acq_init(
    ch585_ads7948_mux_acq_t *acq,
    const ch585_ads7948_mux_profile_t *profile)
{
    uint16_t key;
    uint8_t adc_index;

    if((acq == 0) || (profile == 0) ||
       (ch585_ads7948_mux_profile_active_keys(profile) == 0U))
    {
        return -1;
    }

    for(adc_index = 0U;
        adc_index < CH585_ADS7948_MUX_ADC_COUNT;
        adc_index++)
    {
        if(ch585_ads7948_mux_profile_cs_pin_for_adc(profile, adc_index) == 0U)
        {
            return -1;
        }
    }

    acq->profile = profile;
    acq->frames = 0U;
    acq->spi_frames = 0U;
    for(key = 0U; key < CH585_ADS7948_MUX_KEY_COUNT; key++)
    {
        acq->raw[key] = 0U;
    }

    return 0;
}

void ch585_ads7948_mux_acq_poll(ch585_ads7948_mux_acq_t *acq)
{
    if((acq == 0) || (acq->profile == 0))
    {
        return;
    }

    ch585_ads7948_mux_acq_poll_channel(acq, 0U);
    ch585_ads7948_mux_acq_poll_channel(acq, 1U);
    acq->frames++;
}

int ch585_ads7948_mux_acq_read_compact_key(
    ch585_ads7948_mux_acq_t *acq,
    uint8_t compact_key,
    uint16_t *raw_out)
{
    uint8_t lane_index = 0U;
    uint8_t mux_channel = 0U;
    const ch585_ads7948_mux_lane_t *lane;
    uint16_t code;

    if((acq == 0) || (acq->profile == 0) ||
       (raw_out == 0))
    {
        return -1;
    }

    if(ch585_ads7948_mux_compact_key_to_lane_mux(acq->profile,
                                                 compact_key,
                                                 &lane_index,
                                                 &mux_channel) == 0U)
    {
        return -2;
    }

    lane = &acq->profile->lanes[lane_index];
    ch585_ads7948_mux_set_ch_sel(lane->adc_channel);
    ch585_ads7948_mux_set_mux_addr(mux_channel);

    (void)ch585_ads7948_mux_read_adc_code(acq, lane->adc_index);
    code = ch585_ads7948_mux_read_adc_code(acq, lane->adc_index);
    ch585_ads7948_mux_store(acq, lane_index, mux_channel, code);
    acq->frames++;

    *raw_out = code;
    return 0;
}

const uint16_t *ch585_ads7948_mux_acq_raw(
    const ch585_ads7948_mux_acq_t *acq)
{
    if(acq == 0)
    {
        return 0;
    }

    return acq->raw;
}
