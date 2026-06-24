#include "gd5f1g_spi_nand.h"

#define GD5F1G_CMD_WRITE_ENABLE     0x06u
#define GD5F1G_CMD_WRITE_DISABLE    0x04u
#define GD5F1G_CMD_GET_FEATURE      0x0Fu
#define GD5F1G_CMD_SET_FEATURE      0x1Fu
#define GD5F1G_CMD_PAGE_READ        0x13u
#define GD5F1G_CMD_READ_CACHE       0x03u
#define GD5F1G_CMD_READ_ID          0x9Fu
#define GD5F1G_CMD_PROGRAM_LOAD     0x02u
#define GD5F1G_CMD_PROGRAM_EXECUTE  0x10u
#define GD5F1G_CMD_BLOCK_ERASE      0xD8u
#define GD5F1G_CMD_RESET            0xFFu

#define GD5F1G_WAIT_READY_POLLS     200000u

static int gd5f1g_bus_valid(const gd5f1g_spi_bus_t *bus)
{
    return (bus != 0) &&
           (bus->transfer != 0) &&
           (bus->select != 0) &&
           (bus->deselect != 0);
}

static uint8_t gd5f1g_transfer(const gd5f1g_spi_bus_t *bus, uint8_t tx)
{
    return bus->transfer(bus->context, tx);
}

static void gd5f1g_command(const gd5f1g_spi_bus_t *bus, uint8_t command)
{
    bus->select(bus->context);
    (void)gd5f1g_transfer(bus, command);
    bus->deselect(bus->context);
}

static void gd5f1g_write_row(const gd5f1g_spi_bus_t *bus, uint32_t row_address)
{
    (void)gd5f1g_transfer(bus, (uint8_t)(row_address >> 16));
    (void)gd5f1g_transfer(bus, (uint8_t)(row_address >> 8));
    (void)gd5f1g_transfer(bus, (uint8_t)row_address);
}

static int gd5f1g_wait_ready(const gd5f1g_spi_bus_t *bus, uint8_t *status_out)
{
    uint32_t i;
    uint8_t status = 0xFFu;

    for(i = 0; i < GD5F1G_WAIT_READY_POLLS; ++i)
    {
        if(gd5f1g_get_feature(bus, GD5F1G_FEATURE_STATUS, &status) != GD5F1G_OK)
        {
            return GD5F1G_ERR_PARAM;
        }

        if((status & GD5F1G_STATUS_OIP) == 0u)
        {
            if(status_out != 0)
            {
                *status_out = status;
            }
            return GD5F1G_OK;
        }
    }

    if(status_out != 0)
    {
        *status_out = status;
    }
    return GD5F1G_ERR_TIMEOUT;
}

static int gd5f1g_write_enable(const gd5f1g_spi_bus_t *bus)
{
    uint8_t status = 0u;

    gd5f1g_command(bus, GD5F1G_CMD_WRITE_ENABLE);
    if(gd5f1g_get_feature(bus, GD5F1G_FEATURE_STATUS, &status) != GD5F1G_OK)
    {
        return GD5F1G_ERR_PARAM;
    }

    if((status & GD5F1G_STATUS_WEL) == 0u)
    {
        return GD5F1G_ERR_WRITE_ENABLE;
    }

    return GD5F1G_OK;
}

uint32_t gd5f1g_block_to_row(uint32_t block)
{
    return block * GD5F1G_PAGES_PER_BLOCK;
}

int gd5f1g_get_feature(const gd5f1g_spi_bus_t *bus, uint8_t address, uint8_t *value)
{
    if(!gd5f1g_bus_valid(bus) || (value == 0))
    {
        return GD5F1G_ERR_PARAM;
    }

    bus->select(bus->context);
    (void)gd5f1g_transfer(bus, GD5F1G_CMD_GET_FEATURE);
    (void)gd5f1g_transfer(bus, address);
    *value = gd5f1g_transfer(bus, 0xFFu);
    bus->deselect(bus->context);

    return GD5F1G_OK;
}

int gd5f1g_set_feature(const gd5f1g_spi_bus_t *bus, uint8_t address, uint8_t value)
{
    if(!gd5f1g_bus_valid(bus))
    {
        return GD5F1G_ERR_PARAM;
    }

    bus->select(bus->context);
    (void)gd5f1g_transfer(bus, GD5F1G_CMD_SET_FEATURE);
    (void)gd5f1g_transfer(bus, address);
    (void)gd5f1g_transfer(bus, value);
    bus->deselect(bus->context);

    return GD5F1G_OK;
}

int gd5f1g_reset(const gd5f1g_spi_bus_t *bus)
{
    uint8_t status = 0u;

    if(!gd5f1g_bus_valid(bus))
    {
        return GD5F1G_ERR_PARAM;
    }

    gd5f1g_command(bus, GD5F1G_CMD_RESET);
    if(bus->delay_us != 0)
    {
        bus->delay_us(bus->context, 2000u);
    }

    return gd5f1g_wait_ready(bus, &status);
}

int gd5f1g_read_id(const gd5f1g_spi_bus_t *bus, uint8_t *manufacturer_id, uint8_t *device_id)
{
    if(!gd5f1g_bus_valid(bus) || (manufacturer_id == 0) || (device_id == 0))
    {
        return GD5F1G_ERR_PARAM;
    }

    bus->select(bus->context);
    (void)gd5f1g_transfer(bus, GD5F1G_CMD_READ_ID);
    (void)gd5f1g_transfer(bus, 0x00u);
    *manufacturer_id = gd5f1g_transfer(bus, 0xFFu);
    *device_id = gd5f1g_transfer(bus, 0xFFu);
    bus->deselect(bus->context);

    if((*manufacturer_id != GD5F1G_MANUFACTURER_ID) ||
       (*device_id != GD5F1G_DEVICE_ID_3V))
    {
        return GD5F1G_ERR_ID;
    }

    return GD5F1G_OK;
}

int gd5f1g_read_info(const gd5f1g_spi_bus_t *bus, gd5f1g_info_t *info)
{
    int result;

    if(info == 0)
    {
        return GD5F1G_ERR_PARAM;
    }

    result = gd5f1g_read_id(bus, &info->manufacturer_id, &info->device_id);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = gd5f1g_get_feature(bus, GD5F1G_FEATURE_PROTECTION, &info->protection);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = gd5f1g_get_feature(bus, GD5F1G_FEATURE_CONFIG, &info->config);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = gd5f1g_get_feature(bus, GD5F1G_FEATURE_STATUS, &info->status);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    return gd5f1g_get_feature(bus, GD5F1G_FEATURE_STATUS2, &info->status2);
}

int gd5f1g_unlock_all_blocks(const gd5f1g_spi_bus_t *bus)
{
    uint8_t protection = 0xFFu;
    int result;

    result = gd5f1g_set_feature(bus, GD5F1G_FEATURE_PROTECTION, 0x00u);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    result = gd5f1g_get_feature(bus, GD5F1G_FEATURE_PROTECTION, &protection);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    if((protection & 0xBEu) != 0u)
    {
        return GD5F1G_ERR_PROTECTION;
    }

    return GD5F1G_OK;
}

int gd5f1g_block_erase(const gd5f1g_spi_bus_t *bus, uint32_t block)
{
    uint8_t status = 0u;
    int result;

    if(!gd5f1g_bus_valid(bus) || (block >= GD5F1G_BLOCK_COUNT))
    {
        return GD5F1G_ERR_PARAM;
    }

    result = gd5f1g_unlock_all_blocks(bus);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    result = gd5f1g_write_enable(bus);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    bus->select(bus->context);
    (void)gd5f1g_transfer(bus, GD5F1G_CMD_BLOCK_ERASE);
    gd5f1g_write_row(bus, gd5f1g_block_to_row(block));
    bus->deselect(bus->context);

    result = gd5f1g_wait_ready(bus, &status);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    if((status & GD5F1G_STATUS_E_FAIL) != 0u)
    {
        return GD5F1G_ERR_ERASE;
    }

    return GD5F1G_OK;
}

int gd5f1g_program_page(const gd5f1g_spi_bus_t *bus,
                        uint32_t row_address,
                        uint16_t column,
                        const uint8_t *data,
                        uint32_t length)
{
    uint32_t i;
    uint8_t status = 0u;
    int result;

    if(!gd5f1g_bus_valid(bus) ||
       (data == 0) ||
       (length == 0u) ||
       ((uint32_t)column + length > GD5F1G_PAGE_SIZE))
    {
        return GD5F1G_ERR_PARAM;
    }

    result = gd5f1g_write_enable(bus);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    bus->select(bus->context);
    (void)gd5f1g_transfer(bus, GD5F1G_CMD_PROGRAM_LOAD);
    (void)gd5f1g_transfer(bus, (uint8_t)(column >> 8));
    (void)gd5f1g_transfer(bus, (uint8_t)column);
    for(i = 0; i < length; ++i)
    {
        (void)gd5f1g_transfer(bus, data[i]);
    }
    bus->deselect(bus->context);

    bus->select(bus->context);
    (void)gd5f1g_transfer(bus, GD5F1G_CMD_PROGRAM_EXECUTE);
    gd5f1g_write_row(bus, row_address);
    bus->deselect(bus->context);

    result = gd5f1g_wait_ready(bus, &status);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    if((status & GD5F1G_STATUS_P_FAIL) != 0u)
    {
        return GD5F1G_ERR_PROGRAM;
    }

    return GD5F1G_OK;
}

int gd5f1g_read_page(const gd5f1g_spi_bus_t *bus,
                     uint32_t row_address,
                     uint16_t column,
                     uint8_t *data,
                     uint32_t length,
                     uint8_t *status_out)
{
    uint32_t i;
    uint8_t status = 0u;
    int result;

    if(!gd5f1g_bus_valid(bus) ||
       (data == 0) ||
       (length == 0u) ||
       ((uint32_t)column + length > (GD5F1G_PAGE_SIZE + GD5F1G_SPARE_SIZE_ECC_ON)))
    {
        return GD5F1G_ERR_PARAM;
    }

    bus->select(bus->context);
    (void)gd5f1g_transfer(bus, GD5F1G_CMD_PAGE_READ);
    gd5f1g_write_row(bus, row_address);
    bus->deselect(bus->context);

    result = gd5f1g_wait_ready(bus, &status);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    if((status & GD5F1G_STATUS_ECC_MASK) == GD5F1G_STATUS_ECC_UNCORR)
    {
        if(status_out != 0)
        {
            *status_out = status;
        }
        return GD5F1G_ERR_ECC;
    }

    bus->select(bus->context);
    (void)gd5f1g_transfer(bus, GD5F1G_CMD_READ_CACHE);
    (void)gd5f1g_transfer(bus, (uint8_t)(column >> 8));
    (void)gd5f1g_transfer(bus, (uint8_t)column);
    (void)gd5f1g_transfer(bus, 0x00u);
    for(i = 0; i < length; ++i)
    {
        data[i] = gd5f1g_transfer(bus, 0xFFu);
    }
    bus->deselect(bus->context);

    if(status_out != 0)
    {
        *status_out = status;
    }

    return GD5F1G_OK;
}
