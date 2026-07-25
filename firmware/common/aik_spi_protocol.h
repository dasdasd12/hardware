#ifndef AIK_SPI_PROTOCOL_H
#define AIK_SPI_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AIK_SPI_HOST_CMD_SIZE 32U
#define AIK_SPI_HALF_STATE_SIZE 12U

#define AIK_SPI_HOST_MAGIC 0xA6U
#define AIK_SPI_HALF_MAGIC 0x5AU
#define AIK_SPI_HALF_TYPE_STATE 0x11U
#define AIK_SPI_HALF_TYPE_PROFILE_STATUS 0x21U
#define AIK_SPI_HALF_TYPE_PROFILE_XFER 0x22U
#define AIK_SPI_VERSION 1U

#define AIK_SPI_CMD_POLL 0U
#define AIK_SPI_CMD_POLL_WITH_RF 1U
#define AIK_SPI_CMD_PUSH_RIGHT_STATE 2U
#define AIK_SPI_CMD_GET_PROFILE_STATUS 0x40U

/* Profile transfer command group. All commands keep the fixed 32-byte
 * aik_spi_host_cmd_v1_t frame; command payloads live in the 24-byte
 * region normally used by nkro16[16] + reserved[8] (bytes 6..29).
 * The half responds to each of these with an
 * AIK_SPI_HALF_TYPE_PROFILE_XFER frame whose ack_seq echoes host_seq. */
#define AIK_SPI_CMD_PROFILE_BEGIN      0x41U
#define AIK_SPI_CMD_PROFILE_CHUNK      0x42U
#define AIK_SPI_CMD_PROFILE_COMMIT     0x43U
#define AIK_SPI_CMD_PROFILE_ABORT      0x44U
#define AIK_SPI_CMD_PROFILE_SET_ACTIVE 0x45U
#define AIK_SPI_CMD_PROFILE_GET_XFER   0x46U /* read back the transfer state
                                                without side effects (used to
                                                poll a slow flash commit) */

#define AIK_OUTPUT_MODE_USBHS 0U
#define AIK_OUTPUT_MODE_RF24  1U
#define AIK_OUTPUT_MODE_BLE   2U

#define AIK_SPI_FLAG_OUTPUT_MODE_MASK 0x03U
#define AIK_SPI_FLAG_APPROVAL_ACTIVE       0x04U
#define AIK_SPI_FLAG_APPROVAL_SELECTED_YES 0x08U
#define AIK_SPI_FLAG_RIGHT_STATE_VALID     0x10U

#define AIK_HALF_ID_LEFT 0U
#define AIK_HALF_ID_RIGHT 1U

#define AIK_KEY_COUNT_LEFT 36U
#define AIK_KEY_COUNT_RIGHT 41U
#define AIK_KEY_COUNT_TOTAL 77U
#define AIK_HALF_DOWN_BITS_BYTES 6U
#define AIK_LOCAL_STATE_BYTES 8U
#define AIK_NKRO_REPORT_BYTES 16U

#define AIK_LEFT_LOCAL_BIT_SCR_UP     36U
#define AIK_LEFT_LOCAL_BIT_SCR_DOWN   37U
#define AIK_LEFT_LOCAL_BIT_SCR_RIGHT  38U
#define AIK_LEFT_LOCAL_BIT_SCR_LEFT   39U
#define AIK_LEFT_LOCAL_BIT_SCR_CENTER 40U
#define AIK_LEFT_LOCAL_BIT_SCR_WHEEL_UP   41U
#define AIK_LEFT_LOCAL_BIT_SCR_WHEEL_DOWN 42U
/* Center-only gesture after direction classification, held until release. */
#define AIK_LEFT_LOCAL_BIT_SCR_CENTER_QUALIFIED 43U
#define AIK_HALF_FRAME_BITS_LEFT      44U

#define AIK_RIGHT_LOCAL_BIT_EC11_CW   41U
#define AIK_RIGHT_LOCAL_BIT_EC11_CCW  42U
#define AIK_RIGHT_LOCAL_BIT_EC11_MUTE 43U
#define AIK_RIGHT_LOCAL_ROTARY_CW_COUNT_SHIFT   44U
#define AIK_RIGHT_LOCAL_ROTARY_CCW_COUNT_SHIFT  46U
#define AIK_RIGHT_LOCAL_ROTARY_COUNT_MASK       0x03U
#define AIK_HALF_FRAME_BITS_RIGHT     48U

#define AIK_CONSUMER_USAGE_NONE        0x0000U
#define AIK_CONSUMER_USAGE_MUTE        0x00E2U
#define AIK_CONSUMER_USAGE_VOLUME_UP   0x00E9U
#define AIK_CONSUMER_USAGE_VOLUME_DOWN 0x00EAU

#define AIK_PROFILE_DEBUG_ID16 0xA117U
#define AIK_PROFILE_DEBUG_GENERATION16 1U

#define AIK_PROFILE_STATUS_FLAG_VALID   0x01U
#define AIK_PROFILE_STATUS_FLAG_DEFAULT 0x02U
#define AIK_PROFILE_STATUS_FLAG_BUSY    0x04U
#define AIK_PROFILE_STATUS_FLAG_ERROR   0x08U
#define AIK_PROFILE_STATUS_SLOT_SHIFT   4U
#define AIK_PROFILE_STATUS_SLOT_MASK    0x70U

/* Profile transfer flags (aik_spi_profile_begin_v1_t / _commit_v1_t). */
#define AIK_SPI_PROFILE_FLAG_ACTIVATE 0x01U /* activate after commit */
#define AIK_SPI_PROFILE_FLAG_PERSIST  0x02U /* write slot to Data-Flash */

/* Profile transfer state machine (aik_spi_profile_xfer_v1_t.state). */
#define AIK_SPI_XFER_STATE_IDLE       0x00U
#define AIK_SPI_XFER_STATE_RECEIVING  0x01U
#define AIK_SPI_XFER_STATE_WRITING    0x02U
#define AIK_SPI_XFER_STATE_DONE       0x03U
#define AIK_SPI_XFER_ERR_CRC          0x80U
#define AIK_SPI_XFER_ERR_RANGE        0x81U
#define AIK_SPI_XFER_ERR_STORE        0x82U
#define AIK_SPI_XFER_ERR_STATE        0x83U
#define AIK_SPI_XFER_ERR_UNSUPPORTED  0x84U

#define AIK_SPI_PROFILE_CHUNK_DATA_MAX 20U
#define AIK_SPI_HOST_CMD_PAYLOAD_SIZE  24U

#if defined(__GNUC__)
#define AIK_SPI_PACKED __attribute__((packed))
#else
#define AIK_SPI_PACKED
#endif

typedef struct AIK_SPI_PACKED
{
    uint8_t magic;
    uint8_t version;
    uint8_t cmd;
    uint8_t flags;
    uint16_t host_seq;
    uint8_t nkro16[AIK_NKRO_REPORT_BYTES];
    uint8_t reserved[8];
    uint16_t crc16;
} aik_spi_host_cmd_v1_t;

typedef struct AIK_SPI_PACKED
{
    uint8_t magic;
    uint8_t type;
    uint16_t half_seq;
    uint8_t down_bits[AIK_HALF_DOWN_BITS_BYTES];
    uint16_t crc16;
} aik_spi_half_state_v1_t;

typedef struct AIK_SPI_PACKED
{
    uint8_t magic;
    uint8_t type;
    uint16_t ack_seq;
    uint8_t half_id;
    uint8_t flags;
    uint16_t profile_id16;
    uint16_t generation16;
    uint16_t crc16;
} aik_spi_profile_status_v1_t;

/* Response frame for the profile transfer command group. Same fixed
 * 12-byte footprint as the other half->host frames. */
typedef struct AIK_SPI_PACKED
{
    uint8_t magic;
    uint8_t type;                /* AIK_SPI_HALF_TYPE_PROFILE_XFER */
    uint16_t ack_seq;            /* host_seq of the last processed cmd */
    uint8_t half_id;
    uint8_t state;               /* AIK_SPI_XFER_STATE_* / _ERR_* */
    uint16_t received_len;       /* bytes assembled so far */
    uint16_t detail;             /* expected total_len or error detail */
    uint16_t crc16;
} aik_spi_profile_xfer_v1_t;

/* Command payloads carried in aik_spi_host_cmd_v1_t bytes 6..29
 * (the nkro16[16] + reserved[8] region). */
typedef struct AIK_SPI_PACKED
{
    uint8_t slot_id;             /* AIK_PROFILE_SLOT_* */
    uint8_t patch_flags;         /* AIK_SPI_PROFILE_FLAG_* */
    uint16_t total_len;
    uint16_t total_crc16;        /* aik_hp_crc of the full patch */
    uint16_t profile_id16;
    uint16_t generation16;
    uint16_t reserved;
} aik_spi_profile_begin_v1_t;

typedef struct AIK_SPI_PACKED
{
    uint16_t offset;
    uint8_t len;                 /* 1..AIK_SPI_PROFILE_CHUNK_DATA_MAX */
    uint8_t reserved;
    uint8_t data[AIK_SPI_PROFILE_CHUNK_DATA_MAX];
} aik_spi_profile_chunk_v1_t;

typedef struct AIK_SPI_PACKED
{
    uint8_t slot_id;
    uint8_t patch_flags;
    uint16_t total_len;
    uint16_t total_crc16;
    uint16_t reserved;
} aik_spi_profile_commit_v1_t;

typedef struct AIK_SPI_PACKED
{
    uint8_t slot_id;             /* 0 = factory default */
    uint8_t flags;
    uint16_t reserved;
} aik_spi_profile_set_active_v1_t;

typedef char aik_spi_host_cmd_v1_size_check[
    (sizeof(aik_spi_host_cmd_v1_t) == AIK_SPI_HOST_CMD_SIZE) ? 1 : -1];
typedef char aik_spi_half_state_v1_size_check[
    (sizeof(aik_spi_half_state_v1_t) == AIK_SPI_HALF_STATE_SIZE) ? 1 : -1];
typedef char aik_spi_profile_status_v1_size_check[
    (sizeof(aik_spi_profile_status_v1_t) == AIK_SPI_HALF_STATE_SIZE) ? 1 : -1];
typedef char aik_spi_profile_xfer_v1_size_check[
    (sizeof(aik_spi_profile_xfer_v1_t) == AIK_SPI_HALF_STATE_SIZE) ? 1 : -1];
typedef char aik_spi_profile_begin_v1_size_check[
    (sizeof(aik_spi_profile_begin_v1_t) <= AIK_SPI_HOST_CMD_PAYLOAD_SIZE) ? 1 : -1];
typedef char aik_spi_profile_chunk_v1_size_check[
    (sizeof(aik_spi_profile_chunk_v1_t) <= AIK_SPI_HOST_CMD_PAYLOAD_SIZE) ? 1 : -1];

static inline uint16_t aik_spi_crc16_ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;

    for(i = 0U; i < len; i++)
    {
        uint8_t bit;

        crc ^= (uint16_t)data[i] << 8;
        for(bit = 0U; bit < 8U; bit++)
        {
            if((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static inline uint16_t aik_spi_host_cmd_crc(const aik_spi_host_cmd_v1_t *cmd)
{
    return aik_spi_crc16_ccitt((const uint8_t *)cmd,
                               (uint16_t)offsetof(aik_spi_host_cmd_v1_t, crc16));
}

static inline uint16_t aik_spi_half_state_crc(const aik_spi_half_state_v1_t *state)
{
    return aik_spi_crc16_ccitt((const uint8_t *)state,
                               (uint16_t)offsetof(aik_spi_half_state_v1_t, crc16));
}

static inline uint16_t aik_spi_profile_status_crc(
    const aik_spi_profile_status_v1_t *status)
{
    return aik_spi_crc16_ccitt((const uint8_t *)status,
                               (uint16_t)offsetof(aik_spi_profile_status_v1_t, crc16));
}

static inline uint8_t aik_spi_half_key_count(uint8_t half_id)
{
    return (half_id == AIK_HALF_ID_RIGHT) ? AIK_KEY_COUNT_RIGHT : AIK_KEY_COUNT_LEFT;
}

static inline uint8_t aik_spi_half_frame_bits(uint8_t half_id)
{
    return (half_id == AIK_HALF_ID_RIGHT) ?
        AIK_HALF_FRAME_BITS_RIGHT :
        AIK_HALF_FRAME_BITS_LEFT;
}

static inline uint8_t aik_spi_half_bit_down(const aik_spi_half_state_v1_t *state,
                                            uint8_t bit_id)
{
    if((state == 0) || (bit_id >= (AIK_HALF_DOWN_BITS_BYTES * 8U)))
    {
        return 0U;
    }
    return (uint8_t)((state->down_bits[bit_id >> 3] >> (bit_id & 7U)) & 1U);
}

static inline void aik_spi_half_set_bit(aik_spi_half_state_v1_t *state,
                                        uint8_t bit_id)
{
    if((state != 0) && (bit_id < (AIK_HALF_DOWN_BITS_BYTES * 8U)))
    {
        state->down_bits[bit_id >> 3] |= (uint8_t)(1U << (bit_id & 7U));
    }
}

static inline uint8_t aik_spi_half_get_2bit(const aik_spi_half_state_v1_t *state,
                                            uint8_t bit_shift)
{
    uint8_t value = 0U;

    if((state == 0) || ((uint8_t)(bit_shift + 1U) >=
                        (AIK_HALF_DOWN_BITS_BYTES * 8U)))
    {
        return 0U;
    }

    value = aik_spi_half_bit_down(state, bit_shift);
    value |= (uint8_t)(aik_spi_half_bit_down(state,
                                             (uint8_t)(bit_shift + 1U)) << 1);
    return value;
}

static inline void aik_spi_half_set_2bit(aik_spi_half_state_v1_t *state,
                                         uint8_t bit_shift,
                                         uint8_t value)
{
    if((state == 0) || ((uint8_t)(bit_shift + 1U) >=
                        (AIK_HALF_DOWN_BITS_BYTES * 8U)))
    {
        return;
    }

    if((value & 0x01U) != 0U)
    {
        aik_spi_half_set_bit(state, bit_shift);
    }
    if((value & 0x02U) != 0U)
    {
        aik_spi_half_set_bit(state, (uint8_t)(bit_shift + 1U));
    }
}

static inline uint16_t aik_spi_host_cmd_consumer_usage(
    const aik_spi_host_cmd_v1_t *cmd)
{
    if(cmd == 0)
    {
        return AIK_CONSUMER_USAGE_NONE;
    }
    return (uint16_t)cmd->reserved[0] |
           (uint16_t)((uint16_t)cmd->reserved[1] << 8);
}

static inline void aik_spi_host_cmd_set_consumer_usage(
    aik_spi_host_cmd_v1_t *cmd,
    uint16_t usage)
{
    if(cmd != 0)
    {
        cmd->reserved[0] = (uint8_t)(usage & 0xFFU);
        cmd->reserved[1] = (uint8_t)(usage >> 8);
    }
}

static inline int8_t aik_spi_host_cmd_consumer_delta(
    const aik_spi_host_cmd_v1_t *cmd)
{
    if(cmd == 0)
    {
        return 0;
    }
    return (int8_t)cmd->reserved[2];
}

static inline void aik_spi_host_cmd_set_consumer_delta(
    aik_spi_host_cmd_v1_t *cmd,
    int8_t delta)
{
    if(cmd != 0)
    {
        cmd->reserved[2] = (uint8_t)delta;
    }
}

static inline uint8_t aik_spi_half_down_last_mask(uint8_t key_count)
{
    uint8_t used = (uint8_t)(key_count & 7U);

    if(used == 0U)
    {
        return 0xFFU;
    }
    return (uint8_t)((1U << used) - 1U);
}

static inline void aik_spi_mask_unused_half_bits(uint8_t *down_bits, uint8_t key_count)
{
    uint8_t full_bytes = (uint8_t)(key_count >> 3);
    uint8_t last_mask = aik_spi_half_down_last_mask(key_count);
    uint8_t i;

    if(full_bytes < AIK_HALF_DOWN_BITS_BYTES)
    {
        down_bits[full_bytes] &= last_mask;
        for(i = (uint8_t)(full_bytes + 1U); i < AIK_HALF_DOWN_BITS_BYTES; i++)
        {
            down_bits[i] = 0U;
        }
    }
}

static inline void aik_spi_host_cmd_finish(aik_spi_host_cmd_v1_t *cmd)
{
    cmd->magic = AIK_SPI_HOST_MAGIC;
    cmd->version = AIK_SPI_VERSION;
    cmd->crc16 = aik_spi_host_cmd_crc(cmd);
}

static inline uint8_t aik_spi_host_cmd_valid(const aik_spi_host_cmd_v1_t *cmd)
{
    return (cmd->magic == AIK_SPI_HOST_MAGIC) &&
           (cmd->version == AIK_SPI_VERSION) &&
           (cmd->crc16 == aik_spi_host_cmd_crc(cmd));
}

static inline void aik_spi_half_state_finish(aik_spi_half_state_v1_t *state,
                                             uint8_t key_count)
{
    state->magic = AIK_SPI_HALF_MAGIC;
    state->type = AIK_SPI_HALF_TYPE_STATE;
    aik_spi_mask_unused_half_bits(state->down_bits, key_count);
    state->crc16 = aik_spi_half_state_crc(state);
}

static inline uint8_t aik_spi_half_state_valid(const aik_spi_half_state_v1_t *state)
{
    return (state->magic == AIK_SPI_HALF_MAGIC) &&
           (state->type == AIK_SPI_HALF_TYPE_STATE) &&
           (state->crc16 == aik_spi_half_state_crc(state));
}

static inline void aik_spi_profile_status_finish(
    aik_spi_profile_status_v1_t *status)
{
    status->magic = AIK_SPI_HALF_MAGIC;
    status->type = AIK_SPI_HALF_TYPE_PROFILE_STATUS;
    status->crc16 = aik_spi_profile_status_crc(status);
}

static inline uint8_t aik_spi_profile_status_valid(
    const aik_spi_profile_status_v1_t *status)
{
    return (status->magic == AIK_SPI_HALF_MAGIC) &&
           (status->type == AIK_SPI_HALF_TYPE_PROFILE_STATUS) &&
           (status->crc16 == aik_spi_profile_status_crc(status));
}

static inline uint8_t aik_spi_profile_status_matches_debug_profile(
    const aik_spi_profile_status_v1_t *status,
    uint8_t half_id)
{
    return (aik_spi_profile_status_valid(status) != 0U) &&
           (status->half_id == half_id) &&
           ((status->flags & AIK_PROFILE_STATUS_FLAG_VALID) != 0U) &&
           (status->profile_id16 == AIK_PROFILE_DEBUG_ID16) &&
           (status->generation16 == AIK_PROFILE_DEBUG_GENERATION16);
}

static inline uint8_t aik_spi_profile_status_active_slot(
    const aik_spi_profile_status_v1_t *status)
{
    return (uint8_t)((status->flags & AIK_PROFILE_STATUS_SLOT_MASK) >>
                     AIK_PROFILE_STATUS_SLOT_SHIFT);
}

/* The 24-byte command payload region starts at the nkro16 field and
 * spans through reserved[] (bytes 6..29 of the packed 32-byte frame). */
static inline uint8_t *aik_spi_host_cmd_payload(aik_spi_host_cmd_v1_t *cmd)
{
    return cmd->nkro16;
}

static inline const uint8_t *aik_spi_host_cmd_payload_const(
    const aik_spi_host_cmd_v1_t *cmd)
{
    return cmd->nkro16;
}

static inline void aik_spi_host_cmd_clear_payload(aik_spi_host_cmd_v1_t *cmd)
{
    uint8_t i;
    uint8_t *payload = aik_spi_host_cmd_payload(cmd);

    for(i = 0U; i < AIK_SPI_HOST_CMD_PAYLOAD_SIZE; i++)
    {
        payload[i] = 0U;
    }
}

static inline void aik_spi_host_cmd_set_payload(aik_spi_host_cmd_v1_t *cmd,
                                                const void *payload,
                                                uint8_t len)
{
    uint8_t i;
    const uint8_t *src = (const uint8_t *)payload;
    uint8_t *dst = aik_spi_host_cmd_payload(cmd);

    aik_spi_host_cmd_clear_payload(cmd);
    if(len > AIK_SPI_HOST_CMD_PAYLOAD_SIZE)
    {
        len = AIK_SPI_HOST_CMD_PAYLOAD_SIZE;
    }
    for(i = 0U; i < len; i++)
    {
        dst[i] = src[i];
    }
}

static inline void aik_spi_host_cmd_get_payload(
    const aik_spi_host_cmd_v1_t *cmd,
    void *payload,
    uint8_t len)
{
    uint8_t i;
    const uint8_t *src = aik_spi_host_cmd_payload_const(cmd);
    uint8_t *dst = (uint8_t *)payload;

    if(len > AIK_SPI_HOST_CMD_PAYLOAD_SIZE)
    {
        len = AIK_SPI_HOST_CMD_PAYLOAD_SIZE;
    }
    for(i = 0U; i < len; i++)
    {
        dst[i] = src[i];
    }
}

static inline uint16_t aik_spi_profile_xfer_crc(
    const aik_spi_profile_xfer_v1_t *xfer)
{
    return aik_spi_crc16_ccitt((const uint8_t *)xfer,
                               (uint16_t)offsetof(aik_spi_profile_xfer_v1_t,
                                                  crc16));
}

static inline void aik_spi_profile_xfer_finish(aik_spi_profile_xfer_v1_t *xfer)
{
    xfer->magic = AIK_SPI_HALF_MAGIC;
    xfer->type = AIK_SPI_HALF_TYPE_PROFILE_XFER;
    xfer->crc16 = aik_spi_profile_xfer_crc(xfer);
}

static inline uint8_t aik_spi_profile_xfer_valid(
    const aik_spi_profile_xfer_v1_t *xfer)
{
    return (xfer->magic == AIK_SPI_HALF_MAGIC) &&
           (xfer->type == AIK_SPI_HALF_TYPE_PROFILE_XFER) &&
           (xfer->crc16 == aik_spi_profile_xfer_crc(xfer));
}

#ifdef __cplusplus
}
#endif

#endif
