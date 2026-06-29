#include "rgb_status.h"

#ifndef V3F_ENABLE_RGB_STATUS
#define V3F_ENABLE_RGB_STATUS 0
#endif

#ifndef V3F_RGB_LED_COUNT
#define V3F_RGB_LED_COUNT 1
#endif

#if V3F_ENABLE_RGB_STATUS
#include <stdint.h>
#include "ch32h417_pioc_rgb1w.h"
#endif

void v3f_rgb_status_init(void)
{
#if V3F_ENABLE_RGB_STATUS
    ch32h417_pioc_rgb1w_init(&ch32h417_pioc_rgb1w_pin_pf13);
#endif
}

void v3f_rgb_status_red_once(void)
{
#if V3F_ENABLE_RGB_STATUS
    uint8_t grb[V3F_RGB_LED_COUNT * 3U];
    uint16_t i;

    for(i = 0U; i < V3F_RGB_LED_COUNT; i++)
    {
        grb[(i * 3U) + 0U] = 0x00U;
        grb[(i * 3U) + 1U] = 0x20U;
        grb[(i * 3U) + 2U] = 0x00U;
    }
    (void)ch32h417_pioc_rgb1w_send_ram(&ch32h417_pioc_rgb1w_pin_pf13,
                                       grb,
                                       (uint16_t)sizeof(grb),
                                       100000U);
#endif
}
