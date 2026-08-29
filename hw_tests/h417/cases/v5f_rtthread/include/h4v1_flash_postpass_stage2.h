#ifndef H4V1_FLASH_POSTPASS_STAGE2_H
#define H4V1_FLASH_POSTPASS_STAGE2_H

/*
 * Read-only page/cache path validation.  The caller must run Stage 1 first
 * and invoke this function only after the qualified H4V1 RESULT PASS point.
 */
typedef enum
{
    H4V1_FLASH_POSTPASS_STAGE2_OK = 0,
    H4V1_FLASH_POSTPASS_STAGE2_ERR_TIMEOUT = -1,
    H4V1_FLASH_POSTPASS_STAGE2_ERR_UNSTABLE = -2,
    H4V1_FLASH_POSTPASS_STAGE2_ERR_NO_GOOD_BLOCK = -3,
    H4V1_FLASH_POSTPASS_STAGE2_ERR_ECC_DISABLED = -4,
    H4V1_FLASH_POSTPASS_STAGE2_ERR_UNCORRECTABLE = -5
} h4v1_flash_postpass_stage2_result_t;

int h4v1_flash_postpass_stage2_run(void);

#endif
