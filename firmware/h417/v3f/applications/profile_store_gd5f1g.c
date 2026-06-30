#include "profile_store_gd5f1g.h"

#include <string.h>

#include "ch32h417_gd5f1g_spi1.h"
#include "gd5f1g_spi_nand.h"

#ifndef V3F_PROFILE_FLASH_BLOCK
#define V3F_PROFILE_FLASH_BLOCK 32U
#endif

#ifndef V3F_PROFILE_FLASH_PAGE
#define V3F_PROFILE_FLASH_PAGE 0U
#endif

int v3f_profile_store_load_fixed(aik_profile_runtime_v1_t *runtime,
                                 v3f_profile_store_diag_t *diag)
{
    ch32h417_gd5f1g_spi1_context_t spi_context;
    gd5f1g_spi_bus_t bus;
    uint8_t nand_status = 0U;
    int rc;

    if(runtime == 0)
    {
        return V3F_PROFILE_STORE_ERR_INVALID;
    }

    memset(runtime, 0, sizeof(*runtime));
    if(diag != 0)
    {
        memset(diag, 0, sizeof(*diag));
    }

    ch32h417_gd5f1g_spi1_init(&spi_context, &bus);

    rc = gd5f1g_reset(&bus);
    if(rc == GD5F1G_OK)
    {
        uint32_t row = gd5f1g_block_to_row(V3F_PROFILE_FLASH_BLOCK) +
                       (uint32_t)V3F_PROFILE_FLASH_PAGE;
        rc = gd5f1g_read_page(&bus,
                              row,
                              0U,
                              (uint8_t *)runtime,
                              sizeof(*runtime),
                              &nand_status);
    }

    if(diag != 0)
    {
        diag->flash_result = rc;
        diag->nand_status = nand_status;
        diag->generation = runtime->generation;
        diag->crc16 = runtime->crc16;
        diag->calculated_crc16 = aik_profile_runtime_crc(runtime);
    }

    ch32h417_gd5f1g_spi1_release(&spi_context);

    if(rc != GD5F1G_OK)
    {
        memset(runtime, 0, sizeof(*runtime));
        return V3F_PROFILE_STORE_ERR_FLASH;
    }

    if(aik_profile_runtime_valid(runtime) == 0U)
    {
        return V3F_PROFILE_STORE_ERR_INVALID;
    }

    return V3F_PROFILE_STORE_OK;
}
