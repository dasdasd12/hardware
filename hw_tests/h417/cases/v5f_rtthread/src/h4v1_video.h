#ifndef H4V1_VIDEO_H
#define H4V1_VIDEO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define H4V1_HEADER_BYTES             64u
#define H4V1_INDEX_ENTRY_BYTES        24u
#define H4V1_PIXEL_FORMAT_ARGB1555    1u

#define H4V1_CONTAINER_XOR_DELTA      (1u << 0)
#define H4V1_CONTAINER_ROTATE_180     (1u << 1)
#define H4V1_CONTAINER_LZ4_RAW_BLOCK  (1u << 2)
#define H4V1_CONTAINER_CHUNKED_ABSOLUTE (1u << 3)

#define H4V1_FRAME_KEY                (1u << 0)
#define H4V1_FRAME_XOR_DELTA          (1u << 1)

typedef enum
{
    H4V1_OK = 0,
    H4V1_ERR_ARGUMENT = -1,
    H4V1_ERR_HEADER = -2,
    H4V1_ERR_HEADER_CRC = -3,
    H4V1_ERR_INDEX = -4,
    H4V1_ERR_INPUT = -5,
    H4V1_ERR_OUTPUT = -6,
    H4V1_ERR_LZ4 = -7,
    H4V1_ERR_PAYLOAD_CRC = -8,
    H4V1_ERR_FRAME_CRC = -9,
    H4V1_ERR_FRAME_BASE = -10,
} h4v1_status_t;

typedef struct
{
    uint16_t version;
    uint16_t header_bytes;
    uint16_t width;
    uint16_t height;
    uint16_t fps;
    uint16_t pixel_format;
    uint32_t frame_count;
    uint32_t frame_bytes;
    uint32_t gop;
    uint32_t flags;
    uint32_t index_offset;
    uint32_t index_entry_bytes;
    uint32_t data_offset;
    uint32_t file_bytes;
    uint32_t raw_stream_crc32;
    uint32_t index_crc32;
    uint32_t header_crc32;
} h4v1_header_t;

typedef struct
{
    uint32_t offset;
    uint32_t compressed_bytes;
    uint32_t uncompressed_bytes;
    uint32_t raw_crc32;
    uint32_t payload_crc32;
    uint32_t flags;
} h4v1_index_entry_t;

uint32_t h4v1_crc32_update(uint32_t previous_crc,
                           const void *data,
                           size_t length);

uint32_t h4v1_crc32(const void *data, size_t length);

int h4v1_parse_header(const uint8_t *bytes,
                      size_t available,
                      h4v1_header_t *header);

int h4v1_parse_index_entry(const uint8_t *bytes,
                           size_t available,
                           const h4v1_header_t *header,
                           h4v1_index_entry_t *entry);

int h4v1_lz4_decompress_safe(const uint8_t *compressed,
                             size_t compressed_bytes,
                             uint8_t *output,
                             size_t output_capacity,
                             size_t expected_output_bytes);

int h4v1_decode_frame(const h4v1_index_entry_t *entry,
                      const uint8_t *payload,
                      size_t payload_bytes,
                      const uint8_t *previous_frame,
                      uint8_t *output_frame,
                      size_t output_capacity);

#ifdef __cplusplus
}
#endif

#endif
