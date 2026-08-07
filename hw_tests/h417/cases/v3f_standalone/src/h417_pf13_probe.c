#include "h417_common.h"

enum
{
    H417_ITEM_PF13_PROBE = 20
};

static const h417_pin_t pf13_pin = {GPIOF, GPIO_Pin_13, H417_ITEM_PF13_PROBE};

void h417_pf13_probe_run(void)
{
    h417_pin_output(&pf13_pin);
    h417_pin_set(&pf13_pin, 0);

    while(1)
    {
        h417_status_phase(10, H417_ITEM_PF13_PROBE);
        h417_pin_set(&pf13_pin, 1);
        h417_delay_cycles(20000u);
        h417_pin_set(&pf13_pin, 0);
        h417_delay_cycles(20000u);
        g_h417_status.cycle++;
    }
}
