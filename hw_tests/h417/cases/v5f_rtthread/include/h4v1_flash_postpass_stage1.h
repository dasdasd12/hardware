#ifndef H4V1_FLASH_POSTPASS_STAGE1_H
#define H4V1_FLASH_POSTPASS_STAGE1_H

/*
 * Explicit post-PASS probe only.
 *
 * This interface deliberately exposes no RT-Thread init hook and creates no
 * thread.  The caller must invoke h4v1_flash_postpass_stage1_run() only after
 * the qualified H4V1 USB -> SDRAM -> decode -> LTDC test has reported PASS.
 */
typedef enum
{
    H4V1_FLASH_POSTPASS_STAGE1_OK = 0,
    H4V1_FLASH_POSTPASS_STAGE1_ERR_ID = -1,
    H4V1_FLASH_POSTPASS_STAGE1_ERR_FEATURE = -2,
    H4V1_FLASH_POSTPASS_STAGE1_ERR_SPI_TIMEOUT = -3
} h4v1_flash_postpass_stage1_result_t;

int h4v1_flash_postpass_stage1_run(void);

#endif
