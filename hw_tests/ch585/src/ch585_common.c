#include "ch585_common.h"

#ifndef TEST_NAME
#define TEST_NAME "unknown"
#endif

#ifndef CH585_HALF_NAME
#define CH585_HALF_NAME "unknown"
#endif

static uint16_t str_len(const char *text)
{
    uint16_t len = 0;
    while(text[len] != '\0')
    {
        len++;
    }
    return len;
}

void ch585_board_init(void)
{
    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(CLK_SOURCE_HSE_PLL_62_4MHz);

    GPIOA_SetBits(CH585_SERIAL_TX1_PA9_PIN);
    GPIOA_ModeCfg(CH585_SERIAL_RX1_PA8_PIN, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(CH585_SERIAL_TX1_PA9_PIN, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
}

void ch585_delay_cycles(uint32_t cycles)
{
    volatile uint32_t i;
    for(i = 0; i < cycles; ++i)
    {
        __asm__ volatile("nop");
    }
}

void ch585_delay_ms(uint16_t ms)
{
    while(ms--)
    {
        ch585_delay_cycles(6200u);
    }
}

void ch585_log_str(const char *text)
{
    UART1_SendString((uint8_t *)text, str_len(text));
}

void ch585_log_line(const char *text)
{
    ch585_log_str(text);
    ch585_log_str("\r\n");
}

void ch585_log_u32_dec(uint32_t value)
{
    char buf[11];
    uint8_t pos = sizeof(buf);

    if(value == 0u)
    {
        ch585_log_str("0");
        return;
    }

    while(value != 0u && pos != 0u)
    {
        pos--;
        buf[pos] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    UART1_SendString((uint8_t *)&buf[pos], (uint16_t)(sizeof(buf) - pos));
}

void ch585_log_u32_hex(uint32_t value, uint8_t digits)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[8];
    uint8_t i;

    if(digits > 8u)
    {
        digits = 8u;
    }
    for(i = 0; i < digits; ++i)
    {
        uint8_t shift = (uint8_t)((digits - 1u - i) * 4u);
        buf[i] = hex[(value >> shift) & 0x0Fu];
    }
    UART1_SendString((uint8_t *)buf, digits);
}

void ch585_log_kv_hex(const char *key, uint32_t value, uint8_t digits)
{
    ch585_log_str("DATA ");
    ch585_log_str(key);
    ch585_log_str("=0x");
    ch585_log_u32_hex(value, digits);
    ch585_log_str("\r\n");
}

void ch585_log_start(void)
{
    ch585_log_str("START half=");
    ch585_log_str(CH585_HALF_NAME);
    ch585_log_str(" test=");
    ch585_log_str(TEST_NAME);
    ch585_log_str(" serial=RX1_PA8_TX1_PA9");
    ch585_log_str("\r\n");
}

void ch585_log_pass(const char *item)
{
    ch585_log_str("PASS item=");
    ch585_log_line(item);
}

void ch585_log_fail(const char *item, const char *reason)
{
    ch585_log_str("FAIL item=");
    ch585_log_str(item);
    ch585_log_str(" reason=");
    ch585_log_line(reason);
}

void ch585_log_skip(const char *item, const char *reason)
{
    ch585_log_str("SKIP item=");
    ch585_log_str(item);
    ch585_log_str(" reason=");
    ch585_log_line(reason);
}
