/*
 * CH585 front-end scanner for ADS7948 + external 16:1 MUX chains.
 */

#include "ch585_ads7948_mux_scan.h"

#include <stddef.h>
#include <string.h>

#define CH585_MUX_SCAN_FILTER_SCALE 256U

static uint8_t ch585_mux_scan_key_is_pressed(const ch585_mux_scan_config_t *cfg,
                                             uint16_t value)
{
    if (cfg->pressed_adc >= cfg->released_adc)
    {
        return (uint8_t)(value >= cfg->press_adc);
    }

    return (uint8_t)(value <= cfg->press_adc);
}

static uint8_t ch585_mux_scan_key_is_released(const ch585_mux_scan_config_t *cfg,
                                              uint16_t value)
{
    if (cfg->pressed_adc >= cfg->released_adc)
    {
        return (uint8_t)(value <= cfg->release_adc);
    }

    return (uint8_t)(value >= cfg->release_adc);
}

static void ch585_mux_scan_apply_default_mapping(ch585_mux_scan_config_t *cfg)
{
    cfg->lane_adc_index[0] = 0U;
    cfg->lane_adc_channel[0] = 0U;
    cfg->lane_adc_index[1] = 0U;
    cfg->lane_adc_channel[1] = 1U;
    cfg->lane_adc_index[2] = 1U;
    cfg->lane_adc_channel[2] = 0U;
    cfg->lane_adc_index[3] = 1U;
    cfg->lane_adc_channel[3] = 1U;
}

void ch585_mux_scan_default_config(ch585_mux_scan_config_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->mux_settle_us = CH585_MUX_SCAN_DEFAULT_MUX_SETTLE_US;
    cfg->lane_count = CH585_MUX_SCAN_LANE_COUNT;
    cfg->oversample_count = CH585_MUX_SCAN_DEFAULT_OVERSAMPLE;
    cfg->discard_after_mux_switch = 1U;
    cfg->filter_shift = CH585_MUX_SCAN_DEFAULT_FILTER_SHIFT;
    cfg->released_adc = CH585_MUX_SCAN_DEFAULT_RELEASED_ADC;
    cfg->pressed_adc = CH585_MUX_SCAN_DEFAULT_PRESSED_ADC;
    cfg->press_adc = CH585_MUX_SCAN_DEFAULT_PRESS_ADC;
    cfg->release_adc = CH585_MUX_SCAN_DEFAULT_RELEASE_ADC;
    ch585_mux_scan_apply_default_mapping(cfg);
}

static int ch585_mux_scan_validate_config(const ch585_mux_scan_config_t *cfg)
{
    uint8_t lane;

    if ((cfg == NULL) || (cfg->set_mux_addr == NULL) ||
        (cfg->lane_count == 0U) ||
        (cfg->lane_count > CH585_MUX_SCAN_LANE_COUNT))
    {
        return CH585_MUX_SCAN_STATUS_PARAM;
    }

    for (lane = 0U; lane < cfg->lane_count; lane++)
    {
        uint8_t adc_index = cfg->lane_adc_index[lane];
        uint8_t adc_channel = cfg->lane_adc_channel[lane];

        if ((adc_index >= CH585_MUX_SCAN_ADC_COUNT) ||
            (adc_channel >= ADS7948_CHANNEL_COUNT) ||
            (cfg->adc[adc_index] == NULL))
        {
            return CH585_MUX_SCAN_STATUS_PARAM;
        }
    }

    return CH585_MUX_SCAN_STATUS_OK;
}

int ch585_mux_scan_init(ch585_mux_scan_t *scan,
                        const ch585_mux_scan_config_t *cfg)
{
    uint8_t i;
    uint16_t key;
    int rc;

    if (scan == NULL)
    {
        return CH585_MUX_SCAN_STATUS_PARAM;
    }

    rc = ch585_mux_scan_validate_config(cfg);
    if (rc != CH585_MUX_SCAN_STATUS_OK)
    {
        return rc;
    }

    memset(scan, 0, sizeof(*scan));
    scan->cfg = *cfg;

    if (scan->cfg.oversample_count == 0U)
    {
        scan->cfg.oversample_count = 1U;
    }

    if (scan->cfg.filter_shift > 8U)
    {
        scan->cfg.filter_shift = 8U;
    }

    for (i = 0U; i < 2U; i++)
    {
        for (key = 0U; key < CH585_MUX_SCAN_KEY_COUNT; key++)
        {
            scan->raw[i][key] = scan->cfg.released_adc;
        }
    }

    for (key = 0U; key < CH585_MUX_SCAN_KEY_COUNT; key++)
    {
        scan->filtered[key] = scan->cfg.released_adc;
        scan->filtered_q8[key] = (uint32_t)scan->cfg.released_adc *
                                 CH585_MUX_SCAN_FILTER_SCALE;
    }

    scan->initialized = 1U;
    return CH585_MUX_SCAN_STATUS_OK;
}

static int ch585_mux_scan_read_lane(ch585_mux_scan_t *scan,
                                    uint8_t lane,
                                    uint16_t *value)
{
    uint32_t sum = 0U;
    uint8_t sample;
    uint8_t count;
    uint8_t adc_index;
    uint8_t adc_channel;

    if ((scan == NULL) || (value == NULL) || (lane >= scan->cfg.lane_count))
    {
        return CH585_MUX_SCAN_STATUS_PARAM;
    }

    count = scan->cfg.oversample_count;
    adc_index = scan->cfg.lane_adc_index[lane];
    adc_channel = scan->cfg.lane_adc_channel[lane];

    for (sample = 0U; sample < count; sample++)
    {
        uint16_t code = 0U;
        int rc = ads7948_read_channel(scan->cfg.adc[adc_index],
                                      adc_channel,
                                      &code);
        if (rc != ADS7948_STATUS_OK)
        {
            scan->stats.adc_errors++;
            scan->stats.flags |= CH585_MUX_SCAN_FLAG_ADC_ERROR;
            return CH585_MUX_SCAN_STATUS_ADC;
        }

        sum += code;
    }

    *value = (uint16_t)((sum + (uint32_t)(count / 2U)) / count);
    return CH585_MUX_SCAN_STATUS_OK;
}

static void ch585_mux_scan_update_key(ch585_mux_scan_t *scan,
                                      uint16_t key,
                                      uint16_t raw)
{
    int32_t target_q8;
    int32_t current_q8;
    int32_t next_q8;
    uint16_t filtered;

    target_q8 = (int32_t)raw * (int32_t)CH585_MUX_SCAN_FILTER_SCALE;
    current_q8 = (int32_t)scan->filtered_q8[key];
    next_q8 = current_q8 +
              ((target_q8 - current_q8) >> scan->cfg.filter_shift);

    if (next_q8 < 0)
    {
        next_q8 = 0;
    }

    scan->filtered_q8[key] = (uint32_t)next_q8;
    filtered = (uint16_t)(scan->filtered_q8[key] /
                          CH585_MUX_SCAN_FILTER_SCALE);
    scan->filtered[key] = filtered;

    if (scan->down[key] == 0U)
    {
        if (ch585_mux_scan_key_is_pressed(&scan->cfg, filtered) != 0U)
        {
            scan->down[key] = 1U;
        }
    }
    else
    {
        if (ch585_mux_scan_key_is_released(&scan->cfg, filtered) != 0U)
        {
            scan->down[key] = 0U;
        }
    }
}

int ch585_mux_scan_poll(ch585_mux_scan_t *scan)
{
    uint8_t back;
    uint8_t mux_channel;
    uint8_t lane;

    if ((scan == NULL) || (scan->initialized == 0U))
    {
        return CH585_MUX_SCAN_STATUS_PARAM;
    }

    back = (uint8_t)(scan->front_index ^ 1U);
    memcpy(scan->raw[back], scan->raw[scan->front_index],
           sizeof(scan->raw[back]));
    scan->stats.flags = 0U;

    for (mux_channel = 0U;
         mux_channel < CH585_MUX_SCAN_MUX_CHANNEL_COUNT;
         mux_channel++)
    {
        scan->cfg.set_mux_addr(mux_channel, scan->cfg.user);

        if ((scan->cfg.delay_us != NULL) && (scan->cfg.mux_settle_us != 0U))
        {
            scan->cfg.delay_us(scan->cfg.mux_settle_us, scan->cfg.user);
        }

        if (scan->cfg.discard_after_mux_switch != 0U)
        {
            for (lane = 0U; lane < scan->cfg.lane_count; lane++)
            {
                uint16_t ignored = 0U;
                int rc = ch585_mux_scan_read_lane(scan, lane, &ignored);
                if (rc != CH585_MUX_SCAN_STATUS_OK)
                {
                    return rc;
                }
                scan->stats.discarded_reads++;
            }
            scan->stats.flags |= CH585_MUX_SCAN_FLAG_SETTLE_DISCARD;
        }

        for (lane = 0U; lane < scan->cfg.lane_count; lane++)
        {
            uint16_t sample = 0U;
            uint16_t key = (uint16_t)lane *
                           CH585_MUX_SCAN_MUX_CHANNEL_COUNT +
                           (uint16_t)mux_channel;
            int rc = ch585_mux_scan_read_lane(scan, lane, &sample);
            if (rc != CH585_MUX_SCAN_STATUS_OK)
            {
                return rc;
            }

            scan->raw[back][key] = sample;
            ch585_mux_scan_update_key(scan, key, sample);
        }
    }

    scan->front_index = back;
    scan->stats.seq++;
    scan->stats.frames++;
    return CH585_MUX_SCAN_STATUS_OK;
}

const uint16_t *ch585_mux_scan_front_raw(const ch585_mux_scan_t *scan)
{
    if ((scan == NULL) || (scan->initialized == 0U))
    {
        return NULL;
    }

    return scan->raw[scan->front_index];
}

const uint16_t *ch585_mux_scan_filtered(const ch585_mux_scan_t *scan)
{
    if ((scan == NULL) || (scan->initialized == 0U))
    {
        return NULL;
    }

    return scan->filtered;
}

const ch585_mux_scan_stats_t *ch585_mux_scan_stats(const ch585_mux_scan_t *scan)
{
    if ((scan == NULL) || (scan->initialized == 0U))
    {
        return NULL;
    }

    return &scan->stats;
}

void ch585_mux_scan_front_down_bits(const ch585_mux_scan_t *scan,
                                    uint8_t down_bits[CH585_MUX_SCAN_DOWN_BYTES])
{
    uint16_t key;

    if (down_bits == NULL)
    {
        return;
    }

    memset(down_bits, 0, CH585_MUX_SCAN_DOWN_BYTES);

    if ((scan == NULL) || (scan->initialized == 0U))
    {
        return;
    }

    for (key = 0U; key < CH585_MUX_SCAN_KEY_COUNT; key++)
    {
        if (scan->down[key] != 0U)
        {
            down_bits[key >> 3U] |= (uint8_t)(1U << (key & 7U));
        }
    }
}
