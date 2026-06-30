#ifndef V3F_PROFILE_STORE_GD5F1G_H
#define V3F_PROFILE_STORE_GD5F1G_H

#include <stdint.h>

#include "aik_profile_runtime.h"

#define V3F_PROFILE_STORE_OK 0
#define V3F_PROFILE_STORE_ERR_FLASH -1
#define V3F_PROFILE_STORE_ERR_INVALID -2

typedef struct
{
    int flash_result;
    uint8_t nand_status;
    uint32_t generation;
    uint16_t crc16;
    uint16_t calculated_crc16;
} v3f_profile_store_diag_t;

int v3f_profile_store_load_fixed(aik_profile_runtime_v1_t *runtime,
                                 v3f_profile_store_diag_t *diag);

#endif
