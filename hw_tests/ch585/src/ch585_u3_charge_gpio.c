#include "ch585_common.h"

#define U3_CHARGE GPIO_Pin_6
#define U3_STDBY GPIO_Pin_7

static uint32_t read_charge(void)
{
    uint32_t v = 0;
    v |= GPIOB_ReadPortPin(U3_CHARGE) ? 0x01u : 0u;
    v |= GPIOB_ReadPortPin(U3_STDBY) ? 0x02u : 0u;
    return v;
}

void ch585_u3_charge_gpio_run(void)
{
    uint32_t last;

    GPIOB_ModeCfg(U3_CHARGE | U3_STDBY, GPIO_ModeIN_PU);
    last = read_charge();

    ch585_log_kv_hex("state", last, 1);
    ch585_log_pass("ch585_u3_charge_gpio_ready");

    while(1)
    {
        uint32_t now = read_charge();
        if(now != last)
        {
            ch585_log_kv_hex("state", now, 1);
            last = now;
        }
        ch585_delay_ms(50);
    }
}
