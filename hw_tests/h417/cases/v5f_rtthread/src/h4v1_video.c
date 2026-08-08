#include "h4v1_video.h"

#include <string.h>

static uint16_t h4v1_get_u16_le(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] |
                      ((uint16_t)data[1] << 8));
}

static uint32_t h4v1_get_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

uint32_t __attribute__((optimize("O3")))
h4v1_crc32_update(uint32_t previous_crc,
                  const void *data,
                  size_t length)
{
    static const uint32_t table[16] =
    {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
        0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
        0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
    };
    const uint8_t *cursor = (const uint8_t *)data;
    uint32_t crc = ~previous_crc;

    if((cursor == NULL) && (length != 0u))
    {
        return 0u;
    }

    while(length-- != 0u)
    {
        crc ^= *cursor++;
        crc = table[crc & 0x0Fu] ^ (crc >> 4);
        crc = table[crc & 0x0Fu] ^ (crc >> 4);
    }
    return ~crc;
}

uint32_t h4v1_crc32(const void *data, size_t length)
{
    return h4v1_crc32_update(0u, data, length);
}

int h4v1_parse_header(const uint8_t *bytes,
                      size_t available,
                      h4v1_header_t *header)
{
    uint8_t crc_header[H4V1_HEADER_BYTES];
    uint64_t index_end;

    if((bytes == NULL) || (header == NULL) ||
       (available < H4V1_HEADER_BYTES))
    {
        return H4V1_ERR_ARGUMENT;
    }
    if((bytes[0] != 'H') || (bytes[1] != '4') ||
       (bytes[2] != 'V') || (bytes[3] != '1'))
    {
        return H4V1_ERR_HEADER;
    }

    header->version = h4v1_get_u16_le(&bytes[4]);
    header->header_bytes = h4v1_get_u16_le(&bytes[6]);
    header->width = h4v1_get_u16_le(&bytes[8]);
    header->height = h4v1_get_u16_le(&bytes[10]);
    header->fps = h4v1_get_u16_le(&bytes[12]);
    header->pixel_format = h4v1_get_u16_le(&bytes[14]);
    header->frame_count = h4v1_get_u32_le(&bytes[16]);
    header->frame_bytes = h4v1_get_u32_le(&bytes[20]);
    header->gop = h4v1_get_u32_le(&bytes[24]);
    header->flags = h4v1_get_u32_le(&bytes[28]);
    header->index_offset = h4v1_get_u32_le(&bytes[32]);
    header->index_entry_bytes = h4v1_get_u32_le(&bytes[36]);
    header->data_offset = h4v1_get_u32_le(&bytes[40]);
    header->file_bytes = h4v1_get_u32_le(&bytes[44]);
    header->raw_stream_crc32 = h4v1_get_u32_le(&bytes[48]);
    header->index_crc32 = h4v1_get_u32_le(&bytes[52]);
    header->header_crc32 = h4v1_get_u32_le(&bytes[56]);

    if((header->version != 1u) ||
       (header->header_bytes != H4V1_HEADER_BYTES) ||
       (header->width == 0u) || (header->height == 0u) ||
       (header->fps == 0u) ||
       (header->pixel_format != H4V1_PIXEL_FORMAT_ARGB1555) ||
       (header->frame_count == 0u) || (header->frame_bytes == 0u) ||
       (header->gop == 0u) ||
       (header->index_entry_bytes != H4V1_INDEX_ENTRY_BYTES) ||
       (header->index_offset < H4V1_HEADER_BYTES) ||
       (header->file_bytes < H4V1_HEADER_BYTES) ||
       (header->data_offset > header->file_bytes) ||
       ((header->flags & H4V1_CONTAINER_LZ4_RAW_BLOCK) == 0u))
    {
        return H4V1_ERR_HEADER;
    }

    index_end = (uint64_t)header->index_offset +
                (uint64_t)header->frame_count *
                (uint64_t)header->index_entry_bytes;
    if((index_end > header->data_offset) ||
       (index_end > header->file_bytes))
    {
        return H4V1_ERR_HEADER;
    }

    memcpy(crc_header, bytes, sizeof(crc_header));
    memset(&crc_header[56], 0, 4u);
    if(h4v1_crc32(crc_header, sizeof(crc_header)) !=
       header->header_crc32)
    {
        return H4V1_ERR_HEADER_CRC;
    }
    return H4V1_OK;
}

int h4v1_parse_index_entry(const uint8_t *bytes,
                           size_t available,
                           const h4v1_header_t *header,
                           h4v1_index_entry_t *entry)
{
    uint64_t payload_end;
    uint32_t frame_kind;

    if((bytes == NULL) || (header == NULL) || (entry == NULL) ||
       (available < H4V1_INDEX_ENTRY_BYTES))
    {
        return H4V1_ERR_ARGUMENT;
    }

    entry->offset = h4v1_get_u32_le(&bytes[0]);
    entry->compressed_bytes = h4v1_get_u32_le(&bytes[4]);
    entry->uncompressed_bytes = h4v1_get_u32_le(&bytes[8]);
    entry->raw_crc32 = h4v1_get_u32_le(&bytes[12]);
    entry->payload_crc32 = h4v1_get_u32_le(&bytes[16]);
    entry->flags = h4v1_get_u32_le(&bytes[20]);

    frame_kind = entry->flags & (H4V1_FRAME_KEY | H4V1_FRAME_XOR_DELTA);
    payload_end = (uint64_t)entry->offset + entry->compressed_bytes;
    if((entry->compressed_bytes == 0u) ||
       (entry->uncompressed_bytes != header->frame_bytes) ||
       (entry->offset < header->data_offset) ||
       (payload_end > header->file_bytes) ||
       ((frame_kind != H4V1_FRAME_KEY) &&
        (frame_kind != H4V1_FRAME_XOR_DELTA)) ||
       ((entry->flags & ~(H4V1_FRAME_KEY | H4V1_FRAME_XOR_DELTA)) != 0u))
    {
        return H4V1_ERR_INDEX;
    }
    return H4V1_OK;
}

static int h4v1_lz4_read_length(const uint8_t **input,
                                const uint8_t *input_end,
                                size_t *length)
{
    uint8_t extension;

    do
    {
        if(*input >= input_end)
        {
            return H4V1_ERR_INPUT;
        }
        extension = *(*input)++;
        if(*length > (SIZE_MAX - extension))
        {
            return H4V1_ERR_LZ4;
        }
        *length += extension;
    } while(extension == 255u);
    return H4V1_OK;
}

int h4v1_lz4_decompress_safe(const uint8_t *compressed,
                             size_t compressed_bytes,
                             uint8_t *output,
                             size_t output_capacity,
                             size_t expected_output_bytes)
{
    const uint8_t *input;
    const uint8_t *input_end;
    uint8_t *write;
    uint8_t *output_end;

    if((compressed == NULL) || (output == NULL) ||
       (compressed_bytes == 0u) ||
       (expected_output_bytes > output_capacity))
    {
        return H4V1_ERR_ARGUMENT;
    }

    input = compressed;
    input_end = compressed + compressed_bytes;
    write = output;
    output_end = output + expected_output_bytes;

    while(input < input_end)
    {
        uint8_t token = *input++;
        size_t literal_length = (size_t)(token >> 4);
        size_t match_length;
        size_t remaining_input;
        size_t remaining_output;
        uint16_t offset;
        uint8_t *match;
        int result;

        if(literal_length == 15u)
        {
            result = h4v1_lz4_read_length(&input, input_end,
                                          &literal_length);
            if(result != H4V1_OK)
            {
                return result;
            }
        }
        remaining_input = (size_t)(input_end - input);
        remaining_output = (size_t)(output_end - write);
        if((literal_length > remaining_input) ||
           (literal_length > remaining_output))
        {
            return H4V1_ERR_OUTPUT;
        }
        memcpy(write, input, literal_length);
        input += literal_length;
        write += literal_length;

        if(input == input_end)
        {
            break;
        }
        if((size_t)(input_end - input) < 2u)
        {
            return H4V1_ERR_INPUT;
        }
        offset = h4v1_get_u16_le(input);
        input += 2;
        if((offset == 0u) || ((size_t)(write - output) < offset))
        {
            return H4V1_ERR_LZ4;
        }

        match_length = (size_t)(token & 0x0Fu);
        if(match_length == 15u)
        {
            result = h4v1_lz4_read_length(&input, input_end, &match_length);
            if(result != H4V1_OK)
            {
                return result;
            }
        }
        if(match_length > (SIZE_MAX - 4u))
        {
            return H4V1_ERR_LZ4;
        }
        match_length += 4u;
        if(match_length > (size_t)(output_end - write))
        {
            return H4V1_ERR_OUTPUT;
        }

        match = write - offset;
        while(match_length-- != 0u)
        {
            *write++ = *match++;
        }
    }

    if((input != input_end) || (write != output_end))
    {
        return H4V1_ERR_OUTPUT;
    }
    return H4V1_OK;
}

int h4v1_decode_frame(const h4v1_index_entry_t *entry,
                      const uint8_t *payload,
                      size_t payload_bytes,
                      const uint8_t *previous_frame,
                      uint8_t *output_frame,
                      size_t output_capacity)
{
    size_t i;
    int result;

    if((entry == NULL) || (payload == NULL) || (output_frame == NULL) ||
       (payload_bytes != entry->compressed_bytes) ||
       (output_capacity < entry->uncompressed_bytes))
    {
        return H4V1_ERR_ARGUMENT;
    }
    if(h4v1_crc32(payload, payload_bytes) != entry->payload_crc32)
    {
        return H4V1_ERR_PAYLOAD_CRC;
    }

    result = h4v1_lz4_decompress_safe(payload,
                                      payload_bytes,
                                      output_frame,
                                      output_capacity,
                                      entry->uncompressed_bytes);
    if(result != H4V1_OK)
    {
        return result;
    }

    if((entry->flags & H4V1_FRAME_XOR_DELTA) != 0u)
    {
        if(previous_frame == NULL)
        {
            return H4V1_ERR_FRAME_BASE;
        }
        for(i = 0u; i < entry->uncompressed_bytes; ++i)
        {
            output_frame[i] ^= previous_frame[i];
        }
    }
    else if((entry->flags & H4V1_FRAME_KEY) == 0u)
    {
        return H4V1_ERR_INDEX;
    }

    if(h4v1_crc32(output_frame, entry->uncompressed_bytes) !=
       entry->raw_crc32)
    {
        return H4V1_ERR_FRAME_CRC;
    }
    return H4V1_OK;
}
