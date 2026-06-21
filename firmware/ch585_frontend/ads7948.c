/*
 * ADS7948 conservative read driver.
 *
 * This implementation favors bring-up reliability over maximum throughput:
 * after CH SEL changes, it waits for the analog path to settle and discards
 * one or more pipeline frames before returning a code.
 */

#include "ads7948.h"

#include <stddef.h>
#include <string.h>

void ads7948_default_config(ads7948_config_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->input_settle_us = ADS7948_DEFAULT_SETTLE_US;
    cfg->cs_high_us = ADS7948_DEFAULT_CS_HIGH_US;
    cfg->discard_frames = ADS7948_DEFAULT_DISCARDS;
}

int ads7948_init(ads7948_t *dev, const ads7948_config_t *cfg)
{
    if ((dev == NULL) || (cfg == NULL) ||
        (cfg->set_cs == NULL) || (cfg->set_ch_sel == NULL) ||
        (cfg->read16 == NULL))
    {
        return ADS7948_STATUS_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->cfg = *cfg;
    dev->last_channel = 0xFFU;
    dev->initialized = 1U;

    dev->cfg.set_cs(1U, dev->cfg.user);
    dev->cfg.set_ch_sel(0U, dev->cfg.user);
    if ((dev->cfg.delay_us != NULL) && (dev->cfg.cs_high_us != 0U))
    {
        dev->cfg.delay_us(dev->cfg.cs_high_us, dev->cfg.user);
    }

    return ADS7948_STATUS_OK;
}

uint16_t ads7948_decode_10bit(uint16_t rx_word)
{
    return (uint16_t)((rx_word >> 6U) & ADS7948_CODE_MAX);
}

int ads7948_read_frame(ads7948_t *dev, uint16_t *rx_word)
{
    uint16_t word = 0U;
    int rc;

    if ((dev == NULL) || (rx_word == NULL) || (dev->initialized == 0U))
    {
        return ADS7948_STATUS_PARAM;
    }

    dev->cfg.set_cs(0U, dev->cfg.user);
    rc = dev->cfg.read16(&word, dev->cfg.user);
    dev->cfg.set_cs(1U, dev->cfg.user);

    if ((dev->cfg.delay_us != NULL) && (dev->cfg.cs_high_us != 0U))
    {
        dev->cfg.delay_us(dev->cfg.cs_high_us, dev->cfg.user);
    }

    if (rc != 0)
    {
        dev->io_errors++;
        return ADS7948_STATUS_IO;
    }

    dev->frames++;
    dev->last_word = word;
    dev->last_code = ads7948_decode_10bit(word);
    *rx_word = word;
    return ADS7948_STATUS_OK;
}

int ads7948_read_channel(ads7948_t *dev, uint8_t channel, uint16_t *code)
{
    uint16_t word = 0U;
    uint8_t i;
    uint8_t frame_count;
    int rc;

    if ((dev == NULL) || (code == NULL) || (channel >= ADS7948_CHANNEL_COUNT) ||
        (dev->initialized == 0U))
    {
        return ADS7948_STATUS_PARAM;
    }

    dev->cfg.set_ch_sel(channel, dev->cfg.user);
    dev->last_channel = channel;

    if ((dev->cfg.delay_us != NULL) && (dev->cfg.input_settle_us != 0U))
    {
        dev->cfg.delay_us(dev->cfg.input_settle_us, dev->cfg.user);
    }

    frame_count = (uint8_t)(dev->cfg.discard_frames + 1U);
    for (i = 0U; i < frame_count; i++)
    {
        rc = ads7948_read_frame(dev, &word);
        if (rc != ADS7948_STATUS_OK)
        {
            return rc;
        }
    }

    *code = ads7948_decode_10bit(word);
    return ADS7948_STATUS_OK;
}
