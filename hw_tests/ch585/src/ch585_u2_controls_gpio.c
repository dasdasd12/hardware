#include "ch585_common.h"

#define U2_SCR_COM GPIO_Pin_7
#define U2_SCR_UP GPIO_Pin_7
#define U2_SCR_DOWN GPIO_Pin_6
#define U2_SCR_RIGHT GPIO_Pin_5
#define U2_SCR_LEFT GPIO_Pin_4
#define U2_SCR_CHA GPIO_Pin_10
#define U2_SCR_CHB GPIO_Pin_11

static uint32_t read_controls(void)
{
    uint32_t v = 0;

    v |= GPIOB_ReadPortPin(U2_SCR_UP) ? 0x01u : 0u;
    v |= GPIOB_ReadPortPin(U2_SCR_DOWN) ? 0x02u : 0u;
    v |= GPIOB_ReadPortPin(U2_SCR_RIGHT) ? 0x04u : 0u;
    v |= GPIOB_ReadPortPin(U2_SCR_LEFT) ? 0x08u : 0u;
    v |= GPIOA_ReadPortPin(U2_SCR_CHA) ? 0x10u : 0u;
    v |= GPIOA_ReadPortPin(U2_SCR_CHB) ? 0x20u : 0u;
    return v;
}

void ch585_u2_controls_gpio_run(void)
{
    uint32_t last;

    GPIOA_ResetBits(U2_SCR_COM);
    GPIOA_ModeCfg(U2_SCR_COM, GPIO_ModeOut_PP_5mA);
    GPIOB_ModeCfg(U2_SCR_UP | U2_SCR_DOWN | U2_SCR_RIGHT | U2_SCR_LEFT, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(U2_SCR_CHA | U2_SCR_CHB, GPIO_ModeIN_PU);

    last = read_controls();
    ch585_log_kv_hex("state", last, 2);
    ch585_log_pass("ch585_u2_controls_gpio_ready");

    while(1)
    {
        uint32_t now = read_controls();
        if(now != last)
        {
            ch585_log_kv_hex("state", now, 2);
            last = now;
        }
        ch585_delay_ms(10);
    }
}
