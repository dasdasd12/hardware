#include "ch585_common.h"

#include "CH58x_spi.h"
#include "ads7948.h"
#include "ch585_ads7948_mux_scan.h"

#ifndef CH585_ADS7948_PROBE_RIGHT_HALF
#define CH585_ADS7948_PROBE_RIGHT_HALF 1
#endif

#ifndef CH585_ADS7948_PROBE_KEY
#define CH585_ADS7948_PROBE_KEY 58
#endif

#ifndef CH585_ADS7948_PROBE_PERIOD_MS
#define CH585_ADS7948_PROBE_PERIOD_MS 50
#endif

#ifndef CH585_ADS7948_PROBE_SPI1_DIV
#define CH585_ADS7948_PROBE_SPI1_DIV 2
#endif

#ifndef CH585_ADS7948_PROBE_SETTLE_US
#define CH585_ADS7948_PROBE_SETTLE_US 20
#endif

#ifndef CH585_ADS7948_PROBE_CS_HIGH_US
#define CH585_ADS7948_PROBE_CS_HIGH_US 0
#endif

#ifndef CH585_ADS7948_PROBE_DISCARD_FRAMES
#define CH585_ADS7948_PROBE_DISCARD_FRAMES 1
#endif

#ifndef CH585_ADS7948_PROBE_PDEN_ENABLE_LEVEL
#define CH585_ADS7948_PROBE_PDEN_ENABLE_LEVEL 1
#endif

#ifndef CH585_ADS7948_PROBE_RELEASED_ADC
#define CH585_ADS7948_PROBE_RELEASED_ADC CH585_MUX_SCAN_DEFAULT_RELEASED_ADC
#endif

#ifndef CH585_ADS7948_PROBE_PRESSED_ADC
#define CH585_ADS7948_PROBE_PRESSED_ADC CH585_MUX_SCAN_DEFAULT_PRESSED_ADC
#endif

#ifndef CH585_ADS7948_PROBE_PRESS_ADC
#define CH585_ADS7948_PROBE_PRESS_ADC CH585_MUX_SCAN_DEFAULT_PRESS_ADC
#endif

#ifndef CH585_ADS7948_PROBE_RELEASE_ADC
#define CH585_ADS7948_PROBE_RELEASE_ADC CH585_MUX_SCAN_DEFAULT_RELEASE_ADC
#endif

#define CH585_ADS7948_PROBE_SCAN_ALL 0xFFu

typedef struct
{
    uint8_t adc_index;
} ch585_ads7948_probe_adc_user_t;

static ads7948_t g_ads[CH585_MUX_SCAN_ADC_COUNT];
static ch585_ads7948_probe_adc_user_t g_ads_user[CH585_MUX_SCAN_ADC_COUNT] = {
    {0u},
    {1u},
};
static ch585_mux_scan_t g_scan;
static uint8_t g_report_key;

static void probe_delay_us(uint32_t us, void *user)
{
    (void)user;

    while(us > 60000u)
    {
        mDelayuS(60000u);
        us -= 60000u;
    }
    if(us != 0u)
    {
        mDelayuS((uint16_t)us);
    }
}

static void probe_set_pden(uint8_t enabled)
{
#if CH585_ADS7948_PROBE_PDEN_ENABLE_LEVEL
    if(enabled != 0u)
    {
        GPIOB_SetBits(GPIO_Pin_19);
    }
    else
    {
        GPIOB_ResetBits(GPIO_Pin_19);
    }
#else
    if(enabled != 0u)
    {
        GPIOB_ResetBits(GPIO_Pin_19);
    }
    else
    {
        GPIOB_SetBits(GPIO_Pin_19);
    }
#endif
}

static uint32_t probe_cs_pin(uint8_t adc_index)
{
#if CH585_ADS7948_PROBE_RIGHT_HALF
    return (adc_index == 0u) ? GPIO_Pin_15 : GPIO_Pin_14;
#else
    return (adc_index == 0u) ? GPIO_Pin_14 : GPIO_Pin_15;
#endif
}

static void probe_set_cs(uint8_t level, void *user)
{
    const ch585_ads7948_probe_adc_user_t *ctx =
        (const ch585_ads7948_probe_adc_user_t *)user;
    uint32_t pin = probe_cs_pin(ctx->adc_index);

    if(level != 0u)
    {
        GPIOB_SetBits(pin);
    }
    else
    {
        GPIOB_ResetBits(pin);
    }
}

static void probe_set_ch_sel(uint8_t level, void *user)
{
    (void)user;

    if(level != 0u)
    {
        GPIOB_SetBits(GPIO_Pin_18);
    }
    else
    {
        GPIOB_ResetBits(GPIO_Pin_18);
    }
}

static int probe_read16(uint16_t *rx_word, void *user)
{
    uint8_t hi;
    uint8_t lo;

    (void)user;

    if(rx_word == 0)
    {
        return -1;
    }

    hi = SPI1_MasterRecvByte();
    lo = SPI1_MasterRecvByte();
    *rx_word = (uint16_t)(((uint16_t)hi << 8) | lo);
    return 0;
}

static void probe_set_mux_addr(uint8_t mux_channel, void *user)
{
    uint32_t pins = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
    uint32_t value = 0u;

    (void)user;

    if((mux_channel & 0x01u) != 0u)
    {
        value |= GPIO_Pin_0;
    }
    if((mux_channel & 0x02u) != 0u)
    {
        value |= GPIO_Pin_1;
    }
    if((mux_channel & 0x04u) != 0u)
    {
        value |= GPIO_Pin_2;
    }
    if((mux_channel & 0x08u) != 0u)
    {
        value |= GPIO_Pin_3;
    }

    GPIOB_ResetBits(pins);
    if(value != 0u)
    {
        GPIOB_SetBits(value);
    }
}

static void probe_gpio_init(void)
{
    GPIOA_ModeCfg(GPIO_Pin_0 | GPIO_Pin_1, GPIO_ModeOut_PP_20mA);
    GPIOA_ModeCfg(GPIO_Pin_2, GPIO_ModeIN_PU);

    GPIOB_SetBits(GPIO_Pin_14 | GPIO_Pin_15);
    GPIOB_ModeCfg(GPIO_Pin_14 | GPIO_Pin_15, GPIO_ModeOut_PP_20mA);
    GPIOB_ResetBits(GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3);
    GPIOB_ModeCfg(GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3,
                  GPIO_ModeOut_PP_20mA);
    GPIOB_ModeCfg(GPIO_Pin_18 | GPIO_Pin_19, GPIO_ModeOut_PP_20mA);

    probe_set_ch_sel(0u, 0);
    probe_set_pden(1u);
}

static int probe_scan_init(void)
{
    ads7948_config_t ads_cfg;
    ch585_mux_scan_config_t scan_cfg;
    uint8_t i;

    probe_gpio_init();
    SPI1_MasterDefInit();
    SPI1_CLKCfg((uint8_t)CH585_ADS7948_PROBE_SPI1_DIV);
    SPI1_DataMode(Mode0_HighBitINFront);

    for(i = 0u; i < CH585_MUX_SCAN_ADC_COUNT; i++)
    {
        ads7948_default_config(&ads_cfg);
        ads_cfg.set_cs = probe_set_cs;
        ads_cfg.set_ch_sel = probe_set_ch_sel;
        ads_cfg.read16 = probe_read16;
        ads_cfg.delay_us = probe_delay_us;
        ads_cfg.user = &g_ads_user[i];
        ads_cfg.input_settle_us = (uint16_t)CH585_ADS7948_PROBE_SETTLE_US;
        ads_cfg.cs_high_us = (uint16_t)CH585_ADS7948_PROBE_CS_HIGH_US;
        ads_cfg.discard_frames =
            (uint8_t)CH585_ADS7948_PROBE_DISCARD_FRAMES;
        if(ads7948_init(&g_ads[i], &ads_cfg) != ADS7948_STATUS_OK)
        {
            return -1;
        }
    }

    ch585_mux_scan_default_config(&scan_cfg);
    scan_cfg.adc[0] = &g_ads[0];
    scan_cfg.adc[1] = &g_ads[1];
    scan_cfg.set_mux_addr = probe_set_mux_addr;
    scan_cfg.delay_us = probe_delay_us;
    scan_cfg.mux_settle_us = (uint16_t)CH585_ADS7948_PROBE_SETTLE_US;
    scan_cfg.released_adc = (uint16_t)CH585_ADS7948_PROBE_RELEASED_ADC;
    scan_cfg.pressed_adc = (uint16_t)CH585_ADS7948_PROBE_PRESSED_ADC;
    scan_cfg.press_adc = (uint16_t)CH585_ADS7948_PROBE_PRESS_ADC;
    scan_cfg.release_adc = (uint16_t)CH585_ADS7948_PROBE_RELEASE_ADC;

    return ch585_mux_scan_init(&g_scan, &scan_cfg);
}

static uint8_t probe_next_report_key(void)
{
    uint8_t configured = (uint8_t)CH585_ADS7948_PROBE_KEY;

    if(configured != CH585_ADS7948_PROBE_SCAN_ALL)
    {
        return configured;
    }

    g_report_key++;
    if(g_report_key >= CH585_MUX_SCAN_KEY_COUNT)
    {
        g_report_key = 0u;
    }
    return g_report_key;
}

static void log_kv_dec(const char *key, uint32_t value)
{
    ch585_log_str(key);
    ch585_log_str("=");
    ch585_log_u32_dec(value);
    ch585_log_str(" ");
}

static void probe_log_sample(uint8_t key, int status)
{
    const uint16_t *raw = ch585_mux_scan_front_raw(&g_scan);
    const uint16_t *filtered = ch585_mux_scan_filtered(&g_scan);
    const ch585_mux_scan_stats_t *stats = ch585_mux_scan_stats(&g_scan);

    if((raw == 0) || (filtered == 0) || (stats == 0))
    {
        ch585_log_fail("ads7948_mux_probe", "scan_not_ready");
        return;
    }

    ch585_log_str("AP ");
    log_kv_dec("seq", stats->seq);
    log_kv_dec("key", key);
    log_kv_dec("lane", key / CH585_MUX_SCAN_MUX_CHANNEL_COUNT);
    log_kv_dec("mux", key % CH585_MUX_SCAN_MUX_CHANNEL_COUNT);
    log_kv_dec("raw", raw[key]);
    log_kv_dec("filt", filtered[key]);
    log_kv_dec("down", g_scan.down[key]);
    log_kv_dec("flags", stats->flags);
    log_kv_dec("adc_err", stats->adc_errors);
    log_kv_dec("st", (uint32_t)status);
    ch585_log_str("\r\n");
}

void ch585_ads7948_mux_probe_run(void)
{
    int init_status;

    ch585_log_line("INFO ADS7948/MUX probe pins: SPI1 PA0/PA1/PA2, MUX PB0..PB3, CHSEL PB18, PDEN PB19, CS PB14/PB15");
    ch585_log_str("INFO key=");
    ch585_log_u32_dec(CH585_ADS7948_PROBE_KEY);
    ch585_log_str(" right=");
    ch585_log_u32_dec(CH585_ADS7948_PROBE_RIGHT_HALF);
    ch585_log_str(" spi1_div=");
    ch585_log_u32_dec(CH585_ADS7948_PROBE_SPI1_DIV);
    ch585_log_str("\r\n");

    init_status = probe_scan_init();
    if(init_status != CH585_MUX_SCAN_STATUS_OK)
    {
        ch585_log_fail("ads7948_mux_probe_init", "driver_init_failed");
        return;
    }
    ch585_log_pass("ads7948_mux_probe_init");

    while(1)
    {
        uint8_t key = probe_next_report_key();
        int status = ch585_mux_scan_poll(&g_scan);
        probe_log_sample(key, status);
        ch585_delay_ms((uint16_t)CH585_ADS7948_PROBE_PERIOD_MS);
    }
}
