/*
 * CH585M scan-frame ingest prototype for CH32H417.
 *
 * This module owns the H417-side frame contract for two CH585M sampling
 * frontends. The default backend is a deterministic fake source so the H417
 * merge/CRC/sequence path can be tested before real SPI wiring is available.
 */

#ifndef CH585_SPI_SCAN_H__
#define CH585_SPI_SCAN_H__

#include <stdint.h>

#ifndef APP_CH585_SPI_WIRE_SHORT
#define APP_CH585_SPI_WIRE_SHORT 1
#endif

#define CH585_SCAN_FRAME_MAGIC       0x4BD3U /* Wire bytes: d3 4b. MSB=1 avoids first-bit idle-high ambiguity. */
#define CH585_SCAN_FRAME_VERSION     2U
#define CH585_SCAN_FRAME_TYPE_KEY_STATE 0x10U
#define CH585_SCAN_CMD_MAGIC         0x524BU /* Wire bytes: 4b 52, ASCII-ish "KR". */
#define CH585_SCAN_CMD_GET_STATE     0x01U
#define CH585_SCAN_CMD_GET_DEBUG     0x02U
#define CH585_SCAN_CMD_GET_CONFIG    0x03U
#define CH585_SCAN_CMD_SET_CONFIG    0x04U
#define CH585_SCAN_CMD_CALIBRATE_KEY 0x05U
#define CH585_SCAN_CMD_CALIBRATE_ALL 0x06U
#define CH585_SCAN_SHORT_FRAME_MAGIC 0xD7U
#define CH585_SCAN_SHORT_FRAME_TYPE_KEY_STATE 0x11U
#define CH585_SCAN_SHORT_FRAME_TYPE_KEY_DEBUG 0x12U
#define CH585_SCAN_SHORT_CMD_MAGIC   0xA7U
#define CH585_SCAN_SOURCE_COUNT      2U
#define CH585_SCAN_KEYS_LEFT         36U
#define CH585_SCAN_KEYS_RIGHT        41U
#define CH585_SCAN_KEYS_PER_SOURCE   CH585_SCAN_KEYS_RIGHT
#define CH585_SCAN_TOTAL_KEYS        (CH585_SCAN_SOURCE_COUNT * CH585_SCAN_KEYS_PER_SOURCE)
#define CH585_SCAN_DOWN_BYTES        ((CH585_SCAN_KEYS_PER_SOURCE + 7U) / 8U)
#define CH585_SCAN_RELEASED_ADC      1000U
#define CH585_SCAN_PRESSED_ADC       3000U

#define CH585_SCAN_FLAG_OVERRUN      (1U << 0)
#define CH585_SCAN_FLAG_ADC_ERROR    (1U << 1)
#define CH585_SCAN_FLAG_STALE        (1U << 2)
#define CH585_SCAN_FLAG_SYNC_LOST    (1U << 3)
#define CH585_SCAN_FLAG_CMD_ERROR    (1U << 4)
#define CH585_SCAN_FLAG_READY        (1U << 15)

#define CH585_SCAN_SHORT_FLAG_OVERRUN   (1U << 0)
#define CH585_SCAN_SHORT_FLAG_ADC_ERROR (1U << 1)
#define CH585_SCAN_SHORT_FLAG_STALE     (1U << 2)
#define CH585_SCAN_SHORT_FLAG_SYNC_LOST (1U << 3)
#define CH585_SCAN_SHORT_FLAG_CMD_ERROR (1U << 4)
#define CH585_SCAN_SHORT_FLAG_READY     (1U << 7)
#define CH585_SCAN_SHORT_DEBUG_FLAG_DOWN     (1U << 0)
#define CH585_SCAN_SHORT_DEBUG_FLAG_RT_ARMED (1U << 1)

#define CH585_SCAN_CFG_RELEASED_ADC       0x01U
#define CH585_SCAN_CFG_PRESSED_ADC        0x02U
#define CH585_SCAN_CFG_MIN_ADC            0x03U
#define CH585_SCAN_CFG_MAX_ADC            0x04U
#define CH585_SCAN_CFG_PRESS_POSITION     0x05U
#define CH585_SCAN_CFG_RELEASE_POSITION   0x06U
#define CH585_SCAN_CFG_RT_PRESS_DELTA     0x07U
#define CH585_SCAN_CFG_RT_RELEASE_DELTA   0x08U
#define CH585_SCAN_CFG_FILTER_SHIFT       0x09U
#define CH585_SCAN_CFG_RT_ENABLE          0x0AU
#define CH585_SCAN_CFG_VALID              0x0BU
#define CH585_SCAN_CFG_GLOBAL_KEY_ID      0x0CU

typedef struct __attribute__((packed))
{
    uint16_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t source_id;
    uint8_t key_count;
    uint16_t seq;
    uint16_t flags;
    uint16_t ack_seq;
    uint8_t down_bits[CH585_SCAN_DOWN_BYTES];
    uint16_t diag;
    uint16_t crc16;
} ch585_scan_frame_v2_t;

typedef struct __attribute__((packed))
{
    uint16_t magic;
    uint8_t version;
    uint8_t cmd;
    uint16_t host_seq;
    uint16_t ack_seq;
    uint16_t flags;
    uint16_t crc16;
} ch585_scan_cmd_legacy_t;

typedef struct __attribute__((packed))
{
    uint8_t magic;
    uint8_t type;
    uint8_t source_id;
    uint8_t seq;
    uint8_t ack_seq;
    uint8_t flags;
    uint8_t down_bits[CH585_SCAN_DOWN_BYTES];
    uint16_t crc16;
} ch585_scan_frame_short_t;

typedef struct __attribute__((packed))
{
    uint8_t magic;
    uint8_t type;
    uint8_t source_id;
    uint8_t seq;
    uint8_t key_id;
    uint8_t flags;
    uint16_t raw_adc;
    uint16_t filtered_adc;
    uint16_t position_pm;
    uint16_t peak_pm;
    uint16_t crc16;
} ch585_scan_debug_short_t;

typedef struct __attribute__((packed))
{
    uint8_t magic;
    uint8_t cmd;
    uint8_t host_seq;
    uint8_t ack_seq;
    uint8_t target_key;
    uint8_t param_id;
    uint16_t value;
    uint16_t flags;
    uint16_t aux;
    uint8_t reserved[2];
    uint16_t crc16;
} ch585_scan_cmd_short_t;

#if APP_CH585_SPI_WIRE_SHORT
typedef ch585_scan_frame_short_t ch585_scan_frame_v1_t;
typedef ch585_scan_cmd_short_t ch585_scan_cmd_v1_t;
#else
typedef ch585_scan_frame_v2_t ch585_scan_frame_v1_t;
typedef ch585_scan_cmd_legacy_t ch585_scan_cmd_v1_t;
#endif

typedef struct
{
    uint32_t frames_ok;
    uint32_t fetch_errors;
    uint32_t magic_errors;
    uint32_t version_errors;
    uint32_t source_errors;
    uint32_t length_errors;
    uint32_t crc_errors;
    uint32_t seq_drops;
    uint32_t flag_overrun;
    uint32_t flag_adc_error;
    uint32_t flag_stale;
    uint32_t flag_sync_lost;
    uint16_t last_seq;
    uint8_t have_seq;
} ch585_scan_source_stats_t;

typedef struct
{
    uint32_t frames;
    uint8_t valid;
    uint8_t seq;
    uint8_t key_id;
    uint8_t flags;
    uint8_t is_down;
    uint8_t rt_armed;
    uint16_t raw_adc;
    uint16_t filtered_adc;
    uint16_t position_pm;
    uint16_t peak_pm;
} ch585_scan_debug_status_t;

int ch585_spi_scan_init(void);
void ch585_spi_scan_poll_once(void);
void ch585_spi_scan_dump_stats(void);
const uint16_t *ch585_spi_scan_raw(void);
const ch585_scan_source_stats_t *ch585_spi_scan_source_stats(uint8_t source_id);
int ch585_spi_scan_source0_debug_status(ch585_scan_debug_status_t *out);
int ch585_spi_scan_source0_queue_get_debug(uint8_t key_id);
int ch585_spi_scan_source0_queue_get_config(uint8_t key_id, uint8_t param_id);
int ch585_spi_scan_source0_queue_set_config(uint8_t key_id, uint8_t param_id, uint16_t value);
int ch585_spi_scan_source0_queue_calibrate_key(uint8_t key_id, uint16_t flags);
uint32_t ch585_spi_scan_source0_sck_khz_x10(void);
uint16_t ch585_spi_scan_source0_prescaler(void);
uint8_t ch585_spi_scan_source0_hsrx(void);
uint8_t ch585_spi_scan_source0_cpha_edges(void);
uint8_t ch585_spi_scan_source0_train_done(void);
uint16_t ch585_spi_scan_source0_train_errors(void);
uint16_t ch585_spi_scan_source0_train_frames(void);
uint32_t ch585_spi_scan_source0_cmd_queued(void);
uint32_t ch585_spi_scan_source0_cmd_sent(void);
uint32_t ch585_spi_scan_source0_debug_cmd_sent(void);
uint32_t ch585_spi_scan_source0_calibrate_cmd_sent(void);
uint8_t ch585_spi_scan_source0_last_cmd(void);
uint8_t ch585_spi_scan_source0_last_frame_type(void);
uint8_t ch585_spi_scan_source0_last_frame_seq(void);
uint8_t ch585_spi_scan_source0_last_resync_reason(void);
uint8_t ch585_spi_scan_source0_slave_diag_cmd(void);
uint8_t ch585_spi_scan_source0_slave_diag_host_seq(void);
uint8_t ch585_spi_scan_source0_slave_diag_valid(void);
uint8_t ch585_spi_scan_source0_slave_diag_cmd_error(void);
uint8_t ch585_spi_scan_source0_slave_diag_invalid_count(void);
uint8_t ch585_spi_scan_source0_slave_diag_invalid_reason(void);
uint8_t ch585_spi_scan_source0_slave_diag_raw_magic(void);
uint8_t ch585_spi_scan_source0_slave_diag_raw_cmd(void);
uint8_t ch585_spi_scan_source0_slave_diag_raw_host_seq(void);
uint8_t ch585_spi_scan_source0_slave_diag_raw_ack_seq(void);
uint16_t ch585_spi_scan_source0_slave_diag_rx_crc(void);
uint16_t ch585_spi_scan_source0_slave_diag_expected_crc(void);
uint32_t ch585_spi_scan_source0_ack_errors(void);
uint16_t ch585_spi_scan_source0_host_seq(void);
uint8_t ch585_spi_scan_source0_train_candidate_count(void);
int ch585_spi_scan_source0_train_candidate(uint8_t index,
                                           uint16_t *prescaler,
                                           uint8_t *hsrx,
                                           uint8_t *cpha_edges,
                                           uint16_t *bad_errors,
                                           uint16_t *seq_errors);
uint16_t ch585_spi_scan_crc16(const uint8_t *data, uint16_t len);

#endif /* CH585_SPI_SCAN_H__ */
