#include "h417_common.h"
#include "h417_rgb1w_pioc.h"

enum
{
    H417_ITEM_WS2812 = 21
};

#define WS2812_LED_COUNT 77u
#define WS2812_PROBE_LED_COUNT 10u
#define WS2812_TEST_LEVEL 0x08u
#define WS2812_COLOR_FRAMES 128u
#define WS2812_OFF_FRAMES 32u
#define WS2812_PIOC_TIMEOUT_LOOPS 3000000u

enum
{
    WS2812_PHASE_FULL_RED = 20,
    WS2812_PHASE_FULL_GREEN = 21,
    WS2812_PHASE_FULL_BLUE = 22,
    WS2812_PHASE_FULL_OFF = 23,
    WS2812_PHASE_PIOC_ERROR = 0x200,
    WS2812_PHASE_PIOC_TIMEOUT = 0x2fe,
    WS2812_PHASE_PROBE_ERROR = 0x300
};

static uint8_t ws_frame[WS2812_LED_COUNT * 3u];
static uint8_t ws_probe_frame[WS2812_PROBE_LED_COUNT * 3u];

static void ws_fill(uint8_t *buffer, uint16_t count, uint8_t red, uint8_t green, uint8_t blue)
{
    uint16_t i;

    for(i = 0; i < count; ++i)
    {
        buffer[(3u * i) + 0u] = green;
        buffer[(3u * i) + 1u] = red;
        buffer[(3u * i) + 2u] = blue;
    }
}

static uint8_t ws_pioc_wait(void)
{
    uint32_t timeout = WS2812_PIOC_TIMEOUT_LOOPS;

    while((PIOC->D8_SYS_CFG & RB_INT_REQ) == 0u)
    {
        if(timeout == 0u)
        {
            RGB1W_Halt();
            return 0xfeu;
        }
        timeout--;
    }

    return PIOC->D8_CTRL_RD;
}

static uint8_t ws_send_ram(uint8_t *buffer, uint16_t bytes)
{
    RGB1W_SendRAM(bytes, buffer, 1u);
    return ws_pioc_wait();
}

static uint8_t ws_send_sfr(uint8_t *buffer, uint16_t bytes)
{
    RGB1W_SendSFR(bytes, buffer, 1u);
    return ws_pioc_wait();
}

static void ws_record_status(uint8_t status)
{
    if(status == RGB1W_ERR_OK)
    {
        h417_status_pass(H417_ITEM_WS2812);
    }
    else if(status == 0xfeu)
    {
        h417_status_phase(WS2812_PHASE_PIOC_TIMEOUT, H417_ITEM_WS2812);
        h417_status_fail(H417_ITEM_WS2812);
    }
    else
    {
        h417_status_phase(WS2812_PHASE_PIOC_ERROR + status, H417_ITEM_WS2812);
        h417_status_fail(H417_ITEM_WS2812);
    }
}

static void ws_probe_color(uint8_t red, uint8_t green, uint8_t blue)
{
    uint8_t status;

    ws_fill(ws_probe_frame, WS2812_PROBE_LED_COUNT, red, green, blue);
    status = ws_send_sfr(ws_probe_frame, sizeof(ws_probe_frame));

    if(status != RGB1W_ERR_OK)
    {
        h417_status_phase(WS2812_PHASE_PROBE_ERROR + status, H417_ITEM_WS2812);
        h417_status_fail(H417_ITEM_WS2812);
        RGB1W_Init();
    }
    else
    {
        h417_status_pass(H417_ITEM_WS2812);
    }
}

static void ws_repeat_color(uint8_t red, uint8_t green, uint8_t blue, uint16_t frames, uint32_t phase)
{
    uint16_t i;
    uint8_t status;

    ws_fill(ws_frame, WS2812_LED_COUNT, red, green, blue);

    for(i = 0; i < frames; ++i)
    {
        h417_status_phase(phase, H417_ITEM_WS2812);
        status = ws_send_ram(ws_frame, sizeof(ws_frame));
        ws_record_status(status);

        if(status != RGB1W_ERR_OK)
        {
            RGB1W_Init();
            ws_probe_color(red, green, blue);
        }

        g_h417_status.cycle++;
    }
}

void h417_ws2812_run(void)
{
    RGB1W_Init();
    h417_delay_cycles(100000u);

    while(1)
    {
        ws_repeat_color(WS2812_TEST_LEVEL, 0x00u, 0x00u, WS2812_COLOR_FRAMES, WS2812_PHASE_FULL_RED);
        ws_repeat_color(0x00u, WS2812_TEST_LEVEL, 0x00u, WS2812_COLOR_FRAMES, WS2812_PHASE_FULL_GREEN);
        ws_repeat_color(0x00u, 0x00u, WS2812_TEST_LEVEL, WS2812_COLOR_FRAMES, WS2812_PHASE_FULL_BLUE);
        ws_repeat_color(0x00u, 0x00u, 0x00u, WS2812_OFF_FRAMES, WS2812_PHASE_FULL_OFF);
    }
}
