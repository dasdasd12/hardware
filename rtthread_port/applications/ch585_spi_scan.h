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
#define CH585_SCAN_SHORT_FRAME_MAGIC 0xD7U
#define CH585_SCAN_SHORT_FRAME_TYPE_KEY_STATE 0x11U
#define CH585_SCAN_SHORT_CMD_MAGIC   0xA7U
#define CH585_SCAN_SOURCE_COUNT      2U
#define CH585_SCAN_KEYS_PER_SOURCE   64U
#define CH585_SCAN_TOTAL_KEYS        (CH585_SCAN_SOURCE_COUNT * CH585_SCAN_KEYS_PER_SOURCE)
#define CH585_SCAN_DOWN_BYTES        (CH585_SCAN_KEYS_PER_SOURCE / 8U)
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
    uint8_t cmd;
    uint8_t host_seq;
    uint8_t ack_seq;
    uint8_t flags;
    uint8_t reserved;
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

int ch585_spi_scan_init(void);
void ch585_spi_scan_poll_once(void);
void ch585_spi_scan_dump_stats(void);
const uint16_t *ch585_spi_scan_raw(void);
const ch585_scan_source_stats_t *ch585_spi_scan_source_stats(uint8_t source_id);
uint32_t ch585_spi_scan_source0_sck_khz_x10(void);
uint16_t ch585_spi_scan_source0_prescaler(void);
uint8_t ch585_spi_scan_source0_hsrx(void);
uint8_t ch585_spi_scan_source0_cpha_edges(void);
uint8_t ch585_spi_scan_source0_train_done(void);
uint16_t ch585_spi_scan_source0_train_errors(void);
uint16_t ch585_spi_scan_source0_train_frames(void);
uint8_t ch585_spi_scan_source0_train_candidate_count(void);
int ch585_spi_scan_source0_train_candidate(uint8_t index,
                                           uint16_t *prescaler,
                                           uint8_t *hsrx,
                                           uint8_t *cpha_edges,
                                           uint16_t *bad_errors,
                                           uint16_t *seq_errors);
uint16_t ch585_spi_scan_crc16(const uint8_t *data, uint16_t len);

#endif /* CH585_SPI_SCAN_H__ */
