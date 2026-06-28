/*
 * Compact CH585->H417 sideband status frame.
 *
 * This is for local peripherals that are not part of the 64-key down_bits
 * stream: battery, charging, EEPROM status, EC11, five-way switch and future
 * profile/config acknowledgements.
 */

#ifndef CH585_SIDEBAND_H__
#define CH585_SIDEBAND_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CH585_SIDEBAND_MAGIC              0xC5U
#define CH585_SIDEBAND_VERSION            1U
#define CH585_SIDEBAND_TYPE_STATUS        0x51U
#define CH585_SIDEBAND_MAX_EVENTS         8U
#define CH585_SIDEBAND_FIXED_SIZE         16U
#define CH585_SIDEBAND_EVENT_SIZE         4U
#define CH585_SIDEBAND_MAX_WIRE_SIZE \
    (CH585_SIDEBAND_FIXED_SIZE + \
     (CH585_SIDEBAND_MAX_EVENTS * CH585_SIDEBAND_EVENT_SIZE) + 1U)

#define CH585_SIDEBAND_SOURCE_LEFT        1U
#define CH585_SIDEBAND_SOURCE_RIGHT       2U

#define CH585_SIDEBAND_FLAG_BAT_VALID     (1U << 0)
#define CH585_SIDEBAND_FLAG_INPUT_VALID   (1U << 1)
#define CH585_SIDEBAND_FLAG_EEPROM_OK     (1U << 2)
#define CH585_SIDEBAND_FLAG_I2C_ERROR     (1U << 3)

#define CH585_SIDEBAND_ALERT_BAT_LOW      (1U << 0)
#define CH585_SIDEBAND_ALERT_BAT_PIN      (1U << 1)
#define CH585_SIDEBAND_ALERT_CHARGING     (1U << 2)
#define CH585_SIDEBAND_ALERT_STANDBY      (1U << 3)

typedef enum
{
    CH585_SIDEBAND_EVENT_NONE = 0,
    CH585_SIDEBAND_EVENT_ENCODER_DELTA = 1,
    CH585_SIDEBAND_EVENT_ENCODER_BUTTON = 2,
    CH585_SIDEBAND_EVENT_FIVEWAY = 3,
    CH585_SIDEBAND_EVENT_BATTERY = 4,
    CH585_SIDEBAND_EVENT_EEPROM = 5
} ch585_sideband_event_type_t;

typedef enum
{
    CH585_SIDEBAND_STATUS_OK = 0,
    CH585_SIDEBAND_STATUS_PARAM = -1,
    CH585_SIDEBAND_STATUS_SHORT = -2,
    CH585_SIDEBAND_STATUS_MAGIC = -3,
    CH585_SIDEBAND_STATUS_CRC = -4
} ch585_sideband_status_t;

typedef struct
{
    uint8_t type;
    uint8_t id;
    int8_t value;
    uint8_t flags;
} ch585_sideband_event_t;

typedef struct
{
    uint8_t source_id;
    uint8_t seq;
    uint8_t flags;
    uint8_t alert_flags;
    uint16_t vbat_mv;
    uint16_t soc_q8_percent;
    uint16_t input_mask;
    uint8_t charge_state;
    uint8_t event_count;
    ch585_sideband_event_t events[CH585_SIDEBAND_MAX_EVENTS];
} ch585_sideband_frame_t;

uint8_t ch585_sideband_crc8(const uint8_t *data, uint16_t len);
int ch585_sideband_encode(const ch585_sideband_frame_t *frame,
                          uint8_t *wire,
                          uint16_t wire_cap,
                          uint16_t *wire_len);
int ch585_sideband_decode(const uint8_t *wire,
                          uint16_t wire_len,
                          ch585_sideband_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* CH585_SIDEBAND_H__ */
