#include "h417_common.h"

enum
{
    H417_ITEM_CAPSLOCK = 11,
    H417_ITEM_32K = 12,
    H417_ITEM_24G = 13,
    H417_ITEM_BLE_STATE = 14
};

static const h417_pin_t status_pins[] = {
    {GPIOB, GPIO_Pin_7, H417_ITEM_CAPSLOCK},
    {GPIOF, GPIO_Pin_14, H417_ITEM_32K},
    {GPIOD, GPIO_Pin_4, H417_ITEM_24G},
    {GPIOC, GPIO_Pin_12, H417_ITEM_BLE_STATE},
};

void h417_gpio_status_run(void)
{
    unsigned int i;

    for(i = 0; i < sizeof(status_pins) / sizeof(status_pins[0]); ++i)
    {
        h417_pin_output(&status_pins[i]);
        h417_pin_set(&status_pins[i], 0);
    }

    while(1)
    {
        for(i = 0; i < sizeof(status_pins) / sizeof(status_pins[0]); ++i)
        {
            h417_status_phase(10, status_pins[i].item_id);
            h417_pin_set(&status_pins[i], 1);
            h417_delay_cycles(1000000u);
            h417_pin_set(&status_pins[i], 0);
            h417_delay_cycles(300000u);
            h417_status_pass(status_pins[i].item_id);
        }
        g_h417_status.cycle++;
    }
}
