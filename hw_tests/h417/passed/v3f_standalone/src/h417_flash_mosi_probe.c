#include "h417_common.h"

#define FLASH_CS_PORT             GPIOF
#define FLASH_CS_PIN              GPIO_Pin_6
#define FLASH_SCK_PIN             GPIO_Pin_7
#define FLASH_MOSI_PIN            GPIO_Pin_8
#define FLASH_MISO_PIN            GPIO_Pin_9

#define FLASH_MOSI_PROBE_ITEM     73u
#define PROBE_HALF_PERIOD_CYCLES  50000u
#define GPIO_CFG_OUT_PP           0x1u
#define GPIO_CFG_IN_FLOATING      0x4u
#define GPIO_SPEED_VERY_HIGH      0x3u

typedef struct
{
    volatile uint32_t phase;
    volatile uint32_t gpiof_cfglr;
    volatile uint32_t gpiof_cfghr;
    volatile uint32_t gpiof_outdr;
    volatile uint32_t gpiof_indr;
} h417_flash_mosi_probe_debug_t;

volatile h417_flash_mosi_probe_debug_t g_h417_flash_mosi_probe_debug;

static void flash_mosi_probe_capture(uint32_t phase)
{
    g_h417_flash_mosi_probe_debug.phase = phase;
    g_h417_flash_mosi_probe_debug.gpiof_cfglr = GPIOF->CFGLR;
    g_h417_flash_mosi_probe_debug.gpiof_cfghr = GPIOF->CFGHR;
    g_h417_flash_mosi_probe_debug.gpiof_outdr = GPIOF->OUTDR;
    g_h417_flash_mosi_probe_debug.gpiof_indr = GPIOF->INDR;
}

static void flash_mosi_probe_gpio_init(void)
{
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                          RCC_HB2Periph_GPIOF,
                          ENABLE);

    AFIO->GPIOF_AFLR &= ~((0xFu << 24u) | (0xFu << 28u));
    AFIO->GPIOF_AFHR &= ~((0xFu << 0u) | (0xFu << 4u));

    GPIOF->CFGLR &= ~((0xFu << 24u) | (0xFu << 28u));
    GPIOF->CFGLR |= ((GPIO_CFG_OUT_PP << 24u) |
                     (GPIO_CFG_OUT_PP << 28u));

    GPIOF->CFGHR &= ~((0xFu << 0u) | (0xFu << 4u));
    GPIOF->CFGHR |= ((GPIO_CFG_OUT_PP << 0u) |
                     (GPIO_CFG_IN_FLOATING << 4u));

    GPIOF->SPEED &= ~((0x3u << 12u) |
                      (0x3u << 14u) |
                      (0x3u << 16u));
    GPIOF->SPEED |= ((GPIO_SPEED_VERY_HIGH << 12u) |
                     (GPIO_SPEED_VERY_HIGH << 14u) |
                     (GPIO_SPEED_VERY_HIGH << 16u));

    GPIOF->BSHR = FLASH_CS_PIN;
    GPIOF->BCR = FLASH_SCK_PIN | FLASH_MOSI_PIN;
    flash_mosi_probe_capture(1u);
}

void h417_flash_mosi_probe_run(void)
{
    flash_mosi_probe_gpio_init();
    h417_status_pass(FLASH_MOSI_PROBE_ITEM);

    while(1)
    {
        GPIOF->BSHR = FLASH_CS_PIN | FLASH_MOSI_PIN;
        GPIOF->BCR = FLASH_SCK_PIN;
        g_h417_status.cycle++;
        flash_mosi_probe_capture(2u);
        h417_delay_cycles(PROBE_HALF_PERIOD_CYCLES);

        GPIOF->BSHR = FLASH_CS_PIN;
        GPIOF->BCR = FLASH_SCK_PIN | FLASH_MOSI_PIN;
        g_h417_status.cycle++;
        flash_mosi_probe_capture(3u);
        h417_delay_cycles(PROBE_HALF_PERIOD_CYCLES);
    }
}
