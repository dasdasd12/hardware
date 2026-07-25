#ifndef CH585_H417_SPI_SPEED_PROTO_H
#define CH585_H417_SPI_SPEED_PROTO_H

#include <stdint.h>

#define CH585_H417_SPI_SPEED_FRAME_BYTES 192U
#define CH585_H417_SPI_SPEED_MAGIC       0x53503835UL
#define CH585_H417_SPI_SPEED_TAIL        0x35485053UL
#define CH585_H417_SPI_SPEED_VERSION     1U
#define CH585_H417_SPI_SPEED_READY_BYTE  0xA5U
#define CH585_H417_SPI_SPEED_FRAME_OFF   1U
#define CH585_H417_SPI_SPEED_TRANSFER_BYTES 193U

#define CH585_H417_SPI_SPEED_OFF_MAGIC   0U
#define CH585_H417_SPI_SPEED_OFF_VERSION 4U
#define CH585_H417_SPI_SPEED_OFF_LEN     5U
#define CH585_H417_SPI_SPEED_OFF_SEQ     6U
#define CH585_H417_SPI_SPEED_OFF_PATTERN 8U
#define CH585_H417_SPI_SPEED_OFF_INVERT  12U
#define CH585_H417_SPI_SPEED_OFF_COUNT   16U
#define CH585_H417_SPI_SPEED_OFF_STATUS  20U
#define CH585_H417_SPI_SPEED_OFF_CRC     24U
#define CH585_H417_SPI_SPEED_OFF_FLAGS   26U
#define CH585_H417_SPI_SPEED_PAYLOAD_OFF 28U
#define CH585_H417_SPI_SPEED_OFF_TAIL    (CH585_H417_SPI_SPEED_FRAME_BYTES - 4U)
#define CH585_H417_SPI_SPEED_CRC_BYTES   24U

static inline uint8_t ch585_h417_spi_speed_fixed_byte(uint16_t offset)
{
    if(offset == 0U)
    {
        return (uint8_t)CH585_H417_SPI_SPEED_READY_BYTE;
    }

    return (uint8_t)(0x5AU ^
                     (uint8_t)(offset * 29U) ^
                     (uint8_t)(offset >> 1) ^
                     (uint8_t)((offset * 3U) >> 4));
}

static inline uint8_t ch585_h417_spi_speed_payload_byte(uint16_t seq,
                                                        uint16_t offset)
{
    return (uint8_t)(0xC3U ^
                     (uint8_t)seq ^
                     (uint8_t)(seq >> 8) ^
                     (uint8_t)(offset * 17U) ^
                     (uint8_t)(offset >> 1));
}

static inline void ch585_h417_spi_speed_put16(uint8_t *buffer,
                                              uint16_t offset,
                                              uint16_t value)
{
    buffer[offset + 0U] = (uint8_t)(value & 0xFFU);
    buffer[offset + 1U] = (uint8_t)((value >> 8) & 0xFFU);
}

static inline void ch585_h417_spi_speed_put32(uint8_t *buffer,
                                              uint16_t offset,
                                              uint32_t value)
{
    buffer[offset + 0U] = (uint8_t)(value & 0xFFU);
    buffer[offset + 1U] = (uint8_t)((value >> 8) & 0xFFU);
    buffer[offset + 2U] = (uint8_t)((value >> 16) & 0xFFU);
    buffer[offset + 3U] = (uint8_t)((value >> 24) & 0xFFU);
}

static inline uint16_t ch585_h417_spi_speed_get16(const uint8_t *buffer,
                                                  uint16_t offset)
{
    return (uint16_t)(((uint16_t)buffer[offset + 0U]) |
                      ((uint16_t)buffer[offset + 1U] << 8));
}

static inline uint32_t ch585_h417_spi_speed_get32(const uint8_t *buffer,
                                                  uint16_t offset)
{
    return ((uint32_t)buffer[offset + 0U]) |
           ((uint32_t)buffer[offset + 1U] << 8) |
           ((uint32_t)buffer[offset + 2U] << 16) |
           ((uint32_t)buffer[offset + 3U] << 24);
}

static inline uint16_t ch585_h417_spi_speed_crc16(const uint8_t *data,
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
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}

#endif
