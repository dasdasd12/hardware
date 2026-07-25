#include "gd5f1g_l8_asset_store.h"

static void put32(uint8_t *data, uint32_t offset, uint32_t value)
{
    data[offset + 0u] = (uint8_t)value;
    data[offset + 1u] = (uint8_t)(value >> 8);
    data[offset + 2u] = (uint8_t)(value >> 16);
    data[offset + 3u] = (uint8_t)(value >> 24);
}

static uint32_t get32(const uint8_t *data, uint32_t offset)
{
    return ((uint32_t)data[offset + 0u]) |
           ((uint32_t)data[offset + 1u] << 8) |
           ((uint32_t)data[offset + 2u] << 16) |
           ((uint32_t)data[offset + 3u] << 24);
}

static uint32_t start_row(uint32_t start_block)
{
    return gd5f1g_block_to_row(start_block);
}

static uint8_t scratch_page_valid(const uint8_t *scratch, uint32_t scratch_length)
{
    return (uint8_t)((scratch != 0) && (scratch_length >= GD5F1G_PAGE_SIZE));
}

uint32_t gd5f1g_l8_asset_align_page(uint32_t value)
{
    return ((value + GD5F1G_PAGE_SIZE - 1u) / GD5F1G_PAGE_SIZE) * GD5F1G_PAGE_SIZE;
}

uint32_t gd5f1g_l8_asset_fnv1a_update(uint32_t checksum, uint8_t value)
{
    checksum ^= value;
    checksum *= 16777619u;
    return checksum;
}

uint32_t gd5f1g_l8_asset_fnv1a_buffer(const uint8_t *data, uint32_t length)
{
    uint32_t i;
    uint32_t checksum = GD5F1G_L8_ASSET_FNV1A_SEED;

    if(data == 0)
    {
        return 0u;
    }

    for(i = 0u; i < length; i++)
    {
        checksum = gd5f1g_l8_asset_fnv1a_update(checksum, data[i]);
    }
    return checksum;
}

void gd5f1g_l8_asset_fill_gray_clut(uint8_t *data, uint16_t entries)
{
    uint16_t i;

    if(data == 0)
    {
        return;
    }

    for(i = 0u; i < entries; i++)
    {
        data[((uint32_t)i * 3u) + 0u] = (uint8_t)i;
        data[((uint32_t)i * 3u) + 1u] = (uint8_t)i;
        data[((uint32_t)i * 3u) + 2u] = (uint8_t)i;
    }
}

int gd5f1g_l8_asset_lzss_decode(const uint8_t *source,
                                 uint32_t source_length,
                                 uint8_t *dest,
                                 uint32_t dest_length)
{
    uint32_t source_index = 0u;
    uint32_t dest_index = 0u;
    uint8_t flags = 0u;
    uint8_t bit = 8u;

    if((source == 0) || (dest == 0))
    {
        return GD5F1G_ERR_PARAM;
    }

    while(dest_index < dest_length)
    {
        if(bit >= 8u)
        {
            if(source_index >= source_length)
            {
                return GD5F1G_ERR_VERIFY;
            }
            flags = source[source_index++];
            bit = 0u;
        }

        if((flags & (uint8_t)(1u << bit)) != 0u)
        {
            uint16_t token;
            uint32_t offset;
            uint32_t length;
            uint32_t i;

            if((source_index + 2u) > source_length)
            {
                return GD5F1G_ERR_VERIFY;
            }
            token = (uint16_t)source[source_index] |
                    ((uint16_t)source[source_index + 1u] << 8);
            source_index += 2u;
            offset = token & 0x0FFFu;
            length = ((uint32_t)token >> 12) + 3u;

            if((offset == 0u) ||
               (offset > dest_index) ||
               ((dest_index + length) > dest_length))
            {
                return GD5F1G_ERR_VERIFY;
            }

            for(i = 0u; i < length; i++)
            {
                dest[dest_index] = dest[dest_index - offset];
                dest_index++;
            }
        }
        else
        {
            if(source_index >= source_length)
            {
                return GD5F1G_ERR_VERIFY;
            }
            dest[dest_index++] = source[source_index++];
        }
        bit++;
    }

    return GD5F1G_OK;
}

void gd5f1g_l8_asset_unfilter_left(uint8_t *data,
                                   uint16_t width,
                                   uint16_t height)
{
    uint16_t x;
    uint16_t y;

    if(data == 0)
    {
        return;
    }

    for(y = 0u; y < height; y++)
    {
        uint8_t left = 0u;
        for(x = 0u; x < width; x++)
        {
            uint32_t index = ((uint32_t)y * width) + x;
            data[index] = (uint8_t)(data[index] + left);
            left = data[index];
        }
    }
}

int gd5f1g_l8_asset_program_linear(const gd5f1g_spi_bus_t *bus,
                                   uint32_t start_block,
                                   uint32_t offset,
                                   const uint8_t *data,
                                   uint32_t length)
{
    uint32_t done = 0u;

    if((data == 0) || (length == 0u) || (start_block >= GD5F1G_BLOCK_COUNT))
    {
        return GD5F1G_ERR_PARAM;
    }

    while(done < length)
    {
        uint32_t absolute = offset + done;
        uint32_t row = start_row(start_block) + (absolute / GD5F1G_PAGE_SIZE);
        uint16_t column = (uint16_t)(absolute % GD5F1G_PAGE_SIZE);
        uint32_t chunk = GD5F1G_PAGE_SIZE - column;
        int result;

        if(chunk > (length - done))
        {
            chunk = length - done;
        }

        result = gd5f1g_program_page(bus, row, column, &data[done], chunk);
        if(result != GD5F1G_OK)
        {
            return result;
        }
        done += chunk;
    }

    return GD5F1G_OK;
}

int gd5f1g_l8_asset_read_linear(const gd5f1g_spi_bus_t *bus,
                                uint32_t start_block,
                                uint32_t offset,
                                uint8_t *data,
                                uint32_t length,
                                uint8_t *status_out)
{
    uint32_t done = 0u;

    if((data == 0) || (length == 0u) || (start_block >= GD5F1G_BLOCK_COUNT))
    {
        return GD5F1G_ERR_PARAM;
    }

    while(done < length)
    {
        uint32_t absolute = offset + done;
        uint32_t row = start_row(start_block) + (absolute / GD5F1G_PAGE_SIZE);
        uint16_t column = (uint16_t)(absolute % GD5F1G_PAGE_SIZE);
        uint32_t chunk = GD5F1G_PAGE_SIZE - column;
        uint8_t status = 0u;
        int result;

        if(chunk > (length - done))
        {
            chunk = length - done;
        }

        result = gd5f1g_read_page(bus, row, column, &data[done], chunk, &status);
        if(status_out != 0)
        {
            *status_out = status;
        }
        if(result != GD5F1G_OK)
        {
            return result;
        }
        done += chunk;
    }

    return GD5F1G_OK;
}

int gd5f1g_l8_asset_checksum_linear(const gd5f1g_spi_bus_t *bus,
                                    uint32_t start_block,
                                    uint32_t offset,
                                    uint32_t length,
                                    uint8_t *scratch,
                                    uint32_t scratch_length,
                                    uint32_t *checksum_out,
                                    uint8_t *status_out)
{
    uint32_t done = 0u;
    uint32_t checksum = GD5F1G_L8_ASSET_FNV1A_SEED;

    if((scratch == 0) ||
       (scratch_length == 0u) ||
       (checksum_out == 0) ||
       (length == 0u))
    {
        return GD5F1G_ERR_PARAM;
    }
    if(scratch_length > GD5F1G_PAGE_SIZE)
    {
        scratch_length = GD5F1G_PAGE_SIZE;
    }

    while(done < length)
    {
        uint32_t chunk = length - done;
        uint32_t i;
        int result;

        if(chunk > scratch_length)
        {
            chunk = scratch_length;
        }

        result = gd5f1g_l8_asset_read_linear(bus,
                                             start_block,
                                             offset + done,
                                             scratch,
                                             chunk,
                                             status_out);
        if(result != GD5F1G_OK)
        {
            return result;
        }
        for(i = 0u; i < chunk; i++)
        {
            checksum = gd5f1g_l8_asset_fnv1a_update(checksum, scratch[i]);
        }
        done += chunk;
    }

    *checksum_out = checksum;
    return GD5F1G_OK;
}

int gd5f1g_l8_asset_verify_linear(const gd5f1g_spi_bus_t *bus,
                                  uint32_t start_block,
                                  uint32_t offset,
                                  uint32_t length,
                                  uint32_t expected_checksum,
                                  uint8_t *scratch,
                                  uint32_t scratch_length,
                                  uint8_t *status_out)
{
    uint32_t checksum = 0u;
    int result = gd5f1g_l8_asset_checksum_linear(bus,
                                                 start_block,
                                                 offset,
                                                 length,
                                                 scratch,
                                                 scratch_length,
                                                 &checksum,
                                                 status_out);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    if(checksum != expected_checksum)
    {
        return GD5F1G_ERR_VERIFY;
    }
    return GD5F1G_OK;
}

void gd5f1g_l8_asset_manifest_init(gd5f1g_l8_asset_manifest_t *manifest,
                                   uint32_t width,
                                   uint32_t height,
                                   uint32_t total_bytes)
{
    uint32_t i;

    if(manifest == 0)
    {
        return;
    }

    manifest->width = width;
    manifest->height = height;
    manifest->total_bytes = total_bytes;
    for(i = 0u; i < GD5F1G_L8_ASSET_ENTRY_COUNT; i++)
    {
        manifest->entries[i].type = 0u;
        manifest->entries[i].offset = 0u;
        manifest->entries[i].length = 0u;
        manifest->entries[i].checksum = 0u;
    }
}

int gd5f1g_l8_asset_manifest_set(gd5f1g_l8_asset_manifest_t *manifest,
                                 uint32_t slot,
                                 uint32_t type,
                                 uint32_t offset,
                                 uint32_t length,
                                 uint32_t checksum)
{
    if((manifest == 0) ||
       (slot >= GD5F1G_L8_ASSET_ENTRY_COUNT) ||
       (type == 0u) ||
       (length == 0u))
    {
        return GD5F1G_ERR_PARAM;
    }

    manifest->entries[slot].type = type;
    manifest->entries[slot].offset = offset;
    manifest->entries[slot].length = length;
    manifest->entries[slot].checksum = checksum;
    return GD5F1G_OK;
}

int gd5f1g_l8_asset_manifest_find(const gd5f1g_l8_asset_manifest_t *manifest,
                                  uint32_t type,
                                  gd5f1g_l8_asset_entry_t *entry_out)
{
    uint32_t slot;

    if((manifest == 0) || (entry_out == 0) || (type == 0u))
    {
        return GD5F1G_ERR_PARAM;
    }

    for(slot = 0u; slot < GD5F1G_L8_ASSET_ENTRY_COUNT; slot++)
    {
        if(manifest->entries[slot].type == type)
        {
            *entry_out = manifest->entries[slot];
            return GD5F1G_OK;
        }
    }
    return GD5F1G_ERR_VERIFY;
}

int gd5f1g_l8_asset_write_manifest(const gd5f1g_spi_bus_t *bus,
                                   uint32_t start_block,
                                   const gd5f1g_l8_asset_manifest_t *manifest,
                                   uint8_t *scratch_page,
                                   uint32_t scratch_length)
{
    uint32_t i;

    if((manifest == 0) || !scratch_page_valid(scratch_page, scratch_length))
    {
        return GD5F1G_ERR_PARAM;
    }

    for(i = 0u; i < GD5F1G_PAGE_SIZE; i++)
    {
        scratch_page[i] = 0xFFu;
    }

    put32(scratch_page, 0u, GD5F1G_L8_ASSET_MAGIC);
    put32(scratch_page, 4u, GD5F1G_L8_ASSET_VERSION);
    put32(scratch_page, 8u, manifest->width);
    put32(scratch_page, 12u, manifest->height);
    put32(scratch_page, 16u, GD5F1G_L8_ASSET_ENTRY_COUNT);
    put32(scratch_page, 20u, manifest->total_bytes);

    for(i = 0u; i < GD5F1G_L8_ASSET_ENTRY_COUNT; i++)
    {
        uint32_t base = GD5F1G_L8_ASSET_ENTRY_BASE +
                        (i * GD5F1G_L8_ASSET_ENTRY_STRIDE);
        put32(scratch_page, base + 0u, manifest->entries[i].type);
        put32(scratch_page, base + 4u, manifest->entries[i].offset);
        put32(scratch_page, base + 8u, manifest->entries[i].length);
        put32(scratch_page, base + 12u, manifest->entries[i].checksum);
    }

    return gd5f1g_l8_asset_program_linear(bus,
                                          start_block,
                                          0u,
                                          scratch_page,
                                          GD5F1G_PAGE_SIZE);
}

int gd5f1g_l8_asset_read_manifest(const gd5f1g_spi_bus_t *bus,
                                  uint32_t start_block,
                                  uint32_t expected_width,
                                  uint32_t expected_height,
                                  gd5f1g_l8_asset_manifest_t *manifest_out,
                                  uint8_t *scratch_page,
                                  uint32_t scratch_length,
                                  uint8_t *status_out)
{
    uint32_t i;
    int result;

    if((manifest_out == 0) || !scratch_page_valid(scratch_page, scratch_length))
    {
        return GD5F1G_ERR_PARAM;
    }

    result = gd5f1g_l8_asset_read_linear(bus,
                                         start_block,
                                         0u,
                                         scratch_page,
                                         GD5F1G_PAGE_SIZE,
                                         status_out);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    if((get32(scratch_page, 0u) != GD5F1G_L8_ASSET_MAGIC) ||
       (get32(scratch_page, 4u) != GD5F1G_L8_ASSET_VERSION) ||
       (get32(scratch_page, 8u) != expected_width) ||
       (get32(scratch_page, 12u) != expected_height) ||
       (get32(scratch_page, 16u) != GD5F1G_L8_ASSET_ENTRY_COUNT))
    {
        return GD5F1G_ERR_VERIFY;
    }

    manifest_out->width = get32(scratch_page, 8u);
    manifest_out->height = get32(scratch_page, 12u);
    manifest_out->total_bytes = get32(scratch_page, 20u);
    for(i = 0u; i < GD5F1G_L8_ASSET_ENTRY_COUNT; i++)
    {
        uint32_t base = GD5F1G_L8_ASSET_ENTRY_BASE +
                        (i * GD5F1G_L8_ASSET_ENTRY_STRIDE);
        manifest_out->entries[i].type = get32(scratch_page, base + 0u);
        manifest_out->entries[i].offset = get32(scratch_page, base + 4u);
        manifest_out->entries[i].length = get32(scratch_page, base + 8u);
        manifest_out->entries[i].checksum = get32(scratch_page, base + 12u);
    }

    return GD5F1G_OK;
}

int gd5f1g_l8_asset_check_blocks(const gd5f1g_spi_bus_t *bus,
                                 uint32_t start_block,
                                 uint32_t block_count,
                                 uint8_t *marker_out,
                                 uint8_t *status_out)
{
    uint32_t block;

    if((block_count == 0u) ||
       (start_block >= GD5F1G_BLOCK_COUNT) ||
       ((start_block + block_count) > GD5F1G_BLOCK_COUNT))
    {
        return GD5F1G_ERR_PARAM;
    }

    for(block = 0u; block < block_count; block++)
    {
        uint8_t marker = 0u;
        uint8_t status = 0u;
        int result = gd5f1g_read_bad_block_marker(bus,
                                                  start_block + block,
                                                  &marker,
                                                  &status);
        if(marker_out != 0)
        {
            *marker_out = marker;
        }
        if(status_out != 0)
        {
            *status_out = status;
        }
        if((result != GD5F1G_OK) || (marker != 0xFFu))
        {
            return (result != GD5F1G_OK) ? result : GD5F1G_ERR_NO_SCRATCH_BLOCK;
        }
    }

    return GD5F1G_OK;
}

int gd5f1g_l8_asset_erase_blocks(const gd5f1g_spi_bus_t *bus,
                                 uint32_t start_block,
                                 uint32_t block_count)
{
    uint32_t block;
    int result;

    if((block_count == 0u) ||
       (start_block >= GD5F1G_BLOCK_COUNT) ||
       ((start_block + block_count) > GD5F1G_BLOCK_COUNT))
    {
        return GD5F1G_ERR_PARAM;
    }

    result = gd5f1g_unlock_all_blocks(bus);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    for(block = 0u; block < block_count; block++)
    {
        result = gd5f1g_block_erase(bus, start_block + block);
        if(result != GD5F1G_OK)
        {
            return result;
        }
    }

    return GD5F1G_OK;
}
