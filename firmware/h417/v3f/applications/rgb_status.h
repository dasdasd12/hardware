#ifndef V3F_RGB_STATUS_H
#define V3F_RGB_STATUS_H

#include <stdint.h>

void v3f_rgb_status_init(void);
void v3f_rgb_status_red_once(void);
void v3f_rgb_status_set_enabled(uint8_t enabled);
void v3f_rgb_status_toggle_enabled(void);
void v3f_rgb_status_next_effect(void);
void v3f_rgb_status_task(uint16_t tick);
uint8_t v3f_rgb_status_effect(void);
uint8_t v3f_rgb_status_last_result(void);
uint32_t v3f_rgb_status_render_count(void);
uint32_t v3f_rgb_status_error_count(void);

#endif
