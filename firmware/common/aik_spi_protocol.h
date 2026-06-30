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
#define AIK_SPI_VERSION 1U

#define AIK_SPI_CMD_POLL 0U
#define AIK_SPI_CMD_POLL_WITH_RF 1U
#define AIK_SPI_CMD_PUSH_RIGHT_STATE 2U

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

typedef char aik_spi_host_cmd_v1_size_check[
    (sizeof(aik_spi_host_cmd_v1_t) == AIK_SPI_HOST_CMD_SIZE) ? 1 : -1];
typedef char aik_spi_half_state_v1_size_check[
    (sizeof(aik_spi_half_state_v1_t) == AIK_SPI_HALF_STATE_SIZE) ? 1 : -1];

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

static inline uint8_t aik_spi_half_key_count(uint8_t half_id)
{
    return (half_id == AIK_HALF_ID_RIGHT) ? AIK_KEY_COUNT_RIGHT : AIK_KEY_COUNT_LEFT;
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

#ifdef __cplusplus
}
#endif

#endif
