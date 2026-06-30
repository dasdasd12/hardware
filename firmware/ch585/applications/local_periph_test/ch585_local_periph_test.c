#include "ch585_local_periph_test.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "CH58x_common.h"
#include "ch585_i2c_bus.h"
#include "ch585_local_inputs.h"
#include "ch585_power_status.h"
#include "ch585_soft_i2c.h"
#include "hx24lc16.h"
#include "max17048.h"

#ifndef CH585_LOCAL_PERIPH_TEST_RIGHT
#define CH585_LOCAL_PERIPH_TEST_RIGHT 1
#endif

#ifndef CH585_LOCAL_PERIPH_TEST_UART_PORT
#define CH585_LOCAL_PERIPH_TEST_UART_PORT 1
#endif

#ifndef CH585_LOCAL_PERIPH_TEST_UART0_REMAP
#define CH585_LOCAL_PERIPH_TEST_UART0_REMAP 1
#endif

#ifndef CH585_LOCAL_PERIPH_TEST_UART_BAUD
#define CH585_LOCAL_PERIPH_TEST_UART_BAUD 115200U
#endif

#ifndef CH585_LOCAL_PERIPH_TEST_PERIOD_MS
#define CH585_LOCAL_PERIPH_TEST_PERIOD_MS 250U
#endif

#define U2_SCR_COM_PIN   GPIO_Pin_7
#define U2_SCR_UP_PIN    GPIO_Pin_7
#define U2_SCR_DOWN_PIN  GPIO_Pin_6
#define U2_SCR_RIGHT_PIN GPIO_Pin_5
#define U2_SCR_LEFT_PIN  GPIO_Pin_4
#define U2_SCR_CHA_PIN   GPIO_Pin_10
#define U2_SCR_CHB_PIN   GPIO_Pin_11
#define U2_FIVEWAY_DIRECTION_MASK \
    (CH585_FIVEWAY_UP | CH585_FIVEWAY_DOWN | CH585_FIVEWAY_LEFT | CH585_FIVEWAY_RIGHT)

#define U3_ENC_BUTTON_PIN GPIO_Pin_7
#define U3_ENC_CHA_PIN    GPIO_Pin_10
#define U3_ENC_CHB_PIN    GPIO_Pin_11
#define U3_BAT_ALERT_PIN  GPIO_Pin_5
#define U3_CHARGE_PIN     GPIO_Pin_6
#define U3_STDBY_PIN      GPIO_Pin_7

#ifndef CH585_LOCAL_UNUSED
#define CH585_LOCAL_UNUSED __attribute__((unused))
#endif

static void local_uart_write(const char *text)
{
    uint16_t len;

    if (text == NULL)
    {
        return;
    }

    len = (uint16_t)strlen(text);
#if CH585_LOCAL_PERIPH_TEST_UART_PORT == 1
    UART1_SendString((uint8_t *)text, len);
#else
    UART0_SendString((uint8_t *)text, len);
#endif
}

static void local_printf(const char *fmt, ...)
{
    char buf[192];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n < 0)
    {
        return;
    }
    if ((uint32_t)n >= sizeof(buf))
    {
        buf[sizeof(buf) - 1U] = '\0';
    }
    local_uart_write(buf);
}

static void local_uart_init(void)
{
#if CH585_LOCAL_PERIPH_TEST_UART_PORT == 1
    GPIOPinRemap(DISABLE, RB_PIN_UART1);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
    UART1_BaudRateCfg(CH585_LOCAL_PERIPH_TEST_UART_BAUD);
#else
#if CH585_LOCAL_PERIPH_TEST_UART0_REMAP
    GPIOPinRemap(ENABLE, RB_PIN_UART0);
    GPIOA_ModeCfg(GPIO_Pin_15, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_14, GPIO_ModeOut_PP_5mA);
#else
    GPIOPinRemap(DISABLE, RB_PIN_UART0);
    GPIOB_ModeCfg(GPIO_Pin_4, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_7, GPIO_ModeOut_PP_5mA);
#endif
    UART0_DefInit();
    UART0_BaudRateCfg(CH585_LOCAL_PERIPH_TEST_UART_BAUD);
#endif
}

static void i2c_bus_init(ch585_i2c_bus_t *bus)
{
    ch585_soft_i2c_init();
    (void)ch585_i2c_bus_init(bus,
                             ch585_soft_i2c_probe_cb,
                             ch585_soft_i2c_write_cb,
                             ch585_soft_i2c_read_cb,
                             ch585_soft_i2c_write_read_cb,
                             ch585_soft_i2c_delay_ms_cb,
                             NULL);
}

static uint8_t gpioa_level(uint32_t pin)
{
    return GPIOA_ReadPortPin(pin) ? 1U : 0U;
}

static uint8_t gpiob_level(uint32_t pin)
{
    return GPIOB_ReadPortPin(pin) ? 1U : 0U;
}

static uint8_t read_u2_fiveway_raw(void)
{
    uint8_t raw = 0U;

    raw |= gpioa_level(U2_SCR_COM_PIN) ? CH585_FIVEWAY_COM : 0U;
    raw |= gpiob_level(U2_SCR_UP_PIN) ? CH585_FIVEWAY_UP : 0U;
    raw |= gpiob_level(U2_SCR_DOWN_PIN) ? CH585_FIVEWAY_DOWN : 0U;
    raw |= gpiob_level(U2_SCR_LEFT_PIN) ? CH585_FIVEWAY_LEFT : 0U;
    raw |= gpiob_level(U2_SCR_RIGHT_PIN) ? CH585_FIVEWAY_RIGHT : 0U;
    raw |= gpioa_level(U2_SCR_CHA_PIN) ? CH585_FIVEWAY_A : 0U;
    raw |= gpioa_level(U2_SCR_CHB_PIN) ? CH585_FIVEWAY_B : 0U;
    return raw;
}

static uint8_t read_u3_ec11_ab(void)
{
    uint8_t ab = 0U;

    ab |= gpioa_level(U3_ENC_CHA_PIN) ? 0x01U : 0U;
    ab |= gpioa_level(U3_ENC_CHB_PIN) ? 0x02U : 0U;
    return ab;
}

static uint8_t read_u3_ec11_button_pressed(void)
{
    return gpioa_level(U3_ENC_BUTTON_PIN) ? 0U : 1U;
}

static void print_event(const char *prefix,
                        const ch585_local_input_event_t *event)
{
    if (event == NULL)
    {
        return;
    }

    local_printf("%s event type=%u id=%u value=%d flags=0x%02x\r\n",
                 prefix,
                 (unsigned int)event->type,
                 (unsigned int)event->id,
                 (int)event->value,
                 (unsigned int)event->flags);
}

static void CH585_LOCAL_UNUSED run_u2_left_test(ch585_i2c_bus_t *bus)
{
    hx24lc16_config_t eeprom_cfg;
    hx24lc16_t eeprom;
    uint8_t eeprom_data[16];
    int eeprom_probe_rc;
    ch585_fiveway_t fiveway;
    uint32_t now_ms = 0U;

    GPIOA_ModeCfg(U2_SCR_COM_PIN | U2_SCR_CHA_PIN | U2_SCR_CHB_PIN,
                  GPIO_ModeIN_PU);
    GPIOB_ModeCfg(U2_SCR_UP_PIN | U2_SCR_DOWN_PIN |
                      U2_SCR_RIGHT_PIN | U2_SCR_LEFT_PIN,
                  GPIO_ModeIN_PU);

    hx24lc16_default_config(&eeprom_cfg);
    eeprom_cfg.bus = bus;
    (void)hx24lc16_init(&eeprom, &eeprom_cfg);
    eeprom_probe_rc = hx24lc16_probe(&eeprom);

    local_printf("U2 EEPROM HX24LC16 probe addr=0x50 rc=%d\r\n",
                 eeprom_probe_rc);
    if (eeprom_probe_rc == HX24LC16_STATUS_OK)
    {
        int rc = hx24lc16_read(&eeprom, 0x0000U, eeprom_data,
                               (uint16_t)sizeof(eeprom_data));
        local_printf("U2 EEPROM read 0x0000 len=16 rc=%d data=", rc);
        if (rc == HX24LC16_STATUS_OK)
        {
            uint8_t i;
            for (i = 0U; i < sizeof(eeprom_data); i++)
            {
                local_printf("%02x", (unsigned int)eeprom_data[i]);
            }
        }
        local_printf("\r\n");
    }

    ch585_fiveway_init(&fiveway,
                       read_u2_fiveway_raw(),
                       U2_FIVEWAY_DIRECTION_MASK,
                       U2_FIVEWAY_DIRECTION_MASK,
                       12U,
                       now_ms);

    local_printf("U2 input pins: COM=PA7 UP=PB7 DOWN=PB6 RIGHT=PB5 LEFT=PB4 CHA=PA10 CHB=PA11\r\n");
    local_printf("U2 fiveway event mask: UP/DOWN/LEFT/RIGHT only; COM/CHA/CHB are raw-level debug\r\n");
    while (1)
    {
        ch585_local_input_event_t events[CH585_LOCAL_INPUT_EVENT_CAP];
        int event_count;
        int i;
        uint8_t raw = read_u2_fiveway_raw();

        event_count = ch585_fiveway_update(&fiveway, raw, now_ms, events,
                                           CH585_LOCAL_INPUT_EVENT_CAP);
        for (i = 0; i < event_count; i++)
        {
            print_event("U2 FIVEWAY", &events[i]);
        }

        if ((now_ms % CH585_LOCAL_PERIPH_TEST_PERIOD_MS) == 0U)
        {
            local_printf("U2 ST raw=0x%02x active=0x%02x levels COM=%u UP=%u DOWN=%u LEFT=%u RIGHT=%u A=%u B=%u\r\n",
                         (unsigned int)raw,
                         (unsigned int)fiveway.stable_mask,
                         (unsigned int)gpioa_level(U2_SCR_COM_PIN),
                         (unsigned int)gpiob_level(U2_SCR_UP_PIN),
                         (unsigned int)gpiob_level(U2_SCR_DOWN_PIN),
                         (unsigned int)gpiob_level(U2_SCR_LEFT_PIN),
                         (unsigned int)gpiob_level(U2_SCR_RIGHT_PIN),
                         (unsigned int)gpioa_level(U2_SCR_CHA_PIN),
                         (unsigned int)gpioa_level(U2_SCR_CHB_PIN));
        }

        mDelaymS(2U);
        now_ms += 2U;
    }
}

static void CH585_LOCAL_UNUSED run_u3_right_test(ch585_i2c_bus_t *bus)
{
    max17048_config_t gauge_cfg;
    max17048_t gauge;
    int gauge_probe_rc;
    ch585_ec11_t enc;
    uint32_t now_ms = 0U;
    uint8_t battery_present = 0U;

    GPIOA_ModeCfg(U3_ENC_BUTTON_PIN | U3_ENC_CHA_PIN | U3_ENC_CHB_PIN,
                  GPIO_ModeIN_PU);
    GPIOB_ModeCfg(U3_BAT_ALERT_PIN | U3_CHARGE_PIN | U3_STDBY_PIN,
                  GPIO_ModeIN_PU);

    max17048_default_config(&gauge_cfg);
    gauge_cfg.bus = bus;
    (void)max17048_init(&gauge, &gauge_cfg);
    gauge_probe_rc = max17048_probe(&gauge);
    battery_present = (gauge_probe_rc == MAX17048_STATUS_OK) ? 1U : 0U;

    local_printf("U3 MAX17048 probe addr=0x36 rc=%d\r\n", gauge_probe_rc);
    local_printf("U3 pins: ENC_BUTTON=PA7 ENC_CHA=PA10 ENC_CHB=PA11 BAT_ALERT=PB5 CHARGE=PB6 STDBY=PB7\r\n");

    ch585_ec11_init(&enc,
                    read_u3_ec11_ab(),
                    read_u3_ec11_button_pressed(),
                    12U,
                    now_ms);

    while (1)
    {
        ch585_local_input_event_t events[CH585_LOCAL_INPUT_EVENT_CAP];
        int event_count;
        int i;
        uint8_t ab = read_u3_ec11_ab();
        uint8_t button_pressed = read_u3_ec11_button_pressed();
        uint8_t alert_low = gpiob_level(U3_BAT_ALERT_PIN) ? 0U : 1U;
        uint8_t charge_level = gpiob_level(U3_CHARGE_PIN);
        uint8_t standby_level = gpiob_level(U3_STDBY_PIN);

        event_count = ch585_ec11_update(&enc, ab, button_pressed, now_ms,
                                        events, CH585_LOCAL_INPUT_EVENT_CAP);
        for (i = 0; i < event_count; i++)
        {
            print_event("U3 EC11", &events[i]);
        }

        if ((now_ms % CH585_LOCAL_PERIPH_TEST_PERIOD_MS) == 0U)
        {
            max17048_sample_t sample;
            max17048_sample_t *sample_ptr = NULL;
            ch585_power_status_config_t power_cfg;
            ch585_power_status_t power;
            int read_rc = MAX17048_STATUS_IO;

            if (battery_present != 0U)
            {
                read_rc = max17048_read_sample(&gauge, alert_low, &sample);
                if (read_rc == MAX17048_STATUS_OK)
                {
                    sample_ptr = &sample;
                }
            }

            ch585_power_status_default_config(&power_cfg);
            ch585_power_status_from_sample(&power_cfg,
                                           sample_ptr,
                                           charge_level,
                                           standby_level,
                                           &power);

            local_printf("U3 ST ab=0x%u btn=%u alert=%u chg=%u stdby=%u gauge_probe=%d gauge_read=%d vbat=%umV soc_q8=%u soc=%u.%02u%% charge_state=%u flags=0x%02x status=0x%04x\r\n",
                         (unsigned int)ab,
                         (unsigned int)button_pressed,
                         (unsigned int)alert_low,
                         (unsigned int)charge_level,
                         (unsigned int)standby_level,
                         gauge_probe_rc,
                         read_rc,
                         (unsigned int)power.vbat_mv,
                         (unsigned int)power.soc_q8_percent,
                         (unsigned int)(power.soc_q8_percent / 256U),
                         (unsigned int)(((power.soc_q8_percent & 0xFFU) * 100U) / 256U),
                         (unsigned int)power.charge_state,
                         (unsigned int)power.flags,
                         (unsigned int)power.max17048_status);
        }

        mDelaymS(2U);
        now_ms += 2U;
    }
}

void ch585_local_periph_test_run(void)
{
    ch585_i2c_bus_t bus;

    local_uart_init();
    local_printf("\r\nCH585 local peripheral test start\r\n");
    local_printf("side=%s uart=UART%u baud=%u i2c=PB20/PB21 period=%ums\r\n",
#if CH585_LOCAL_PERIPH_TEST_RIGHT
                 "right/U3",
#else
                 "left/U2",
#endif
                 (unsigned int)CH585_LOCAL_PERIPH_TEST_UART_PORT,
                 (unsigned int)CH585_LOCAL_PERIPH_TEST_UART_BAUD,
                 (unsigned int)CH585_LOCAL_PERIPH_TEST_PERIOD_MS);

    i2c_bus_init(&bus);

#if CH585_LOCAL_PERIPH_TEST_RIGHT
    run_u3_right_test(&bus);
#else
    run_u2_left_test(&bus);
#endif
}
