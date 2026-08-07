#include "h4v1_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_file(const char *path, uint8_t **bytes_out, size_t *size_out)
{
    FILE *file;
    long length;
    uint8_t *bytes;

    file = fopen(path, "rb");
    if(file == NULL)
    {
        return -1;
    }
    if((fseek(file, 0, SEEK_END) != 0) ||
       ((length = ftell(file)) <= 0) ||
       (fseek(file, 0, SEEK_SET) != 0))
    {
        fclose(file);
        return -1;
    }
    bytes = (uint8_t *)malloc((size_t)length);
    if(bytes == NULL)
    {
        fclose(file);
        return -1;
    }
    if(fread(bytes, 1u, (size_t)length, file) != (size_t)length)
    {
        free(bytes);
        fclose(file);
        return -1;
    }
    fclose(file);
    *bytes_out = bytes;
    *size_out = (size_t)length;
    return 0;
}

int main(int argc, char **argv)
{
    h4v1_header_t header;
    h4v1_index_entry_t entry;
    uint8_t *file_bytes = NULL;
    uint8_t *frame_a = NULL;
    uint8_t *frame_b = NULL;
    uint8_t *previous = NULL;
    uint8_t *output;
    size_t file_size = 0u;
    size_t index_bytes;
    uint32_t stream_crc = 0u;
    uint32_t frame;
    int result = 1;

    if(argc != 2)
    {
        fprintf(stderr, "usage: %s FILE.h4v\n", argv[0]);
        return 2;
    }
    if(read_file(argv[1], &file_bytes, &file_size) != 0)
    {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        goto done;
    }
    result = h4v1_parse_header(file_bytes, file_size, &header);
    if(result != H4V1_OK)
    {
        fprintf(stderr, "HEADER FAIL status=%d\n", result);
        goto done;
    }
    if(header.file_bytes != file_size)
    {
        fprintf(stderr, "HEADER FAIL file=%u/%lu\n",
                (unsigned)header.file_bytes, (unsigned long)file_size);
        result = 1;
        goto done;
    }
    index_bytes = (size_t)header.frame_count * header.index_entry_bytes;
    if(h4v1_crc32(&file_bytes[header.index_offset], index_bytes) !=
       header.index_crc32)
    {
        fprintf(stderr, "INDEX FAIL crc\n");
        result = 1;
        goto done;
    }

    frame_a = (uint8_t *)malloc(header.frame_bytes);
    frame_b = (uint8_t *)malloc(header.frame_bytes);
    if((frame_a == NULL) || (frame_b == NULL))
    {
        fprintf(stderr, "out of memory\n");
        result = 1;
        goto done;
    }

    printf("H4V1 HEADER %ux%u fps=%u frames=%u frame_bytes=%u "
           "file_bytes=%u gop=%u\n",
           (unsigned)header.width, (unsigned)header.height,
           (unsigned)header.fps, (unsigned)header.frame_count,
           (unsigned)header.frame_bytes, (unsigned)header.file_bytes,
           (unsigned)header.gop);
    for(frame = 0u; frame < header.frame_count; ++frame)
    {
        const uint8_t *index = &file_bytes[header.index_offset +
                                           frame * header.index_entry_bytes];
        output = ((frame & 1u) == 0u) ? frame_a : frame_b;
        result = h4v1_parse_index_entry(index,
                                        header.index_entry_bytes,
                                        &header,
                                        &entry);
        if(result != H4V1_OK)
        {
            fprintf(stderr, "FRAME %u INDEX FAIL status=%d\n",
                    (unsigned)frame, result);
            goto done;
        }
        result = h4v1_decode_frame(&entry,
                                   &file_bytes[entry.offset],
                                   entry.compressed_bytes,
                                   previous,
                                   output,
                                   header.frame_bytes);
        if(result != H4V1_OK)
        {
            fprintf(stderr, "FRAME %u DECODE FAIL status=%d\n",
                    (unsigned)frame, result);
            goto done;
        }
        stream_crc = h4v1_crc32_update(stream_crc,
                                       output,
                                       header.frame_bytes);
        previous = output;
        if((((frame + 1u) % 30u) == 0u) ||
           ((frame + 1u) == header.frame_count))
        {
            printf("H4V1 VERIFY progress=%u/%u\n",
                   (unsigned)(frame + 1u), (unsigned)header.frame_count);
        }
    }
    if(stream_crc != header.raw_stream_crc32)
    {
        fprintf(stderr, "STREAM FAIL crc=%08x/%08x\n",
                (unsigned)stream_crc,
                (unsigned)header.raw_stream_crc32);
        result = 1;
        goto done;
    }
    printf("H4V1 VERIFY PASS frames=%u crc=%08x\n",
           (unsigned)header.frame_count, (unsigned)stream_crc);
    result = 0;

done:
    free(frame_b);
    free(frame_a);
    free(file_bytes);
    return result;
}
