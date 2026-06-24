#ifndef CH32H417_PIOC_RGB1W_H
#define CH32H417_PIOC_RGB1W_H

#include <stdint.h>
#include "ch32h417_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CH32H417 PIOC one-wire RGB sender.
 *
 * This driver is intentionally limited to the PIOC waveform path. It does not
 * depend on V3F/V5F selection macros, interrupts, RTOS services, DMA, libc,
 * or board-to-board links. The caller owns system clock setup and board-level
 * VIO18 voltage setup.
 *
 * Timing note:
 * The bundled WCH PIOC microcode is configured for HCLK = 100 MHz. Re-tune the
 * command cycle constants before using this driver at another HCLK.
 */

#define CH32H417_PIOC_RGB1W_SFR_SIZE       32u
#define CH32H417_PIOC_RGB1W_RAM_SIZE       3072u

#define CH32H417_PIOC_RGB1W_OK             0u
#define CH32H417_PIOC_RGB1W_ERR_CMD        1u
#define CH32H417_PIOC_RGB1W_ERR_PARAM      2u
#define CH32H417_PIOC_RGB1W_ERR_OUT_HIGH   4u
#define CH32H417_PIOC_RGB1W_ERR_PIN_HIGH   6u
#define CH32H417_PIOC_RGB1W_ERR_TIMEOUT    0xfeu

typedef enum
{
    CH32H417_PIOC_RGB1W_CHANNEL_IO0 = 0,
    CH32H417_PIOC_RGB1W_CHANNEL_IO1 = 1
} ch32h417_pioc_rgb1w_channel_t;

typedef struct
{
    GPIO_TypeDef *port;
    uint32_t port_clock;
    uint16_t pin;
    uint8_t pin_source;
    uint8_t alternate_function;
    ch32h417_pioc_rgb1w_channel_t channel;
} ch32h417_pioc_rgb1w_pin_t;

/* Board routing from LaTeX: WS2812 DIN is PF13, PIOC IO1, AF5, VIO18 domain. */
extern const ch32h417_pioc_rgb1w_pin_t ch32h417_pioc_rgb1w_pin_pf13;

void ch32h417_pioc_rgb1w_init(const ch32h417_pioc_rgb1w_pin_t *pin);
uint8_t ch32h417_pioc_rgb1w_send_sfr(const ch32h417_pioc_rgb1w_pin_t *pin,
                                     const uint8_t *data,
                                     uint16_t bytes,
                                     uint32_t timeout_loops);
uint8_t ch32h417_pioc_rgb1w_send_ram(const ch32h417_pioc_rgb1w_pin_t *pin,
                                     const uint8_t *data,
                                     uint16_t bytes,
                                     uint32_t timeout_loops);
uint8_t ch32h417_pioc_rgb1w_wait(uint32_t timeout_loops);
void ch32h417_pioc_rgb1w_halt(void);

#ifdef __cplusplus
}
#endif

#endif
