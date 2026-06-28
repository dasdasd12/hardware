#ifndef CH585_COMMON_H
#define CH585_COMMON_H

#include <stdint.h>
#include "CH585SFR.h"
#include "core_riscv.h"
#include "CH58x_clk.h"
#include "CH58x_gpio.h"
#include "CH58x_sys.h"
#include "CH58x_uart.h"

#define CH585_SERIAL_RX1_PA8_PIN GPIO_Pin_8
#define CH585_SERIAL_TX1_PA9_PIN GPIO_Pin_9

void ch585_board_init(void);
void ch585_delay_cycles(uint32_t cycles);
void ch585_delay_ms(uint16_t ms);
void ch585_log_str(const char *text);
void ch585_log_line(const char *text);
void ch585_log_u32_dec(uint32_t value);
void ch585_log_u32_hex(uint32_t value, uint8_t digits);
void ch585_log_kv_hex(const char *key, uint32_t value, uint8_t digits);
void ch585_log_start(void);
void ch585_log_pass(const char *item);
void ch585_log_fail(const char *item, const char *reason);
void ch585_log_skip(const char *item, const char *reason);

void ch585_u2_eeprom_i2c_run(void);
void ch585_u2_controls_gpio_run(void);
void ch585_u3_max17048_i2c_run(void);
void ch585_u3_charge_gpio_run(void);
void ch585_u3_ec11_gpio_run(void);
void ch585_adc_mux_scan_run(void);
void ch585_spi0_speed_slave_run(void);

#endif
