#ifndef CH585_H417_ADC_KEY_CAL_PROTO_H
#define CH585_H417_ADC_KEY_CAL_PROTO_H

#include <stdint.h>

#define CH585_H417_ADC_KEY_CAL_FRAME_BYTES 24U
#define CH585_H417_ADC_KEY_CAL_VERSION 1U
#define CH585_H417_ADC_KEY_CAL_CMD_MAGIC 0xC6U
#define CH585_H417_ADC_KEY_CAL_SAMPLE_MAGIC 0x6CU
#define CH585_H417_ADC_KEY_CAL_CMD_IDLE 0U
#define CH585_H417_ADC_KEY_CAL_CMD_SELECT 1U
#define CH585_H417_ADC_KEY_CAL_FLAG_RESET_STATS 0x0001U
#define CH585_H417_ADC_KEY_CAL_STATUS_OK 0U
#define CH585_H417_ADC_KEY_CAL_STATUS_BAD_KEY 1U
#define CH585_H417_ADC_KEY_CAL_STATUS_NOT_READY 2U
#define CH585_H417_ADC_KEY_CAL_CRC_OFFSET \
    (CH585_H417_ADC_KEY_CAL_FRAME_BYTES - 2U)

typedef struct __attribute__((packed))
{
    uint8_t magic;
    uint8_t version;
    uint8_t cmd;
    uint8_t key_id;
    uint16_t host_seq;
    uint16_t flags;
    uint8_t reserved[14];
    uint16_t crc16;
} ch585_h417_adc_key_cal_cmd_t;

typedef struct __attribute__((packed))
{
    uint8_t magic;
    uint8_t version;
    uint8_t status;
    uint8_t side;
    uint16_t sample_seq;
    uint8_t key_id;
    uint8_t key_count;
    uint16_t raw;
    uint16_t min_raw;
    uint16_t max_raw;
    uint32_t sample_count;
    uint16_t flags;
    uint8_t reserved[2];
    uint16_t crc16;
} ch585_h417_adc_key_cal_sample_t;

#define CH585_H417_ADC_KEY_CAL_STATIC_ASSERT(name, expr) \
    typedef char ch585_h417_adc_key_cal_static_assert_##name[(expr) ? 1 : -1]

CH585_H417_ADC_KEY_CAL_STATIC_ASSERT(
    cmd_size,
    sizeof(ch585_h417_adc_key_cal_cmd_t) ==
        CH585_H417_ADC_KEY_CAL_FRAME_BYTES);
CH585_H417_ADC_KEY_CAL_STATIC_ASSERT(
    sample_size,
    sizeof(ch585_h417_adc_key_cal_sample_t) ==
        CH585_H417_ADC_KEY_CAL_FRAME_BYTES);

static inline uint16_t ch585_h417_adc_key_cal_crc16(const uint8_t *data,
                                                    uint16_t len)
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
                crc = (uint16_t)((crc << 1U) ^ 0x1021U);
            }
            else
            {
                crc = (uint16_t)(crc << 1U);
            }
        }
    }

    return crc;
}

static inline void ch585_h417_adc_key_cal_finish_cmd(
    ch585_h417_adc_key_cal_cmd_t *cmd)
{
    cmd->crc16 = ch585_h417_adc_key_cal_crc16(
        (const uint8_t *)cmd,
        (uint16_t)CH585_H417_ADC_KEY_CAL_CRC_OFFSET);
}

static inline void ch585_h417_adc_key_cal_finish_sample(
    ch585_h417_adc_key_cal_sample_t *sample)
{
    sample->crc16 = ch585_h417_adc_key_cal_crc16(
        (const uint8_t *)sample,
        (uint16_t)CH585_H417_ADC_KEY_CAL_CRC_OFFSET);
}

static inline uint8_t ch585_h417_adc_key_cal_cmd_valid(
    const ch585_h417_adc_key_cal_cmd_t *cmd)
{
    uint16_t crc;

    if((cmd == 0) ||
       (cmd->magic != CH585_H417_ADC_KEY_CAL_CMD_MAGIC) ||
       (cmd->version != CH585_H417_ADC_KEY_CAL_VERSION))
    {
        return 0U;
    }

    crc = ch585_h417_adc_key_cal_crc16(
        (const uint8_t *)cmd,
        (uint16_t)CH585_H417_ADC_KEY_CAL_CRC_OFFSET);
    return (uint8_t)(crc == cmd->crc16);
}

static inline uint8_t ch585_h417_adc_key_cal_sample_valid(
    const ch585_h417_adc_key_cal_sample_t *sample)
{
    uint16_t crc;

    if((sample == 0) ||
       (sample->magic != CH585_H417_ADC_KEY_CAL_SAMPLE_MAGIC) ||
       (sample->version != CH585_H417_ADC_KEY_CAL_VERSION))
    {
        return 0U;
    }

    crc = ch585_h417_adc_key_cal_crc16(
        (const uint8_t *)sample,
        (uint16_t)CH585_H417_ADC_KEY_CAL_CRC_OFFSET);
    return (uint8_t)(crc == sample->crc16);
}

#endif
