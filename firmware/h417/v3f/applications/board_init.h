#ifndef V3F_BOARD_INIT_H
#define V3F_BOARD_INIT_H

#include <stdint.h>

void v3f_board_init(void);
void v3f_board_start_v5f(void);
void v3f_board_caps_lock_led_set(uint8_t enabled);
void v3f_board_rgb_power_set(uint8_t enabled);
void v3f_board_delay_1ms(void);
void v3f_board_delay_us(uint32_t us);
void v3f_trace_set(uint32_t index, uint32_t value);
void v3f_trace_inc(uint32_t index);

#endif
