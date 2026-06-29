#ifndef CH585_ADS7948_MUX_ACQ_H__
#define CH585_ADS7948_MUX_ACQ_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CH585_ADS7948_MUX_ADC_COUNT 2U
#define CH585_ADS7948_MUX_LANE_COUNT 4U
#define CH585_ADS7948_MUX_MUX_CHANNEL_COUNT 16U
#define CH585_ADS7948_MUX_KEY_COUNT \
    (CH585_ADS7948_MUX_LANE_COUNT * CH585_ADS7948_MUX_MUX_CHANNEL_COUNT)

typedef enum
{
    CH585_ADS7948_MUX_SIDE_LEFT = 0,
    CH585_ADS7948_MUX_SIDE_RIGHT = 1
} ch585_ads7948_mux_side_t;

typedef struct
{
    uint8_t adc_index;
    uint8_t adc_channel;
    uint32_t cs_pin;
    const char *cs_name;
    uint8_t mux_first;
    uint8_t mux_count;
} ch585_ads7948_mux_lane_t;

typedef struct
{
    const char *name;
    const char *target;
    ch585_ads7948_mux_lane_t lanes[CH585_ADS7948_MUX_LANE_COUNT];
} ch585_ads7948_mux_profile_t;

typedef struct
{
    const ch585_ads7948_mux_profile_t *profile;
    uint16_t raw[CH585_ADS7948_MUX_KEY_COUNT];
    uint32_t frames;
    uint32_t spi_frames;
} ch585_ads7948_mux_acq_t;

const ch585_ads7948_mux_profile_t *ch585_ads7948_mux_profile(
    ch585_ads7948_mux_side_t side);
uint8_t ch585_ads7948_mux_profile_active_keys(
    const ch585_ads7948_mux_profile_t *profile);
const char *ch585_ads7948_mux_cs_pin_name(uint32_t pin);
uint32_t ch585_ads7948_mux_target_sps(void);
uint32_t ch585_ads7948_mux_frame_sps(void);
uint32_t ch585_ads7948_mux_spi_clock_div(void);
void ch585_ads7948_mux_gpio_init(void);
int ch585_ads7948_mux_acq_init(
    ch585_ads7948_mux_acq_t *acq,
    const ch585_ads7948_mux_profile_t *profile);
void ch585_ads7948_mux_acq_poll(ch585_ads7948_mux_acq_t *acq);
int ch585_ads7948_mux_acq_read_compact_key(
    ch585_ads7948_mux_acq_t *acq,
    uint8_t compact_key,
    uint16_t *raw_out);
const uint16_t *ch585_ads7948_mux_acq_raw(
    const ch585_ads7948_mux_acq_t *acq);

#ifdef __cplusplus
}
#endif

#endif /* CH585_ADS7948_MUX_ACQ_H__ */
