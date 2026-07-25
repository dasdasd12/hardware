#include "ch585_common.h"

#define U3_ENC_BUTTON GPIO_Pin_7
#define U3_ENC_CHA GPIO_Pin_10
#define U3_ENC_CHB GPIO_Pin_11

static uint8_t read_ab(void)
{
    uint8_t v = 0;
    v |= GPIOA_ReadPortPin(U3_ENC_CHA) ? 0x01u : 0u;
    v |= GPIOA_ReadPortPin(U3_ENC_CHB) ? 0x02u : 0u;
    return v;
}

void ch585_u3_ec11_gpio_run(void)
{
    uint8_t last_ab;
    uint8_t last_button;
    int32_t count = 0;

    GPIOA_ModeCfg(U3_ENC_BUTTON | U3_ENC_CHA | U3_ENC_CHB, GPIO_ModeIN_PU);
    last_ab = read_ab();
    last_button = GPIOA_ReadPortPin(U3_ENC_BUTTON) ? 1u : 0u;

    ch585_log_kv_hex("ab", last_ab, 1);
    ch585_log_kv_hex("button", last_button, 1);
    ch585_log_pass("ch585_u3_ec11_gpio_ready");

    while(1)
    {
        uint8_t ab = read_ab();
        uint8_t button = GPIOA_ReadPortPin(U3_ENC_BUTTON) ? 1u : 0u;

        if(ab != last_ab)
        {
            uint8_t transition = (uint8_t)((last_ab << 2) | ab);
            if(transition == 0x01u || transition == 0x07u || transition == 0x0Eu || transition == 0x08u)
            {
                count++;
            }
            else if(transition == 0x02u || transition == 0x0Bu || transition == 0x0Du || transition == 0x04u)
            {
                count--;
            }
            ch585_log_kv_hex("ab", ab, 1);
            ch585_log_str("DATA count=");
            if(count < 0)
            {
                ch585_log_str("-");
                ch585_log_u32_dec((uint32_t)(-count));
            }
            else
            {
                ch585_log_u32_dec((uint32_t)count);
            }
            ch585_log_str("\r\n");
            last_ab = ab;
        }

        if(button != last_button)
        {
            ch585_log_kv_hex("button", button, 1);
            last_button = button;
        }
        ch585_delay_ms(2);
    }
}
