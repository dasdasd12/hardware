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
#define AIK_SPI_VERSION 1U

#define AIK_SPI_CMD_POLL 0U
#define AIK_SPI_CMD_POLL_WITH_RF 1U
#define AIK_SPI_CMD_PUSH_RIGHT_STATE 2U
#define AIK_SPI_CMD_GET_PROFILE_STATUS 0x40U

#define AIK_OUTPUT_MODE_USBHS 0U
#define AIK_OUTPUT_MODE_RF24  1U
#define AIK_OUTPUT_MODE_BLE   2U

#define AIK_SPI_FLAG_OUTPUT_MODE_MASK 0x03U

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
#define AIK_HALF_FRAME_BITS_LEFT      41U

#define AIK_RIGHT_LOCAL_BIT_EC11_CW   41U
#define AIK_RIGHT_LOCAL_BIT_EC11_CCW  42U
#define AIK_RIGHT_LOCAL_BIT_EC11_MUTE 43U
#define AIK_HALF_FRAME_BITS_RIGHT     44U

#define AIK_CONSUMER_USAGE_NONE        0x0000U
#define AIK_CONSUMER_USAGE_MUTE        0x00E2U
#define AIK_CONSUMER_USAGE_VOLUME_UP   0x00E9U
#define AIK_CONSUMER_USAGE_VOLUME_DOWN 0x00EAU

#define AIK_PROFILE_DEBUG_ID16 0xA117U
#define AIK_PROFILE_DEBUG_GENERATION16 1U

#define AIK_PROFILE_STATUS_FLAG_VALID   0x01U
#define AIK_PROFILE_STATUS_FLAG_DEFAULT 0x02U

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

typedef char aik_spi_host_cmd_v1_size_check[
    (sizeof(aik_spi_host_cmd_v1_t) == AIK_SPI_HOST_CMD_SIZE) ? 1 : -1];
typedef char aik_spi_half_state_v1_size_check[
    (sizeof(aik_spi_half_state_v1_t) == AIK_SPI_HALF_STATE_SIZE) ? 1 : -1];
typedef char aik_spi_profile_status_v1_size_check[
    (sizeof(aik_spi_profile_status_v1_t) == AIK_SPI_HALF_STATE_SIZE) ? 1 : -1];

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

#ifdef __cplusplus
}
#endif

#endif
