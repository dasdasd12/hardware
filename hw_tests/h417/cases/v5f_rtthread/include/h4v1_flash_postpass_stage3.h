#ifndef H4V1_FLASH_POSTPASS_STAGE3_H
#define H4V1_FLASH_POSTPASS_STAGE3_H

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    H4V1_FLASH_POSTPASS_STAGE3_OK = 0,
    H4V1_FLASH_POSTPASS_STAGE3_ERR_DMA_BUSY = -1,
    H4V1_FLASH_POSTPASS_STAGE3_ERR_BASELINE = -2,
    H4V1_FLASH_POSTPASS_STAGE3_ERR_NO_VALID_RATE = -3,
    H4V1_FLASH_POSTPASS_STAGE3_ERR_DMA_QUIESCE = -4
};

int h4v1_flash_postpass_stage3_run(void);

#ifdef __cplusplus
}
#endif

#endif
