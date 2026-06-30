#include "v5f_hw_test.h"

#include <rtthread.h>
#include <string.h>

#include "ch32h417_fmc.h"
#include "ch32h417_gpha_2d.h"
#include "ch32h417_ltdc_rgb.h"
#include "ch32h417_pwr.h"

#if (APP_V5F_HW_TEST == APP_V5F_HW_TEST_CH585_SPI_SPEED) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_CH585_ADC_KEY_CAL)
#include "ch32h417_ch585_spi_link.h"
#include "ch32h417_gpio.h"
#include "ch32h417_rcc.h"
#include "ch32h417_spi.h"
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_CH585_SPI_SPEED
#include "ch585_h417_spi_speed_proto.h"
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_CH585_ADC_KEY_CAL
#include "ch585_h417_adc_key_cal_proto.h"
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC
#include "v5f_ltdc_gray_image.h"
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_L8_PALETTE_IMAGE
#include "v5f_ltdc_palette_image.h"
#endif

#if (APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH_L8_ASSETS)
#include "ch32h417_gd5f1g_spi1.h"
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH_L8_ASSETS
#include "gd5f1g_l8_asset_store.h"
#include "v5f_ltdc_flash_assets.h"
#endif

#ifndef APP_V5F_HW_TEST
#define APP_V5F_HW_TEST APP_V5F_HW_TEST_NONE
#endif

#ifndef APP_V5F_HW_TEST_NAME
#define APP_V5F_HW_TEST_NAME "unknown"
#endif

#ifndef APP_ENABLE_USB_TEST
#define APP_ENABLE_USB_TEST 0
#endif

#if APP_ENABLE_USB_TEST && \
    ((APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_DQ_PROBE) || \
     (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_OFFICIAL_16BIT))
#define V5F_SDRAM_USB_DEBUG_ENABLED 1
#else
#define V5F_SDRAM_USB_DEBUG_ENABLED 0
#endif

#define V5F_MAYBE_UNUSED       __attribute__((unused))

#if (APP_V5F_HW_TEST != APP_V5F_HW_TEST_SDRAM_MEMTEST) && \
    (APP_V5F_HW_TEST != APP_V5F_HW_TEST_SDRAM_LTDC_RGB565) && \
    (APP_V5F_HW_TEST != APP_V5F_HW_TEST_SDRAM_REMAP_PROBE) && \
    (APP_V5F_HW_TEST != APP_V5F_HW_TEST_SDRAM_DQ_PROBE) && \
    (APP_V5F_HW_TEST != APP_V5F_HW_TEST_SDRAM_OFFICIAL_16BIT)
static const char *v5f_hw_test_runtime_name(void)
{
    switch(APP_V5F_HW_TEST)
    {
        case APP_V5F_HW_TEST_SDRAM_MEMTEST:
            return "sdram_memtest";
        case APP_V5F_HW_TEST_SDRAM_LTDC_RGB565:
            return "sdram_ltdc_rgb565";
        case APP_V5F_HW_TEST_SDRAM_REMAP_PROBE:
            return "sdram_remap_probe";
        case APP_V5F_HW_TEST_SDRAM_DQ_PROBE:
            return "sdram_dq_probe";
        case APP_V5F_HW_TEST_SDRAM_OFFICIAL_16BIT:
            return "sdram_official_16bit";
        default:
            return APP_V5F_HW_TEST_NAME;
    }
}
#endif

extern uint32_t HCLKClock;
extern uint32_t SystemClock;
extern uint32_t SystemCoreClock;

#if APP_ENABLE_USB_TEST
extern int ch32h417_dual_cdc_init(void);
extern void ch32h417_dual_cdc_poll(void);
extern int ch32h417_usb_cdc_write(const void *data, rt_uint32_t len);
extern int ch32h417_usb_cdc_read_line(char *out, rt_uint32_t out_len);
#endif

#define V5F_L8_FB_WIDTH        800u
#define V5F_L8_FB_HEIGHT       480u
#define V5F_L8_FB_BYTES        (V5F_L8_FB_WIDTH * V5F_L8_FB_HEIGHT)
#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_PFC_L8_RGB565
#define V5F_RGB_FB_WIDTH       800u
#define V5F_RGB_FB_HEIGHT      160u
#else
#define V5F_RGB_FB_WIDTH       320u
#define V5F_RGB_FB_HEIGHT      160u
#endif
#define V5F_RGB_FB_PIXELS      (V5F_RGB_FB_WIDTH * V5F_RGB_FB_HEIGHT)
#define V5F_RGB_FB_BYTES       (V5F_RGB_FB_PIXELS * 2u)
#define V5F_GPHA_L8_SRC_BYTES  V5F_RGB_FB_PIXELS
#define V5F_GPHA_L8_SRC_OFFSET V5F_RGB_FB_BYTES
#define V5F_GPHA_L8_CLUT_ENTRIES 256u
#define V5F_GPHA_L8_CLUT_BYTES   (V5F_GPHA_L8_CLUT_ENTRIES * 4u)
#define V5F_GPHA_L8_CLUT_OFFSET  (V5F_GPHA_L8_SRC_OFFSET + V5F_GPHA_L8_SRC_BYTES)
#define V5F_GPHA_BLEND_BG_BYTES  V5F_RGB_FB_BYTES
#define V5F_GPHA_BLEND_BG_OFFSET V5F_RGB_FB_BYTES
#define V5F_GPHA_BLEND_FG_BYTES  (V5F_RGB_FB_PIXELS * 2u)
#define V5F_GPHA_BLEND_FG_OFFSET (V5F_GPHA_BLEND_BG_OFFSET + V5F_GPHA_BLEND_BG_BYTES)
#define V5F_LCD_FB_REGION_SIZE (384u * 1024u)

#if V5F_L8_FB_BYTES > V5F_LCD_FB_REGION_SIZE
#error V5F L8 framebuffer exceeds reserved LCD_FB memory.
#endif

#if V5F_RGB_FB_BYTES > V5F_LCD_FB_REGION_SIZE
#error V5F RGB565 framebuffer exceeds reserved LCD_FB memory.
#endif

#if (V5F_GPHA_L8_SRC_OFFSET + V5F_GPHA_L8_SRC_BYTES) > V5F_LCD_FB_REGION_SIZE
#error V5F GPHA L8 source buffer exceeds reserved LCD_FB memory.
#endif

#if (V5F_GPHA_L8_CLUT_OFFSET + V5F_GPHA_L8_CLUT_BYTES) > V5F_LCD_FB_REGION_SIZE
#error V5F GPHA L8 CLUT buffer exceeds reserved LCD_FB memory.
#endif

#if (APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_BLEND_RGB565) && \
    ((V5F_GPHA_BLEND_FG_OFFSET + V5F_GPHA_BLEND_FG_BYTES) > V5F_LCD_FB_REGION_SIZE)
#error V5F GPHA blend source buffers exceed reserved LCD_FB memory.
#endif

typedef enum
{
    V5F_HW_PHASE_BOOT = 0,
    V5F_HW_PHASE_LCD_READY = 1,
    V5F_HW_PHASE_RUNNING = 2,
    V5F_HW_PHASE_FAILED = 3,
} v5f_hw_phase_t;

static struct rt_thread s_test_thread;
static rt_uint8_t s_test_thread_stack[4096] __attribute__((aligned(8)));
static uint8_t s_lcd_fb[V5F_LCD_FB_REGION_SIZE] __attribute__((section(".lcd_fb"), aligned(64)));
#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_L8_LTDC_FULLSCREEN
static uint8_t s_gpha_l8_ltdc_clut_rgb888[CH32H417_LTDC_RGB_CLUT_ENTRIES * 3u];
#endif
#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH_L8_ASSETS
static uint8_t s_flash_page[GD5F1G_PAGE_SIZE] __attribute__((aligned(4)));
static gd5f1g_l8_asset_manifest_t s_flash_manifest;
#endif

volatile v5f_hw_test_diag_t g_v5f_hw_test_diag;

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_CH585_SPI_SPEED
#if !APP_ENABLE_USB_TEST
#error "ch585_spi_speed hw_test requires APP_ENABLE_USB_TEST for CDC debug logs"
#endif

#if !defined(APP_CH585_SPI_SPEED_SOURCE_LEFT) && !defined(APP_CH585_SPI_SPEED_SOURCE_RIGHT)
#define APP_CH585_SPI_SPEED_SOURCE_LEFT 1
#endif

#if defined(APP_CH585_SPI_SPEED_SOURCE_LEFT) && defined(APP_CH585_SPI_SPEED_SOURCE_RIGHT)
#error "Select only one CH585 SPI speed source"
#endif

#if defined(APP_CH585_SPI_SPEED_SOURCE_RIGHT)
#define CH585_SPI_SPEED_SOURCE_DESC "right/U3 CS=PD9 other=PF2"
#define CH585_SPI_SPEED_CS_PORT GPIOD
#define CH585_SPI_SPEED_CS_PIN GPIO_Pin_9
#define CH585_SPI_SPEED_OTHER_CS_PORT GPIOF
#define CH585_SPI_SPEED_OTHER_CS_PIN GPIO_Pin_2
#define CH585_SPI_SPEED_LINK_SIDE CH32H417_CH585_SPI_LINK_SIDE_RIGHT
#else
#define CH585_SPI_SPEED_SOURCE_DESC "left/U2 CS=PF2 other=PD9"
#define CH585_SPI_SPEED_CS_PORT GPIOF
#define CH585_SPI_SPEED_CS_PIN GPIO_Pin_2
#define CH585_SPI_SPEED_OTHER_CS_PORT GPIOD
#define CH585_SPI_SPEED_OTHER_CS_PIN GPIO_Pin_9
#define CH585_SPI_SPEED_LINK_SIDE CH32H417_CH585_SPI_LINK_SIDE_LEFT
#endif

#ifndef CH585_SPI_SPEED_FRAMES_PER_RATE
#define CH585_SPI_SPEED_FRAMES_PER_RATE 512U
#endif

#ifndef CH585_SPI_SPEED_BYTE_TIMEOUT_POLLS
#define CH585_SPI_SPEED_BYTE_TIMEOUT_POLLS 500000U
#endif

#ifndef CH585_SPI_SPEED_CS_SETUP_CYCLES
#define CH585_SPI_SPEED_CS_SETUP_CYCLES 256U
#endif

#ifndef CH585_SPI_SPEED_CS_GAP_CYCLES
#define CH585_SPI_SPEED_CS_GAP_CYCLES 1024U
#endif

#ifndef CH585_SPI_SPEED_MAX_ATTEMPTS_PER_RATE
#define CH585_SPI_SPEED_MAX_ATTEMPTS_PER_RATE (CH585_SPI_SPEED_FRAMES_PER_RATE * 8U)
#endif

#ifndef CH585_SPI_SPEED_SYNC_RETRY_CYCLES
#define CH585_SPI_SPEED_SYNC_RETRY_CYCLES 48000U
#endif

#define CH585_SPI_SPEED_LINE_BYTES 512U
#define CH585_SPI_SPEED_CMD_BYTES 64U
#define CH585_SPI_SPEED_DIV2_DIAG_SAMPLES 4U
#define CH585_SPI_SPEED_DIV2_DIAG_BYTES 8U

typedef enum
{
    CH585_SPI_SPEED_CMD_AUTO = 0,
    CH585_SPI_SPEED_CMD_RATE = 1,
    CH585_SPI_SPEED_CMD_STOP = 2,
    CH585_SPI_SPEED_CMD_HF = 3,
} ch585_spi_speed_cmd_mode_t;

typedef struct
{
    uint16_t prescaler;
    uint16_t cpha;
    uint16_t div;
    uint8_t hsrx;
    const char *name;
} ch585_spi_speed_rate_t;

typedef struct
{
    uint32_t ok;
    uint32_t bad_ready;
    uint32_t timeout;
    uint32_t bad_fixed;
    uint8_t first_bad[4];
    uint8_t first_expected[4];
    uint16_t first_bad_off;
    uint8_t div2_samples[CH585_SPI_SPEED_DIV2_DIAG_SAMPLES][CH585_SPI_SPEED_DIV2_DIAG_BYTES];
    uint8_t div2_sample_count;
    uint8_t div2_rx0_and;
    uint8_t div2_rx0_or;
    uint8_t div2_rx1_and;
    uint8_t div2_rx1_or;
    uint8_t have_first_bad;
} ch585_spi_speed_stats_t;

typedef struct
{
    ch585_spi_speed_cmd_mode_t mode;
    const ch585_spi_speed_rate_t *rate;
    uint8_t once_pending;
    uint8_t once_hf_pending;
} ch585_spi_speed_cmd_state_t;

static uint8_t s_ch585_spi_speed_tx[CH585_H417_SPI_SPEED_TRANSFER_BYTES] __attribute__((aligned(4)));
static uint8_t s_ch585_spi_speed_rx[CH585_H417_SPI_SPEED_TRANSFER_BYTES] __attribute__((aligned(4)));
static ch32h417_ch585_spi_link_config_t s_ch585_spi_speed_stable_link;

static const ch585_spi_speed_rate_t s_ch585_spi_speed_rates[] =
{
    { SPI_BaudRatePrescaler_Mode2, SPI_CPHA_1Edge, 8U, 0U, "div8" },
    { SPI_BaudRatePrescaler_Mode2, SPI_CPHA_1Edge, 8U, 1U, "div8-hsrx1" },
    { SPI_BaudRatePrescaler_Mode2, SPI_CPHA_2Edge, 8U, 1U, "div8-hsrx1-cpha2" },
    { SPI_BaudRatePrescaler_Mode2, SPI_CPHA_1Edge, 8U, 2U, "div8-hsrx2" },
    { SPI_BaudRatePrescaler_Mode1, SPI_CPHA_1Edge, 4U, 0U, "div4" },
    { SPI_BaudRatePrescaler_Mode1, SPI_CPHA_2Edge, 4U, 0U, "div4-cpha2" },
    { SPI_BaudRatePrescaler_Mode1, SPI_CPHA_1Edge, 4U, 1U, "div4-hsrx1" },
    { SPI_BaudRatePrescaler_Mode1, SPI_CPHA_2Edge, 4U, 1U, "div4-hsrx1-cpha2" },
    { SPI_BaudRatePrescaler_Mode1, SPI_CPHA_1Edge, 4U, 2U, "div4-hsrx2" },
    { SPI_BaudRatePrescaler_Mode0, SPI_CPHA_1Edge, 2U, 0U, "div2" },
    { SPI_BaudRatePrescaler_Mode0, SPI_CPHA_2Edge, 2U, 0U, "div2-cpha2" },
    { SPI_BaudRatePrescaler_Mode0, SPI_CPHA_1Edge, 2U, 1U, "div2-hsrx1" },
    { SPI_BaudRatePrescaler_Mode0, SPI_CPHA_1Edge, 2U, 2U, "div2-hsrx2" },
    { SPI_BaudRatePrescaler_Mode0, SPI_CPHA_2Edge, 2U, 2U, "div2-hsrx2-cpha2" },
};

static uint32_t ch585_spi_speed_rate_count(void)
{
    return (uint32_t)(sizeof(s_ch585_spi_speed_rates) /
                      sizeof(s_ch585_spi_speed_rates[0]));
}

static uint32_t ch585_spi_speed_mcycle(void)
{
    uint32_t value;
    __asm volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
}

static void ch585_spi_speed_delay_cycles(uint32_t cycles)
{
    while(cycles-- != 0U)
    {
        __asm volatile("nop");
    }
}

static int ch585_spi_speed_cdc_write_full(const char *data, rt_size_t len)
{
    rt_size_t offset = 0u;
    uint8_t retries = 0u;

    if(data == RT_NULL)
    {
        return -1;
    }

    while(offset < len)
    {
        int wrote = ch32h417_usb_cdc_write(&data[offset], (rt_uint32_t)(len - offset));

        if(wrote > 0)
        {
            offset += (rt_size_t)wrote;
            retries = 0u;
            continue;
        }

        if((wrote == -2) || (retries >= 16u))
        {
            return (offset > 0u) ? (int)offset : wrote;
        }

        retries++;
        rt_thread_mdelay(1);
    }

    return (int)offset;
}

static void ch585_spi_speed_log_line(const char *line)
{
    if(line == RT_NULL)
    {
        return;
    }

    rt_kprintf("%s\n", line);
    (void)ch585_spi_speed_cdc_write_full(line, (rt_size_t)strlen(line));
    (void)ch585_spi_speed_cdc_write_full("\r\n", 2u);
}

static uint8_t ch585_spi_speed_is_stable_12m5(const ch585_spi_speed_rate_t *rate)
{
    if(rate == RT_NULL)
    {
        return 0U;
    }

    if((rate->div == 8U) &&
       (rate->hsrx == 0U) &&
       (rate->cpha == SPI_CPHA_1Edge))
    {
        return 1U;
    }

    return 0U;
}

static uint32_t ch585_spi_speed_rate_khz(const ch585_spi_speed_rate_t *rate)
{
    uint32_t hclk = (HCLKClock != 0U) ? HCLKClock : SystemCoreClock;

    if(ch585_spi_speed_is_stable_12m5(rate) != 0U)
    {
        return CH32H417_CH585_SPI_LINK_SPI_KHZ;
    }

    if(rate->div == 0U)
    {
        return 0U;
    }

    return (hclk / (uint32_t)rate->div) / 1000U;
}

static uint8_t ch585_spi_speed_is_div2(const ch585_spi_speed_rate_t *rate)
{
    return (rate->div == 2U) ? 1U : 0U;
}

static uint8_t ch585_spi_speed_is_high_frequency(const ch585_spi_speed_rate_t *rate)
{
    if(rate == RT_NULL)
    {
        return 0U;
    }

    if((rate->div == 4U) || (rate->div == 2U))
    {
        return 1U;
    }

    return 0U;
}

static char *ch585_spi_speed_trim_command(char *line)
{
    char *end;

    if(line == RT_NULL)
    {
        return RT_NULL;
    }

    while((*line == ' ') || (*line == '\t') ||
          (*line == '\r') || (*line == '\n'))
    {
        line++;
    }

    end = line + strlen(line);
    while(end > line)
    {
        char ch = *(end - 1);
        if((ch != ' ') && (ch != '\t') &&
           (ch != '\r') && (ch != '\n'))
        {
            break;
        }
        end--;
        *end = '\0';
    }

    return line;
}

static const ch585_spi_speed_rate_t *ch585_spi_speed_find_rate(const char *name)
{
    uint32_t i;

    if(name == RT_NULL)
    {
        return RT_NULL;
    }

    while((*name == ' ') || (*name == '\t') ||
          (*name == '\r') || (*name == '\n'))
    {
        name++;
    }

    for(i = 0U; i < ch585_spi_speed_rate_count(); i++)
    {
        if(strcmp(name, s_ch585_spi_speed_rates[i].name) == 0)
        {
            return &s_ch585_spi_speed_rates[i];
        }
    }

    return RT_NULL;
}

static void ch585_spi_speed_log_help(void)
{
    ch585_spi_speed_log_line("SPI_CMD help commands: auto | hf | oncehf | stop | rate <name> | once <name> | go <name> | help");
    ch585_spi_speed_log_line("SPI_CMD rates: div8 div8-hsrx1 div8-hsrx1-cpha2 div8-hsrx2 div4 div4-cpha2 div4-hsrx1 div4-hsrx1-cpha2 div4-hsrx2 div2 div2-cpha2 div2-hsrx1 div2-hsrx2 div2-hsrx2-cpha2");
}

static void ch585_spi_speed_log_command(const char *status,
                                        const ch585_spi_speed_cmd_state_t *cmd)
{
    char line[CH585_SPI_SPEED_LINE_BYTES];
    const char *mode = "auto";
    const char *rate = "none";

    if(cmd != RT_NULL)
    {
        if(cmd->mode == CH585_SPI_SPEED_CMD_RATE)
        {
            mode = "rate";
        }
        else if(cmd->mode == CH585_SPI_SPEED_CMD_STOP)
        {
            mode = "stop";
        }
        else if(cmd->mode == CH585_SPI_SPEED_CMD_HF)
        {
            mode = "hf";
        }

        if(cmd->rate != RT_NULL)
        {
            rate = cmd->rate->name;
        }
    }

    (void)rt_snprintf(line,
                      sizeof(line),
                      "SPI_CMD %s mode=%s rate=%s once=%u oncehf=%u",
                      status,
                      mode,
                      rate,
                      (cmd != RT_NULL) ? (unsigned int)cmd->once_pending : 0U,
                      (cmd != RT_NULL) ? (unsigned int)cmd->once_hf_pending : 0U);
    ch585_spi_speed_log_line(line);
}

static void ch585_spi_speed_handle_command(ch585_spi_speed_cmd_state_t *cmd,
                                           char *line)
{
    const ch585_spi_speed_rate_t *rate;

    if((cmd == RT_NULL) || (line == RT_NULL))
    {
        return;
    }

    line = ch585_spi_speed_trim_command(line);

    if((strcmp(line, "help") == 0) || (strcmp(line, "?") == 0))
    {
        ch585_spi_speed_log_help();
        return;
    }

    if(strcmp(line, "auto") == 0)
    {
        cmd->mode = CH585_SPI_SPEED_CMD_AUTO;
        cmd->once_pending = 0U;
        cmd->once_hf_pending = 0U;
        ch585_spi_speed_log_command("ok", cmd);
        return;
    }

    if(strcmp(line, "hf") == 0)
    {
        cmd->mode = CH585_SPI_SPEED_CMD_HF;
        cmd->once_pending = 0U;
        cmd->once_hf_pending = 0U;
        ch585_spi_speed_log_command("ok", cmd);
        return;
    }

    if(strcmp(line, "oncehf") == 0)
    {
        cmd->mode = CH585_SPI_SPEED_CMD_STOP;
        cmd->once_pending = 0U;
        cmd->once_hf_pending = 1U;
        ch585_spi_speed_log_command("ok", cmd);
        return;
    }

    if(strcmp(line, "stop") == 0)
    {
        cmd->mode = CH585_SPI_SPEED_CMD_STOP;
        cmd->once_pending = 0U;
        cmd->once_hf_pending = 0U;
        ch585_spi_speed_log_command("ok", cmd);
        return;
    }

    if(strncmp(line, "rate ", 5U) == 0)
    {
        rate = ch585_spi_speed_find_rate(&line[5]);
        if(rate == RT_NULL)
        {
            ch585_spi_speed_log_line("SPI_CMD err unknown rate");
            ch585_spi_speed_log_help();
            return;
        }

        cmd->rate = rate;
        cmd->mode = CH585_SPI_SPEED_CMD_RATE;
        cmd->once_pending = 0U;
        cmd->once_hf_pending = 0U;
        ch585_spi_speed_log_command("ok", cmd);
        return;
    }

    if((strncmp(line, "once ", 5U) == 0) || (strncmp(line, "go ", 3U) == 0))
    {
        const char *name = (line[0] == 'g') ? &line[3] : &line[5];
        rate = ch585_spi_speed_find_rate(name);
        if(rate == RT_NULL)
        {
            ch585_spi_speed_log_line("SPI_CMD err unknown rate");
            ch585_spi_speed_log_help();
            return;
        }

        cmd->rate = rate;
        cmd->mode = CH585_SPI_SPEED_CMD_STOP;
        cmd->once_pending = 1U;
        cmd->once_hf_pending = 0U;
        ch585_spi_speed_log_command("ok", cmd);
        return;
    }

    ch585_spi_speed_log_line("SPI_CMD err unknown command");
    ch585_spi_speed_log_help();
}

static void ch585_spi_speed_poll_command(ch585_spi_speed_cmd_state_t *cmd)
{
    char line[CH585_SPI_SPEED_CMD_BYTES];
    int len;

    ch32h417_dual_cdc_poll();
    do
    {
        len = ch32h417_usb_cdc_read_line(line, sizeof(line));
        if(len > 0)
        {
            ch585_spi_speed_handle_command(cmd, line);
        }
    } while(len > 0);
}

static void ch585_spi_speed_poll_delay(ch585_spi_speed_cmd_state_t *cmd,
                                       uint32_t delay_ms)
{
    while(delay_ms >= 10U)
    {
        ch585_spi_speed_poll_command(cmd);
        rt_thread_mdelay(10);
        delay_ms -= 10U;
    }

    if(delay_ms != 0U)
    {
        ch585_spi_speed_poll_command(cmd);
        rt_thread_mdelay(delay_ms);
    }
}

static void ch585_spi_speed_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                          RCC_HB2Periph_GPIOB |
                          RCC_HB2Periph_GPIOD |
                          RCC_HB2Periph_GPIOF |
                          RCC_HB2Periph_SPI1, ENABLE);

    GPIO_PinRemapConfig(GPIO_Remap_VIO3V3_IO_HSLV, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_VDD3V3_IO_HSLV, ENABLE);

    GPIO_SetBits(CH585_SPI_SPEED_CS_PORT, CH585_SPI_SPEED_CS_PIN);
    GPIO_SetBits(CH585_SPI_SPEED_OTHER_CS_PORT, CH585_SPI_SPEED_OTHER_CS_PIN);
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Pin = CH585_SPI_SPEED_CS_PIN;
    GPIO_Init(CH585_SPI_SPEED_CS_PORT, &gpio);
    gpio.GPIO_Pin = CH585_SPI_SPEED_OTHER_CS_PIN;
    GPIO_Init(CH585_SPI_SPEED_OTHER_CS_PORT, &gpio);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource3, GPIO_AF5);
    gpio.GPIO_Pin = GPIO_Pin_3;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &gpio);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF5);
    gpio.GPIO_Pin = GPIO_Pin_5;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &gpio);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource4, GPIO_AF5);
    gpio.GPIO_Pin = GPIO_Pin_4;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &gpio);
}

static void ch585_spi_speed_spi_apply(const ch585_spi_speed_rate_t *rate)
{
    SPI_InitTypeDef spi = {0};

    SPI_Cmd(SPI1, DISABLE);
    SPI_I2S_DeInit(SPI1);

    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_Low;
    spi.SPI_CPHA = rate->cpha;
    spi.SPI_NSS = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = rate->prescaler;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7U;
    SPI_Init(SPI1, &spi);
    SPI_NSSInternalSoftwareConfig(SPI1, SPI_NSSInternalSoft_Set);

    SPI_HighSpeedMode_Config(SPI1, SPI_HIGH_SPEED_MODE1, DISABLE);
    SPI_HighSpeedMode_Config(SPI1, SPI_HIGH_SPEED_MODE2, DISABLE);
    if(rate->hsrx == 1U)
    {
        SPI_HighSpeedMode_Config(SPI1, SPI_HIGH_SPEED_MODE1, ENABLE);
    }
    else if(rate->hsrx == 2U)
    {
        SPI_HighSpeedMode_Config(SPI1, SPI_HIGH_SPEED_MODE2, ENABLE);
    }

    SPI_Cmd(SPI1, ENABLE);
}

static void ch585_spi_speed_spi_drain_rx(void)
{
    uint8_t guard = 16U;

    while((guard-- != 0U) &&
          (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) != RESET))
    {
        (void)SPI_I2S_ReceiveData(SPI1);
    }
    (void)SPI1->STATR;
}

static int ch585_spi_speed_wait_flag(uint16_t flag)
{
    uint32_t polls = CH585_SPI_SPEED_BYTE_TIMEOUT_POLLS;

    while(SPI_I2S_GetFlagStatus(SPI1, flag) == RESET)
    {
        if(polls-- == 0U)
        {
            return -1;
        }
    }

    return 0;
}

static int ch585_spi_speed_transfer_bytes(uint8_t *rx,
                                          const uint8_t *tx,
                                          uint16_t len)
{
    uint16_t i;
    uint32_t polls;

    ch585_spi_speed_spi_drain_rx();
    GPIO_ResetBits(CH585_SPI_SPEED_CS_PORT, CH585_SPI_SPEED_CS_PIN);
    ch585_spi_speed_delay_cycles(CH585_SPI_SPEED_CS_SETUP_CYCLES);

    for(i = 0U; i < len; i++)
    {
        if(ch585_spi_speed_wait_flag(SPI_I2S_FLAG_TXE) != 0)
        {
            GPIO_SetBits(CH585_SPI_SPEED_CS_PORT, CH585_SPI_SPEED_CS_PIN);
            return -1;
        }

        SPI_I2S_SendData(SPI1, tx[i]);

        if(ch585_spi_speed_wait_flag(SPI_I2S_FLAG_RXNE) != 0)
        {
            GPIO_SetBits(CH585_SPI_SPEED_CS_PORT, CH585_SPI_SPEED_CS_PIN);
            return -2;
        }

        rx[i] = (uint8_t)SPI_I2S_ReceiveData(SPI1);
    }

    polls = CH585_SPI_SPEED_BYTE_TIMEOUT_POLLS;
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) != RESET)
    {
        if(polls-- == 0U)
        {
            GPIO_SetBits(CH585_SPI_SPEED_CS_PORT, CH585_SPI_SPEED_CS_PIN);
            return -3;
        }
    }

    GPIO_SetBits(CH585_SPI_SPEED_CS_PORT, CH585_SPI_SPEED_CS_PIN);
    ch585_spi_speed_delay_cycles(CH585_SPI_SPEED_CS_GAP_CYCLES);
    return 0;
}

static int ch585_spi_speed_transfer_frame(uint8_t *rx, const uint8_t *tx)
{
    return ch585_spi_speed_transfer_bytes(rx,
                                          tx,
                                          (uint16_t)CH585_H417_SPI_SPEED_TRANSFER_BYTES);
}

static void ch585_spi_speed_div2_diag_reset(ch585_spi_speed_stats_t *stats)
{
    stats->div2_rx0_and = 0xFFU;
    stats->div2_rx1_and = 0xFFU;
}

static void ch585_spi_speed_div2_diag_capture(const uint8_t *rx,
                                              ch585_spi_speed_stats_t *stats)
{
    uint8_t slot;
    uint8_t i;

    stats->div2_rx0_and &= rx[0];
    stats->div2_rx0_or |= rx[0];
    stats->div2_rx1_and &= rx[1];
    stats->div2_rx1_or |= rx[1];

    if(stats->div2_sample_count >= CH585_SPI_SPEED_DIV2_DIAG_SAMPLES)
    {
        return;
    }

    slot = stats->div2_sample_count;
    for(i = 0U; i < CH585_SPI_SPEED_DIV2_DIAG_BYTES; i++)
    {
        stats->div2_samples[slot][i] = rx[i];
    }
    stats->div2_sample_count++;
}

static void ch585_spi_speed_note_first_bad_at(const uint8_t *rx,
                                              uint16_t offset,
                                              ch585_spi_speed_stats_t *stats)
{
    uint8_t i;

    if(stats->have_first_bad != 0U)
    {
        return;
    }

    for(i = 0U; i < 4U; i++)
    {
        uint16_t pos = (uint16_t)(offset + i);
        if(pos < CH585_H417_SPI_SPEED_TRANSFER_BYTES)
        {
            stats->first_bad[i] = rx[pos];
            stats->first_expected[i] = ch585_h417_spi_speed_fixed_byte(pos);
        }
    }
    stats->first_bad_off = offset;
    stats->have_first_bad = 1U;
}

static int ch585_spi_speed_validate_fixed_transfer(const uint8_t *rx,
                                                   ch585_spi_speed_stats_t *stats)
{
    uint16_t i;

    if(rx[0] != (uint8_t)CH585_H417_SPI_SPEED_READY_BYTE)
    {
        stats->bad_ready++;
        ch585_spi_speed_note_first_bad_at(rx, 0U, stats);
        return -1;
    }

    for(i = 1U; i < CH585_H417_SPI_SPEED_TRANSFER_BYTES; i++)
    {
        if(rx[i] != ch585_h417_spi_speed_fixed_byte(i))
        {
            stats->bad_fixed++;
            ch585_spi_speed_note_first_bad_at(rx, i, stats);
            return -1;
        }
    }

    stats->ok++;
    return 0;
}

static uint32_t ch585_spi_speed_error_total(const ch585_spi_speed_stats_t *stats)
{
    return stats->bad_ready + stats->timeout + stats->bad_fixed;
}

static uint8_t ch585_spi_speed_rate_success(const ch585_spi_speed_stats_t *stats)
{
    if(stats->ok != CH585_SPI_SPEED_FRAMES_PER_RATE)
    {
        return 0U;
    }

    if((stats->timeout != 0U) || (stats->bad_fixed != 0U))
    {
        return 0U;
    }

    return 1U;
}

static void ch585_spi_speed_log_div2_diag(const ch585_spi_speed_rate_t *rate,
                                          const ch585_spi_speed_stats_t *stats)
{
    char line[CH585_SPI_SPEED_LINE_BYTES];
    int used;

    used = rt_snprintf(line,
                       sizeof(line),
                       "DIV2_DIAG name=%s rx0_and=%02x rx0_or=%02x rx1_and=%02x rx1_or=%02x samples=%u exp=%02x%02x%02x%02x%02x%02x%02x%02x s0=%02x%02x%02x%02x%02x%02x%02x%02x s1=%02x%02x%02x%02x%02x%02x%02x%02x s2=%02x%02x%02x%02x%02x%02x%02x%02x s3=%02x%02x%02x%02x%02x%02x%02x%02x",
                       rate->name,
                       (unsigned int)stats->div2_rx0_and,
                       (unsigned int)stats->div2_rx0_or,
                       (unsigned int)stats->div2_rx1_and,
                       (unsigned int)stats->div2_rx1_or,
                       (unsigned int)stats->div2_sample_count,
                       (unsigned int)ch585_h417_spi_speed_fixed_byte(0U),
                       (unsigned int)ch585_h417_spi_speed_fixed_byte(1U),
                       (unsigned int)ch585_h417_spi_speed_fixed_byte(2U),
                       (unsigned int)ch585_h417_spi_speed_fixed_byte(3U),
                       (unsigned int)ch585_h417_spi_speed_fixed_byte(4U),
                       (unsigned int)ch585_h417_spi_speed_fixed_byte(5U),
                       (unsigned int)ch585_h417_spi_speed_fixed_byte(6U),
                       (unsigned int)ch585_h417_spi_speed_fixed_byte(7U),
                       (unsigned int)stats->div2_samples[0][0],
                       (unsigned int)stats->div2_samples[0][1],
                       (unsigned int)stats->div2_samples[0][2],
                       (unsigned int)stats->div2_samples[0][3],
                       (unsigned int)stats->div2_samples[0][4],
                       (unsigned int)stats->div2_samples[0][5],
                       (unsigned int)stats->div2_samples[0][6],
                       (unsigned int)stats->div2_samples[0][7],
                       (unsigned int)stats->div2_samples[1][0],
                       (unsigned int)stats->div2_samples[1][1],
                       (unsigned int)stats->div2_samples[1][2],
                       (unsigned int)stats->div2_samples[1][3],
                       (unsigned int)stats->div2_samples[1][4],
                       (unsigned int)stats->div2_samples[1][5],
                       (unsigned int)stats->div2_samples[1][6],
                       (unsigned int)stats->div2_samples[1][7],
                       (unsigned int)stats->div2_samples[2][0],
                       (unsigned int)stats->div2_samples[2][1],
                       (unsigned int)stats->div2_samples[2][2],
                       (unsigned int)stats->div2_samples[2][3],
                       (unsigned int)stats->div2_samples[2][4],
                       (unsigned int)stats->div2_samples[2][5],
                       (unsigned int)stats->div2_samples[2][6],
                       (unsigned int)stats->div2_samples[2][7],
                       (unsigned int)stats->div2_samples[3][0],
                       (unsigned int)stats->div2_samples[3][1],
                       (unsigned int)stats->div2_samples[3][2],
                       (unsigned int)stats->div2_samples[3][3],
                       (unsigned int)stats->div2_samples[3][4],
                       (unsigned int)stats->div2_samples[3][5],
                       (unsigned int)stats->div2_samples[3][6],
                       (unsigned int)stats->div2_samples[3][7]);
    if(used > 0)
    {
        ch585_spi_speed_log_line(line);
    }
}

static void ch585_spi_speed_run_rate(const ch585_spi_speed_rate_t *rate,
                                     uint32_t *best_khz,
                                     const char **best_name)
{
    ch585_spi_speed_stats_t stats;
    uint32_t start_cycle;
    uint32_t elapsed_cycles;
    uint32_t khz;
    uint32_t attempts;
    char line[CH585_SPI_SPEED_LINE_BYTES];
    int used;

    memset(&stats, 0, sizeof(stats));
    ch585_spi_speed_div2_diag_reset(&stats);
    memset(s_ch585_spi_speed_rx, 0, sizeof(s_ch585_spi_speed_rx));
    if(ch585_spi_speed_is_stable_12m5(rate) != 0U)
    {
        ch32h417_ch585_spi_link_init(&s_ch585_spi_speed_stable_link);
    }
    else
    {
        ch585_spi_speed_spi_apply(rate);
    }
    rt_thread_mdelay(2);

    start_cycle = ch585_spi_speed_mcycle();
    attempts = 0U;
    while((stats.ok < CH585_SPI_SPEED_FRAMES_PER_RATE) &&
          (attempts < CH585_SPI_SPEED_MAX_ATTEMPTS_PER_RATE))
    {
        int ret;

        if(ch585_spi_speed_is_stable_12m5(rate) != 0U)
        {
            ret = ch32h417_ch585_spi_link_transfer(
                s_ch585_spi_speed_tx,
                s_ch585_spi_speed_rx,
                (uint16_t)CH585_H417_SPI_SPEED_TRANSFER_BYTES);
        }
        else
        {
            ret = ch585_spi_speed_transfer_frame(s_ch585_spi_speed_rx,
                                                 s_ch585_spi_speed_tx);
        }
        attempts++;
        if(ret != 0)
        {
            stats.timeout++;
            break;
        }

        if(ch585_spi_speed_validate_fixed_transfer(s_ch585_spi_speed_rx,
                                                   &stats) != 0)
        {
            if(ch585_spi_speed_is_div2(rate) != 0U)
            {
                ch585_spi_speed_div2_diag_capture(s_ch585_spi_speed_rx,
                                                  &stats);
            }

            if(s_ch585_spi_speed_rx[0] != (uint8_t)CH585_H417_SPI_SPEED_READY_BYTE)
            {
                ch585_spi_speed_delay_cycles(CH585_SPI_SPEED_SYNC_RETRY_CYCLES);
            }
        }
        g_v5f_hw_test_diag.frame_count++;
    }
    elapsed_cycles = ch585_spi_speed_mcycle() - start_cycle;
    khz = ch585_spi_speed_rate_khz(rate);

    if((ch585_spi_speed_rate_success(&stats) != 0U) && (khz >= *best_khz))
    {
        *best_khz = khz;
        *best_name = rate->name;
    }

    g_v5f_hw_test_diag.spi_timeout_count += stats.timeout;
    g_v5f_hw_test_diag.gpha_ok_count = stats.ok;
    g_v5f_hw_test_diag.gpha_fail_count =
        (ch585_spi_speed_rate_success(&stats) != 0U) ? 0U :
        ch585_spi_speed_error_total(&stats);
    g_v5f_hw_test_diag.last_error =
        (ch585_spi_speed_rate_success(&stats) != 0U) ? 0 : -300;

    used = rt_snprintf(line,
                       sizeof(line),
                       "SPI_RATE name=%s div=%u hsrx=%u cpha=%u khz=%u bytes=%u frames=%u attempts=%u ok=%u bad_ready=%u timeout=%u bad_fixed=%u first_bad_off=%u first_bad=%02x%02x%02x%02x first_exp=%02x%02x%02x%02x cycles=%u best_khz=%u",
                       rate->name,
                       (unsigned int)rate->div,
                       (unsigned int)rate->hsrx,
                       (unsigned int)((rate->cpha == SPI_CPHA_2Edge) ? 2U : 1U),
                       (unsigned int)khz,
                       (unsigned int)CH585_H417_SPI_SPEED_TRANSFER_BYTES,
                       (unsigned int)CH585_SPI_SPEED_FRAMES_PER_RATE,
                       (unsigned int)attempts,
                       (unsigned int)stats.ok,
                       (unsigned int)stats.bad_ready,
                       (unsigned int)stats.timeout,
                       (unsigned int)stats.bad_fixed,
                       (unsigned int)stats.first_bad_off,
                       (unsigned int)stats.first_bad[0],
                       (unsigned int)stats.first_bad[1],
                       (unsigned int)stats.first_bad[2],
                       (unsigned int)stats.first_bad[3],
                       (unsigned int)stats.first_expected[0],
                       (unsigned int)stats.first_expected[1],
                       (unsigned int)stats.first_expected[2],
                       (unsigned int)stats.first_expected[3],
                       (unsigned int)elapsed_cycles,
                       (unsigned int)*best_khz);
    if(used > 0)
    {
        ch585_spi_speed_log_line(line);
    }

    if(ch585_spi_speed_is_div2(rate) != 0U)
    {
        ch585_spi_speed_log_div2_diag(rate, &stats);
    }
}

static void ch585_spi_speed_run_high_frequency(ch585_spi_speed_cmd_state_t *cmd,
                                               uint32_t *best_khz,
                                               const char **best_name,
                                               uint8_t stop_on_command)
{
    uint32_t i;

    for(i = 0U; i < ch585_spi_speed_rate_count(); i++)
    {
        const ch585_spi_speed_rate_t *rate = &s_ch585_spi_speed_rates[i];

        if(ch585_spi_speed_is_high_frequency(rate) == 0U)
        {
            continue;
        }

        if(stop_on_command != 0U)
        {
            ch585_spi_speed_poll_command(cmd);
            if((cmd == RT_NULL) || (cmd->mode != CH585_SPI_SPEED_CMD_HF))
            {
                break;
            }
        }

        ch585_spi_speed_run_rate(rate, best_khz, best_name);
    }
}

static void run_ch585_spi_speed_test(void)
{
    ch585_spi_speed_cmd_state_t cmd;
    char line[CH585_SPI_SPEED_LINE_BYTES];
    uint32_t i;

    memset(&cmd, 0, sizeof(cmd));
    cmd.mode = CH585_SPI_SPEED_CMD_AUTO;
    cmd.rate = &s_ch585_spi_speed_rates[0];

    for(i = 0U; i < sizeof(s_ch585_spi_speed_tx); i++)
    {
        s_ch585_spi_speed_tx[i] = (uint8_t)(0xA0U + i);
    }

    (void)ch32h417_dual_cdc_init();
    rt_thread_mdelay(300);
    ch32h417_ch585_spi_link_config_for_side(CH585_SPI_SPEED_LINK_SIDE,
                                            &s_ch585_spi_speed_stable_link);
    ch585_spi_speed_gpio_init();
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;

    (void)rt_snprintf(line,
                      sizeof(line),
                      "CH585_SPI_SPEED START source=%s pins=PB3_SCK_PB5_MOSI_PB4_MISO mode=fixed-sync stable_khz=%u transfer_bytes=%u frame_bytes=%u hclk=%u sys=%u ready_byte=0x%02x frame_off=%u cs_gap_cycles=%u sync_retry_cycles=%u max_attempts=%u",
                      CH585_SPI_SPEED_SOURCE_DESC,
                      (unsigned int)CH32H417_CH585_SPI_LINK_SPI_KHZ,
                      (unsigned int)CH585_H417_SPI_SPEED_TRANSFER_BYTES,
                      (unsigned int)CH585_H417_SPI_SPEED_FRAME_BYTES,
                      (unsigned int)HCLKClock,
                      (unsigned int)SystemClock,
                      (unsigned int)CH585_H417_SPI_SPEED_READY_BYTE,
                      (unsigned int)CH585_H417_SPI_SPEED_FRAME_OFF,
                      (unsigned int)CH585_SPI_SPEED_CS_GAP_CYCLES,
                      (unsigned int)CH585_SPI_SPEED_SYNC_RETRY_CYCLES,
                      (unsigned int)CH585_SPI_SPEED_MAX_ATTEMPTS_PER_RATE);
    ch585_spi_speed_log_line(line);
    ch585_spi_speed_log_help();

    while(1)
    {
        uint32_t best_khz = 0U;
        const char *best_name = "none";

        ch585_spi_speed_poll_command(&cmd);

        if(cmd.once_hf_pending != 0U)
        {
            cmd.once_hf_pending = 0U;
            ch585_spi_speed_run_high_frequency(&cmd,
                                               &best_khz,
                                               &best_name,
                                               0U);
            (void)rt_snprintf(line,
                              sizeof(line),
                              "SPI_MAX best_khz=%u best_name=%s source=%s mode=oncehf",
                              (unsigned int)best_khz,
                              best_name,
                              CH585_SPI_SPEED_SOURCE_DESC);
            ch585_spi_speed_log_line(line);
            ch585_spi_speed_poll_command(&cmd);
            continue;
        }

        if(cmd.once_pending != 0U)
        {
            cmd.once_pending = 0U;
            ch585_spi_speed_run_rate(cmd.rate,
                                     &best_khz,
                                     &best_name);
            (void)rt_snprintf(line,
                              sizeof(line),
                              "SPI_MAX best_khz=%u best_name=%s source=%s mode=once rate=%s",
                              (unsigned int)best_khz,
                              best_name,
                              CH585_SPI_SPEED_SOURCE_DESC,
                              cmd.rate->name);
            ch585_spi_speed_log_line(line);
            ch585_spi_speed_poll_command(&cmd);
            continue;
        }

        if(cmd.mode == CH585_SPI_SPEED_CMD_STOP)
        {
            ch585_spi_speed_poll_delay(&cmd, 50U);
            continue;
        }

        if(cmd.mode == CH585_SPI_SPEED_CMD_HF)
        {
            ch585_spi_speed_run_high_frequency(&cmd,
                                               &best_khz,
                                               &best_name,
                                               1U);
            if(cmd.mode == CH585_SPI_SPEED_CMD_HF)
            {
                (void)rt_snprintf(line,
                                  sizeof(line),
                                  "SPI_MAX best_khz=%u best_name=%s source=%s mode=hf",
                                  (unsigned int)best_khz,
                                  best_name,
                                  CH585_SPI_SPEED_SOURCE_DESC);
                ch585_spi_speed_log_line(line);
                ch585_spi_speed_poll_delay(&cmd, 250U);
            }
            continue;
        }

        if(cmd.mode == CH585_SPI_SPEED_CMD_RATE)
        {
            ch585_spi_speed_run_rate(cmd.rate,
                                     &best_khz,
                                     &best_name);
            (void)rt_snprintf(line,
                              sizeof(line),
                              "SPI_MAX best_khz=%u best_name=%s source=%s mode=rate rate=%s",
                              (unsigned int)best_khz,
                              best_name,
                              CH585_SPI_SPEED_SOURCE_DESC,
                              cmd.rate->name);
            ch585_spi_speed_log_line(line);
            ch585_spi_speed_poll_command(&cmd);
            continue;
        }

        for(i = 0U; i < ch585_spi_speed_rate_count(); i++)
        {
            ch585_spi_speed_poll_command(&cmd);
            if(cmd.mode != CH585_SPI_SPEED_CMD_AUTO)
            {
                break;
            }

            ch585_spi_speed_run_rate(&s_ch585_spi_speed_rates[i],
                                     &best_khz,
                                     &best_name);
        }

        if(cmd.mode == CH585_SPI_SPEED_CMD_AUTO)
        {
            (void)rt_snprintf(line,
                              sizeof(line),
                              "SPI_MAX best_khz=%u best_name=%s source=%s mode=auto",
                              (unsigned int)best_khz,
                              best_name,
                              CH585_SPI_SPEED_SOURCE_DESC);
            ch585_spi_speed_log_line(line);
            ch585_spi_speed_poll_delay(&cmd, 1000U);
        }
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_CH585_ADC_KEY_CAL
#if !APP_ENABLE_USB_TEST
#error "ch585_adc_key_cal hw_test requires APP_ENABLE_USB_TEST for CDC output"
#endif

#if !defined(APP_CH585_ADC_KEY_CAL_SOURCE_LEFT) && \
    !defined(APP_CH585_ADC_KEY_CAL_SOURCE_RIGHT)
#define APP_CH585_ADC_KEY_CAL_SOURCE_LEFT 1
#endif

#if defined(APP_CH585_ADC_KEY_CAL_SOURCE_LEFT) && \
    defined(APP_CH585_ADC_KEY_CAL_SOURCE_RIGHT)
#error "Select only one CH585 ADC key calibration source"
#endif

#if defined(APP_CH585_ADC_KEY_CAL_SOURCE_RIGHT)
#define CH585_ADC_KEY_CAL_SOURCE_DESC "right/U3 CS=PD9 other=PF2"
#define CH585_ADC_KEY_CAL_SOURCE_TEXT "right"
#define CH585_ADC_KEY_CAL_LINK_SIDE CH32H417_CH585_SPI_LINK_SIDE_RIGHT
#define CH585_ADC_KEY_CAL_DEFAULT_KEYS 41U
#else
#define CH585_ADC_KEY_CAL_SOURCE_DESC "left/U2 CS=PF2 other=PD9"
#define CH585_ADC_KEY_CAL_SOURCE_TEXT "left"
#define CH585_ADC_KEY_CAL_LINK_SIDE CH32H417_CH585_SPI_LINK_SIDE_LEFT
#define CH585_ADC_KEY_CAL_DEFAULT_KEYS 36U
#endif

#define CH585_ADC_KEY_CAL_LINE_BYTES 192U
#define CH585_ADC_KEY_CAL_CMD_BYTES 64U

typedef struct
{
    uint8_t key;
    uint8_t key_count;
    uint8_t stream;
    uint8_t reset_pending;
    uint16_t host_seq;
    uint32_t valid_samples;
    uint32_t stale_samples;
    uint32_t bad_crc;
    uint32_t spi_errors;
} ch585_adc_key_cal_state_t;

static ch585_h417_adc_key_cal_cmd_t s_ch585_adc_key_cal_tx
    __attribute__((aligned(4)));
static ch585_h417_adc_key_cal_sample_t s_ch585_adc_key_cal_rx
    __attribute__((aligned(4)));
static ch32h417_ch585_spi_link_config_t s_ch585_adc_key_cal_link;

static int ch585_adc_key_cal_cdc_write_full(const char *data, rt_size_t len)
{
    rt_size_t offset = 0u;
    uint8_t retries = 0u;

    if(data == RT_NULL)
    {
        return -1;
    }

    while(offset < len)
    {
        int wrote = ch32h417_usb_cdc_write(&data[offset],
                                           (rt_uint32_t)(len - offset));

        if(wrote > 0)
        {
            offset += (rt_size_t)wrote;
            retries = 0u;
            continue;
        }

        if((wrote == -2) || (retries >= 16u))
        {
            return (offset > 0u) ? (int)offset : wrote;
        }

        retries++;
        rt_thread_mdelay(1);
    }

    return (int)offset;
}

static void ch585_adc_key_cal_cdc_line(const char *line)
{
    if(line == RT_NULL)
    {
        return;
    }

    (void)ch585_adc_key_cal_cdc_write_full(line, (rt_size_t)strlen(line));
    (void)ch585_adc_key_cal_cdc_write_full("\r\n", 2u);
}

static void ch585_adc_key_cal_log_line(const char *line)
{
    if(line == RT_NULL)
    {
        return;
    }

    rt_kprintf("%s\n", line);
    ch585_adc_key_cal_cdc_line(line);
}

static char *ch585_adc_key_cal_trim_command(char *line)
{
    char *end;

    if(line == RT_NULL)
    {
        return RT_NULL;
    }

    while((*line == ' ') || (*line == '\t') ||
          (*line == '\r') || (*line == '\n'))
    {
        line++;
    }

    end = line + strlen(line);
    while(end > line)
    {
        char ch = *(end - 1);
        if((ch != ' ') && (ch != '\t') &&
           (ch != '\r') && (ch != '\n'))
        {
            break;
        }
        end--;
        *end = '\0';
    }

    return line;
}

static int ch585_adc_key_cal_parse_u8(const char *text,
                                      uint8_t max_value,
                                      uint8_t *out)
{
    uint32_t value = 0U;
    uint8_t have_digit = 0U;

    if((text == RT_NULL) || (out == RT_NULL))
    {
        return -1;
    }

    while((*text == ' ') || (*text == '\t'))
    {
        text++;
    }

    while((*text >= '0') && (*text <= '9'))
    {
        value = (value * 10U) + (uint32_t)(*text - '0');
        if(value > max_value)
        {
            return -1;
        }
        have_digit = 1U;
        text++;
    }

    if(have_digit == 0U)
    {
        return -1;
    }

    *out = (uint8_t)value;
    return 0;
}

static void ch585_adc_key_cal_log_help(void)
{
    ch585_adc_key_cal_log_line(
        "CAL_CMD help commands: key <n> [reset] | reset | start | stop | help");
}

static void ch585_adc_key_cal_log_state(const char *status,
                                        const ch585_adc_key_cal_state_t *state)
{
    char line[CH585_ADC_KEY_CAL_LINE_BYTES];

    if(state == RT_NULL)
    {
        return;
    }

    (void)rt_snprintf(line,
                      sizeof(line),
                      "CAL_CMD %s side=%s key=%u key_count=%u stream=%u reset=%u",
                      status,
                      CH585_ADC_KEY_CAL_SOURCE_TEXT,
                      (unsigned int)state->key,
                      (unsigned int)state->key_count,
                      (unsigned int)state->stream,
                      (unsigned int)state->reset_pending);
    ch585_adc_key_cal_log_line(line);
}

static void ch585_adc_key_cal_handle_command(
    ch585_adc_key_cal_state_t *state,
    char *line)
{
    uint8_t key;

    if((state == RT_NULL) || (line == RT_NULL))
    {
        return;
    }

    line = ch585_adc_key_cal_trim_command(line);

    if((strcmp(line, "help") == 0) || (strcmp(line, "?") == 0))
    {
        ch585_adc_key_cal_log_help();
        return;
    }

    if(strcmp(line, "start") == 0)
    {
        state->stream = 1U;
        ch585_adc_key_cal_log_state("ok", state);
        return;
    }

    if(strcmp(line, "stop") == 0)
    {
        state->stream = 0U;
        ch585_adc_key_cal_log_state("ok", state);
        return;
    }

    if(strcmp(line, "reset") == 0)
    {
        state->reset_pending = 1U;
        ch585_adc_key_cal_log_state("ok", state);
        return;
    }

    if(strncmp(line, "key ", 4U) == 0)
    {
        uint8_t reset = (strstr(line, " reset") != RT_NULL) ? 1U : 0U;
        if(ch585_adc_key_cal_parse_u8(&line[4], 63U, &key) != 0)
        {
            ch585_adc_key_cal_log_line("CAL_CMD err bad key");
            return;
        }

        state->key = key;
        state->stream = 1U;
        if(reset != 0U)
        {
            state->reset_pending = 1U;
        }
        ch585_adc_key_cal_log_state("ok", state);
        return;
    }

    ch585_adc_key_cal_log_line("CAL_CMD err unknown command");
    ch585_adc_key_cal_log_help();
}

static void ch585_adc_key_cal_poll_command(ch585_adc_key_cal_state_t *state)
{
    char line[CH585_ADC_KEY_CAL_CMD_BYTES];
    int len;

    ch32h417_dual_cdc_poll();
    do
    {
        len = ch32h417_usb_cdc_read_line(line, sizeof(line));
        if(len > 0)
        {
            ch585_adc_key_cal_handle_command(state, line);
        }
    } while(len > 0);
}

static void ch585_adc_key_cal_build_cmd(ch585_adc_key_cal_state_t *state)
{
    memset(&s_ch585_adc_key_cal_tx, 0, sizeof(s_ch585_adc_key_cal_tx));
    s_ch585_adc_key_cal_tx.magic = CH585_H417_ADC_KEY_CAL_CMD_MAGIC;
    s_ch585_adc_key_cal_tx.version = CH585_H417_ADC_KEY_CAL_VERSION;
    s_ch585_adc_key_cal_tx.cmd = CH585_H417_ADC_KEY_CAL_CMD_SELECT;
    s_ch585_adc_key_cal_tx.key_id = state->key;
    s_ch585_adc_key_cal_tx.host_seq = state->host_seq++;
    if(state->reset_pending != 0U)
    {
        s_ch585_adc_key_cal_tx.flags =
            CH585_H417_ADC_KEY_CAL_FLAG_RESET_STATS;
        state->reset_pending = 0U;
    }
    ch585_h417_adc_key_cal_finish_cmd(&s_ch585_adc_key_cal_tx);
}

static void ch585_adc_key_cal_emit_sample(ch585_adc_key_cal_state_t *state,
                                          int spi_rc,
                                          uint32_t diag)
{
    char line[CH585_ADC_KEY_CAL_LINE_BYTES];
    const ch585_h417_adc_key_cal_sample_t *sample = &s_ch585_adc_key_cal_rx;

    if(ch585_h417_adc_key_cal_sample_valid(sample) == 0U)
    {
        state->bad_crc++;
        if((state->bad_crc & 0x3FU) == 1U)
        {
            (void)rt_snprintf(line,
                              sizeof(line),
                              "CAL_ERR side=%s spi=%d bad_crc=%u h=%02x%02x diag=0x%08x",
                              CH585_ADC_KEY_CAL_SOURCE_TEXT,
                              spi_rc,
                              (unsigned int)state->bad_crc,
                              (unsigned int)((const uint8_t *)sample)[0],
                              (unsigned int)((const uint8_t *)sample)[1],
                              (unsigned int)diag);
            ch585_adc_key_cal_log_line(line);
        }
        return;
    }

    if(sample->key_count != 0U)
    {
        state->key_count = sample->key_count;
    }

    if(sample->key_id != state->key)
    {
        state->stale_samples++;
        return;
    }

    state->valid_samples++;
    (void)rt_snprintf(line,
                      sizeof(line),
                      "CAL_SAMPLE side=%s key=%u seq=%u raw=%04u min=%04u max=%04u count=%u status=%u spi=%d diag=0x%08x",
                      CH585_ADC_KEY_CAL_SOURCE_TEXT,
                      (unsigned int)sample->key_id,
                      (unsigned int)sample->sample_seq,
                      (unsigned int)sample->raw,
                      (unsigned int)sample->min_raw,
                      (unsigned int)sample->max_raw,
                      (unsigned int)sample->sample_count,
                      (unsigned int)sample->status,
                      spi_rc,
                      (unsigned int)diag);
    ch585_adc_key_cal_cdc_line(line);
}

static void run_ch585_adc_key_cal_test(void)
{
    ch585_adc_key_cal_state_t state;
    char line[CH585_ADC_KEY_CAL_LINE_BYTES];

    memset(&state, 0, sizeof(state));
    state.key = 0U;
    state.key_count = CH585_ADC_KEY_CAL_DEFAULT_KEYS;
    state.stream = 1U;
    state.reset_pending = 1U;

    (void)ch32h417_dual_cdc_init();
    rt_thread_mdelay(300);
    ch32h417_ch585_spi_link_config_for_side(CH585_ADC_KEY_CAL_LINK_SIDE,
                                            &s_ch585_adc_key_cal_link);
    ch32h417_ch585_spi_link_init(&s_ch585_adc_key_cal_link);
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;

    (void)rt_snprintf(line,
                      sizeof(line),
                      "CH585_ADC_KEY_CAL START source=%s frame_bytes=%u default_keys=%u spi_khz=%u",
                      CH585_ADC_KEY_CAL_SOURCE_DESC,
                      (unsigned int)CH585_H417_ADC_KEY_CAL_FRAME_BYTES,
                      (unsigned int)CH585_ADC_KEY_CAL_DEFAULT_KEYS,
                      (unsigned int)CH32H417_CH585_SPI_LINK_SPI_KHZ);
    ch585_adc_key_cal_log_line(line);
    ch585_adc_key_cal_log_help();

    while(1)
    {
        int spi_rc;
        uint32_t diag;

        ch585_adc_key_cal_poll_command(&state);
        if(state.stream == 0U)
        {
            rt_thread_mdelay(20);
            continue;
        }

        ch585_adc_key_cal_build_cmd(&state);
        memset(&s_ch585_adc_key_cal_rx, 0, sizeof(s_ch585_adc_key_cal_rx));
        spi_rc = ch32h417_ch585_spi_link_transfer(
            (const uint8_t *)&s_ch585_adc_key_cal_tx,
            (uint8_t *)&s_ch585_adc_key_cal_rx,
            (uint16_t)CH585_H417_ADC_KEY_CAL_FRAME_BYTES);
        diag = ch32h417_ch585_spi_link_last_diag();

        if(spi_rc == CH32H417_CH585_SPI_LINK_OK)
        {
            ch585_adc_key_cal_emit_sample(&state, spi_rc, diag);
        }
        else
        {
            state.spi_errors++;
            if((state.spi_errors & 0x3FU) == 1U)
            {
                (void)rt_snprintf(line,
                                  sizeof(line),
                                  "CAL_ERR side=%s spi=%d spi_errors=%u diag=0x%08x",
                                  CH585_ADC_KEY_CAL_SOURCE_TEXT,
                                  spi_rc,
                                  (unsigned int)state.spi_errors,
                                  (unsigned int)diag);
                ch585_adc_key_cal_log_line(line);
            }
        }

        ch32h417_dual_cdc_poll();
        rt_thread_mdelay(1);
    }
}
#endif

static void fail_forever(int error);

static void V5F_MAYBE_UNUSED memory_barrier(void)
{
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

static uint16_t *rgb_fb(void)
{
    return (uint16_t *)&s_lcd_fb[0];
}

static uint8_t *l8_fb(void) V5F_MAYBE_UNUSED;
static uint8_t *l8_fb(void)
{
    return &s_lcd_fb[0];
}

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_PFC_L8_RGB565
static uint8_t *gpha_l8_src(void)
{
    return &s_lcd_fb[V5F_GPHA_L8_SRC_OFFSET];
}

static uint32_t *gpha_l8_clut(void)
{
    return (uint32_t *)&s_lcd_fb[V5F_GPHA_L8_CLUT_OFFSET];
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_BLEND_RGB565
static uint16_t *gpha_blend_bg(void)
{
    return (uint16_t *)&s_lcd_fb[V5F_GPHA_BLEND_BG_OFFSET];
}

static uint16_t *gpha_blend_fg_argb4444(void)
{
    return (uint16_t *)&s_lcd_fb[V5F_GPHA_BLEND_FG_OFFSET];
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_L8_LTDC_FULLSCREEN
static void gpha_l8_ltdc_set_clut(uint8_t index,
                                  uint8_t red,
                                  uint8_t green,
                                  uint8_t blue)
{
    s_gpha_l8_ltdc_clut_rgb888[((uint32_t)index * 3u) + 0u] = red;
    s_gpha_l8_ltdc_clut_rgb888[((uint32_t)index * 3u) + 1u] = green;
    s_gpha_l8_ltdc_clut_rgb888[((uint32_t)index * 3u) + 2u] = blue;
}

static void gpha_l8_ltdc_build_clut(void)
{
    uint16_t i;

    for(i = 0u; i < CH32H417_LTDC_RGB_CLUT_ENTRIES; i++)
    {
        uint8_t level = (uint8_t)i;
        gpha_l8_ltdc_set_clut((uint8_t)i, level, level, level);
    }

    gpha_l8_ltdc_set_clut(0u, 0u, 0u, 0u);
    gpha_l8_ltdc_set_clut(1u, 255u, 0u, 0u);
    gpha_l8_ltdc_set_clut(2u, 0u, 255u, 0u);
    gpha_l8_ltdc_set_clut(3u, 0u, 0u, 255u);
    gpha_l8_ltdc_set_clut(4u, 255u, 255u, 255u);
    gpha_l8_ltdc_set_clut(5u, 0u, 255u, 255u);
    gpha_l8_ltdc_set_clut(6u, 255u, 255u, 0u);
    gpha_l8_ltdc_set_clut(7u, 255u, 0u, 255u);
    gpha_l8_ltdc_set_clut(8u, 255u, 128u, 0u);
    gpha_l8_ltdc_set_clut(9u, 128u, 64u, 255u);
    gpha_l8_ltdc_set_clut(10u, 32u, 32u, 32u);
    gpha_l8_ltdc_set_clut(11u, 192u, 192u, 192u);
}
#endif

static void load_l8_clut_after_layer_start(void)
{
    /*
     * On this H417 board, LTDC L8 color lookup writes are reliable only after
     * the controller and layer are running. Pre-start CLUT writes produced a
     * stable but shifted color mapping during hardware validation.
     */
    rt_thread_mdelay(100);
#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_L8_PALETTE_IMAGE
    (void)ch32h417_ltdc_rgb_layer1_load_clut_rgb888(
        v5f_ltdc_palette_800x480_clut_rgb888,
        V5F_LTDC_PALETTE_CLUT_ENTRIES);
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_L8_LTDC_FULLSCREEN
    gpha_l8_ltdc_build_clut();
    (void)ch32h417_ltdc_rgb_layer1_load_clut_rgb888(
        s_gpha_l8_ltdc_clut_rgb888,
        CH32H417_LTDC_RGB_CLUT_ENTRIES);
#else
    ch32h417_ltdc_rgb_layer1_load_grayscale_clut();
#endif
}

static void fb_fill_rgb565(uint16_t color)
{
    ch32h417_ltdc_rgb_fb_fill_rgb565(rgb_fb(),
                                     V5F_RGB_FB_WIDTH,
                                     V5F_RGB_FB_HEIGHT,
                                     color);
}

static void V5F_MAYBE_UNUSED fb_plot_user_rgb565(uint16_t x, uint16_t y, uint16_t color)
{
    /*
     * The mounted panel is rotated 180 degrees. Keep test coordinates in
     * the user's visual direction and mirror them into framebuffer memory.
     */
    ch32h417_ltdc_rgb_fb_plot_rgb565_rot180(rgb_fb(),
                                            V5F_RGB_FB_WIDTH,
                                            V5F_RGB_FB_HEIGHT,
                                            x,
                                            y,
                                            color);
}

static void V5F_MAYBE_UNUSED fb_fill_user_rect_rgb565(uint16_t x,
                                                      uint16_t y,
                                                      uint16_t width,
                                                      uint16_t height,
                                                      uint16_t color)
{
    ch32h417_ltdc_rgb_fb_fill_rect_rgb565_rot180(rgb_fb(),
                                                 V5F_RGB_FB_WIDTH,
                                                 V5F_RGB_FB_HEIGHT,
                                                 x,
                                                 y,
                                                 width,
                                                 height,
                                                 color);
}

static void V5F_MAYBE_UNUSED fb_draw_border_rgb565(uint16_t color)
{
    ch32h417_ltdc_rgb_fb_draw_border_rgb565_rot180(rgb_fb(),
                                                   V5F_RGB_FB_WIDTH,
                                                   V5F_RGB_FB_HEIGHT,
                                                   color);
}

static int V5F_MAYBE_UNUSED lcd_start_rgb565_window(void);

#if (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_MEMTEST) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_LTDC_RGB565) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_REMAP_PROBE) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_DQ_PROBE) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_OFFICIAL_16BIT)
#define V5F_SDRAM_BASE_ADDR            0xC0000000u
#define V5F_SDRAM_REMAP_ADDR           0x60000000u
#define V5F_SDRAM_BYTES                (32u * 1024u * 1024u)
#define V5F_SDRAM_LTDC_WIDTH           CH32H417_LCD_RGB_WIDTH
#define V5F_SDRAM_LTDC_HEIGHT          CH32H417_LCD_RGB_HEIGHT
#define V5F_SDRAM_LTDC_RGB565_BYTES    (V5F_SDRAM_LTDC_WIDTH * V5F_SDRAM_LTDC_HEIGHT * 2u)
#define V5F_SDRAM_QUICK_TEST_BYTES     (2u * 1024u * 1024u)
#define V5F_FMC_SDRAM_REMAP_TO_0X60000000 (1u << 24)
#define V5F_SDRAM_MAX_SDCLK_HZ         100000000u
#define V5F_SDRAM_REFRESH_CYCLES       8192u
#define V5F_SDRAM_REFRESH_PERIOD_US    64000u
#define V5F_SDRAM_REFRESH_MARGIN       20u
#define V5F_SDRAM_TIMEOUT_POLLS        1000000u
#define V5F_SDRAM_MODE_REGISTER        0x0230u
#define V5F_SDRAM_DEFAULT_PHASE_SEL    0x0Au

#define V5F_SDRAM_OK                   0
#define V5F_SDRAM_ERR_CLOCK            (-200)
#define V5F_SDRAM_ERR_TIMEOUT          (-201)
#define V5F_SDRAM_ERR_PARAM            (-202)
#define V5F_SDRAM_ERR_VERIFY           (-203)
#define V5F_SDRAM_ERR_LCD              (-204)
#define V5F_SDRAM_USB_LINE_BYTES       128u
#define V5F_SDRAM_SCOPE_CYCLES         262144u

static uint8_t s_sdram_debug_phase = V5F_SDRAM_DEFAULT_PHASE_SEL;
static uint8_t s_sdram_debug_pipe = FMC_ReadPipeDelay_none;
static uint8_t s_sdram_debug_score;
static uint8_t s_sdram_debug_bit_score;

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_OFFICIAL_16BIT
extern void h417_v5f_sdram_official_16bit_init(void);
#endif

typedef enum
{
    V5F_SDRAM_STAGE_NONE = 0,
    V5F_SDRAM_STAGE_INIT = 1,
    V5F_SDRAM_STAGE_DATA_BUS = 2,
    V5F_SDRAM_STAGE_ADDRESS_BUS = 3,
    V5F_SDRAM_STAGE_PATTERN = 4,
    V5F_SDRAM_STAGE_PATTERN_INV = 5,
    V5F_SDRAM_STAGE_LTDC_FILL = 6,
    V5F_SDRAM_STAGE_LTDC_RUNNING = 7,
} v5f_sdram_stage_t;

typedef enum
{
    V5F_SDRAM_STATUS_BOOT = 1,
    V5F_SDRAM_STATUS_INIT = 2,
    V5F_SDRAM_STATUS_DATA_BUS = 3,
    V5F_SDRAM_STATUS_ADDRESS_BUS = 4,
    V5F_SDRAM_STATUS_PATTERN = 5,
    V5F_SDRAM_STATUS_LTDC_FILL = 6,
    V5F_SDRAM_STATUS_RUNNING = 7,
    V5F_SDRAM_STATUS_PASS = 8,
} v5f_sdram_status_t;

static uint16_t sdram_status_error_count(void)
{
    switch(g_v5f_hw_test_diag.last_error)
    {
        case V5F_SDRAM_ERR_CLOCK:
            return 1u;
        case V5F_SDRAM_ERR_TIMEOUT:
            return 2u;
        case V5F_SDRAM_ERR_PARAM:
            return 3u;
        case V5F_SDRAM_ERR_VERIFY:
            return 4u;
        case V5F_SDRAM_ERR_LCD:
            return 5u;
        default:
            return 6u;
    }
}

typedef struct
{
    uint32_t stage;
    uint32_t offset;
    uint32_t expected;
    uint32_t actual;
} v5f_sdram_memtest_result_t;

static void sdram_status_show(v5f_sdram_status_t status, uint16_t color)
{
    uint16_t i;
    uint16_t slot_width = (uint16_t)(V5F_RGB_FB_WIDTH / 9u);
    uint16_t body_x = (uint16_t)(V5F_RGB_FB_WIDTH / 6u);
    uint16_t body_y = (uint16_t)(V5F_RGB_FB_HEIGHT / 3u);
    uint16_t body_w = (uint16_t)((V5F_RGB_FB_WIDTH * 2u) / 3u);
    uint16_t body_h = (uint16_t)(V5F_RGB_FB_HEIGHT / 3u);
    uint16_t bg = ch32h417_ltdc_rgb_pack_rgb565(2u, 4u, 7u);
    uint16_t dim = ch32h417_ltdc_rgb_pack_rgb565(18u, 18u, 22u);
    uint16_t white = ch32h417_ltdc_rgb_pack_rgb565(240u, 240u, 240u);

    fb_fill_rgb565(bg);
    fb_draw_border_rgb565(color);
    fb_fill_user_rect_rgb565(body_x, body_y, body_w, body_h, color);

    for(i = 0u; i < 9u; i++)
    {
        uint16_t x = (uint16_t)(4u + (i * slot_width));
        uint16_t w = (slot_width > 8u) ? (uint16_t)(slot_width - 8u) : 1u;
        uint16_t slot_color = (i < (uint16_t)status) ? color : dim;
        fb_fill_user_rect_rgb565(x, 8u, w, 14u, slot_color);
    }

    if(status == V5F_SDRAM_STATUS_PASS)
    {
        fb_fill_user_rect_rgb565((uint16_t)(body_x + 8u),
                                 (uint16_t)(body_y + 8u),
                                 (uint16_t)(body_w - 16u),
                                 (uint16_t)(body_h - 16u),
                                 ch32h417_ltdc_rgb_pack_rgb565(0u, 180u, 48u));
    }

    fb_fill_user_rect_rgb565(0u, (uint16_t)(V5F_RGB_FB_HEIGHT - 10u),
                             V5F_RGB_FB_WIDTH, 10u, white);
    memory_barrier();
}

static void sdram_status_word_bits_show(uint32_t value, uint16_t y, uint16_t color)
{
    uint16_t bit;
    uint16_t slot_width = (uint16_t)(V5F_RGB_FB_WIDTH / 32u);
    uint16_t block_width = (slot_width > 2u) ? (uint16_t)(slot_width - 2u) : 1u;
    uint16_t dim = ch32h417_ltdc_rgb_pack_rgb565(18u, 18u, 22u);

    for(bit = 0u; bit < 32u; bit++)
    {
        uint16_t x = (uint16_t)(1u + (bit * slot_width));
        uint16_t bit_color = ((value & (1u << bit)) != 0u) ? color : dim;
        fb_fill_user_rect_rgb565(x, y, block_width, 8u, bit_color);
    }
}

static void sdram_status_compare_bits_show(uint32_t expected, uint32_t actual, uint16_t y)
{
    uint16_t bit;
    uint16_t slot_width = (uint16_t)(V5F_RGB_FB_WIDTH / 32u);
    uint16_t block_width = (slot_width > 2u) ? (uint16_t)(slot_width - 2u) : 1u;
    uint16_t correct_zero = ch32h417_ltdc_rgb_pack_rgb565(18u, 18u, 22u);
    uint16_t correct_one = ch32h417_ltdc_rgb_pack_rgb565(0u, 210u, 80u);
    uint16_t unexpected_one = ch32h417_ltdc_rgb_pack_rgb565(0u, 200u, 255u);
    uint16_t missing_one = ch32h417_ltdc_rgb_pack_rgb565(255u, 255u, 255u);

    for(bit = 0u; bit < 32u; bit++)
    {
        uint32_t mask = 1u << bit;
        uint8_t expected_one = ((expected & mask) != 0u) ? 1u : 0u;
        uint8_t actual_one = ((actual & mask) != 0u) ? 1u : 0u;
        uint16_t x = (uint16_t)(1u + (bit * slot_width));
        uint16_t bit_color;

        if(expected_one == actual_one)
        {
            bit_color = (actual_one != 0u) ? correct_one : correct_zero;
        }
        else
        {
            bit_color = (actual_one != 0u) ? unexpected_one : missing_one;
        }

        fb_fill_user_rect_rgb565(x, y, block_width, 8u, bit_color);
    }
}

static uint32_t sdram_probe_write_read(volatile uint32_t *base, uint32_t expected)
{
    base[0] = expected;
    ch32h417_ltdc_rgb_framebuffer_barrier();
    return base[0];
}

static void sdram_probe_data_bus_show(void)
{
    volatile uint32_t *base = (volatile uint32_t *)V5F_SDRAM_BASE_ADDR;
    uint32_t actual;

    actual = sdram_probe_write_read(base, 0x00000000u);
    sdram_status_compare_bits_show(0x00000000u, actual, 78u);

    actual = sdram_probe_write_read(base, 0xFFFFFFFFu);
    sdram_status_compare_bits_show(0xFFFFFFFFu, actual, 92u);

    actual = sdram_probe_write_read(base, 0xAAAAAAAAu);
    sdram_status_compare_bits_show(0xAAAAAAAAu, actual, 106u);

    actual = sdram_probe_write_read(base, 0x55555555u);
    sdram_status_compare_bits_show(0x55555555u, actual, 120u);
}

static uint8_t sdram_probe_window_show(uint32_t base_addr, uint16_t y)
{
    volatile uint32_t *base = (volatile uint32_t *)base_addr;
    uint32_t actual;
    uint8_t pass = 1u;

    actual = sdram_probe_write_read(base, 0x00000000u);
    sdram_status_compare_bits_show(0x00000000u, actual, y);
    if(actual != 0x00000000u)
    {
        pass = 0u;
    }

    actual = sdram_probe_write_read(base, 0xFFFFFFFFu);
    sdram_status_compare_bits_show(0xFFFFFFFFu, actual, (uint16_t)(y + 14u));
    if(actual != 0xFFFFFFFFu)
    {
        pass = 0u;
    }

    actual = sdram_probe_write_read(base, 0xAAAAAAAAu);
    sdram_status_compare_bits_show(0xAAAAAAAAu, actual, (uint16_t)(y + 28u));
    if(actual != 0xAAAAAAAAu)
    {
        pass = 0u;
    }

    actual = sdram_probe_write_read(base, 0x55555555u);
    sdram_status_compare_bits_show(0x55555555u, actual, (uint16_t)(y + 42u));
    if(actual != 0x55555555u)
    {
        pass = 0u;
    }

    return pass;
}

static uint16_t sdram_probe_write_read16(volatile uint16_t *probe, uint16_t expected)
{
    *probe = 0u;
    ch32h417_ltdc_rgb_framebuffer_barrier();
    *probe = expected;
    ch32h417_ltdc_rgb_framebuffer_barrier();
    return *probe;
}

static void sdram_dq_probe_matrix_show(volatile uint16_t *probe, uint16_t x0, uint16_t y0)
{
    uint16_t row;
    uint16_t col;
    uint16_t cell = 12u;
    uint16_t gap = 2u;
    uint16_t dim = ch32h417_ltdc_rgb_pack_rgb565(18u, 18u, 22u);
    uint16_t good = ch32h417_ltdc_rgb_pack_rgb565(0u, 210u, 80u);
    uint16_t moved = ch32h417_ltdc_rgb_pack_rgb565(0u, 200u, 255u);
    uint16_t missing = ch32h417_ltdc_rgb_pack_rgb565(255u, 220u, 0u);

    for(row = 0u; row < 16u; row++)
    {
        uint16_t expected = (uint16_t)(1u << row);
        uint16_t actual = sdram_probe_write_read16(probe, expected);

        for(col = 0u; col < 16u; col++)
        {
            uint16_t mask = (uint16_t)(1u << col);
            uint16_t color = dim;

            if((actual & mask) != 0u)
            {
                color = (row == col) ? good : moved;
            }
            else if(row == col)
            {
                color = missing;
            }

            fb_fill_user_rect_rgb565((uint16_t)(x0 + (col * (cell + gap))),
                                     (uint16_t)(y0 + (row * (cell + gap))),
                                     cell,
                                     cell,
                                     color);
        }

        g_v5f_hw_test_diag.sdram_expected = expected;
        g_v5f_hw_test_diag.sdram_actual = actual;
    }
}

static void sdram_dq_probe_patterns_show(volatile uint16_t *probe, uint16_t x0, uint16_t y0)
{
    const uint16_t patterns[4] = {0x0000u, 0xFFFFu, 0xAAAAu, 0x5555u};
    uint16_t row;

    for(row = 0u; row < 4u; row++)
    {
        uint16_t expected = patterns[row];
        uint16_t actual = sdram_probe_write_read16(probe, expected);
        sdram_status_compare_bits_show(expected, actual, (uint16_t)(y0 + (row * 14u)));
        g_v5f_hw_test_diag.sdram_expected = expected;
        g_v5f_hw_test_diag.sdram_actual = actual;
        (void)x0;
    }
}

static void sdram_dq_probe_revision_marker_show(void)
{
    uint16_t magenta = ch32h417_ltdc_rgb_pack_rgb565(255u, 0u, 255u);
    uint16_t orange = ch32h417_ltdc_rgb_pack_rgb565(255u, 120u, 0u);
    uint16_t cyan = ch32h417_ltdc_rgb_pack_rgb565(0u, 200u, 255u);
    uint16_t green = ch32h417_ltdc_rgb_pack_rgb565(0u, 210u, 80u);

    fb_fill_user_rect_rgb565(8u, 8u, 28u, 10u, magenta);
    fb_fill_user_rect_rgb565(44u, 8u, 28u, 10u, orange);
    fb_fill_user_rect_rgb565(80u, 8u, 28u, 10u, cyan);
    fb_fill_user_rect_rgb565(116u, 8u, 28u, 10u, green);
}

static void sdram_phase_probe_apply(uint8_t phase)
{
    uint32_t misc = FMC_Bank5_6->MISC;

    misc &= ~FMC_MISC_Phase_Sel;
    misc |= (uint32_t)((phase & 0x0Fu) << 4);
    FMC_Bank5_6->MISC = misc;
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

static void sdram_read_pipe_probe_apply(uint8_t pipe)
{
    uint32_t sdcr = FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM];

    if(pipe > FMC_ReadPipeDelay_2HCLK)
    {
        pipe = FMC_ReadPipeDelay_2HCLK;
    }
    sdcr &= ~FMC_SDCR1_RPIPE;
    sdcr |= (uint32_t)((uint32_t)pipe << 13);
    FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM] = sdcr;
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

static uint8_t sdram_phase_probe_bit_score(uint16_t expected, uint16_t actual)
{
    uint16_t diff = (uint16_t)(expected ^ actual);
    uint8_t matching_bits = 16u;

    while(diff != 0u)
    {
        matching_bits--;
        diff = (uint16_t)(diff & (uint16_t)(diff - 1u));
    }

    return matching_bits;
}

static uint8_t sdram_phase_probe_score(volatile uint16_t *probe, uint8_t *bit_score)
{
    const uint16_t patterns[4] = {0x0000u, 0xFFFFu, 0xAAAAu, 0x5555u};
    uint8_t score = 0u;
    uint8_t matching_bits = 0u;
    uint8_t i;

    for(i = 0u; i < 4u; i++)
    {
        uint16_t expected = patterns[i];
        uint16_t actual = sdram_probe_write_read16(probe, expected);

        matching_bits += sdram_phase_probe_bit_score(expected, actual);
        if(actual == expected)
        {
            score++;
        }
    }

    *bit_score = matching_bits;
    return score;
}

static uint8_t sdram_phase_probe_show(volatile uint16_t *probe, uint16_t y)
{
    uint8_t phase;
    uint8_t pipe;
    uint8_t best_phase = V5F_SDRAM_DEFAULT_PHASE_SEL;
    uint8_t best_pipe = FMC_ReadPipeDelay_none;
    uint8_t best_score = 0u;
    uint8_t best_bit_score = 0u;
    uint16_t red = ch32h417_ltdc_rgb_pack_rgb565(255u, 0u, 0u);
    uint16_t yellow = ch32h417_ltdc_rgb_pack_rgb565(255u, 220u, 0u);
    uint16_t cyan = ch32h417_ltdc_rgb_pack_rgb565(0u, 200u, 255u);
    uint16_t green = ch32h417_ltdc_rgb_pack_rgb565(0u, 210u, 80u);
    uint16_t dim = ch32h417_ltdc_rgb_pack_rgb565(18u, 18u, 22u);

    for(pipe = 0u; pipe < 3u; pipe++)
    {
        sdram_read_pipe_probe_apply(pipe);
        for(phase = 0u; phase < 16u; phase++)
        {
            uint8_t score;
            uint8_t bit_score;
            uint16_t block_height;
            uint16_t color;

            sdram_phase_probe_apply(phase);
            score = sdram_phase_probe_score(probe, &bit_score);

            if((bit_score > best_bit_score) ||
               ((bit_score == best_bit_score) && (score > best_score)))
            {
                best_score = score;
                best_bit_score = bit_score;
                best_phase = phase;
                best_pipe = pipe;
            }

            if(score == 4u)
            {
                color = green;
            }
            else if(bit_score >= 48u)
            {
                color = cyan;
            }
            else if(bit_score >= 32u)
            {
                color = yellow;
            }
            else
            {
                color = red;
            }

            block_height = (uint16_t)((uint16_t)bit_score / 8u);
            if((block_height == 0u) && (bit_score != 0u))
            {
                block_height = 1u;
            }
            fb_fill_user_rect_rgb565((uint16_t)(8u + (phase * 18u)),
                                     (uint16_t)(y + (pipe * 14u) + (8u - block_height)),
                                     14u,
                                     block_height,
                                     color);
        }
    }

    sdram_read_pipe_probe_apply(best_pipe);
    sdram_phase_probe_apply(best_phase);
    s_sdram_debug_phase = best_phase;
    s_sdram_debug_pipe = best_pipe;
    s_sdram_debug_score = best_score;
    s_sdram_debug_bit_score = best_bit_score;
    fb_fill_user_rect_rgb565((uint16_t)(8u + (best_phase * 18u)),
                             (uint16_t)(y + (best_pipe * 14u) + 10u),
                             14u,
                             4u,
                             (best_bit_score != 0u) ? green : dim);
    g_v5f_hw_test_diag.sdram_expected = best_phase;
    g_v5f_hw_test_diag.sdram_actual = ((uint32_t)best_pipe << 8) | best_bit_score;

    return best_phase;
}

static void sdram_dqm_byte_probe_show(volatile uint16_t *probe, uint16_t y0)
{
    volatile uint8_t *probe_bytes = (volatile uint8_t *)probe;
    uint16_t actual;
    uint16_t white = ch32h417_ltdc_rgb_pack_rgb565(240u, 240u, 240u);

    fb_fill_user_rect_rgb565(16u, (uint16_t)(y0 - 12u), 224u, 4u, white);

    *probe = 0x0000u;
    ch32h417_ltdc_rgb_framebuffer_barrier();
    probe_bytes[0] = 0xFFu;
    ch32h417_ltdc_rgb_framebuffer_barrier();
    actual = *probe;
    sdram_status_compare_bits_show(0x00FFu, actual, y0);
    g_v5f_hw_test_diag.sdram_expected = 0x00FFu;
    g_v5f_hw_test_diag.sdram_actual = actual;

    *probe = 0x0000u;
    ch32h417_ltdc_rgb_framebuffer_barrier();
    probe_bytes[1] = 0xFFu;
    ch32h417_ltdc_rgb_framebuffer_barrier();
    actual = *probe;
    sdram_status_compare_bits_show(0xFF00u, actual, (uint16_t)(y0 + 14u));
    g_v5f_hw_test_diag.sdram_expected = 0xFF00u;
    g_v5f_hw_test_diag.sdram_actual = actual;

    *probe = 0xFFFFu;
    ch32h417_ltdc_rgb_framebuffer_barrier();
    probe_bytes[0] = 0x00u;
    ch32h417_ltdc_rgb_framebuffer_barrier();
    actual = *probe;
    sdram_status_compare_bits_show(0xFF00u, actual, (uint16_t)(y0 + 28u));
    g_v5f_hw_test_diag.sdram_expected = 0xFF00u;
    g_v5f_hw_test_diag.sdram_actual = actual;

    *probe = 0xFFFFu;
    ch32h417_ltdc_rgb_framebuffer_barrier();
    probe_bytes[1] = 0x00u;
    ch32h417_ltdc_rgb_framebuffer_barrier();
    actual = *probe;
    sdram_status_compare_bits_show(0x00FFu, actual, (uint16_t)(y0 + 42u));
    g_v5f_hw_test_diag.sdram_expected = 0x00FFu;
    g_v5f_hw_test_diag.sdram_actual = actual;
}

static void sdram_dq_probe_lower_show(volatile uint16_t *probe)
{
    uint16_t white = ch32h417_ltdc_rgb_pack_rgb565(240u, 240u, 240u);

    sdram_dqm_byte_probe_show(probe, 82u);
    fb_fill_user_rect_rgb565(16u, 150u, 224u, 8u, white);
    sdram_dq_probe_matrix_show(probe, 20u, 166u);
    sdram_dq_probe_patterns_show(probe, 0u, 400u);
    memory_barrier();
}

static void sdram_dq_probe_full_show(volatile uint16_t *probe)
{
    uint16_t bg = ch32h417_ltdc_rgb_pack_rgb565(2u, 4u, 7u);
    uint16_t red = ch32h417_ltdc_rgb_pack_rgb565(255u, 0u, 0u);

    fb_fill_rgb565(bg);
    fb_draw_border_rgb565(red);
    sdram_dq_probe_revision_marker_show();
    sdram_phase_probe_show(probe, 22u);
    sdram_dq_probe_lower_show(probe);
}

static void sdram_gpio_af(GPIO_TypeDef *port,
                          uint16_t pin,
                          uint8_t pin_source,
                          uint8_t alternate_function);

#if V5F_SDRAM_USB_DEBUG_ENABLED
static int sdram_usb_debug_write_full(const char *data, rt_size_t len)
{
    rt_size_t offset = 0u;
    uint8_t retries = 0u;

    if(data == RT_NULL)
    {
        return -1;
    }

    while(offset < len)
    {
        int wrote = ch32h417_usb_cdc_write(&data[offset], (rt_uint32_t)(len - offset));

        if(wrote > 0)
        {
            offset += (rt_size_t)wrote;
            retries = 0u;
            continue;
        }

        if((wrote == -2) || (retries >= 8u))
        {
            return (offset > 0u) ? (int)offset : wrote;
        }

        retries++;
        rt_thread_mdelay(1);
    }

    return (int)offset;
}

static void sdram_usb_debug_write_line(const char *line)
{
    if(line == RT_NULL)
    {
        return;
    }

    (void)sdram_usb_debug_write_full(line, (rt_size_t)strlen(line));
    (void)sdram_usb_debug_write_full("\r\n", 2u);
}

static const char *sdram_usb_debug_skip_arg_sep(const char *text)
{
    while((*text == ' ') || (*text == '\t') || (*text == '='))
    {
        text++;
    }

    return text;
}

static int sdram_usb_debug_parse_u8(const char *text, uint8_t max_value, uint8_t *value)
{
    uint32_t parsed = 0u;
    uint8_t digits = 0u;
    uint8_t base = 10u;

    if((text == RT_NULL) || (value == RT_NULL))
    {
        return -1;
    }

    text = sdram_usb_debug_skip_arg_sep(text);
    if((text[0] == '0') && ((text[1] == 'x') || (text[1] == 'X')))
    {
        base = 16u;
        text += 2;
    }

    while(*text != '\0')
    {
        uint8_t digit;

        if((*text >= '0') && (*text <= '9'))
        {
            digit = (uint8_t)(*text - '0');
        }
        else if((base == 16u) && (*text >= 'a') && (*text <= 'f'))
        {
            digit = (uint8_t)(10u + (uint8_t)(*text - 'a'));
        }
        else if((base == 16u) && (*text >= 'A') && (*text <= 'F'))
        {
            digit = (uint8_t)(10u + (uint8_t)(*text - 'A'));
        }
        else
        {
            break;
        }

        if(digit >= base)
        {
            return -1;
        }
        parsed = (parsed * base) + digit;
        digits++;
        text++;
    }

    while((*text == ' ') || (*text == '\t'))
    {
        text++;
    }

    if((digits == 0u) || (*text != '\0') || (parsed > max_value))
    {
        return -1;
    }

    *value = (uint8_t)parsed;
    return 0;
}

static int sdram_usb_debug_parse_u16(const char *text, uint16_t *value)
{
    uint32_t parsed = 0u;
    uint8_t digits = 0u;
    uint8_t base = 16u;

    if((text == RT_NULL) || (value == RT_NULL))
    {
        return -1;
    }

    text = sdram_usb_debug_skip_arg_sep(text);
    if((text[0] == '0') && ((text[1] == 'x') || (text[1] == 'X')))
    {
        text += 2;
    }

    while(*text != '\0')
    {
        uint8_t digit;

        if((*text >= '0') && (*text <= '9'))
        {
            digit = (uint8_t)(*text - '0');
        }
        else if((base == 16u) && (*text >= 'a') && (*text <= 'f'))
        {
            digit = (uint8_t)(10u + (uint8_t)(*text - 'a'));
        }
        else if((base == 16u) && (*text >= 'A') && (*text <= 'F'))
        {
            digit = (uint8_t)(10u + (uint8_t)(*text - 'A'));
        }
        else
        {
            break;
        }

        if(digit >= base)
        {
            return -1;
        }
        parsed = (parsed * base) + digit;
        digits++;
        text++;
    }

    while((*text == ' ') || (*text == '\t'))
    {
        text++;
    }

    if((digits == 0u) || (*text != '\0') || (parsed > 0xFFFFu))
    {
        return -1;
    }

    *value = (uint16_t)parsed;
    return 0;
}

static int sdram_usb_debug_command_is(const char *line, const char *command)
{
    rt_size_t len = (rt_size_t)strlen(command);

    if(strncmp(line, command, len) != 0)
    {
        return 0;
    }

    return (line[len] == '\0') || (line[len] == ' ') ||
           (line[len] == '\t') || (line[len] == '=');
}

static void sdram_usb_debug_help(void)
{
    sdram_usb_debug_write_line("SDRAM CDC commands: dump, scan, regs, rcc, pad, bias, wlow, hslv, uport, dq, addr, scope <hex16>, p <0-15>, r <0-2>");
}

static void sdram_usb_debug_report(volatile uint16_t *probe, const char *tag)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    uint16_t actual_0000;
    uint16_t actual_ffff;
    uint16_t actual_aaaa;
    uint16_t actual_5555;
    int used;

    actual_0000 = sdram_probe_write_read16(probe, 0x0000u);
    actual_ffff = sdram_probe_write_read16(probe, 0xFFFFu);
    actual_aaaa = sdram_probe_write_read16(probe, 0xAAAAu);
    actual_5555 = sdram_probe_write_read16(probe, 0x5555u);

    used = rt_snprintf(line,
                       sizeof(line),
                       "SDRAM %s frame=%u p=%u r=%u score=%u bits=%u err=%d exp=%08x act=%08x rd=%04x/%04x/%04x/%04x",
                       (tag != RT_NULL) ? tag : "stat",
                       (unsigned int)g_v5f_hw_test_diag.frame_count,
                       (unsigned int)s_sdram_debug_phase,
                       (unsigned int)s_sdram_debug_pipe,
                       (unsigned int)s_sdram_debug_score,
                       (unsigned int)s_sdram_debug_bit_score,
                       (int)g_v5f_hw_test_diag.last_error,
                       (unsigned int)g_v5f_hw_test_diag.sdram_expected,
                       (unsigned int)g_v5f_hw_test_diag.sdram_actual,
                       (unsigned int)actual_0000,
                       (unsigned int)actual_ffff,
                       (unsigned int)actual_aaaa,
                       (unsigned int)actual_5555);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static void sdram_usb_debug_regs(void)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    uint32_t pcfr1 = AFIO->PCFR1;
    uint32_t pd_aflr = AFIO->GPIOD_AFLR;
    uint32_t pd_cfglr = GPIOD->CFGLR;
    int used;

    used = rt_snprintf(line,
                       sizeof(line),
                       "SDRAM regs pcfr1=%08x pdaflr=%08x pdcfg=%08x pdin=%04x pdout=%04x",
                       (unsigned int)pcfr1,
                       (unsigned int)pd_aflr,
                       (unsigned int)pd_cfglr,
                       (unsigned int)(GPIOD->INDR & 0xFFFFu),
                       (unsigned int)(GPIOD->OUTDR & 0xFFFFu));
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }

    used = rt_snprintf(line,
                       sizeof(line),
                       "SDRAM regs pd0rm=%u pd0af=%u pd1af=%u pd0cfg=%x pd1cfg=%x",
                       (unsigned int)(pcfr1 & 0x1u),
                       (unsigned int)((pd_aflr >> 0) & 0xFu),
                       (unsigned int)((pd_aflr >> 4) & 0xFu),
                       (unsigned int)((pd_cfglr >> 0) & 0xFu),
                       (unsigned int)((pd_cfglr >> 4) & 0xFu));
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }

    used = rt_snprintf(line,
                       sizeof(line),
                       "SDRAM regs fmc sdcr=%08x sdtr=%08x sdrtr=%08x sdsr=%08x misc=%08x",
                       (unsigned int)FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM],
                       (unsigned int)FMC_Bank5_6->SDTR[FMC_Bank5_SDRAM],
                       (unsigned int)FMC_Bank5_6->SDRTR,
                       (unsigned int)FMC_Bank5_6->SDSR,
                       (unsigned int)FMC_Bank5_6->MISC);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static void sdram_usb_debug_rcc(void)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    uint32_t ctlr = RCC->CTLR;
    uint32_t cfgr0 = RCC->CFGR0;
    uint32_t pllcfgr = RCC->PLLCFGR;
    uint32_t pllcfgr2 = RCC->PLLCFGR2;
    uint32_t pwr_ctlr = PWR->CTLR;
    uint32_t pwr_csr = PWR->CSR;
    int used;

    used = rt_snprintf(line,
                       sizeof(line),
                       "SDRAM rcc ctlr=%08x cfgr0=%08x pll=%08x pll2=%08x hclk=%u sys=%u core=%u",
                       (unsigned int)ctlr,
                       (unsigned int)cfgr0,
                       (unsigned int)pllcfgr,
                       (unsigned int)pllcfgr2,
                       (unsigned int)HCLKClock,
                       (unsigned int)SystemClock,
                       (unsigned int)SystemCoreClock);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }

    used = rt_snprintf(line,
                       sizeof(line),
                       "SDRAM rcc hseon=%u hserdy=%u hsebyp=%u sws=%u pllsrc=%02x vioinit=%u pwrctl=%04x pwrcsr=%04x",
                       (unsigned int)((ctlr >> 16) & 0x1u),
                       (unsigned int)((ctlr >> 17) & 0x1u),
                       (unsigned int)((ctlr >> 18) & 0x1u),
                       (unsigned int)((cfgr0 >> 2) & 0x3u),
                       (unsigned int)(pllcfgr & 0xE0u),
                       (unsigned int)PWR_GetVIO18InitialStatus(),
                       (unsigned int)(pwr_ctlr & 0xFFFFu),
                       (unsigned int)(pwr_csr & 0xFFFFu));
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static uint16_t sdram_usb_debug_pd0_pd1_drive_read(uint16_t value)
{
    volatile uint32_t delay;
    uint32_t out = GPIOD->OUTDR;

    GPIOD->OUTDR = (out & ~0x3u) | (uint32_t)(value & 0x3u);
    memory_barrier();
    for(delay = 0u; delay < 64u; delay++)
    {
        __asm volatile ("nop");
    }
    return (uint16_t)(GPIOD->INDR & 0x3u);
}

static void sdram_usb_debug_restore_pd0_pd1(uint32_t pcfr1, uint32_t outdr)
{
    AFIO->PCFR1 = pcfr1;
    GPIOD->OUTDR = (GPIOD->OUTDR & ~0x3u) | (outdr & 0x3u);
    sdram_gpio_af(GPIOD, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF1);
    sdram_gpio_af(GPIOD, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF1);
}

static void sdram_usb_debug_pad(void)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    GPIO_InitTypeDef init = {0};
    uint32_t saved_pcfr1 = AFIO->PCFR1;
    uint32_t saved_aflr = AFIO->GPIOD_AFLR;
    uint32_t saved_cfglr = GPIOD->CFGLR;
    uint32_t saved_outdr = GPIOD->OUTDR;
    uint16_t idle_in = (uint16_t)(GPIOD->INDR & 0x3u);
    uint16_t low_in;
    uint16_t high_in;
    uint16_t restored_in;
    int used;

    init.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    init.GPIO_Mode = GPIO_Mode_Out_PP;
    init.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(GPIOD, &init);

    low_in = sdram_usb_debug_pd0_pd1_drive_read(0x0u);
    high_in = sdram_usb_debug_pd0_pd1_drive_read(0x3u);
    sdram_usb_debug_restore_pd0_pd1(saved_pcfr1, saved_outdr);
    restored_in = (uint16_t)(GPIOD->INDR & 0x3u);

    used = rt_snprintf(line,
                       sizeof(line),
                       "SDRAM pad save pcfr1=%08x pdaflr=%08x pdcfg=%08x out=%04x in=%u",
                       (unsigned int)saved_pcfr1,
                       (unsigned int)saved_aflr,
                       (unsigned int)saved_cfglr,
                       (unsigned int)(saved_outdr & 0xFFFFu),
                       (unsigned int)idle_in);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }

    used = rt_snprintf(line,
                       sizeof(line),
                       "SDRAM pad drive low=%u high=%u restored=%u pdaflr=%08x pdcfg=%08x",
                       (unsigned int)low_in,
                       (unsigned int)high_in,
                       (unsigned int)restored_in,
                       (unsigned int)AFIO->GPIOD_AFLR,
                       (unsigned int)GPIOD->CFGLR);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static uint16_t sdram_usb_debug_bias_read(volatile uint16_t *probe,
                                          GPIOMode_TypeDef mode,
                                          uint16_t *pin_in)
{
    GPIO_InitTypeDef init = {0};
    volatile uint32_t delay;

    init.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    init.GPIO_Mode = mode;
    init.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(GPIOD, &init);

    memory_barrier();
    for(delay = 0u; delay < 64u; delay++)
    {
        __asm volatile ("nop");
    }

    if(pin_in != RT_NULL)
    {
        *pin_in = (uint16_t)(GPIOD->INDR & 0x3u);
    }

    return *probe;
}

static void sdram_usb_debug_bias(volatile uint16_t *probe)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    uint32_t saved_pcfr1 = AFIO->PCFR1;
    uint32_t saved_outdr = GPIOD->OUTDR;
    uint16_t af_actual;
    uint16_t ipd_actual;
    uint16_t ipu_actual;
    uint16_t restored_actual;
    uint16_t af_in;
    uint16_t ipd_in = 0u;
    uint16_t ipu_in = 0u;
    uint16_t restored_in;
    int used;

    af_actual = sdram_probe_write_read16(probe, 0x0000u);
    af_in = (uint16_t)(GPIOD->INDR & 0x3u);
    ipd_actual = sdram_usb_debug_bias_read(probe, GPIO_Mode_IPD, &ipd_in);
    ipu_actual = sdram_usb_debug_bias_read(probe, GPIO_Mode_IPU, &ipu_in);
    sdram_usb_debug_restore_pd0_pd1(saved_pcfr1, saved_outdr);
    memory_barrier();
    restored_actual = *probe;
    restored_in = (uint16_t)(GPIOD->INDR & 0x3u);

    used = rt_snprintf(line,
                       sizeof(line),
                       "SDRAM bias exp=0000 af=%04x ipd=%04x ipu=%04x restored=%04x in=%u/%u/%u/%u",
                       (unsigned int)af_actual,
                       (unsigned int)ipd_actual,
                       (unsigned int)ipu_actual,
                       (unsigned int)restored_actual,
                       (unsigned int)af_in,
                       (unsigned int)ipd_in,
                       (unsigned int)ipu_in,
                       (unsigned int)restored_in);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static void sdram_usb_debug_wlow(volatile uint16_t *probe)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    GPIO_InitTypeDef init = {0};
    uint32_t saved_pcfr1 = AFIO->PCFR1;
    uint32_t saved_outdr = GPIOD->OUTDR;
    uint16_t normal_zero;
    uint16_t forced_zero;
    uint16_t low_in;
    uint16_t restored_in;
    int used;

    normal_zero = sdram_probe_write_read16(probe, 0x0000u);

    init.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    init.GPIO_Mode = GPIO_Mode_Out_PP;
    init.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(GPIOD, &init);
    low_in = sdram_usb_debug_pd0_pd1_drive_read(0x0u);

    *probe = 0x0000u;
    memory_barrier();

    sdram_usb_debug_restore_pd0_pd1(saved_pcfr1, saved_outdr);
    memory_barrier();
    forced_zero = *probe;
    restored_in = (uint16_t)(GPIOD->INDR & 0x3u);

    used = rt_snprintf(line,
                       sizeof(line),
                       "SDRAM wlow normal0=%04x forced0=%04x in=%u/%u",
                       (unsigned int)normal_zero,
                       (unsigned int)forced_zero,
                       (unsigned int)low_in,
                       (unsigned int)restored_in);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static void sdram_usb_debug_hslv(volatile uint16_t *probe)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    uint32_t before = AFIO->PCFR1;
    uint32_t fixed = before;
    uint16_t actual_0000;
    uint16_t actual_ffff;
    uint16_t actual_aaaa;
    uint16_t actual_5555;
    int used;

    fixed &= ~(AFIO_PCFR1_VIO18_IO_HSLV |
               AFIO_PCFR1_VIO33_IO_HSLV |
               AFIO_PCFR1_VDD33_IO_HSLV |
               0x00080000u);
    fixed |= (AFIO_PCFR1_VIO18_IO_HSLV |
              AFIO_PCFR1_VIO33_IO_HSLV |
              AFIO_PCFR1_VDD33_IO_HSLV);
    AFIO->PCFR1 = fixed;
    sdram_gpio_af(GPIOD, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF1);
    sdram_gpio_af(GPIOD, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF1);
    memory_barrier();

    actual_0000 = sdram_probe_write_read16(probe, 0x0000u);
    actual_ffff = sdram_probe_write_read16(probe, 0xFFFFu);
    actual_aaaa = sdram_probe_write_read16(probe, 0xAAAAu);
    actual_5555 = sdram_probe_write_read16(probe, 0x5555u);

    used = rt_snprintf(line,
                       sizeof(line),
                       "SDRAM hslv pcfr1=%08x->%08x rd=%04x/%04x/%04x/%04x",
                       (unsigned int)before,
                       (unsigned int)AFIO->PCFR1,
                       (unsigned int)actual_0000,
                       (unsigned int)actual_ffff,
                       (unsigned int)actual_aaaa,
                       (unsigned int)actual_5555);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static void sdram_usb_debug_uport(volatile uint16_t *probe)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    uint32_t saved_pcfr1 = AFIO->PCFR1;
    uint32_t base = saved_pcfr1 & ~AFIO_PCFR1_UHSIF_PORT_REMAP;
    uint8_t rm;

    for(rm = 0u; rm < 4u; rm++)
    {
        uint16_t actual_0000;
        uint16_t actual_ffff;
        uint16_t actual_aaaa;
        uint16_t actual_5555;
        int used;

        AFIO->PCFR1 = base | ((uint32_t)rm << 8);
        sdram_gpio_af(GPIOD, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF1);
        sdram_gpio_af(GPIOD, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF1);
        memory_barrier();

        actual_0000 = sdram_probe_write_read16(probe, 0x0000u);
        actual_ffff = sdram_probe_write_read16(probe, 0xFFFFu);
        actual_aaaa = sdram_probe_write_read16(probe, 0xAAAAu);
        actual_5555 = sdram_probe_write_read16(probe, 0x5555u);

        used = rt_snprintf(line,
                           sizeof(line),
                           "SDRAM uport rm=%u pcfr1=%08x rd=%04x/%04x/%04x/%04x",
                           (unsigned int)rm,
                           (unsigned int)AFIO->PCFR1,
                           (unsigned int)actual_0000,
                           (unsigned int)actual_ffff,
                           (unsigned int)actual_aaaa,
                           (unsigned int)actual_5555);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    AFIO->PCFR1 = saved_pcfr1;
    sdram_gpio_af(GPIOD, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF1);
    sdram_gpio_af(GPIOD, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF1);
}

static void sdram_usb_debug_dq(volatile uint16_t *probe)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    uint8_t bit;

    for(bit = 0u; bit < 16u; bit++)
    {
        uint16_t expected = (uint16_t)(1u << bit);
        uint16_t actual = sdram_probe_write_read16(probe, expected);
        int used = rt_snprintf(line,
                               sizeof(line),
                               "SDRAM dq bit=%u exp=%04x act=%04x xor=%04x",
                               (unsigned int)bit,
                               (unsigned int)expected,
                               (unsigned int)actual,
                               (unsigned int)(expected ^ actual));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
}

static void sdram_usb_debug_addr(void)
{
    static const uint32_t addr_offsets[] = {
        0x00000000u,
        0x00000002u,
        0x00000400u,
        0x00002000u,
        0x00010000u,
        0x00100000u,
        0x01000000u,
        0x01ff0000u,
    };
    char line[V5F_SDRAM_USB_LINE_BYTES];
    uint8_t i;

    for(i = 0u; i < (uint8_t)(sizeof(addr_offsets) / sizeof(addr_offsets[0])); i++)
    {
        uint32_t offset = addr_offsets[i];
        volatile uint16_t *probe = (volatile uint16_t *)(V5F_SDRAM_BASE_ADDR + offset);
        uint16_t actual_0000 = sdram_probe_write_read16(probe, 0x0000u);
        uint16_t actual_ffff = sdram_probe_write_read16(probe, 0xFFFFu);
        uint16_t actual_aaaa = sdram_probe_write_read16(probe, 0xAAAAu);
        uint16_t actual_5555 = sdram_probe_write_read16(probe, 0x5555u);
        int used = rt_snprintf(line,
                               sizeof(line),
                               "SDRAM addr off=%08x rd=%04x/%04x/%04x/%04x",
                               (unsigned int)offset,
                               (unsigned int)actual_0000,
                               (unsigned int)actual_ffff,
                               (unsigned int)actual_aaaa,
                               (unsigned int)actual_5555);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
}

static uint32_t sdram_scope_cycle_count(volatile uint16_t *probe,
                                        uint16_t expected,
                                        uint16_t *actual)
{
    uint32_t i;
    uint16_t last = 0u;

    *probe = expected;
    ch32h417_ltdc_rgb_framebuffer_barrier();

    for(i = 0u; i < V5F_SDRAM_SCOPE_CYCLES; i++)
    {
        last = *probe;
        ch32h417_ltdc_rgb_framebuffer_barrier();
    }

    if(actual != RT_NULL)
    {
        *actual = last;
    }

    return i;
}

static void sdram_usb_debug_scope(volatile uint16_t *probe, uint16_t expected)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    uint16_t actual = 0u;
    uint32_t cycles = sdram_scope_cycle_count(probe, expected, &actual);
    int used;

    g_v5f_hw_test_diag.sdram_expected = expected;
    g_v5f_hw_test_diag.sdram_actual = actual;
    used = rt_snprintf(line,
                       sizeof(line),
                       "SDRAM scope exp=%04x act=%04x xor=%04x cycles=%u",
                       (unsigned int)expected,
                       (unsigned int)actual,
                       (unsigned int)(expected ^ actual),
                       (unsigned int)cycles);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static void sdram_usb_debug_measure_current(volatile uint16_t *probe)
{
    s_sdram_debug_score =
        sdram_phase_probe_score(probe, &s_sdram_debug_bit_score);
    g_v5f_hw_test_diag.sdram_expected = s_sdram_debug_phase;
    g_v5f_hw_test_diag.sdram_actual =
        ((uint32_t)s_sdram_debug_pipe << 8) | s_sdram_debug_bit_score;
}

static void sdram_usb_debug_handle_command(volatile uint16_t *probe, const char *line)
{
    uint8_t value = 0u;
    uint16_t pattern = 0u;

    if((line == RT_NULL) || (line[0] == '\0'))
    {
        return;
    }

    if(sdram_usb_debug_command_is(line, "help") ||
       sdram_usb_debug_command_is(line, "?"))
    {
        sdram_usb_debug_help();
        return;
    }

    if(sdram_usb_debug_command_is(line, "dump"))
    {
        sdram_usb_debug_report(probe, "dump");
        return;
    }

    if(sdram_usb_debug_command_is(line, "regs"))
    {
        sdram_usb_debug_regs();
        return;
    }

    if(sdram_usb_debug_command_is(line, "rcc"))
    {
        sdram_usb_debug_rcc();
        return;
    }

    if(sdram_usb_debug_command_is(line, "pad"))
    {
        sdram_usb_debug_pad();
        return;
    }

    if(sdram_usb_debug_command_is(line, "bias"))
    {
        sdram_usb_debug_bias(probe);
        return;
    }

    if(sdram_usb_debug_command_is(line, "wlow"))
    {
        sdram_usb_debug_wlow(probe);
        return;
    }

    if(sdram_usb_debug_command_is(line, "hslv"))
    {
        sdram_usb_debug_hslv(probe);
        return;
    }

    if(sdram_usb_debug_command_is(line, "uport"))
    {
        sdram_usb_debug_uport(probe);
        return;
    }

    if(sdram_usb_debug_command_is(line, "dq"))
    {
        sdram_usb_debug_dq(probe);
        return;
    }

    if(sdram_usb_debug_command_is(line, "addr"))
    {
        sdram_usb_debug_addr();
        return;
    }

    if(sdram_usb_debug_command_is(line, "scope"))
    {
        if(sdram_usb_debug_parse_u16(&line[5], &pattern) == 0)
        {
            sdram_usb_debug_scope(probe, pattern);
        }
        else
        {
            sdram_usb_debug_write_line("ERR scope pattern is 0x0000..0xffff");
        }
        return;
    }

    if(sdram_usb_debug_command_is(line, "scan"))
    {
        sdram_dq_probe_full_show(probe);
        sdram_usb_debug_report(probe, "scan");
        return;
    }

    if(sdram_usb_debug_command_is(line, "p"))
    {
        if(sdram_usb_debug_parse_u8(&line[1], 15u, &value) == 0)
        {
            s_sdram_debug_phase = value;
            sdram_phase_probe_apply(value);
            sdram_usb_debug_measure_current(probe);
            sdram_dq_probe_lower_show(probe);
            sdram_usb_debug_report(probe, "phase");
        }
        else
        {
            sdram_usb_debug_write_line("ERR p range is 0..15");
        }
        return;
    }

    if(sdram_usb_debug_command_is(line, "r"))
    {
        if(sdram_usb_debug_parse_u8(&line[1], 2u, &value) == 0)
        {
            s_sdram_debug_pipe = value;
            sdram_read_pipe_probe_apply(value);
            sdram_usb_debug_measure_current(probe);
            sdram_dq_probe_lower_show(probe);
            sdram_usb_debug_report(probe, "pipe");
        }
        else
        {
            sdram_usb_debug_write_line("ERR r range is 0..2");
        }
        return;
    }

    sdram_usb_debug_write_line("ERR unknown command");
    sdram_usb_debug_help();
}

static void sdram_usb_debug_init(volatile uint16_t *probe)
{
    static uint8_t initialized;

    if(initialized == 0u)
    {
        initialized = 1u;
        (void)ch32h417_dual_cdc_init();
    }

    sdram_usb_debug_help();
    sdram_usb_debug_report(probe, "boot");
}

static void sdram_usb_debug_poll(volatile uint16_t *probe)
{
    char line[64];
    int len;

    ch32h417_dual_cdc_poll();
    do
    {
        len = ch32h417_usb_cdc_read_line(line, sizeof(line));
        if(len > 0)
        {
            sdram_usb_debug_handle_command(probe, line);
        }
    } while(len > 0);
}
#endif

static void sdram_enable_0x60000000_remap(void)
{
    FMC_Bank1->BTCR[0] |= V5F_FMC_SDRAM_REMAP_TO_0X60000000;
}

static void sdram_status_fail_show(void)
{
    uint16_t i;
    uint16_t stage = (uint16_t)g_v5f_hw_test_diag.sdram_stage;
    uint16_t error_count = sdram_status_error_count();
    uint16_t stage_slot_width = (uint16_t)(V5F_RGB_FB_WIDTH / 7u);
    uint16_t error_slot_width = (uint16_t)(V5F_RGB_FB_WIDTH / 6u);
    uint16_t red = ch32h417_ltdc_rgb_pack_rgb565(255u, 0u, 0u);
    uint16_t yellow = ch32h417_ltdc_rgb_pack_rgb565(255u, 220u, 0u);
    uint16_t dim = ch32h417_ltdc_rgb_pack_rgb565(16u, 16u, 20u);
    uint16_t bg = ch32h417_ltdc_rgb_pack_rgb565(2u, 4u, 7u);

    if(stage > 7u)
    {
        stage = 7u;
    }
    if(error_count > 6u)
    {
        error_count = 6u;
    }

    fb_fill_rgb565(bg);
    fb_draw_border_rgb565(red);

    for(i = 0u; i < 7u; i++)
    {
        uint16_t x = (uint16_t)(4u + (i * stage_slot_width));
        uint16_t w = (stage_slot_width > 8u) ? (uint16_t)(stage_slot_width - 8u) : 1u;
        fb_fill_user_rect_rgb565(x,
                                 8u,
                                 w,
                                 16u,
                                 (i < stage) ? red : dim);
    }

    fb_fill_user_rect_rgb565((uint16_t)(V5F_RGB_FB_WIDTH / 6u),
                             (uint16_t)(V5F_RGB_FB_HEIGHT / 3u),
                             (uint16_t)((V5F_RGB_FB_WIDTH * 2u) / 3u),
                             (uint16_t)(V5F_RGB_FB_HEIGHT / 3u),
                             red);
    fb_fill_user_rect_rgb565(0u, 30u, V5F_RGB_FB_WIDTH, 102u, bg);
    sdram_status_word_bits_show(g_v5f_hw_test_diag.sdram_expected,
                                36u,
                                ch32h417_ltdc_rgb_pack_rgb565(0u, 220u, 80u));
    sdram_status_word_bits_show(g_v5f_hw_test_diag.sdram_actual,
                                50u,
                                ch32h417_ltdc_rgb_pack_rgb565(0u, 200u, 255u));
    sdram_status_word_bits_show(g_v5f_hw_test_diag.sdram_expected ^
                                    g_v5f_hw_test_diag.sdram_actual,
                                64u,
                                ch32h417_ltdc_rgb_pack_rgb565(255u, 220u, 0u));

    if(stage == V5F_SDRAM_STAGE_DATA_BUS)
    {
        sdram_probe_data_bus_show();
    }

    for(i = 0u; i < 6u; i++)
    {
        uint16_t x = (uint16_t)(4u + (i * error_slot_width));
        uint16_t w = (error_slot_width > 8u) ? (uint16_t)(error_slot_width - 8u) : 1u;
        fb_fill_user_rect_rgb565(x,
                                 (uint16_t)(V5F_RGB_FB_HEIGHT - 22u),
                                 w,
                                 14u,
                                 (i < error_count) ? yellow : dim);
    }

    memory_barrier();
}

static int sdram_status_lcd_start(void)
{
    int result = lcd_start_rgb565_window();

    if(result == CH32H417_LTDC_RGB_OK)
    {
        sdram_status_show(V5F_SDRAM_STATUS_BOOT,
                          ch32h417_ltdc_rgb_pack_rgb565(0u, 80u, 255u));
    }

    return result;
}

static void sdram_diag_clear(void)
{
    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_NONE;
    g_v5f_hw_test_diag.sdram_test_bytes = 0u;
    g_v5f_hw_test_diag.sdram_fail_offset = 0u;
    g_v5f_hw_test_diag.sdram_expected = 0u;
    g_v5f_hw_test_diag.sdram_actual = 0u;
}

static void sdram_diag_fail(int error, const v5f_sdram_memtest_result_t *result)
{
    g_v5f_hw_test_diag.last_error = error;
    g_v5f_hw_test_diag.sdram_fail_count++;
    if(result != 0)
    {
        g_v5f_hw_test_diag.sdram_stage = result->stage;
        g_v5f_hw_test_diag.sdram_fail_offset = result->offset;
        g_v5f_hw_test_diag.sdram_expected = result->expected;
        g_v5f_hw_test_diag.sdram_actual = result->actual;
    }
    sdram_status_fail_show();
}

static void sdram_delay_us(uint32_t us)
{
    uint32_t cycles;

    if((us == 0u) || (SystemCoreClock == 0u))
    {
        return;
    }

    cycles = (uint32_t)((((uint64_t)SystemCoreClock * us) + 999999u) / 1000000u);
    while(cycles != 0u)
    {
        __asm volatile ("nop");
        cycles--;
    }
}

static int sdram_wait_ready(void)
{
    uint32_t timeout = V5F_SDRAM_TIMEOUT_POLLS;

    while(timeout != 0u)
    {
        if((FMC_Bank5_6->SDSR & FMC_SDSR_BUSY) == 0u)
        {
            return V5F_SDRAM_OK;
        }
        timeout--;
    }
    return V5F_SDRAM_ERR_TIMEOUT;
}

static int sdram_send_command(uint32_t command, uint32_t refresh_count, uint32_t mode_register)
{
    int result = sdram_wait_ready();
    if(result != V5F_SDRAM_OK)
    {
        return result;
    }

    FMC_SDRAM_SendCMDConfig(FMC_SDRAM_SEL_Bank5,
                            command,
                            refresh_count,
                            mode_register);
    return sdram_wait_ready();
}

static uint32_t sdram_select_clock_period(uint32_t hclk_hz, uint32_t *sdclk_hz)
{
    if(hclk_hz == 0u)
    {
        hclk_hz = V5F_SDRAM_MAX_SDCLK_HZ;
    }

    if((hclk_hz / 2u) <= V5F_SDRAM_MAX_SDCLK_HZ)
    {
        *sdclk_hz = hclk_hz / 2u;
        return FMC_SDClockPeriod_2HCLK;
    }
    if((hclk_hz / 3u) <= V5F_SDRAM_MAX_SDCLK_HZ)
    {
        *sdclk_hz = hclk_hz / 3u;
        return FMC_SDClockPeriod_3HCLK;
    }

    *sdclk_hz = 0u;
    return 0u;
}

static uint16_t sdram_refresh_count(uint32_t sdclk_hz)
{
    uint32_t cycles =
        (uint32_t)((((uint64_t)sdclk_hz * V5F_SDRAM_REFRESH_PERIOD_US) /
                    V5F_SDRAM_REFRESH_CYCLES) /
                   1000000u);

    if(cycles > V5F_SDRAM_REFRESH_MARGIN)
    {
        cycles -= V5F_SDRAM_REFRESH_MARGIN;
    }
    if(cycles > 0x1FFFu)
    {
        cycles = 0x1FFFu;
    }
    return (uint16_t)cycles;
}

static void sdram_gpio_af(GPIO_TypeDef *port,
                          uint16_t pin,
                          uint8_t pin_source,
                          uint8_t alternate_function)
{
    GPIO_InitTypeDef init = {0};

    GPIO_PinAFConfig(port, pin_source, alternate_function);
    init.GPIO_Pin = pin;
    init.GPIO_Mode = GPIO_Mode_AF_PP;
    init.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(port, &init);
}

static void sdram_gpio_init(void)
{
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                          RCC_HB2Periph_GPIOA |
                          RCC_HB2Periph_GPIOB |
                          RCC_HB2Periph_GPIOC |
                          RCC_HB2Periph_GPIOD |
                          RCC_HB2Periph_GPIOE |
                          RCC_HB2Periph_GPIOF,
                          ENABLE);

    GPIO_PinRemapConfig(GPIO_Remap_VIO1V8_IO_HSLV, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_VIO3V3_IO_HSLV, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_VDD3V3_IO_HSLV, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_PD0PD1, ENABLE);

    sdram_gpio_af(GPIOE, GPIO_Pin_2, GPIO_PinSource2, GPIO_AF9);
    sdram_gpio_af(GPIOC, GPIO_Pin_3, GPIO_PinSource3, GPIO_AF12);
    sdram_gpio_af(GPIOD, GPIO_Pin_6, GPIO_PinSource6, GPIO_AF3);
    sdram_gpio_af(GPIOF, GPIO_Pin_3, GPIO_PinSource3, GPIO_AF3);
    sdram_gpio_af(GPIOF, GPIO_Pin_12, GPIO_PinSource12, GPIO_AF12);
    sdram_gpio_af(GPIOA, GPIO_Pin_7, GPIO_PinSource7, GPIO_AF12);
    sdram_gpio_af(GPIOC, GPIO_Pin_2, GPIO_PinSource2, GPIO_AF15);
    sdram_gpio_af(GPIOE, GPIO_Pin_3, GPIO_PinSource3, GPIO_AF1);
    sdram_gpio_af(GPIOB, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF7);
    sdram_gpio_af(GPIOB, GPIO_Pin_15, GPIO_PinSource15, GPIO_AF12);
    sdram_gpio_af(GPIOF, GPIO_Pin_5, GPIO_PinSource5, GPIO_AF12);
    sdram_gpio_af(GPIOA, GPIO_Pin_15, GPIO_PinSource15, GPIO_AF12);
    sdram_gpio_af(GPIOE, GPIO_Pin_13, GPIO_PinSource13, GPIO_AF15);
    sdram_gpio_af(GPIOE, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF0);
    sdram_gpio_af(GPIOE, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF0);
    sdram_gpio_af(GPIOB, GPIO_Pin_6, GPIO_PinSource6, GPIO_AF11);
    sdram_gpio_af(GPIOB, GPIO_Pin_11, GPIO_PinSource11, GPIO_AF0);
    sdram_gpio_af(GPIOB, GPIO_Pin_12, GPIO_PinSource12, GPIO_AF0);
    sdram_gpio_af(GPIOB, GPIO_Pin_13, GPIO_PinSource13, GPIO_AF0);
    sdram_gpio_af(GPIOB, GPIO_Pin_14, GPIO_PinSource14, GPIO_AF0);
    sdram_gpio_af(GPIOB, GPIO_Pin_10, GPIO_PinSource10, GPIO_AF12);
    sdram_gpio_af(GPIOD, GPIO_Pin_11, GPIO_PinSource11, GPIO_AF0);
    sdram_gpio_af(GPIOD, GPIO_Pin_12, GPIO_PinSource12, GPIO_AF0);
    sdram_gpio_af(GPIOD, GPIO_Pin_13, GPIO_PinSource13, GPIO_AF0);
    sdram_gpio_af(GPIOD, GPIO_Pin_14, GPIO_PinSource14, GPIO_AF0);
    sdram_gpio_af(GPIOD, GPIO_Pin_15, GPIO_PinSource15, GPIO_AF0);
    sdram_gpio_af(GPIOF, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF2);
    sdram_gpio_af(GPIOE, GPIO_Pin_7, GPIO_PinSource7, GPIO_AF12);
    sdram_gpio_af(GPIOE, GPIO_Pin_8, GPIO_PinSource8, GPIO_AF12);
    sdram_gpio_af(GPIOE, GPIO_Pin_9, GPIO_PinSource9, GPIO_AF12);
    sdram_gpio_af(GPIOE, GPIO_Pin_10, GPIO_PinSource10, GPIO_AF12);
    sdram_gpio_af(GPIOE, GPIO_Pin_11, GPIO_PinSource11, GPIO_AF12);
    sdram_gpio_af(GPIOC, GPIO_Pin_9, GPIO_PinSource9, GPIO_AF0);
    sdram_gpio_af(GPIOD, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF1);
    sdram_gpio_af(GPIOD, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF1);
    sdram_gpio_af(GPIOE, GPIO_Pin_15, GPIO_PinSource15, GPIO_AF12);
    sdram_gpio_af(GPIOD, GPIO_Pin_8, GPIO_PinSource8, GPIO_AF12);
    sdram_gpio_af(GPIOA, GPIO_Pin_13, GPIO_PinSource13, GPIO_AF0);
    sdram_gpio_af(GPIOD, GPIO_Pin_10, GPIO_PinSource10, GPIO_AF12);
}

static int sdram_init(void)
{
    FMC_SDRAM_InitTypeDef init = {0};
    FMC_SDRAM_TimingTypeDef timing = {0};
    uint32_t sdclock_period;
    uint32_t sdclk_hz = 0u;
    uint16_t refresh_count;
    int result;

    sdram_diag_clear();
    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_INIT;
    g_v5f_hw_test_diag.sdram_hclk_hz = HCLKClock;
    sdram_status_show(V5F_SDRAM_STATUS_INIT,
                      ch32h417_ltdc_rgb_pack_rgb565(0u, 170u, 220u));

    sdclock_period = sdram_select_clock_period(HCLKClock, &sdclk_hz);
    if(sdclock_period == 0u)
    {
        return V5F_SDRAM_ERR_CLOCK;
    }

    refresh_count = sdram_refresh_count(sdclk_hz);
    g_v5f_hw_test_diag.sdram_sdclk_hz = sdclk_hz;
    g_v5f_hw_test_diag.sdram_refresh_count = refresh_count;

    RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE);
    PWR_VIO18ModeCfg(PWR_VIO18CFGMODE_SW);
    PWR_VIO18LevelCfg(PWR_VIO18Level_MODE3);
    sdram_delay_us(1000u);

    RCC_HBPeriphClockCmd(RCC_HBPeriph_FMC, ENABLE);
    sdram_gpio_init();

    init.FMC_Bank = FMC_Bank5_SDRAM;
    init.FMC_ColumnBitsNumber = FMC_ColumnBitsNumber_9;
    init.FMC_RowBitsNumber = FMC_ROWBitsNumber_13;
    init.FMC_MemoryDataWidth = FMC_MemoryDataWidth_16;
    init.FMC_InternalBankNumber = FMC_InternalBankNumber_4;
    init.FMC_CASLatency = FMC_CASLatency_3CLk;
    init.FMC_WriteProtection = FMC_WriteProtection_Disable;
    init.FMC_SDClockPeriod = sdclock_period;
    init.FMC_ReadBurst = FMC_ReadBurst_Disable;
    init.FMC_ReadPipeDelay = FMC_ReadPipeDelay_none;
    init.FMC_PHASE_SEL = V5F_SDRAM_DEFAULT_PHASE_SEL;
    init.FMC_ENHANCE_READ_MODE = FMC_ENHANCE_READ_MODE_Disable;

    timing.FMC_LoadToActiveDelay = 2u;
    timing.FMC_ExitSelfRefreshDelay = 8u;
    timing.FMC_SelfRefreshTime = 5u;
    timing.FMC_RowCycleDelay = 6u;
    timing.FMC_WriteRecoveryTime = 2u;
    timing.FMC_RPDelay = 2u;
    timing.FMC_RCDDelay = 2u;
    init.FMC_SDRAM_Timing = &timing;

    FMC_SDRAM_Init(&init);
    FMC_Bank1->BTCR[0] |= FMC_BCR1_FMCEN;
    FMC_SDRAMCmd(FMC_Bank5_SDRAM, ENABLE);

    result = sdram_send_command(FMC_SDRAM_CMD_Mode1, 1u, 0u);
    if(result != V5F_SDRAM_OK)
    {
        return result;
    }
    sdram_delay_us(200u);
    result = sdram_send_command(FMC_SDRAM_CMD_Mode2, 1u, 0u);
    if(result != V5F_SDRAM_OK)
    {
        return result;
    }
    result = sdram_send_command(FMC_SDRAM_CMD_Mode3, 8u, 0u);
    if(result != V5F_SDRAM_OK)
    {
        return result;
    }
    result = sdram_send_command(FMC_SDRAM_CMD_Mode4, 1u, V5F_SDRAM_MODE_REGISTER);
    if(result != V5F_SDRAM_OK)
    {
        return result;
    }

    FMC_SDRAM_SetRefreshCnt(refresh_count);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    return V5F_SDRAM_OK;
}

static uint32_t sdram_pattern(uint32_t index)
{
    return 0xA5A50000u ^ (index * 2654435761u);
}

static int sdram_fail_result(v5f_sdram_memtest_result_t *result,
                             uint32_t stage,
                             uint32_t offset,
                             uint32_t expected,
                             uint32_t actual)
{
    if(result != 0)
    {
        result->stage = stage;
        result->offset = offset;
        result->expected = expected;
        result->actual = actual;
    }
    return V5F_SDRAM_ERR_VERIFY;
}

static int sdram_memtest_range(uint32_t offset,
                               uint32_t bytes,
                               v5f_sdram_memtest_result_t *result)
{
    volatile uint32_t *base;
    uint32_t words;
    uint32_t bit;
    uint32_t word;
    uint32_t address_word;

    if((bytes == 0u) || ((offset & 0x3u) != 0u) ||
       ((offset + bytes) > V5F_SDRAM_BYTES))
    {
        return V5F_SDRAM_ERR_PARAM;
    }

    bytes &= ~0x3u;
    words = bytes / 4u;
    base = (volatile uint32_t *)(V5F_SDRAM_BASE_ADDR + offset);
    g_v5f_hw_test_diag.sdram_test_bytes = bytes;

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_DATA_BUS;
    sdram_status_show(V5F_SDRAM_STATUS_DATA_BUS,
                      ch32h417_ltdc_rgb_pack_rgb565(255u, 220u, 0u));
    for(bit = 0u; bit < 32u; bit++)
    {
        uint32_t expected = 1u << bit;
        base[0] = expected;
        ch32h417_ltdc_rgb_framebuffer_barrier();
        if(base[0] != expected)
        {
            return sdram_fail_result(result, V5F_SDRAM_STAGE_DATA_BUS, offset, expected, base[0]);
        }
        expected = ~expected;
        base[0] = expected;
        ch32h417_ltdc_rgb_framebuffer_barrier();
        if(base[0] != expected)
        {
            return sdram_fail_result(result, V5F_SDRAM_STAGE_DATA_BUS, offset, expected, base[0]);
        }
    }

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_ADDRESS_BUS;
    sdram_status_show(V5F_SDRAM_STATUS_ADDRESS_BUS,
                      ch32h417_ltdc_rgb_pack_rgb565(220u, 0u, 255u));
    base[0] = 0xAAAAAAAAu;
    for(address_word = 1u; address_word < words; address_word <<= 1)
    {
        base[address_word] = 0x5A5A0000u ^ address_word;
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
    if(base[0] != 0xAAAAAAAAu)
    {
        return sdram_fail_result(result, V5F_SDRAM_STAGE_ADDRESS_BUS, offset, 0xAAAAAAAAu, base[0]);
    }
    for(address_word = 1u; address_word < words; address_word <<= 1)
    {
        uint32_t expected = 0x5A5A0000u ^ address_word;
        if(base[address_word] != expected)
        {
            return sdram_fail_result(result,
                                     V5F_SDRAM_STAGE_ADDRESS_BUS,
                                     offset + (address_word * 4u),
                                     expected,
                                     base[address_word]);
        }
    }

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_PATTERN;
    sdram_status_show(V5F_SDRAM_STATUS_PATTERN,
                      ch32h417_ltdc_rgb_pack_rgb565(255u, 96u, 0u));
    for(word = 0u; word < words; word++)
    {
        base[word] = sdram_pattern(word + (offset / 4u));
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
    for(word = 0u; word < words; word++)
    {
        uint32_t expected = sdram_pattern(word + (offset / 4u));
        if(base[word] != expected)
        {
            return sdram_fail_result(result,
                                     V5F_SDRAM_STAGE_PATTERN,
                                     offset + (word * 4u),
                                     expected,
                                     base[word]);
        }
    }

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_PATTERN_INV;
    sdram_status_show(V5F_SDRAM_STATUS_PATTERN,
                      ch32h417_ltdc_rgb_pack_rgb565(255u, 140u, 0u));
    for(word = 0u; word < words; word++)
    {
        base[word] = ~sdram_pattern(word + (offset / 4u));
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
    for(word = 0u; word < words; word++)
    {
        uint32_t expected = ~sdram_pattern(word + (offset / 4u));
        if(base[word] != expected)
        {
            return sdram_fail_result(result,
                                     V5F_SDRAM_STAGE_PATTERN_INV,
                                     offset + (word * 4u),
                                     expected,
                                     base[word]);
        }
    }

    return V5F_SDRAM_OK;
}

static uint16_t *sdram_rgb565_fb(void)
{
    return (uint16_t *)V5F_SDRAM_BASE_ADDR;
}

static void sdram_fill_ltdc_pattern(void)
{
    uint16_t *fb = sdram_rgb565_fb();
    uint16_t x;
    uint16_t y;

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_LTDC_FILL;
    sdram_status_show(V5F_SDRAM_STATUS_LTDC_FILL,
                      ch32h417_ltdc_rgb_pack_rgb565(0u, 210u, 120u));
    for(y = 0u; y < V5F_SDRAM_LTDC_HEIGHT; y++)
    {
        for(x = 0u; x < V5F_SDRAM_LTDC_WIDTH; x++)
        {
            uint8_t red = (uint8_t)(((uint32_t)x * 255u) /
                                    (V5F_SDRAM_LTDC_WIDTH - 1u));
            uint8_t green = (uint8_t)(((uint32_t)y * 255u) /
                                      (V5F_SDRAM_LTDC_HEIGHT - 1u));
            uint8_t blue = (uint8_t)((((uint32_t)x + y) * 255u) /
                                     (V5F_SDRAM_LTDC_WIDTH + V5F_SDRAM_LTDC_HEIGHT - 2u));
            fb[((uint32_t)(V5F_SDRAM_LTDC_HEIGHT - 1u - y) * V5F_SDRAM_LTDC_WIDTH) +
               (V5F_SDRAM_LTDC_WIDTH - 1u - x)] =
                ch32h417_ltdc_rgb_pack_rgb565(red, green, blue);
        }
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_TICK_DIAG
static uint32_t v5f_cycle_now(void)
{
    uint32_t value;

    __asm volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
}

static void fb_draw_tick_diag_half(uint8_t rt_side, uint8_t state)
{
    uint16_t x = (rt_side != 0u) ? 0u : (V5F_RGB_FB_WIDTH / 2u);
    uint16_t width = V5F_RGB_FB_WIDTH / 2u;
    uint16_t color;

    if(rt_side != 0u)
    {
        color = (state != 0u) ? ch32h417_ltdc_rgb_pack_rgb565(255u, 0u, 0u) : ch32h417_ltdc_rgb_pack_rgb565(255u, 255u, 0u);
    }
    else
    {
        color = (state != 0u) ? ch32h417_ltdc_rgb_pack_rgb565(0u, 0u, 255u) : ch32h417_ltdc_rgb_pack_rgb565(0u, 255u, 0u);
    }

    fb_fill_user_rect_rgb565(x, 0u, width, V5F_RGB_FB_HEIGHT, color);
    fb_fill_user_rect_rgb565((V5F_RGB_FB_WIDTH / 2u) - 1u,
                             0u,
                             2u,
                             V5F_RGB_FB_HEIGHT,
                             ch32h417_ltdc_rgb_pack_rgb565(255u, 255u, 255u));
}

static void run_tick_diag_test(void)
{
    rt_tick_t last_rt_tick = rt_tick_get();
    uint32_t last_cycle = v5f_cycle_now();
    uint32_t cycle_interval = (SystemCoreClock != 0u) ? SystemCoreClock : 400000000u;
    uint8_t rt_state = 0u;
    uint8_t cycle_state = 0u;

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    fb_draw_tick_diag_half(1u, rt_state);
    fb_draw_tick_diag_half(0u, cycle_state);
    fb_draw_border_rgb565(ch32h417_ltdc_rgb_pack_rgb565(255u, 255u, 255u));

    while(1)
    {
        if(rt_tick_get_delta(last_rt_tick) >= RT_TICK_PER_SECOND)
        {
            last_rt_tick += RT_TICK_PER_SECOND;
            rt_state ^= 1u;
            fb_draw_tick_diag_half(1u, rt_state);
        }

        if((uint32_t)(v5f_cycle_now() - last_cycle) >= cycle_interval)
        {
            last_cycle += cycle_interval;
            cycle_state ^= 1u;
            fb_draw_tick_diag_half(0u, cycle_state);
        }

        g_v5f_hw_test_diag.frame_count++;
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_RGB565_DIAG
static void fb_draw_rgb565_channel_diag(void)
{
    uint16_t x;
    uint16_t y;

    for(y = 0u; y < V5F_RGB_FB_HEIGHT; y++)
    {
        for(x = 0u; x < V5F_RGB_FB_WIDTH; x++)
        {
            uint8_t level = (uint8_t)(((uint32_t)x * 255u) / (V5F_RGB_FB_WIDTH - 1u));
            uint16_t color;

            if(y < (V5F_RGB_FB_HEIGHT / 4u))
            {
                color = ch32h417_ltdc_rgb_pack_rgb565(level, 0u, 0u);
            }
            else if(y < (V5F_RGB_FB_HEIGHT / 2u))
            {
                color = ch32h417_ltdc_rgb_pack_rgb565(0u, level, 0u);
            }
            else if(y < ((V5F_RGB_FB_HEIGHT * 3u) / 4u))
            {
                color = ch32h417_ltdc_rgb_pack_rgb565(0u, 0u, level);
            }
            else
            {
                color = ch32h417_ltdc_rgb_pack_rgb565(level, level, level);
            }
            fb_plot_user_rgb565(x, y, color);
        }
    }
    memory_barrier();
}

static void run_ltdc_rgb565_diag_test(void)
{
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    while(1)
    {
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(1000);
    }
}
#endif

static int lcd_start_layer_at(uint16_t width,
                              uint16_t height,
                              uint32_t pixel_format,
                              uint32_t line_pitch,
                              uint32_t framebuffer)
{
    ch32h417_ltdc_rgb_layer_t layer = {0};
    ch32h417_ltdc_rgb_color_t black = {0u, 0u, 0u};
    int result;

    ch32h417_lcd_rgb_control_init();
    ch32h417_lcd_rgb_disp_enable(1u);

    layer.width = width;
    layer.height = height;
    layer.offset_x = (uint16_t)((CH32H417_LCD_RGB_WIDTH - width) / 2u);
    layer.offset_y = (uint16_t)((CH32H417_LCD_RGB_HEIGHT - height) / 2u);
    layer.pixel_format = pixel_format;
    layer.framebuffer = framebuffer;
    layer.line_pitch = line_pitch;

    result = ch32h417_ltdc_rgb_start_layer1(&ch32h417_ltdc_rgb_panel_800x480,
                                            &layer,
                                            &black);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        return result;
    }
    ch32h417_lcd_rgb_backlight_enable(1u);
    if(pixel_format == LTDC_Pixelformat_L8)
    {
        load_l8_clut_after_layer_start();
    }
    return CH32H417_LTDC_RGB_OK;
}

static int lcd_start_layer(uint16_t width,
                           uint16_t height,
                           uint32_t pixel_format,
                           uint32_t line_pitch)
{
    return lcd_start_layer_at(width,
                              height,
                              pixel_format,
                              line_pitch,
                              (uint32_t)&s_lcd_fb[0]);
}

static int V5F_MAYBE_UNUSED lcd_start_l8_fullscreen(void)
{
    return lcd_start_layer(V5F_L8_FB_WIDTH,
                           V5F_L8_FB_HEIGHT,
                           LTDC_Pixelformat_L8,
                           V5F_L8_FB_WIDTH);
}

static int V5F_MAYBE_UNUSED lcd_start_rgb565_window(void)
{
    fb_fill_rgb565(ch32h417_ltdc_rgb_pack_rgb565(0u, 0u, 0u));
    return lcd_start_layer(V5F_RGB_FB_WIDTH,
                           V5F_RGB_FB_HEIGHT,
                           LTDC_Pixelformat_RGB565,
                           V5F_RGB_FB_WIDTH * 2u);
}

#if (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_MEMTEST) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_LTDC_RGB565) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_REMAP_PROBE) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_DQ_PROBE) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_OFFICIAL_16BIT)
static int sdram_run_memtest_bytes(uint32_t bytes)
{
    v5f_sdram_memtest_result_t result = {0};
    int test_result;

    test_result = sdram_init();
    if(test_result != V5F_SDRAM_OK)
    {
        result.stage = V5F_SDRAM_STAGE_INIT;
        sdram_diag_fail(test_result, &result);
        return test_result;
    }

    test_result = sdram_memtest_range(0u, bytes, &result);
    if(test_result != V5F_SDRAM_OK)
    {
        sdram_diag_fail(test_result, &result);
        return test_result;
    }

    g_v5f_hw_test_diag.sdram_ok_count++;
    return V5F_SDRAM_OK;
}

static uint16_t sdram_gradient_color(uint16_t x, uint16_t y)
{
    uint8_t red = (uint8_t)(((uint32_t)x * 255u) /
                            (V5F_SDRAM_LTDC_WIDTH - 1u));
    uint8_t green = (uint8_t)(((uint32_t)y * 255u) /
                              (V5F_SDRAM_LTDC_HEIGHT - 1u));
    uint8_t blue = (uint8_t)((((uint32_t)x + y) * 255u) /
                             (V5F_SDRAM_LTDC_WIDTH + V5F_SDRAM_LTDC_HEIGHT - 2u));

    return ch32h417_ltdc_rgb_pack_rgb565(red, green, blue);
}

static void sdram_restore_user_rect(uint16_t x,
                                    uint16_t y,
                                    uint16_t width,
                                    uint16_t height)
{
    uint16_t *fb = sdram_rgb565_fb();
    uint16_t px;
    uint16_t py;

    for(py = y; py < (uint16_t)(y + height); py++)
    {
        for(px = x; px < (uint16_t)(x + width); px++)
        {
            fb[((uint32_t)(V5F_SDRAM_LTDC_HEIGHT - 1u - py) * V5F_SDRAM_LTDC_WIDTH) +
               (V5F_SDRAM_LTDC_WIDTH - 1u - px)] = sdram_gradient_color(px, py);
        }
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

static int V5F_MAYBE_UNUSED sdram_ltdc_prepare(void)
{
    v5f_sdram_memtest_result_t result = {0};
    int test_result;
    ch32h417_ltdc_rgb_color_t black = {0u, 0u, 0u};

    test_result = sdram_status_lcd_start();
    if(test_result != CH32H417_LTDC_RGB_OK)
    {
        return test_result;
    }

    test_result = sdram_run_memtest_bytes(V5F_SDRAM_QUICK_TEST_BYTES);
    if(test_result != V5F_SDRAM_OK)
    {
        return test_result;
    }

    test_result = sdram_memtest_range(V5F_SDRAM_BYTES - 65536u, 65536u, &result);
    if(test_result != V5F_SDRAM_OK)
    {
        sdram_diag_fail(test_result, &result);
        return test_result;
    }

    sdram_fill_ltdc_pattern();
    test_result = lcd_start_layer_at(V5F_SDRAM_LTDC_WIDTH,
                                     V5F_SDRAM_LTDC_HEIGHT,
                                     LTDC_Pixelformat_RGB565,
                                     V5F_SDRAM_LTDC_WIDTH * 2u,
                                     V5F_SDRAM_BASE_ADDR);
    if(test_result != CH32H417_LTDC_RGB_OK)
    {
        result.stage = V5F_SDRAM_STAGE_LTDC_FILL;
        sdram_diag_fail(V5F_SDRAM_ERR_LCD, &result);
        ch32h417_ltdc_rgb_set_background(&black);
        return V5F_SDRAM_ERR_LCD;
    }

    return V5F_SDRAM_OK;
}

static void V5F_MAYBE_UNUSED run_sdram_memtest_test(void)
{
    int result;
    uint8_t blink = 0u;

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;

    result = sdram_status_lcd_start();
    if(result != CH32H417_LTDC_RGB_OK)
    {
        fail_forever(result);
    }

    result = sdram_run_memtest_bytes(V5F_SDRAM_BYTES);
    if(result != V5F_SDRAM_OK)
    {
        fail_forever(result);
    }

    sdram_status_show(V5F_SDRAM_STATUS_PASS,
                      ch32h417_ltdc_rgb_pack_rgb565(0u, 220u, 60u));
    while(1)
    {
        uint16_t color = (blink != 0u) ?
                             ch32h417_ltdc_rgb_pack_rgb565(255u, 255u, 255u) :
                             ch32h417_ltdc_rgb_pack_rgb565(0u, 220u, 60u);

        fb_fill_user_rect_rgb565((uint16_t)(V5F_RGB_FB_WIDTH - 46u),
                                 (uint16_t)(V5F_RGB_FB_HEIGHT - 34u),
                                 34u,
                                 22u,
                                 color);
        memory_barrier();
        blink ^= 1u;
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(250);
    }
}

static void V5F_MAYBE_UNUSED run_sdram_ltdc_rgb565_test(void)
{
    const uint16_t box_width = 96u;
    const uint16_t box_height = 64u;
    const uint16_t box_y = 208u;
    uint16_t x = 0u;
    uint16_t old_x = 0xFFFFu;
    uint8_t forward = 1u;

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_LTDC_RUNNING;
    while(1)
    {
        uint16_t color;

        if(old_x != 0xFFFFu)
        {
            sdram_restore_user_rect(old_x, box_y, box_width, box_height);
        }

        color = (g_v5f_hw_test_diag.frame_count & 0x20u) ?
                    ch32h417_ltdc_rgb_pack_rgb565(255u, 255u, 255u) :
                    ch32h417_ltdc_rgb_pack_rgb565(255u, 80u, 0u);
        ch32h417_ltdc_rgb_fb_fill_rect_rgb565_rot180(sdram_rgb565_fb(),
                                                     V5F_SDRAM_LTDC_WIDTH,
                                                     V5F_SDRAM_LTDC_HEIGHT,
                                                     x,
                                                     box_y,
                                                     box_width,
                                                     box_height,
                                                     color);
        old_x = x;
        if(forward != 0u)
        {
            if(x >= (uint16_t)(V5F_SDRAM_LTDC_WIDTH - box_width - 8u))
            {
                forward = 0u;
            }
            else
            {
                x = (uint16_t)(x + 8u);
            }
        }
        else if(x <= 8u)
        {
            forward = 1u;
        }
        else
        {
            x = (uint16_t)(x - 8u);
        }

        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(16);
    }
}

static void V5F_MAYBE_UNUSED run_sdram_remap_probe_test(void)
{
    v5f_sdram_memtest_result_t fail = {0};
    uint16_t bg = ch32h417_ltdc_rgb_pack_rgb565(2u, 4u, 7u);
    uint16_t red = ch32h417_ltdc_rgb_pack_rgb565(255u, 0u, 0u);
    uint16_t green = ch32h417_ltdc_rgb_pack_rgb565(0u, 210u, 80u);
    uint16_t white = ch32h417_ltdc_rgb_pack_rgb565(240u, 240u, 240u);
    uint8_t base_pass;
    uint8_t remap_pass;
    int result;

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;

    result = sdram_status_lcd_start();
    if(result != CH32H417_LTDC_RGB_OK)
    {
        fail_forever(result);
    }

    result = sdram_init();
    if(result != V5F_SDRAM_OK)
    {
        fail.stage = V5F_SDRAM_STAGE_INIT;
        sdram_diag_fail(result, &fail);
        fail_forever(result);
    }

    fb_fill_rgb565(bg);
    fb_draw_border_rgb565(red);

    base_pass = sdram_probe_window_show(V5F_SDRAM_BASE_ADDR, 36u);
    sdram_enable_0x60000000_remap();
    remap_pass = sdram_probe_window_show(V5F_SDRAM_REMAP_ADDR, 96u);

    fb_fill_user_rect_rgb565(4u,
                             8u,
                             148u,
                             18u,
                             (base_pass != 0u) ? green : red);
    fb_fill_user_rect_rgb565(168u,
                             8u,
                             148u,
                             18u,
                             (remap_pass != 0u) ? green : red);
    fb_fill_user_rect_rgb565(0u, 84u, V5F_RGB_FB_WIDTH, 4u, white);

    g_v5f_hw_test_diag.sdram_expected = base_pass;
    g_v5f_hw_test_diag.sdram_actual = remap_pass;
    g_v5f_hw_test_diag.sdram_ok_count = (uint32_t)base_pass + (uint32_t)remap_pass;
    g_v5f_hw_test_diag.sdram_fail_count =
        (uint32_t)((base_pass == 0u) ? 1u : 0u) +
        (uint32_t)((remap_pass == 0u) ? 1u : 0u);
    memory_barrier();

    while(1)
    {
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(250);
    }
}

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_OFFICIAL_16BIT
static void V5F_MAYBE_UNUSED run_sdram_official_16bit_test(void)
{
    v5f_sdram_memtest_result_t fail = {0};
    volatile uint16_t *probe = (volatile uint16_t *)V5F_SDRAM_REMAP_ADDR;
    uint16_t green = ch32h417_ltdc_rgb_pack_rgb565(0u, 210u, 80u);
    uint8_t pass;
    int result;

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;

    result = sdram_status_lcd_start();
    if(result != CH32H417_LTDC_RGB_OK)
    {
        fail_forever(result);
    }

    sdram_diag_clear();
    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_INIT;
    g_v5f_hw_test_diag.sdram_hclk_hz = HCLKClock;
    g_v5f_hw_test_diag.sdram_sdclk_hz = HCLKClock;
    g_v5f_hw_test_diag.sdram_refresh_count = 677u;
    sdram_status_show(V5F_SDRAM_STATUS_INIT,
                      ch32h417_ltdc_rgb_pack_rgb565(0u, 170u, 220u));

    h417_v5f_sdram_official_16bit_init();

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_DATA_BUS;
    pass = sdram_probe_window_show(V5F_SDRAM_REMAP_ADDR, 36u);
    if(pass == 0u)
    {
        fail.stage = V5F_SDRAM_STAGE_DATA_BUS;
        fail.expected = g_v5f_hw_test_diag.sdram_expected;
        fail.actual = g_v5f_hw_test_diag.sdram_actual;
        sdram_diag_fail(V5F_SDRAM_ERR_VERIFY, &fail);
        sdram_status_fail_show();
        fail_forever(V5F_SDRAM_ERR_VERIFY);
    }

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_LTDC_RUNNING;
    g_v5f_hw_test_diag.sdram_ok_count++;
    sdram_status_show(V5F_SDRAM_STATUS_PASS, green);
    fb_fill_user_rect_rgb565(4u, 36u, 148u, 18u, green);
    fb_fill_user_rect_rgb565(168u, 36u, 148u, 18u, green);
    memory_barrier();

#if V5F_SDRAM_USB_DEBUG_ENABLED
    sdram_usb_debug_init(probe);
#endif

    while(1)
    {
#if V5F_SDRAM_USB_DEBUG_ENABLED
        sdram_usb_debug_poll(probe);
        if((g_v5f_hw_test_diag.frame_count % 8u) == 0u)
        {
            sdram_usb_debug_report(probe, "tick");
        }
#endif
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(250);
    }
}
#endif

static void V5F_MAYBE_UNUSED run_sdram_dq_probe_test(void)
{
    v5f_sdram_memtest_result_t fail = {0};
    volatile uint16_t *probe = (volatile uint16_t *)V5F_SDRAM_BASE_ADDR;
    int result;

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;

    result = sdram_status_lcd_start();
    if(result != CH32H417_LTDC_RGB_OK)
    {
        fail_forever(result);
    }

    result = sdram_init();
    if(result != V5F_SDRAM_OK)
    {
        fail.stage = V5F_SDRAM_STAGE_INIT;
        sdram_diag_fail(result, &fail);
        fail_forever(result);
    }

    sdram_dq_probe_full_show(probe);
#if V5F_SDRAM_USB_DEBUG_ENABLED
    sdram_usb_debug_init(probe);
#endif

    while(1)
    {
#if V5F_SDRAM_USB_DEBUG_ENABLED
        sdram_usb_debug_poll(probe);
        if((g_v5f_hw_test_diag.frame_count % 8u) == 0u)
        {
            sdram_usb_debug_report(probe, "tick");
        }
#endif
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(250);
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC
static int ltdc_gray_image_valid(void)
{
    uint32_t image_size = (uint32_t)(v5f_ltdc_gray_800x480_end - v5f_ltdc_gray_800x480);

    return (image_size == V5F_L8_FB_BYTES) &&
           (V5F_LTDC_GRAY_IMAGE_WIDTH == V5F_L8_FB_WIDTH) &&
           (V5F_LTDC_GRAY_IMAGE_HEIGHT == V5F_L8_FB_HEIGHT) &&
           (V5F_LTDC_GRAY_IMAGE_BYTES == V5F_L8_FB_BYTES);
}

static void fb_load_ltdc_gray_image(void)
{
    uint32_t i;

    /*
     * The generated asset is already cropped to 800x480 grayscale and stored
     * in 180-degree rotated framebuffer order for the mounted panel.
     */
    for(i = 0u; i < V5F_L8_FB_BYTES; i++)
    {
        s_lcd_fb[i] = v5f_ltdc_gray_800x480[i];
    }
    memory_barrier();
}

static void run_ltdc_test(void)
{
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    while(1)
    {
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(1000);
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_L8_PALETTE_IMAGE
static int ltdc_palette_image_valid(void)
{
    uint32_t image_size =
        (uint32_t)(v5f_ltdc_palette_800x480_end - v5f_ltdc_palette_800x480);
    uint32_t clut_size =
        (uint32_t)(v5f_ltdc_palette_800x480_clut_rgb888_end -
                   v5f_ltdc_palette_800x480_clut_rgb888);

    return (image_size == V5F_L8_FB_BYTES) &&
           (clut_size == V5F_LTDC_PALETTE_CLUT_BYTES) &&
           (V5F_LTDC_PALETTE_IMAGE_WIDTH == V5F_L8_FB_WIDTH) &&
           (V5F_LTDC_PALETTE_IMAGE_HEIGHT == V5F_L8_FB_HEIGHT) &&
           (V5F_LTDC_PALETTE_IMAGE_BYTES == V5F_L8_FB_BYTES);
}

static void fb_load_ltdc_palette_image(void)
{
    uint32_t i;

    for(i = 0u; i < V5F_L8_FB_BYTES; i++)
    {
        s_lcd_fb[i] = v5f_ltdc_palette_800x480[i];
    }
    memory_barrier();
}

static void run_ltdc_l8_palette_image_test(void)
{
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    while(1)
    {
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(1000);
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_R2M_FILL
static int gpha_fill_rect_actual(uint16_t x,
                                 uint16_t y,
                                 uint16_t width,
                                 uint16_t height,
                                 uint16_t color)
{
    return ch32h417_gpha_2d_fill_rgb565(rgb_fb(),
                                        V5F_RGB_FB_WIDTH,
                                        V5F_RGB_FB_HEIGHT,
                                        x,
                                        y,
                                        width,
                                        height,
                                        color);
}

static int gpha_fill_user_rect(uint16_t x,
                               uint16_t y,
                               uint16_t width,
                               uint16_t height,
                               uint16_t color)
{
    uint16_t actual_x;
    uint16_t actual_y;

    if((width == 0u) || (height == 0u) ||
       (((uint32_t)x + width) > V5F_RGB_FB_WIDTH) ||
       (((uint32_t)y + height) > V5F_RGB_FB_HEIGHT))
    {
        return -1;
    }

    actual_x = (uint16_t)(V5F_RGB_FB_WIDTH - x - width);
    actual_y = (uint16_t)(V5F_RGB_FB_HEIGHT - y - height);
    return gpha_fill_rect_actual(actual_x, actual_y, width, height, color);
}

static uint16_t advance_position(uint16_t pos, uint8_t *forward)
{
    const uint16_t step = 4u;
    const uint16_t max_pos = V5F_RGB_FB_WIDTH - 112u;

    if(*forward != 0u)
    {
        if((uint16_t)(pos + step) >= max_pos)
        {
            *forward = 0u;
            return max_pos;
        }
        return (uint16_t)(pos + step);
    }

    if(pos <= step)
    {
        *forward = 1u;
        return 0u;
    }
    return (uint16_t)(pos - step);
}

static void run_gpha_r2m_fill_test(void)
{
    const uint16_t bg = ch32h417_ltdc_rgb_pack_rgb565(8u, 10u, 18u);
    const uint16_t orange = ch32h417_ltdc_rgb_pack_rgb565(255u, 108u, 16u);
    const uint16_t cyan = ch32h417_ltdc_rgb_pack_rgb565(0u, 255u, 255u);
    uint16_t pos = 0u;
    uint16_t old_pos = 0xFFFFu;
    uint8_t forward = 1u;
    int result;

    ch32h417_gpha_2d_init();
    result = gpha_fill_rect_actual(0u, 0u, V5F_RGB_FB_WIDTH, V5F_RGB_FB_HEIGHT, bg);
    if(result == 0)
    {
        fb_draw_border_rgb565(ch32h417_ltdc_rgb_pack_rgb565(255u, 255u, 255u));
    }

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    while(1)
    {
        result = 0;
        if(old_pos != 0xFFFFu)
        {
            result = gpha_fill_user_rect(old_pos, 36u, 88u, 42u, bg);
            if(result == 0)
            {
                result = gpha_fill_user_rect((uint16_t)(V5F_RGB_FB_WIDTH - 112u - old_pos),
                                             92u,
                                             64u,
                                             26u,
                                             bg);
            }
        }
        if(result == 0)
        {
            result = gpha_fill_user_rect(pos, 36u, 88u, 42u, orange);
        }
        if(result == 0)
        {
            result = gpha_fill_user_rect((uint16_t)(V5F_RGB_FB_WIDTH - 112u - pos),
                                         92u,
                                         64u,
                                         26u,
                                         cyan);
        }

        if(result == 0)
        {
            g_v5f_hw_test_diag.gpha_ok_count++;
            old_pos = pos;
            pos = advance_position(pos, &forward);
        }
        else
        {
            g_v5f_hw_test_diag.gpha_fail_count++;
            g_v5f_hw_test_diag.last_error = result;
            fb_fill_rgb565(ch32h417_ltdc_rgb_pack_rgb565(255u, 0u, 0u));
            old_pos = 0xFFFFu;
        }

        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(16);
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_L8_LTDC_FULLSCREEN
static int gpha_fill_l8_quad_actual(uint16_t x,
                                    uint16_t y,
                                    uint16_t width,
                                    uint16_t height,
                                    uint8_t index0,
                                    uint8_t index1,
                                    uint8_t index2,
                                    uint8_t index3)
{
    return ch32h417_gpha_2d_fill_l8_quad(l8_fb(),
                                         V5F_L8_FB_WIDTH,
                                         V5F_L8_FB_HEIGHT,
                                         x,
                                         y,
                                         width,
                                         height,
                                         index0,
                                         index1,
                                         index2,
                                         index3);
}

static int gpha_fill_l8_quad_user(uint16_t x,
                                  uint16_t y,
                                  uint16_t width,
                                  uint16_t height,
                                  uint8_t index0,
                                  uint8_t index1,
                                  uint8_t index2,
                                  uint8_t index3)
{
    uint16_t actual_x;
    uint16_t actual_y;

    if((width == 0u) || (height == 0u) ||
       ((x & 0x3u) != 0u) || ((width & 0x3u) != 0u) ||
       (((uint32_t)x + width) > V5F_L8_FB_WIDTH) ||
       (((uint32_t)y + height) > V5F_L8_FB_HEIGHT))
    {
        return -1;
    }

    actual_x = (uint16_t)(V5F_L8_FB_WIDTH - x - width);
    actual_y = (uint16_t)(V5F_L8_FB_HEIGHT - y - height);
    return gpha_fill_l8_quad_actual(actual_x,
                                    actual_y,
                                    width,
                                    height,
                                    index3,
                                    index2,
                                    index1,
                                    index0);
}

static int gpha_fill_l8_solid_user(uint16_t x,
                                   uint16_t y,
                                   uint16_t width,
                                   uint16_t height,
                                   uint8_t index)
{
    return gpha_fill_l8_quad_user(x, y, width, height, index, index, index, index);
}

static int gpha_l8_ltdc_draw_pattern(void)
{
    uint16_t x;
    int result;

    result = gpha_fill_l8_solid_user(0u, 0u, V5F_L8_FB_WIDTH, V5F_L8_FB_HEIGHT, 10u);
    if(result != 0)
    {
        return result;
    }

    result = gpha_fill_l8_quad_user(0u, 0u, V5F_L8_FB_WIDTH, 32u, 1u, 2u, 3u, 4u);
    if(result != 0)
    {
        return result;
    }

    result = gpha_fill_l8_solid_user(0u, 32u, V5F_L8_FB_WIDTH, 64u, 1u);
    if(result != 0)
    {
        return result;
    }
    result = gpha_fill_l8_solid_user(0u, 96u, V5F_L8_FB_WIDTH, 64u, 2u);
    if(result != 0)
    {
        return result;
    }
    result = gpha_fill_l8_solid_user(0u, 160u, V5F_L8_FB_WIDTH, 64u, 3u);
    if(result != 0)
    {
        return result;
    }

    result = gpha_fill_l8_solid_user(0u, 224u, 200u, 96u, 5u);
    if(result != 0)
    {
        return result;
    }
    result = gpha_fill_l8_solid_user(200u, 224u, 200u, 96u, 6u);
    if(result != 0)
    {
        return result;
    }
    result = gpha_fill_l8_solid_user(400u, 224u, 200u, 96u, 7u);
    if(result != 0)
    {
        return result;
    }
    result = gpha_fill_l8_solid_user(600u, 224u, 200u, 96u, 4u);
    if(result != 0)
    {
        return result;
    }

    for(x = 0u; x < V5F_L8_FB_WIDTH; x = (uint16_t)(x + 32u))
    {
        uint8_t index = (uint8_t)(16u + ((uint32_t)x * 224u / V5F_L8_FB_WIDTH));
        result = gpha_fill_l8_solid_user(x, 320u, 32u, 160u, index);
        if(result != 0)
        {
            return result;
        }
    }

    result = gpha_fill_l8_solid_user(0u, 0u, 4u, V5F_L8_FB_HEIGHT, 4u);
    if(result != 0)
    {
        return result;
    }
    result = gpha_fill_l8_solid_user((uint16_t)(V5F_L8_FB_WIDTH - 4u), 0u, 4u, V5F_L8_FB_HEIGHT, 4u);
    if(result != 0)
    {
        return result;
    }
    result = gpha_fill_l8_solid_user(0u, (uint16_t)(V5F_L8_FB_HEIGHT - 4u), V5F_L8_FB_WIDTH, 4u, 4u);
    if(result != 0)
    {
        return result;
    }

    return gpha_fill_l8_solid_user(0u, 0u, V5F_L8_FB_WIDTH, 4u, 4u);
}

static void run_gpha_l8_ltdc_fullscreen_test(void)
{
    int result;

    ch32h417_gpha_2d_init();
    result = gpha_l8_ltdc_draw_pattern();
    if(result == 0)
    {
        g_v5f_hw_test_diag.gpha_ok_count++;
    }
    else
    {
        g_v5f_hw_test_diag.gpha_fail_count++;
        g_v5f_hw_test_diag.last_error = result;
        ch32h417_ltdc_rgb_fb_fill_l8(l8_fb(), V5F_L8_FB_BYTES, 1u);
        memory_barrier();
    }

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    while(1)
    {
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(1000);
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_PFC_L8_RGB565
static uint32_t gpha_argb8888(uint8_t red, uint8_t green, uint8_t blue)
{
    return ch32h417_gpha_2d_argb8888(red, green, blue);
}

static void gpha_l8_fill_clut(void)
{
    uint32_t i;
    uint32_t *clut = gpha_l8_clut();

    for(i = 0u; i < V5F_GPHA_L8_CLUT_ENTRIES; i++)
    {
        clut[i] = gpha_argb8888(0u, 0u, 0u);
    }

    clut[1] = gpha_argb8888(255u, 0u, 0u);
    clut[2] = gpha_argb8888(0u, 255u, 0u);
    clut[3] = gpha_argb8888(0u, 0u, 255u);
    clut[4] = gpha_argb8888(255u, 255u, 255u);
    memory_barrier();
}

static void gpha_l8_plot_user(uint16_t x, uint16_t y, uint8_t index)
{
    uint8_t *src;

    if((x >= V5F_RGB_FB_WIDTH) || (y >= V5F_RGB_FB_HEIGHT))
    {
        return;
    }

    src = gpha_l8_src();
    src[((uint32_t)(V5F_RGB_FB_HEIGHT - 1u - y) * V5F_RGB_FB_WIDTH) +
        (V5F_RGB_FB_WIDTH - 1u - x)] = index;
}

static void gpha_l8_draw_user_bars(void)
{
    uint16_t x;
    uint16_t y;

    for(y = 0u; y < V5F_RGB_FB_HEIGHT; y++)
    {
        for(x = 0u; x < V5F_RGB_FB_WIDTH; x++)
        {
            uint8_t index = (uint8_t)((uint32_t)x * 5u / V5F_RGB_FB_WIDTH);
            gpha_l8_plot_user(x, y, index);
        }
    }
    memory_barrier();
}

static int gpha_l8_clut_to_rgb565(void)
{
    return ch32h417_gpha_2d_l8_to_rgb565(gpha_l8_src(),
                                          rgb_fb(),
                                          gpha_l8_clut(),
                                          V5F_RGB_FB_WIDTH,
                                          V5F_RGB_FB_HEIGHT,
                                          V5F_GPHA_L8_CLUT_ENTRIES);
}

static void run_gpha_pfc_l8_rgb565_test(void)
{
    int result;

    ch32h417_gpha_2d_init();
    gpha_l8_fill_clut();
    gpha_l8_draw_user_bars();
    result = gpha_l8_clut_to_rgb565();
    if(result == 0)
    {
        g_v5f_hw_test_diag.gpha_ok_count++;
    }
    else
    {
        g_v5f_hw_test_diag.gpha_fail_count++;
        g_v5f_hw_test_diag.last_error = result;
        fb_fill_rgb565(ch32h417_ltdc_rgb_pack_rgb565(255u, 0u, 0u));
    }

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    while(1)
    {
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(1000);
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_BLEND_RGB565
static uint16_t gpha_argb4444(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue)
{
    return ch32h417_gpha_2d_argb4444(alpha, red, green, blue);
}

static uint32_t gpha_user_index(uint16_t x, uint16_t y)
{
    return ((uint32_t)(V5F_RGB_FB_HEIGHT - 1u - y) * V5F_RGB_FB_WIDTH) +
           (uint32_t)(V5F_RGB_FB_WIDTH - 1u - x);
}

static void gpha_blend_draw_background(void)
{
    uint16_t x;
    uint16_t y;
    uint16_t *bg = gpha_blend_bg();

    for(y = 0u; y < V5F_RGB_FB_HEIGHT; y++)
    {
        for(x = 0u; x < V5F_RGB_FB_WIDTH; x++)
        {
            uint8_t band = (uint8_t)((x * 4u) / V5F_RGB_FB_WIDTH);
            uint8_t checker = (uint8_t)(((x / 32u) ^ (y / 32u)) & 1u);
            uint16_t color;

            if(band == 0u)
            {
                color = checker ? ch32h417_ltdc_rgb_pack_rgb565(20u, 70u, 180u) :
                                  ch32h417_ltdc_rgb_pack_rgb565(10u, 28u, 90u);
            }
            else if(band == 1u)
            {
                color = checker ? ch32h417_ltdc_rgb_pack_rgb565(20u, 150u, 80u) :
                                  ch32h417_ltdc_rgb_pack_rgb565(0u, 80u, 40u);
            }
            else if(band == 2u)
            {
                color = checker ? ch32h417_ltdc_rgb_pack_rgb565(170u, 130u, 30u) :
                                  ch32h417_ltdc_rgb_pack_rgb565(80u, 52u, 12u);
            }
            else
            {
                color = checker ? ch32h417_ltdc_rgb_pack_rgb565(150u, 50u, 130u) :
                                  ch32h417_ltdc_rgb_pack_rgb565(70u, 20u, 90u);
            }
            bg[gpha_user_index(x, y)] = color;
        }
    }
    memory_barrier();
}

static void gpha_blend_draw_foreground(uint16_t pos)
{
    uint32_t i;
    uint16_t x;
    uint16_t y;
    uint16_t *fg = gpha_blend_fg_argb4444();
    const uint16_t rect_w = 92u;
    const uint16_t rect_h = 58u;
    const uint16_t rect_y = 48u;

    for(i = 0u; i < V5F_RGB_FB_PIXELS; i++)
    {
        fg[i] = 0u;
    }

    for(y = 0u; y < rect_h; y++)
    {
        for(x = 0u; x < rect_w; x++)
        {
            uint16_t draw_x = (uint16_t)(pos + x);
            uint16_t draw_y = (uint16_t)(rect_y + y);
            uint8_t edge = (uint8_t)((x < 3u) || (y < 3u) ||
                                     (x >= (rect_w - 3u)) ||
                                     (y >= (rect_h - 3u)));
            uint16_t color = edge ? gpha_argb4444(0xFu, 0xFu, 0xFu, 0xFu) :
                                    gpha_argb4444(0xAu, 0xFu, 0x6u, 0x0u);

            if((draw_x < V5F_RGB_FB_WIDTH) && (draw_y < V5F_RGB_FB_HEIGHT))
            {
                fg[gpha_user_index(draw_x, draw_y)] = color;
            }
        }
    }
    memory_barrier();
}

static int gpha_blend_to_rgb565(void)
{
    return ch32h417_gpha_2d_blend_argb4444_over_rgb565(gpha_blend_fg_argb4444(),
                                                        gpha_blend_bg(),
                                                        rgb_fb(),
                                                        V5F_RGB_FB_WIDTH,
                                                        V5F_RGB_FB_HEIGHT);
}

static uint16_t gpha_blend_advance_position(uint16_t pos, uint8_t *forward)
{
    const uint16_t step = 6u;
    const uint16_t rect_w = 92u;
    const uint16_t max_pos = V5F_RGB_FB_WIDTH - rect_w;

    if(*forward != 0u)
    {
        if((uint16_t)(pos + step) >= max_pos)
        {
            *forward = 0u;
            return max_pos;
        }
        return (uint16_t)(pos + step);
    }

    if(pos <= step)
    {
        *forward = 1u;
        return 0u;
    }
    return (uint16_t)(pos - step);
}

static void run_gpha_blend_rgb565_test(void)
{
    uint16_t pos = 0u;
    uint8_t forward = 1u;
    int result;

    ch32h417_gpha_2d_init();
    gpha_blend_draw_background();

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    while(1)
    {
        gpha_blend_draw_foreground(pos);
        result = gpha_blend_to_rgb565();
        if(result == 0)
        {
            g_v5f_hw_test_diag.gpha_ok_count++;
            pos = gpha_blend_advance_position(pos, &forward);
        }
        else
        {
            g_v5f_hw_test_diag.gpha_fail_count++;
            g_v5f_hw_test_diag.last_error = result;
            fb_fill_rgb565(ch32h417_ltdc_rgb_pack_rgb565(255u, 0u, 0u));
        }

        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(16);
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH_L8_ASSETS
#define FLASH_ASSET_BLOCKS             8u
#define FLASH_ASSET_START_BLOCK        (GD5F1G_BLOCK_COUNT - FLASH_ASSET_BLOCKS)
#define FLASH_ASSET_MANIFEST_OFFSET    0u
#define FLASH_ASSET_GRAY_CLUT_OFFSET   GD5F1G_PAGE_SIZE
#define FLASH_ASSET_GRAY_IMAGE_OFFSET  (FLASH_ASSET_GRAY_CLUT_OFFSET + GD5F1G_PAGE_SIZE)
#define FLASH_ASSET_PALETTE_CLUT_OFFSET \
    GD5F1G_L8_ASSET_ALIGN_PAGE_CONST(FLASH_ASSET_GRAY_IMAGE_OFFSET + V5F_LTDC_FLASH_ASSET_IMAGE_BYTES)
#define FLASH_ASSET_PALETTE_IMAGE_OFFSET (FLASH_ASSET_PALETTE_CLUT_OFFSET + GD5F1G_PAGE_SIZE)
#define FLASH_ASSET_TOTAL_BYTES \
    GD5F1G_L8_ASSET_ALIGN_PAGE_CONST(FLASH_ASSET_PALETTE_IMAGE_OFFSET + V5F_LTDC_FLASH_ASSET_IMAGE_BYTES)

#if FLASH_ASSET_TOTAL_BYTES > (FLASH_ASSET_BLOCKS * GD5F1G_BLOCK_SIZE)
#error V5F flash L8 asset package exceeds reserved SPI-NAND blocks.
#endif

static uint32_t flash_assets_checksum_buffer(const uint8_t *data, uint32_t length)
{
    return gd5f1g_l8_asset_fnv1a_buffer(data, length);
}

static void flash_assets_fill_page(uint8_t value)
{
    uint32_t i;

    for(i = 0u; i < GD5F1G_PAGE_SIZE; i++)
    {
        s_flash_page[i] = value;
    }
}

static void flash_assets_show_stage(uint8_t index)
{
    ch32h417_ltdc_rgb_fb_fill_l8(l8_fb(), V5F_L8_FB_BYTES, index);
    memory_barrier();
    g_v5f_hw_test_diag.frame_count++;
}

static void flash_assets_fill_gray_clut(uint8_t *data)
{
    gd5f1g_l8_asset_fill_gray_clut(data, V5F_LTDC_FLASH_ASSET_CLUT_ENTRIES);
}

static int flash_assets_program_linear(const gd5f1g_spi_bus_t *bus,
                                       uint32_t offset,
                                       const uint8_t *data,
                                       uint32_t length)
{
    return gd5f1g_l8_asset_program_linear(bus,
                                          FLASH_ASSET_START_BLOCK,
                                          offset,
                                          data,
                                          length);
}

static int flash_assets_read_linear(const gd5f1g_spi_bus_t *bus,
                                    uint32_t offset,
                                    uint8_t *data,
                                    uint32_t length)
{
    uint8_t status = 0u;
    int result = gd5f1g_l8_asset_read_linear(bus,
                                             FLASH_ASSET_START_BLOCK,
                                             offset,
                                             data,
                                             length,
                                             &status);
    g_v5f_hw_test_diag.flash_status = status;
    return result;
}

static int flash_assets_verify_linear(const gd5f1g_spi_bus_t *bus,
                                      uint32_t offset,
                                      uint32_t length,
                                      uint32_t expected_checksum)
{
    uint8_t status = 0u;
    int result = gd5f1g_l8_asset_verify_linear(bus,
                                               FLASH_ASSET_START_BLOCK,
                                               offset,
                                               length,
                                               expected_checksum,
                                               s_flash_page,
                                               GD5F1G_PAGE_SIZE,
                                               &status);
    g_v5f_hw_test_diag.flash_status = status;
    return result;
}

static int flash_assets_write_manifest(const gd5f1g_spi_bus_t *bus)
{
    int result;

    gd5f1g_l8_asset_manifest_init(&s_flash_manifest,
                                  V5F_LTDC_FLASH_ASSET_WIDTH,
                                  V5F_LTDC_FLASH_ASSET_HEIGHT,
                                  FLASH_ASSET_TOTAL_BYTES);
    result = gd5f1g_l8_asset_manifest_set(&s_flash_manifest,
                                          0u,
                                          GD5F1G_L8_ASSET_TYPE_GRAY_CLUT,
                                          FLASH_ASSET_GRAY_CLUT_OFFSET,
                                          V5F_LTDC_FLASH_ASSET_CLUT_BYTES,
                                          V5F_LTDC_FLASH_GRAY_CLUT_FNV);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = gd5f1g_l8_asset_manifest_set(&s_flash_manifest,
                                          1u,
                                          GD5F1G_L8_ASSET_TYPE_GRAY_IMAGE,
                                          FLASH_ASSET_GRAY_IMAGE_OFFSET,
                                          V5F_LTDC_FLASH_ASSET_IMAGE_BYTES,
                                          V5F_LTDC_FLASH_GRAY_IMAGE_FNV);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = gd5f1g_l8_asset_manifest_set(&s_flash_manifest,
                                          2u,
                                          GD5F1G_L8_ASSET_TYPE_PALETTE_CLUT,
                                          FLASH_ASSET_PALETTE_CLUT_OFFSET,
                                          V5F_LTDC_FLASH_ASSET_CLUT_BYTES,
                                          V5F_LTDC_FLASH_PALETTE_CLUT_FNV);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = gd5f1g_l8_asset_manifest_set(&s_flash_manifest,
                                          3u,
                                          GD5F1G_L8_ASSET_TYPE_PALETTE_IMAGE,
                                          FLASH_ASSET_PALETTE_IMAGE_OFFSET,
                                          V5F_LTDC_FLASH_ASSET_IMAGE_BYTES,
                                          V5F_LTDC_FLASH_PALETTE_IMAGE_FNV);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    return gd5f1g_l8_asset_write_manifest(bus,
                                          FLASH_ASSET_START_BLOCK,
                                          &s_flash_manifest,
                                          s_flash_page,
                                          GD5F1G_PAGE_SIZE);
}

static int flash_assets_read_manifest(const gd5f1g_spi_bus_t *bus)
{
    uint8_t status = 0u;
    int result = gd5f1g_l8_asset_read_manifest(bus,
                                               FLASH_ASSET_START_BLOCK,
                                               V5F_LTDC_FLASH_ASSET_WIDTH,
                                               V5F_LTDC_FLASH_ASSET_HEIGHT,
                                               &s_flash_manifest,
                                               s_flash_page,
                                               GD5F1G_PAGE_SIZE,
                                               &status);
    g_v5f_hw_test_diag.flash_status = status;
    return result;
}

static int flash_assets_manifest_find(uint32_t type,
                                      uint32_t *offset_out,
                                      uint32_t *length_out,
                                      uint32_t *checksum_out)
{
    gd5f1g_l8_asset_entry_t entry;
    int result;

    if((offset_out == 0) || (length_out == 0) || (checksum_out == 0))
    {
        return GD5F1G_ERR_PARAM;
    }

    result = gd5f1g_l8_asset_manifest_find(&s_flash_manifest, type, &entry);
    if(result == GD5F1G_OK)
    {
        *offset_out = entry.offset;
        *length_out = entry.length;
        *checksum_out = entry.checksum;
    }
    return result;
}

static int flash_assets_check_blocks(const gd5f1g_spi_bus_t *bus)
{
    uint8_t marker = 0u;
    uint8_t status = 0u;
    int result = gd5f1g_l8_asset_check_blocks(bus,
                                              FLASH_ASSET_START_BLOCK,
                                              FLASH_ASSET_BLOCKS,
                                              &marker,
                                              &status);
    g_v5f_hw_test_diag.flash_bad_marker = marker;
    g_v5f_hw_test_diag.flash_bad_marker_status = status;
    return result;
}

static int flash_assets_erase_blocks(const gd5f1g_spi_bus_t *bus)
{
    return gd5f1g_l8_asset_erase_blocks(bus,
                                        FLASH_ASSET_START_BLOCK,
                                        FLASH_ASSET_BLOCKS);
}

static int flash_assets_decode_gray(void)
{
    uint32_t size =
        (uint32_t)(v5f_ltdc_flash_gray_lzss_end - v5f_ltdc_flash_gray_lzss);
    int result;

    if(size != V5F_LTDC_FLASH_GRAY_LZSS_BYTES)
    {
        return -105;
    }

    result = gd5f1g_l8_asset_lzss_decode(v5f_ltdc_flash_gray_lzss,
                                          size,
                                          l8_fb(),
                                          V5F_LTDC_FLASH_ASSET_IMAGE_BYTES);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    gd5f1g_l8_asset_unfilter_left(l8_fb(),
                                  V5F_LTDC_FLASH_ASSET_WIDTH,
                                  V5F_LTDC_FLASH_ASSET_HEIGHT);
    if(flash_assets_checksum_buffer(l8_fb(), V5F_LTDC_FLASH_ASSET_IMAGE_BYTES) !=
       V5F_LTDC_FLASH_GRAY_IMAGE_FNV)
    {
        return GD5F1G_ERR_VERIFY;
    }
    return GD5F1G_OK;
}

static int flash_assets_decode_palette(void)
{
    uint32_t size =
        (uint32_t)(v5f_ltdc_flash_palette_lzss_end - v5f_ltdc_flash_palette_lzss);
    int result;

    if(size != V5F_LTDC_FLASH_PALETTE_LZSS_BYTES)
    {
        return -106;
    }

    result = gd5f1g_l8_asset_lzss_decode(v5f_ltdc_flash_palette_lzss,
                                          size,
                                          l8_fb(),
                                          V5F_LTDC_FLASH_ASSET_IMAGE_BYTES);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    if(flash_assets_checksum_buffer(l8_fb(), V5F_LTDC_FLASH_ASSET_IMAGE_BYTES) !=
       V5F_LTDC_FLASH_PALETTE_IMAGE_FNV)
    {
        return GD5F1G_ERR_VERIFY;
    }
    return GD5F1G_OK;
}

static int flash_assets_write_all(const gd5f1g_spi_bus_t *bus)
{
    uint32_t palette_clut_size =
        (uint32_t)(v5f_ltdc_flash_palette_clut_rgb888_end -
                   v5f_ltdc_flash_palette_clut_rgb888);
    int result;

    if(palette_clut_size != V5F_LTDC_FLASH_ASSET_CLUT_BYTES)
    {
        return -107;
    }

    flash_assets_show_stage(24u);
    flash_assets_fill_page(0xFFu);
    flash_assets_fill_gray_clut(s_flash_page);
    if(flash_assets_checksum_buffer(s_flash_page, V5F_LTDC_FLASH_ASSET_CLUT_BYTES) !=
       V5F_LTDC_FLASH_GRAY_CLUT_FNV)
    {
        return GD5F1G_ERR_VERIFY;
    }
    result = flash_assets_program_linear(bus,
                                         FLASH_ASSET_GRAY_CLUT_OFFSET,
                                         s_flash_page,
                                         V5F_LTDC_FLASH_ASSET_CLUT_BYTES);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    flash_assets_show_stage(48u);
    result = flash_assets_decode_gray();
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = flash_assets_program_linear(bus,
                                         FLASH_ASSET_GRAY_IMAGE_OFFSET,
                                         l8_fb(),
                                         V5F_LTDC_FLASH_ASSET_IMAGE_BYTES);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    flash_assets_show_stage(72u);
    result = flash_assets_program_linear(bus,
                                         FLASH_ASSET_PALETTE_CLUT_OFFSET,
                                         v5f_ltdc_flash_palette_clut_rgb888,
                                         V5F_LTDC_FLASH_ASSET_CLUT_BYTES);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    flash_assets_show_stage(96u);
    result = flash_assets_decode_palette();
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = flash_assets_program_linear(bus,
                                         FLASH_ASSET_PALETTE_IMAGE_OFFSET,
                                         l8_fb(),
                                         V5F_LTDC_FLASH_ASSET_IMAGE_BYTES);
    if(result != GD5F1G_OK)
    {
        return result;
    }

    return flash_assets_write_manifest(bus);
}

static int flash_assets_verify_all(const gd5f1g_spi_bus_t *bus)
{
    int result;

    result = flash_assets_verify_linear(bus,
                                        FLASH_ASSET_GRAY_CLUT_OFFSET,
                                        V5F_LTDC_FLASH_ASSET_CLUT_BYTES,
                                        V5F_LTDC_FLASH_GRAY_CLUT_FNV);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = flash_assets_verify_linear(bus,
                                        FLASH_ASSET_GRAY_IMAGE_OFFSET,
                                        V5F_LTDC_FLASH_ASSET_IMAGE_BYTES,
                                        V5F_LTDC_FLASH_GRAY_IMAGE_FNV);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = flash_assets_verify_linear(bus,
                                        FLASH_ASSET_PALETTE_CLUT_OFFSET,
                                        V5F_LTDC_FLASH_ASSET_CLUT_BYTES,
                                        V5F_LTDC_FLASH_PALETTE_CLUT_FNV);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    return flash_assets_verify_linear(bus,
                                      FLASH_ASSET_PALETTE_IMAGE_OFFSET,
                                      V5F_LTDC_FLASH_ASSET_IMAGE_BYTES,
                                      V5F_LTDC_FLASH_PALETTE_IMAGE_FNV);
}

static int flash_assets_display_palette_from_flash(const gd5f1g_spi_bus_t *bus)
{
    uint32_t clut_offset = 0u;
    uint32_t clut_length = 0u;
    uint32_t clut_checksum = 0u;
    uint32_t image_offset = 0u;
    uint32_t image_length = 0u;
    uint32_t image_checksum = 0u;
    int result;

    result = flash_assets_read_manifest(bus);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = flash_assets_manifest_find(GD5F1G_L8_ASSET_TYPE_PALETTE_CLUT,
                                        &clut_offset,
                                        &clut_length,
                                        &clut_checksum);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    result = flash_assets_manifest_find(GD5F1G_L8_ASSET_TYPE_PALETTE_IMAGE,
                                        &image_offset,
                                        &image_length,
                                        &image_checksum);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    if((clut_length != V5F_LTDC_FLASH_ASSET_CLUT_BYTES) ||
       (image_length != V5F_LTDC_FLASH_ASSET_IMAGE_BYTES))
    {
        return GD5F1G_ERR_VERIFY;
    }

    result = flash_assets_read_linear(bus, clut_offset, s_flash_page, clut_length);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    if(flash_assets_checksum_buffer(s_flash_page, clut_length) != clut_checksum)
    {
        return GD5F1G_ERR_VERIFY;
    }

    result = flash_assets_read_linear(bus, image_offset, l8_fb(), image_length);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    if(flash_assets_checksum_buffer(l8_fb(), image_length) != image_checksum)
    {
        return GD5F1G_ERR_VERIFY;
    }

    (void)ch32h417_ltdc_rgb_layer1_load_clut_rgb888(s_flash_page,
                                                    V5F_LTDC_FLASH_ASSET_CLUT_ENTRIES);
    memory_barrier();
    return GD5F1G_OK;
}

static int flash_assets_prepare_bus(ch32h417_gd5f1g_spi1_context_t *context,
                                    gd5f1g_spi_bus_t *bus)
{
    uint8_t manufacturer_id = 0u;
    uint8_t device_id = 0u;
    int result;

    ch32h417_gd5f1g_spi1_init(context, bus);
    result = gd5f1g_read_id(bus, &manufacturer_id, &device_id);
    if(result != GD5F1G_OK)
    {
        ch32h417_gd5f1g_spi1_set_mode(context, CH32H417_GD5F1G_SPI_MODE0);
        result = gd5f1g_read_id(bus, &manufacturer_id, &device_id);
    }
    g_v5f_hw_test_diag.flash_manufacturer_id = manufacturer_id;
    g_v5f_hw_test_diag.flash_device_id = device_id;
    if(result != GD5F1G_OK)
    {
        return result;
    }

    result = gd5f1g_reset(bus);
    if(result != GD5F1G_OK)
    {
        return result;
    }
    return flash_assets_check_blocks(bus);
}

static void run_flash_l8_assets_test(void)
{
    ch32h417_gd5f1g_spi1_context_t context;
    gd5f1g_spi_bus_t bus;
    int result;

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;

    flash_assets_show_stage(16u);
    result = flash_assets_prepare_bus(&context, &bus);
    if(result == GD5F1G_OK)
    {
        flash_assets_show_stage(32u);
        result = flash_assets_erase_blocks(&bus);
    }
    if(result == GD5F1G_OK)
    {
        result = flash_assets_write_all(&bus);
    }
    if(result == GD5F1G_OK)
    {
        flash_assets_show_stage(128u);
        result = flash_assets_read_manifest(&bus);
    }
    if(result == GD5F1G_OK)
    {
        result = flash_assets_verify_all(&bus);
    }
    if(result == GD5F1G_OK)
    {
        flash_assets_show_stage(192u);
        result = flash_assets_display_palette_from_flash(&bus);
    }

    g_v5f_hw_test_diag.spi_timeout_count = context.timeout_count;
    if((result == GD5F1G_OK) && (context.timeout_count == 0u))
    {
        g_v5f_hw_test_diag.gpha_ok_count++;
    }
    else
    {
        g_v5f_hw_test_diag.gpha_fail_count++;
        g_v5f_hw_test_diag.last_error =
            (result != GD5F1G_OK) ? result : GD5F1G_ERR_TIMEOUT;
        flash_assets_show_stage(255u);
    }

    while(1)
    {
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(1000);
    }
}
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH
static void draw_byte_bits(uint16_t x, uint16_t y, uint8_t value)
{
    uint8_t bit;

    for(bit = 0u; bit < 8u; bit++)
    {
        fb_fill_user_rect_rgb565((uint16_t)(x + bit * 12u),
                                 y,
                                 9u,
                                 16u,
                                 ((value & (uint8_t)(0x80u >> bit)) != 0u) ?
                                     ch32h417_ltdc_rgb_pack_rgb565(255u, 255u, 255u) :
                                     ch32h417_ltdc_rgb_pack_rgb565(32u, 32u, 40u));
    }
}

static void draw_flash_report(int pass)
{
    uint16_t ok = ch32h417_ltdc_rgb_pack_rgb565(0u, 180u, 80u);
    uint16_t fail = ch32h417_ltdc_rgb_pack_rgb565(255u, 32u, 16u);
    uint16_t warn = ch32h417_ltdc_rgb_pack_rgb565(255u, 180u, 0u);
    uint16_t base = pass ? ok : fail;

    fb_fill_rgb565(ch32h417_ltdc_rgb_pack_rgb565(4u, 6u, 10u));
    fb_fill_user_rect_rgb565(0u, 0u, V5F_RGB_FB_WIDTH, 30u, base);
    fb_fill_user_rect_rgb565(0u, 42u, V5F_RGB_FB_WIDTH, 20u,
                             (g_v5f_hw_test_diag.spi_timeout_count == 0u) ? ok : warn);
    fb_fill_user_rect_rgb565(0u, 74u, V5F_RGB_FB_WIDTH, 20u,
                             (g_v5f_hw_test_diag.flash_bad_marker == 0xFFu) ? ok : warn);
    draw_byte_bits(28u, 112u, g_v5f_hw_test_diag.flash_manufacturer_id);
    draw_byte_bits(150u, 112u, g_v5f_hw_test_diag.flash_device_id);
    fb_draw_border_rgb565(ch32h417_ltdc_rgb_pack_rgb565(255u, 255u, 255u));
}

static void run_flash_test(void)
{
    ch32h417_gd5f1g_spi1_context_t context;
    gd5f1g_spi_bus_t bus;
    gd5f1g_info_t info = {0};
    uint8_t manufacturer_id = 0u;
    uint8_t device_id = 0u;
    uint8_t marker = 0u;
    uint8_t marker_status = 0u;
    int id_result;
    int reset_result;
    int info_result;
    int marker_result;
    int pass;

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    ch32h417_gd5f1g_spi1_init(&context, &bus);

    id_result = gd5f1g_read_id(&bus, &manufacturer_id, &device_id);
    reset_result = gd5f1g_reset(&bus);
    info_result = gd5f1g_read_info(&bus, &info);
    marker_result = gd5f1g_read_bad_block_marker(&bus,
                                                 GD5F1G_BLOCK_COUNT - 1u,
                                                 &marker,
                                                 &marker_status);

    g_v5f_hw_test_diag.flash_manufacturer_id = manufacturer_id;
    g_v5f_hw_test_diag.flash_device_id = device_id;
    g_v5f_hw_test_diag.flash_protection = info.protection;
    g_v5f_hw_test_diag.flash_config = info.config;
    g_v5f_hw_test_diag.flash_status = info.status;
    g_v5f_hw_test_diag.flash_status2 = info.status2;
    g_v5f_hw_test_diag.flash_bad_marker = marker;
    g_v5f_hw_test_diag.flash_bad_marker_status = marker_status;
    g_v5f_hw_test_diag.spi_timeout_count = context.timeout_count;

    pass = (id_result == GD5F1G_OK) &&
           (reset_result == GD5F1G_OK) &&
           (info_result == GD5F1G_OK) &&
           (marker_result == GD5F1G_OK) &&
           (manufacturer_id == GD5F1G_MANUFACTURER_ID) &&
           (device_id == GD5F1G_DEVICE_ID_3V) &&
           (context.timeout_count == 0u);
    if(!pass)
    {
        g_v5f_hw_test_diag.last_error =
            (id_result != GD5F1G_OK) ? id_result :
            (reset_result != GD5F1G_OK) ? reset_result :
            (info_result != GD5F1G_OK) ? info_result :
            marker_result;
    }

    while(1)
    {
        draw_flash_report(pass);
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(1000);
    }
}
#endif

static void fail_forever(int error)
{
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_FAILED;
    g_v5f_hw_test_diag.last_error = error;
    while(1)
    {
        rt_thread_mdelay(1000);
    }
}

static void v5f_hw_thread_entry(void *parameter)
{
    int result = CH32H417_LTDC_RGB_OK;
    (void)parameter;

    g_v5f_hw_test_diag.mode = APP_V5F_HW_TEST;
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_BOOT;
#if (APP_V5F_HW_TEST != APP_V5F_HW_TEST_SDRAM_MEMTEST) && \
    (APP_V5F_HW_TEST != APP_V5F_HW_TEST_SDRAM_LTDC_RGB565) && \
    (APP_V5F_HW_TEST != APP_V5F_HW_TEST_SDRAM_REMAP_PROBE) && \
    (APP_V5F_HW_TEST != APP_V5F_HW_TEST_SDRAM_DQ_PROBE) && \
    (APP_V5F_HW_TEST != APP_V5F_HW_TEST_SDRAM_OFFICIAL_16BIT)
    rt_kprintf("V5F hardware test: %s\n", v5f_hw_test_runtime_name());
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC
    if(!ltdc_gray_image_valid())
    {
        fail_forever(-10);
    }
    fb_load_ltdc_gray_image();
    result = lcd_start_l8_fullscreen();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_L8_PALETTE_IMAGE
    if(!ltdc_palette_image_valid())
    {
        fail_forever(-11);
    }
    fb_load_ltdc_palette_image();
    result = lcd_start_l8_fullscreen();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_L8_LTDC_FULLSCREEN
    ch32h417_ltdc_rgb_fb_fill_l8(l8_fb(), V5F_L8_FB_BYTES, 0u);
    result = lcd_start_l8_fullscreen();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH_L8_ASSETS
    ch32h417_ltdc_rgb_fb_fill_l8(l8_fb(), V5F_L8_FB_BYTES, 0u);
    result = lcd_start_l8_fullscreen();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_RGB565_DIAG
    fb_draw_rgb565_channel_diag();
    result = lcd_start_rgb565_window();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_MEMTEST
    result = CH32H417_LTDC_RGB_OK;
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_LTDC_RGB565
    result = sdram_ltdc_prepare();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_REMAP_PROBE
    result = CH32H417_LTDC_RGB_OK;
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_DQ_PROBE
    result = CH32H417_LTDC_RGB_OK;
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_OFFICIAL_16BIT
    result = CH32H417_LTDC_RGB_OK;
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_CH585_SPI_SPEED
    result = CH32H417_LTDC_RGB_OK;
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_CH585_ADC_KEY_CAL
    result = CH32H417_LTDC_RGB_OK;
#elif (APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_R2M_FILL) || \
      (APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_PFC_L8_RGB565) || \
      (APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_BLEND_RGB565) || \
      (APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH) || \
      (APP_V5F_HW_TEST == APP_V5F_HW_TEST_TICK_DIAG)
    result = lcd_start_rgb565_window();
#endif

    if(result != CH32H417_LTDC_RGB_OK)
    {
        fail_forever(result);
    }
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_LCD_READY;

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC
    run_ltdc_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_L8_PALETTE_IMAGE
    run_ltdc_l8_palette_image_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_RGB565_DIAG
    run_ltdc_rgb565_diag_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_MEMTEST
    run_sdram_memtest_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_LTDC_RGB565
    run_sdram_ltdc_rgb565_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_REMAP_PROBE
    run_sdram_remap_probe_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_DQ_PROBE
    run_sdram_dq_probe_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_OFFICIAL_16BIT
    run_sdram_official_16bit_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_R2M_FILL
    run_gpha_r2m_fill_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_PFC_L8_RGB565
    run_gpha_pfc_l8_rgb565_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_BLEND_RGB565
    run_gpha_blend_rgb565_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_GPHA_L8_LTDC_FULLSCREEN
    run_gpha_l8_ltdc_fullscreen_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH
    run_flash_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_FLASH_L8_ASSETS
    run_flash_l8_assets_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_TICK_DIAG
    run_tick_diag_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_CH585_SPI_SPEED
    run_ch585_spi_speed_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_CH585_ADC_KEY_CAL
    run_ch585_adc_key_cal_test();
#else
    while(1)
    {
        rt_thread_mdelay(1000);
    }
#endif
}

int v5f_hw_test_start(void)
{
    rt_err_t err;

    err = rt_thread_init(&s_test_thread,
                         "v5f_hw",
                         v5f_hw_thread_entry,
                         RT_NULL,
                         s_test_thread_stack,
                         sizeof(s_test_thread_stack),
                         18,
                         10);
    if(err != RT_EOK)
    {
        return (int)err;
    }

    return (int)rt_thread_startup(&s_test_thread);
}
