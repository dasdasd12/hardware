#ifndef V3F_RGB_STATUS_H
#define V3F_RGB_STATUS_H

#include <stdint.h>

void v3f_rgb_status_init(void);
void v3f_rgb_status_red_once(void);
void v3f_rgb_status_set_enabled(uint8_t enabled);
void v3f_rgb_status_toggle_enabled(void);
void v3f_rgb_status_next_effect(void);
void v3f_rgb_status_task(uint16_t tick);

#endif
