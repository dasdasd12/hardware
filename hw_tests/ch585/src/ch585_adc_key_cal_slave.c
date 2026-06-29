#include "ch585_common.h"

#include <string.h>

#include "ch585_ads7948_mux_acq.h"
#include "ch585_h417_adc_key_cal_proto.h"
#include "ch585_spi0_slave_link.h"

#if defined(CH585_ADC_MUX_HALF_RIGHT)
#define CH585_ADC_KEY_CAL_SIDE CH585_ADS7948_MUX_SIDE_RIGHT
#define CH585_ADC_KEY_CAL_SIDE_ID 1U
#define CH585_ADC_KEY_CAL_SIDE_NAME "right"
#else
#define CH585_ADC_KEY_CAL_SIDE CH585_ADS7948_MUX_SIDE_LEFT
#define CH585_ADC_KEY_CAL_SIDE_ID 0U
#define CH585_ADC_KEY_CAL_SIDE_NAME "left"
#endif

static ch585_ads7948_mux_acq_t s_ch585_adc_key_cal_acq;
static ch585_h417_adc_key_cal_sample_t s_ch585_adc_key_cal_tx
    __attribute__((aligned(4)));
static ch585_h417_adc_key_cal_cmd_t s_ch585_adc_key_cal_rx
    __attribute__((aligned(4)));
static uint8_t s_ch585_adc_key_cal_current_key;
static uint8_t s_ch585_adc_key_cal_key_count;
static uint16_t s_ch585_adc_key_cal_min;
static uint16_t s_ch585_adc_key_cal_max;
static uint32_t s_ch585_adc_key_cal_count;
static uint16_t s_ch585_adc_key_cal_seq;

static void ch585_adc_key_cal_reset_stats(uint16_t raw)
{
    s_ch585_adc_key_cal_min = raw;
    s_ch585_adc_key_cal_max = raw;
    s_ch585_adc_key_cal_count = 0U;
}

static void ch585_adc_key_cal_apply_cmd(void)
{
    if(ch585_h417_adc_key_cal_cmd_valid(&s_ch585_adc_key_cal_rx) == 0U)
    {
        return;
    }

    if((s_ch585_adc_key_cal_rx.cmd == CH585_H417_ADC_KEY_CAL_CMD_SELECT) &&
       (s_ch585_adc_key_cal_rx.key_id < s_ch585_adc_key_cal_key_count))
    {
        uint16_t raw = 0U;
        uint8_t reset = 0U;

        if(s_ch585_adc_key_cal_rx.key_id != s_ch585_adc_key_cal_current_key)
        {
            reset = 1U;
        }
        if((s_ch585_adc_key_cal_rx.flags &
            CH585_H417_ADC_KEY_CAL_FLAG_RESET_STATS) != 0U)
        {
            reset = 1U;
        }

        s_ch585_adc_key_cal_current_key = s_ch585_adc_key_cal_rx.key_id;
        if((reset != 0U) &&
           (ch585_ads7948_mux_acq_read_compact_key(
                &s_ch585_adc_key_cal_acq,
                s_ch585_adc_key_cal_current_key,
                &raw) == 0))
        {
            ch585_adc_key_cal_reset_stats(raw);
        }
    }
}

static void ch585_adc_key_cal_build_sample(uint8_t status, uint16_t raw)
{
    memset(&s_ch585_adc_key_cal_tx, 0, sizeof(s_ch585_adc_key_cal_tx));
    s_ch585_adc_key_cal_tx.magic = CH585_H417_ADC_KEY_CAL_SAMPLE_MAGIC;
    s_ch585_adc_key_cal_tx.version = CH585_H417_ADC_KEY_CAL_VERSION;
    s_ch585_adc_key_cal_tx.status = status;
    s_ch585_adc_key_cal_tx.side = CH585_ADC_KEY_CAL_SIDE_ID;
    s_ch585_adc_key_cal_tx.sample_seq = s_ch585_adc_key_cal_seq++;
    s_ch585_adc_key_cal_tx.key_id = s_ch585_adc_key_cal_current_key;
    s_ch585_adc_key_cal_tx.key_count = s_ch585_adc_key_cal_key_count;
    s_ch585_adc_key_cal_tx.raw = raw;
    s_ch585_adc_key_cal_tx.min_raw = s_ch585_adc_key_cal_min;
    s_ch585_adc_key_cal_tx.max_raw = s_ch585_adc_key_cal_max;
    s_ch585_adc_key_cal_tx.sample_count = s_ch585_adc_key_cal_count;
    ch585_h417_adc_key_cal_finish_sample(&s_ch585_adc_key_cal_tx);
}

void ch585_adc_key_cal_slave_run(void)
{
    const ch585_ads7948_mux_profile_t *profile =
        ch585_ads7948_mux_profile(CH585_ADC_KEY_CAL_SIDE);
    uint16_t raw = 0U;

    ch585_ads7948_mux_gpio_init();
    if(ch585_ads7948_mux_acq_init(&s_ch585_adc_key_cal_acq, profile) != 0)
    {
        ch585_log_fail("ch585_adc_key_cal_slave", "adc_mux_init");
        while(1)
        {
        }
    }

    s_ch585_adc_key_cal_current_key = 0U;
    s_ch585_adc_key_cal_key_count =
        ch585_ads7948_mux_profile_active_keys(profile);
    (void)ch585_ads7948_mux_acq_read_compact_key(
        &s_ch585_adc_key_cal_acq,
        s_ch585_adc_key_cal_current_key,
        &raw);
    ch585_adc_key_cal_reset_stats(raw);
    ch585_adc_key_cal_build_sample(CH585_H417_ADC_KEY_CAL_STATUS_OK, raw);

    ch585_spi0_slave_link_init();

    ch585_log_str("DATA adc_key_cal_slave side=");
    ch585_log_str(CH585_ADC_KEY_CAL_SIDE_NAME);
    ch585_log_str(" keys=");
    ch585_log_u32_dec(s_ch585_adc_key_cal_key_count);
    ch585_log_str(" frame_bytes=");
    ch585_log_u32_dec(CH585_H417_ADC_KEY_CAL_FRAME_BYTES);
    ch585_log_str(" sysclk=");
#ifdef FREQ_SYS
    ch585_log_u32_dec((uint32_t)FREQ_SYS);
#else
    ch585_log_u32_dec(0U);
#endif
    ch585_log_str("\r\n");

    while(1)
    {
        uint8_t status = CH585_H417_ADC_KEY_CAL_STATUS_OK;

        if(ch585_ads7948_mux_acq_read_compact_key(
               &s_ch585_adc_key_cal_acq,
               s_ch585_adc_key_cal_current_key,
               &raw) != 0)
        {
            status = CH585_H417_ADC_KEY_CAL_STATUS_BAD_KEY;
            raw = 0U;
        }
        else
        {
            if(raw < s_ch585_adc_key_cal_min)
            {
                s_ch585_adc_key_cal_min = raw;
            }
            if(raw > s_ch585_adc_key_cal_max)
            {
                s_ch585_adc_key_cal_max = raw;
            }
            s_ch585_adc_key_cal_count++;
        }

        ch585_adc_key_cal_build_sample(status, raw);
        if(ch585_spi0_slave_link_serve_frame(
               (const uint8_t *)&s_ch585_adc_key_cal_tx,
               (uint8_t *)&s_ch585_adc_key_cal_rx,
               (uint16_t)CH585_H417_ADC_KEY_CAL_FRAME_BYTES) ==
           CH585_SPI0_SLAVE_LINK_OK)
        {
            ch585_adc_key_cal_apply_cmd();
        }
    }
}
