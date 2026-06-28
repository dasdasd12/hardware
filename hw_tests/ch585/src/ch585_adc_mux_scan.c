#include "ch585_common.h"

#include "ch585_ads7948_mux_acq.h"

#define CH585_ADC_MUX_ADC_SCALE 1024U
#define CH585_ADC_MUX_PERCENT_SCALE 1000U
#define CH585_ADC_MUX_BAR_WIDTH 16U

static ch585_ads7948_mux_acq_t g_ch585_adc_mux_acq;

static const ch585_ads7948_mux_profile_t *ch585_adc_mux_active_profile(void)
{
#if defined(CH585_ADC_MUX_HALF_RIGHT)
    return ch585_ads7948_mux_profile(CH585_ADS7948_MUX_SIDE_RIGHT);
#else
    return ch585_ads7948_mux_profile(CH585_ADS7948_MUX_SIDE_LEFT);
#endif
}

static void ch585_adc_mux_log_kv_dec(const char *key, uint32_t value)
{
    ch585_log_str("DATA ");
    ch585_log_str(key);
    ch585_log_str("=");
    ch585_log_u32_dec(value);
    ch585_log_str("\r\n");
}

static void ch585_adc_mux_log_kv_str(const char *key, const char *value)
{
    ch585_log_str("DATA ");
    ch585_log_str(key);
    ch585_log_str("=");
    ch585_log_line(value);
}

static void ch585_adc_mux_log_u32_dec_padded(uint32_t value, uint8_t width)
{
    uint32_t limit = 1U;
    uint8_t i;

    for(i = 1U; i < width; i++)
    {
        limit *= 10U;
    }

    while((width > 1U) && (value < limit))
    {
        ch585_log_str("0");
        limit /= 10U;
        width--;
    }

    ch585_log_u32_dec(value);
}

static uint16_t ch585_adc_mux_raw_to_permille(uint16_t raw)
{
    uint32_t value = raw;

    if(value >= CH585_ADC_MUX_ADC_SCALE)
    {
        value = CH585_ADC_MUX_ADC_SCALE - 1U;
    }

    return (uint16_t)((value * CH585_ADC_MUX_PERCENT_SCALE) /
                      CH585_ADC_MUX_ADC_SCALE);
}

static void ch585_adc_mux_log_percent(uint16_t raw)
{
    uint16_t permille = ch585_adc_mux_raw_to_permille(raw);

    ch585_log_u32_dec((uint32_t)(permille / 10U));
    ch585_log_str(".");
    ch585_log_u32_dec((uint32_t)(permille % 10U));
    ch585_log_str("%");
}

static void ch585_adc_mux_log_bar(uint16_t raw)
{
    uint16_t permille = ch585_adc_mux_raw_to_permille(raw);
    uint8_t filled = (uint8_t)(((uint32_t)permille *
                                CH585_ADC_MUX_BAR_WIDTH + 999U) /
                               CH585_ADC_MUX_PERCENT_SCALE);
    uint8_t i;

    if(filled > CH585_ADC_MUX_BAR_WIDTH)
    {
        filled = CH585_ADC_MUX_BAR_WIDTH;
    }

    ch585_log_str("[");
    for(i = 0U; i < CH585_ADC_MUX_BAR_WIDTH; i++)
    {
        ch585_log_str((i < filled) ? "#" : ".");
    }
    ch585_log_str("]");
}

static void ch585_adc_mux_log_profile(
    const ch585_ads7948_mux_profile_t *profile)
{
    uint8_t lane;

    ch585_adc_mux_log_kv_str("adc_mux_profile", profile->name);
    ch585_adc_mux_log_kv_str("adc_mux_mode", "frame_acq");
    ch585_adc_mux_log_kv_str("pin_spi1", "sck=PA0 mosi=PA1 miso=PA2");
    ch585_adc_mux_log_kv_str("pin_ch_sel", "PB18");
    ch585_adc_mux_log_kv_str("pin_mux_sel", "SEL0..3=PB0..PB3");
    ch585_adc_mux_log_kv_str("pin_pden", "PB19=0 normal");
    ch585_adc_mux_log_kv_str("scale", "1024 raw-counts, travel=raw/1024");
    ch585_adc_mux_log_kv_str("target", profile->target);
    ch585_adc_mux_log_kv_dec("active_keys",
                             ch585_ads7948_mux_profile_active_keys(profile));
    ch585_adc_mux_log_kv_dec("physical_slots",
                             CH585_ADS7948_MUX_KEY_COUNT);
    ch585_adc_mux_log_kv_dec("spi_div",
                             ch585_ads7948_mux_spi_clock_div());
    ch585_adc_mux_log_kv_dec("target_sps",
                             ch585_ads7948_mux_target_sps());
    ch585_adc_mux_log_kv_dec("frame_sps",
                             ch585_ads7948_mux_frame_sps());

    for(lane = 0U; lane < CH585_ADS7948_MUX_LANE_COUNT; lane++)
    {
        const ch585_ads7948_mux_lane_t *lane_cfg = &profile->lanes[lane];

        ch585_log_str("DATA lane=L");
        ch585_log_u32_dec((uint32_t)lane + 1U);
        ch585_log_str(" adc=");
        ch585_log_u32_dec(lane_cfg->adc_index);
        ch585_log_str(" ch=");
        ch585_log_u32_dec(lane_cfg->adc_channel);
        ch585_log_str(" cs=");
        ch585_log_str(lane_cfg->cs_name);
        ch585_log_str("(");
        ch585_log_str(ch585_ads7948_mux_cs_pin_name(lane_cfg->cs_pin));
        ch585_log_str(")");
        ch585_log_str(" first=");
        ch585_log_u32_dec(lane_cfg->mux_first);
        ch585_log_str(" count=");
        ch585_log_u32_dec(lane_cfg->mux_count);
        ch585_log_str("\r\n");
    }
}

static void ch585_adc_mux_log_lane_raw(
    const ch585_ads7948_mux_profile_t *profile,
    const uint16_t *raw,
    uint8_t lane)
{
    const ch585_ads7948_mux_lane_t *lane_cfg;
    uint8_t mux_channel;
    uint8_t mux_end;

    if((profile == 0) || (raw == 0) ||
       (lane >= CH585_ADS7948_MUX_LANE_COUNT))
    {
        return;
    }

    lane_cfg = &profile->lanes[lane];
    mux_end = (uint8_t)(lane_cfg->mux_first + lane_cfg->mux_count);

    if(lane_cfg->mux_count == 0U)
    {
        return;
    }

    ch585_log_str(" L");
    ch585_log_u32_dec((uint32_t)lane + 1U);
    ch585_log_str(" ");
    ch585_log_str(lane_cfg->cs_name);
    ch585_log_str("(");
    ch585_log_str(ch585_ads7948_mux_cs_pin_name(lane_cfg->cs_pin));
    ch585_log_str(") CH");
    ch585_log_u32_dec(lane_cfg->adc_channel);
    ch585_log_str(" mux=");
    ch585_log_u32_dec(lane_cfg->mux_count);
    ch585_log_str(" first=D");
    ch585_adc_mux_log_u32_dec_padded((uint32_t)lane_cfg->mux_first + 1U,
                                     2U);
    if((profile->target != 0) &&
       (ch585_ads7948_mux_profile_active_keys(profile) == 1U))
    {
        ch585_log_str(" target=");
        ch585_log_str(profile->target);
    }
    ch585_log_str("\r\n");

    for(mux_channel = lane_cfg->mux_first;
        mux_channel < mux_end;
        mux_channel++)
    {
        uint16_t key = (uint16_t)lane *
                       CH585_ADS7948_MUX_MUX_CHANNEL_COUNT +
                       (uint16_t)mux_channel;

        ch585_log_str("  D");
        ch585_adc_mux_log_u32_dec_padded((uint32_t)mux_channel + 1U, 2U);
        ch585_log_str(" raw=");
        ch585_adc_mux_log_u32_dec_padded(raw[key], 4U);
        ch585_log_str(" travel=");
        ch585_adc_mux_log_percent(raw[key]);
        ch585_log_str(" ");
        ch585_adc_mux_log_bar(raw[key]);
        ch585_log_str("\r\n");
    }
}

static void ch585_adc_mux_log_frame(const ch585_ads7948_mux_acq_t *acq)
{
    const uint16_t *raw;
    uint8_t lane;

    if((acq == 0) || (acq->profile == 0))
    {
        ch585_log_fail("ch585_adc_mux_scan", "no_frame");
        return;
    }

    raw = ch585_ads7948_mux_acq_raw(acq);
    if(raw == 0)
    {
        ch585_log_fail("ch585_adc_mux_scan", "no_raw");
        return;
    }

    ch585_log_str("\r\nFRAME side=");
    ch585_log_str(acq->profile->name);
    ch585_log_str(" seq=");
    ch585_log_u32_dec(acq->frames);
    ch585_log_str(" scale=1024");
    ch585_log_str(" keys=");
    ch585_log_u32_dec(ch585_ads7948_mux_profile_active_keys(acq->profile));
    ch585_log_str(" sampled=");
    ch585_log_u32_dec(ch585_ads7948_mux_profile_active_keys(acq->profile));
    ch585_log_str(" flags=0x00");
    ch585_log_str(" down=0x0000000000000000");
    ch585_log_str(" spi_frames=");
    ch585_log_u32_dec(acq->spi_frames);
    ch585_log_str(" frame_sps=");
    ch585_log_u32_dec(ch585_ads7948_mux_frame_sps());
    ch585_log_str("\r\n");

    for(lane = 0U; lane < CH585_ADS7948_MUX_LANE_COUNT; lane++)
    {
        ch585_adc_mux_log_lane_raw(acq->profile, raw, lane);
    }

    ch585_log_str("\r\n");
}

void ch585_adc_mux_scan_run(void)
{
    const ch585_ads7948_mux_profile_t *profile = ch585_adc_mux_active_profile();
    int rc;

    ch585_ads7948_mux_gpio_init();
    ch585_adc_mux_log_profile(profile);

    rc = ch585_ads7948_mux_acq_init(&g_ch585_adc_mux_acq, profile);
    if(rc != 0)
    {
        ch585_log_fail("ch585_ads7948_mux_acq_init", "profile");
        return;
    }

    ch585_log_pass("ch585_adc_mux_scan_ready");

    while(1)
    {
        ch585_ads7948_mux_acq_poll(&g_ch585_adc_mux_acq);
        ch585_adc_mux_log_frame(&g_ch585_adc_mux_acq);
    }
}
