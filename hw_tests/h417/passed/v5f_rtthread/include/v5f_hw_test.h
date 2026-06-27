#ifndef V5F_HW_TEST_H
#define V5F_HW_TEST_H

#include <stdint.h>

#define APP_V5F_HW_TEST_NONE  0
#define APP_V5F_HW_TEST_LTDC  1
#define APP_V5F_HW_TEST_GPHA_R2M_FILL 2
#define APP_V5F_HW_TEST_FLASH 3
#define APP_V5F_HW_TEST_LTDC_L8_PALETTE_IMAGE 4
#define APP_V5F_HW_TEST_LTDC_RGB565_DIAG 5
#define APP_V5F_HW_TEST_GPHA_PFC_L8_RGB565 6
#define APP_V5F_HW_TEST_TICK_DIAG 7
#define APP_V5F_HW_TEST_GPHA_BLEND_RGB565 8
#define APP_V5F_HW_TEST_GPHA_L8_LTDC_FULLSCREEN 9
#define APP_V5F_HW_TEST_FLASH_L8_ASSETS 10

typedef struct
{
    volatile uint32_t mode;
    volatile uint32_t phase;
    volatile int32_t last_error;
    volatile uint32_t frame_count;
    volatile uint32_t gpha_ok_count;
    volatile uint32_t gpha_fail_count;
    volatile uint32_t spi_timeout_count;
    volatile uint8_t flash_manufacturer_id;
    volatile uint8_t flash_device_id;
    volatile uint8_t flash_protection;
    volatile uint8_t flash_config;
    volatile uint8_t flash_status;
    volatile uint8_t flash_status2;
    volatile uint8_t flash_bad_marker;
    volatile uint8_t flash_bad_marker_status;
} v5f_hw_test_diag_t;

extern volatile v5f_hw_test_diag_t g_v5f_hw_test_diag;

int v5f_hw_test_start(void);

#endif
