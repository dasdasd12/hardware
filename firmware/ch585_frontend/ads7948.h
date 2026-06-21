/*
 * ADS7948 conservative read driver.
 *
 * The ADS7948 is a dual-channel, 10-bit SAR ADC with a 16-clock
 * SPI-compatible read-only frame. Channel selection is done with the
 * CH SEL pin, not with command bits.
 *
 * This file is hardware-portable on purpose. The CH585 project should provide
 * GPIO/SPI/delay callbacks and keep the MCU-specific register code outside of
 * this module.
 */

#ifndef ADS7948_H__
#define ADS7948_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADS7948_CHANNEL_COUNT        2U
#define ADS7948_RESOLUTION_BITS      10U
#define ADS7948_CODE_MAX             1023U
#define ADS7948_DEFAULT_DISCARDS     1U
#define ADS7948_DEFAULT_SETTLE_US    1U
#define ADS7948_DEFAULT_CS_HIGH_US   1U

typedef enum
{
    ADS7948_STATUS_OK = 0,
    ADS7948_STATUS_PARAM = -1,
    ADS7948_STATUS_IO = -2
} ads7948_status_t;

typedef void (*ads7948_gpio_write_fn)(uint8_t level, void *user);
typedef void (*ads7948_delay_us_fn)(uint32_t us, void *user);

/*
 * Clock exactly one ADS7948 frame while CS is already low.
 *
 * The callback should return 0 on success and place the MSB-first 16-bit word
 * in rx_word. For ADS7948 the 10-bit result is expected in bits [15:6].
 */
typedef int (*ads7948_read16_fn)(uint16_t *rx_word, void *user);

typedef struct
{
    ads7948_gpio_write_fn set_cs;
    ads7948_gpio_write_fn set_ch_sel;
    ads7948_read16_fn read16;
    ads7948_delay_us_fn delay_us;
    void *user;

    /*
     * input_settle_us covers external MUX settling plus ADS input settling
     * before the first dummy frame. Keep this conservative during bring-up.
     */
    uint16_t input_settle_us;
    uint16_t cs_high_us;

    /*
     * The first frame after a channel/MUX change returns the previous
     * conversion. Default 1 means: one dummy frame, then one valid frame.
     */
    uint8_t discard_frames;
} ads7948_config_t;

typedef struct
{
    ads7948_config_t cfg;
    uint32_t frames;
    uint32_t io_errors;
    uint16_t last_word;
    uint16_t last_code;
    uint8_t last_channel;
    uint8_t initialized;
} ads7948_t;

void ads7948_default_config(ads7948_config_t *cfg);
int ads7948_init(ads7948_t *dev, const ads7948_config_t *cfg);
int ads7948_read_frame(ads7948_t *dev, uint16_t *rx_word);
int ads7948_read_channel(ads7948_t *dev, uint8_t channel, uint16_t *code);
uint16_t ads7948_decode_10bit(uint16_t rx_word);

#ifdef __cplusplus
}
#endif

#endif /* ADS7948_H__ */
