/*
 * CH585 front-end scanner for ADS7948 + external 16:1 MUX chains.
 *
 * Default logical layout for one CH585:
 *   lane 0 -> MUX0 -> local keys  0..15
 *   lane 1 -> MUX1 -> local keys 16..31
 *   lane 2 -> MUX2 -> local keys 32..47
 *   lane 3 -> MUX3 -> local keys 48..63
 *
 * Each lane maps to one ADS7948 input channel. The default hardware model is
 * two ADS7948 devices, two channels each, giving four ADC lanes per CH585.
 */

#ifndef CH585_ADS7948_MUX_SCAN_H__
#define CH585_ADS7948_MUX_SCAN_H__

#include <stdint.h>

#include "ads7948.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CH585_MUX_SCAN_ADC_COUNT             2U
#define CH585_MUX_SCAN_LANE_COUNT            4U
#define CH585_MUX_SCAN_MUX_CHANNEL_COUNT     16U
#define CH585_MUX_SCAN_KEY_COUNT \
    (CH585_MUX_SCAN_LANE_COUNT * CH585_MUX_SCAN_MUX_CHANNEL_COUNT)
#define CH585_MUX_SCAN_DOWN_BYTES \
    (CH585_MUX_SCAN_KEY_COUNT / 8U)

#define CH585_MUX_SCAN_FLAG_ADC_ERROR        (1U << 0)
#define CH585_MUX_SCAN_FLAG_SETTLE_DISCARD   (1U << 1)

#define CH585_MUX_SCAN_DEFAULT_MUX_SETTLE_US 5U
#define CH585_MUX_SCAN_DEFAULT_OVERSAMPLE    4U
#define CH585_MUX_SCAN_DEFAULT_FILTER_SHIFT  2U
#define CH585_MUX_SCAN_DEFAULT_RELEASED_ADC  200U
#define CH585_MUX_SCAN_DEFAULT_PRESSED_ADC   850U
#define CH585_MUX_SCAN_DEFAULT_PRESS_ADC     550U
#define CH585_MUX_SCAN_DEFAULT_RELEASE_ADC   450U

typedef enum
{
    CH585_MUX_SCAN_STATUS_OK = 0,
    CH585_MUX_SCAN_STATUS_PARAM = -1,
    CH585_MUX_SCAN_STATUS_ADC = -2
} ch585_mux_scan_status_t;

typedef void (*ch585_mux_scan_set_addr_fn)(uint8_t mux_channel, void *user);
typedef void (*ch585_mux_scan_delay_us_fn)(uint32_t us, void *user);

typedef struct
{
    ads7948_t *adc[CH585_MUX_SCAN_ADC_COUNT];
    ch585_mux_scan_set_addr_fn set_mux_addr;
    ch585_mux_scan_delay_us_fn delay_us;
    void *user;

    uint16_t mux_settle_us;
    uint8_t lane_count;
    uint8_t lane_adc_index[CH585_MUX_SCAN_LANE_COUNT];
    uint8_t lane_adc_channel[CH585_MUX_SCAN_LANE_COUNT];
    uint8_t oversample_count;
    uint8_t discard_after_mux_switch;
    uint8_t filter_shift;

    /*
     * Thresholds are raw ADS7948 10-bit codes. If pressed_adc is lower than
     * released_adc, the scanner automatically treats the key as inverted.
     */
    uint16_t released_adc;
    uint16_t pressed_adc;
    uint16_t press_adc;
    uint16_t release_adc;
} ch585_mux_scan_config_t;

typedef struct
{
    uint32_t frames;
    uint32_t adc_errors;
    uint32_t discarded_reads;
    uint16_t seq;
    uint8_t flags;
} ch585_mux_scan_stats_t;

typedef struct
{
    ch585_mux_scan_config_t cfg;
    uint16_t raw[2][CH585_MUX_SCAN_KEY_COUNT];
    uint16_t filtered[CH585_MUX_SCAN_KEY_COUNT];
    uint32_t filtered_q8[CH585_MUX_SCAN_KEY_COUNT];
    uint8_t down[CH585_MUX_SCAN_KEY_COUNT];
    uint8_t front_index;
    uint8_t initialized;
    ch585_mux_scan_stats_t stats;
} ch585_mux_scan_t;

void ch585_mux_scan_default_config(ch585_mux_scan_config_t *cfg);
int ch585_mux_scan_init(ch585_mux_scan_t *scan,
                        const ch585_mux_scan_config_t *cfg);
int ch585_mux_scan_poll(ch585_mux_scan_t *scan);
const uint16_t *ch585_mux_scan_front_raw(const ch585_mux_scan_t *scan);
const uint16_t *ch585_mux_scan_filtered(const ch585_mux_scan_t *scan);
const ch585_mux_scan_stats_t *ch585_mux_scan_stats(const ch585_mux_scan_t *scan);
void ch585_mux_scan_front_down_bits(const ch585_mux_scan_t *scan,
                                    uint8_t down_bits[CH585_MUX_SCAN_DOWN_BYTES]);

#ifdef __cplusplus
}
#endif

#endif /* CH585_ADS7948_MUX_SCAN_H__ */
