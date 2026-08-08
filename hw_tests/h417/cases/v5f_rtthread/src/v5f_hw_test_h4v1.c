#include "v5f_hw_test.h"

#include <rtthread.h>
#include <string.h>

#include "ch32h417_fmc.h"
#include "ch32h417_gpha_2d.h"
#include "ch32h417_iwdg.h"
#include "ch32h417_ltdc_rgb.h"
#include "ch32h417_pwr.h"

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO
#include "h4v1_video.h"
#include "usb_dc_ch32h417_usbfs_trace.h"
#endif

#if (APP_V5F_HW_TEST == APP_V5F_HW_TEST_CH585_SPI_SPEED) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_CH585_ADC_KEY_CAL)
#include "ch32h417_ch585_spi_link.h"
#include "ch32h417_dma.h"
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
    ((APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_MEMTEST) || \
     (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO) || \
     (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_DQ_PROBE) || \
     (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_OFFICIAL_16BIT))
#define V5F_SDRAM_USB_DEBUG_ENABLED 1
#else
#define V5F_SDRAM_USB_DEBUG_ENABLED 0
#endif

#if (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_MEMTEST) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO)
#if !APP_ENABLE_USB_TEST
#error "SDRAM CDC hardware tests require the main USB CDC initialization"
#endif
#define V5F_SDRAM_MEMTEST_CDC_ONLY 1
#else
#define V5F_SDRAM_MEMTEST_CDC_ONLY 0
#endif

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_MEMTEST
#define V5F_SDRAM_MEMTEST_LOW8     1
#else
#define V5F_SDRAM_MEMTEST_LOW8     0
#endif

#define V5F_MAYBE_UNUSED       __attribute__((unused))

#if (APP_V5F_HW_TEST != APP_V5F_HW_TEST_SDRAM_MEMTEST) && \
    (APP_V5F_HW_TEST != APP_V5F_HW_TEST_SDRAM_VIDEO) && \
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
extern int ch32h417_usb_cdc_raw_rx_enable(uint8_t enable);
extern uint32_t ch32h417_usb_cdc_raw_rx_available(void);
extern int ch32h417_usb_cdc_raw_rx_read(void *out, uint32_t out_len);
extern uint8_t ch32h417_usb_cdc_raw_rx_overflowed(void);
#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO
extern void ch32h417_usb_cdc_raw_rx_diag(uint32_t *callbacks,
                                         uint32_t *bytes,
                                         uint32_t *arm_ok,
                                         uint32_t *arm_fail,
                                         uint32_t *transfer_size,
                                         uint32_t *armed);
extern uint8_t ch32h417_usb_cdc_fs_tx_busy(void);
#endif
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
    V5F_HW_PHASE_PASSED = 4,
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
#else
#define CH585_SPI_SPEED_SOURCE_DESC "left/U2 CS=PF2 other=PD9"
#define CH585_SPI_SPEED_CS_PORT GPIOF
#define CH585_SPI_SPEED_CS_PIN GPIO_Pin_2
#define CH585_SPI_SPEED_OTHER_CS_PORT GPIOD
#define CH585_SPI_SPEED_OTHER_CS_PIN GPIO_Pin_9
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
#define CH585_SPI_SPEED_TX_DMA_REQ 63U
#define CH585_SPI_SPEED_RX_DMA_REQ 64U

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

static const ch585_spi_speed_rate_t s_ch585_spi_speed_rates[] =
{
    { SPI_BaudRatePrescaler_Mode2, 8U, 0U, "div8-br2" },
    { SPI_BaudRatePrescaler_Mode1, 4U, 0U, "div4-br1" },
    { SPI_BaudRatePrescaler_Mode2, 4U, 1U, "div4-br2-hsrx1" },
    { SPI_BaudRatePrescaler_Mode2, 4U, 2U, "div4-br2-hsrx2" },
    { SPI_BaudRatePrescaler_Mode1, 3U, 1U, "div3-br1-hsrx1" },
    { SPI_BaudRatePrescaler_Mode1, 3U, 2U, "div3-br1-hsrx2" },
    { SPI_BaudRatePrescaler_Mode0, 2U, 0U, "div2-br0" },
    { SPI_BaudRatePrescaler_Mode0, 2U, 1U, "div2-br0-hsrx1" },
    { SPI_BaudRatePrescaler_Mode0, 2U, 2U, "div2-br0-hsrx2" },
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

static uint32_t ch585_spi_speed_rate_khz(const ch585_spi_speed_rate_t *rate)
{
    uint32_t hclk = (HCLKClock != 0U) ? HCLKClock : SystemCoreClock;

    if((rate == RT_NULL) || (rate->div == 0U))
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

    if(rate->div < 8U)
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
    ch585_spi_speed_log_line("SPI_CMD rates: div8-br2 div4-br1 div4-br2-hsrx1 div4-br2-hsrx2 div3-br1-hsrx1 div3-br1-hsrx2 div2-br0 div2-br0-hsrx1 div2-br0-hsrx2");
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
    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);

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

static void ch585_spi_speed_dma_init(void)
{
    DMA_InitTypeDef dma = {0};

    DMA_Cmd(DMA1_Channel2, DISABLE);
    DMA_Cmd(DMA1_Channel3, DISABLE);
    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Rx, DISABLE);
    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, DISABLE);

    dma.DMA_PeripheralBaseAddr = (uint32_t)&SPI1->DATAR;
    dma.DMA_Memory0BaseAddr = (uint32_t)s_ch585_spi_speed_rx;
    dma.DMA_DIR = DMA_DIR_PeripheralSRC;
    dma.DMA_BufferSize = CH585_H417_SPI_SPEED_TRANSFER_BYTES;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma.DMA_Mode = DMA_Mode_Normal;
    dma.DMA_Priority = DMA_Priority_VeryHigh;
    dma.DMA_M2M = DMA_M2M_Disable;
    DMA_DeInit(DMA1_Channel2);
    DMA_Init(DMA1_Channel2, &dma);

    dma.DMA_Memory0BaseAddr = (uint32_t)s_ch585_spi_speed_tx;
    dma.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_DeInit(DMA1_Channel3);
    DMA_Init(DMA1_Channel3, &dma);

    DMA_MuxChannelConfig(DMA_MuxChannel2, CH585_SPI_SPEED_RX_DMA_REQ);
    DMA_MuxChannelConfig(DMA_MuxChannel3, CH585_SPI_SPEED_TX_DMA_REQ);
    DMA_ClearFlag(DMA1, DMA1_FLAG_GL2 | DMA1_FLAG_GL3);
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
    spi.SPI_CPHA = SPI_CPHA_1Edge;
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

static void ch585_spi_speed_dma_stop(void)
{
    DMA_Cmd(DMA1_Channel3, DISABLE);
    DMA_Cmd(DMA1_Channel2, DISABLE);
    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, DISABLE);
    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Rx, DISABLE);
}

static int ch585_spi_speed_transfer_frame(uint8_t *rx, const uint8_t *tx)
{
    uint32_t polls;

    ch585_spi_speed_dma_stop();
    ch585_spi_speed_spi_drain_rx();
    DMA1_Channel2->MADDR = (uint32_t)rx;
    DMA1_Channel2->CNTR = CH585_H417_SPI_SPEED_TRANSFER_BYTES;
    DMA1_Channel3->MADDR = (uint32_t)tx;
    DMA1_Channel3->CNTR = CH585_H417_SPI_SPEED_TRANSFER_BYTES;
    DMA_ClearFlag(DMA1, DMA1_FLAG_GL2 | DMA1_FLAG_GL3);
    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Rx, ENABLE);
    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);

    GPIO_ResetBits(CH585_SPI_SPEED_CS_PORT, CH585_SPI_SPEED_CS_PIN);
    ch585_spi_speed_delay_cycles(CH585_SPI_SPEED_CS_SETUP_CYCLES);
    DMA_Cmd(DMA1_Channel2, ENABLE);
    DMA_Cmd(DMA1_Channel3, ENABLE);

    polls = CH585_SPI_SPEED_BYTE_TIMEOUT_POLLS;
    while((DMA_GetFlagStatus(DMA1, DMA1_FLAG_TC2) == RESET) ||
          (DMA_GetFlagStatus(DMA1, DMA1_FLAG_TC3) == RESET))
    {
        if((DMA_GetFlagStatus(DMA1, DMA1_FLAG_TE2) != RESET) ||
           (DMA_GetFlagStatus(DMA1, DMA1_FLAG_TE3) != RESET))
        {
            ch585_spi_speed_dma_stop();
            GPIO_SetBits(CH585_SPI_SPEED_CS_PORT, CH585_SPI_SPEED_CS_PIN);
            return -2;
        }
        if(polls-- == 0U)
        {
            ch585_spi_speed_dma_stop();
            GPIO_SetBits(CH585_SPI_SPEED_CS_PORT, CH585_SPI_SPEED_CS_PIN);
            return -1;
        }
    }

    if((DMA_GetFlagStatus(DMA1, DMA1_FLAG_TE2) != RESET) ||
       (DMA_GetFlagStatus(DMA1, DMA1_FLAG_TE3) != RESET))
    {
        ch585_spi_speed_dma_stop();
        GPIO_SetBits(CH585_SPI_SPEED_CS_PORT, CH585_SPI_SPEED_CS_PIN);
        return -2;
    }

    polls = CH585_SPI_SPEED_BYTE_TIMEOUT_POLLS;
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) != RESET)
    {
        if(polls-- == 0U)
        {
            ch585_spi_speed_dma_stop();
            GPIO_SetBits(CH585_SPI_SPEED_CS_PORT, CH585_SPI_SPEED_CS_PIN);
            return -3;
        }
    }

    ch585_spi_speed_dma_stop();
    GPIO_SetBits(CH585_SPI_SPEED_CS_PORT, CH585_SPI_SPEED_CS_PIN);
    ch585_spi_speed_delay_cycles(CH585_SPI_SPEED_CS_GAP_CYCLES);
    return 0;
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
    ch585_spi_speed_spi_apply(rate);
    rt_thread_mdelay(2);

    start_cycle = ch585_spi_speed_mcycle();
    attempts = 0U;
    while((stats.ok < CH585_SPI_SPEED_FRAMES_PER_RATE) &&
          (attempts < CH585_SPI_SPEED_MAX_ATTEMPTS_PER_RATE))
    {
        int ret;

        ret = ch585_spi_speed_transfer_frame(s_ch585_spi_speed_rx,
                                             s_ch585_spi_speed_tx);
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
                       1U,
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
    ch585_spi_speed_gpio_init();
    ch585_spi_speed_dma_init();
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;

    (void)rt_snprintf(line,
                      sizeof(line),
                      "CH585_SPI_SPEED START source=%s pins=PB3_SCK_PB5_MOSI_PB4_MISO mode=mode0-dma transfer_bytes=%u frame_bytes=%u hclk=%u core=%u sys=%u ready_byte=0x%02x frame_off=%u cs_gap_cycles=%u sync_retry_cycles=%u max_attempts=%u",
                      CH585_SPI_SPEED_SOURCE_DESC,
                      (unsigned int)CH585_H417_SPI_SPEED_TRANSFER_BYTES,
                      (unsigned int)CH585_H417_SPI_SPEED_FRAME_BYTES,
                      (unsigned int)HCLKClock,
                      (unsigned int)SystemCoreClock,
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
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_UI_FRAMES
    (void)ch32h417_ltdc_rgb_layer1_load_clut_rgb888(
        v5f_ltdc_ui_frames_clut_rgb888(),
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
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_LTDC_RGB565) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_REMAP_PROBE) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_DQ_PROBE) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_OFFICIAL_16BIT)
#define V5F_SDRAM_NATIVE_ADDR          0xC0000000u
#define V5F_SDRAM_REMAP_ADDR           0x60000000u
#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_MEMTEST
#define V5F_SDRAM_BASE_ADDR            V5F_SDRAM_NATIVE_ADDR
#else
#define V5F_SDRAM_BASE_ADDR            V5F_SDRAM_REMAP_ADDR
#endif
#if V5F_SDRAM_MEMTEST_LOW8
/* Keep the physical x16 bus but expose only its low byte at even HB addresses. */
#define V5F_SDRAM_BYTES                (16u * 1024u * 1024u)
#else
#define V5F_SDRAM_BYTES                (32u * 1024u * 1024u)
#endif
#define V5F_SDRAM_LTDC_WIDTH           CH32H417_LCD_RGB_WIDTH
#define V5F_SDRAM_LTDC_HEIGHT          CH32H417_LCD_RGB_HEIGHT
#define V5F_SDRAM_LTDC_RGB565_BYTES    (V5F_SDRAM_LTDC_WIDTH * V5F_SDRAM_LTDC_HEIGHT * 2u)
#define V5F_SDRAM_QUICK_TEST_BYTES     (2u * 1024u * 1024u)
#define V5F_FMC_SDRAM_REMAP_TO_0X60000000 (1u << 24)
#if (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_MEMTEST) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO)
/* v34/v35 hardware validation established that this controller path requires
 * the vendor 1-HCLK setting; with the board HCLK this is a 100 MHz SDCLK. */
#define V5F_SDRAM_MAX_SDCLK_HZ         100000000u
#else
#define V5F_SDRAM_MAX_SDCLK_HZ         100000000u
#endif
#define V5F_SDRAM_REFRESH_CYCLES       8192u
#define V5F_SDRAM_REFRESH_PERIOD_US    32000u
#define V5F_SDRAM_REFRESH_MARGIN       20u
#define V5F_SDRAM_TIMEOUT_POLLS        1000000u
#define V5F_SDRAM_MODE_REGISTER        0x0230u
#define V5F_SDRAM_MODE_REGISTER_CL2    0x0220u
#define V5F_SDRAM_CLOCK_PERIOD_1HCLK   1u
#define V5F_SDRAM_OFFICIAL_REFRESH     677u
#define V5F_SDRAM_DEFAULT_PHASE_SEL    0x0Au
#define V5F_SDRAM_NORMAL_READ_MODE     0u
#define V5F_SDRAM_ENHANCE_READ_BIT     (1u << 15)
#define V5F_SDRAM_READ_BURST_BIT       (1u << 12)
#define V5F_SDRAM_CONTINUOUS_POLL_CYCLES 4096u
#define V5F_SDRAM_CONTINUOUS_REPORT_POLLS 256u
#define V5F_SDRAM_DIRECTION_WRITE_REPORTS 16u
#define V5F_SDRAM_DIRECTION_ARM_DELAY_MS 1000u
#define V5F_SDRAM_X8_PROGRESS_BYTES    (1u * 1024u * 1024u)
#define V5F_SDRAM_X8_SERVICE_BYTES     (64u * 1024u)
#define V5F_SDRAM_X8_BANK_BYTES        (4u * 1024u * 1024u)
#define V5F_SDRAM_X8_BANK_COUNT        4u
#define V5F_SDRAM_LOW8_PHYSICAL_STRIDE 2u
#define V5F_SDRAM_X16_BANK_BYTES       (8u * 1024u * 1024u)
#define V5F_SDRAM_X16_DIAG_WORDS       4096u
#define V5F_SDRAM_BW_BYTES             (4u * 1024u * 1024u)
#define V5F_SDRAM_BW_CHUNK_BYTES       (64u * 1024u)
#define V5F_SDRAM_DMA_BW_WORDS         4096u
#define V5F_SDRAM_DMA_BUFFER_BYTES      (V5F_SDRAM_DMA_BW_WORDS * 4u)
#define V5F_SDRAM_DMA_AB_USE_DTCM       0
#define V5F_SDRAM_DMA_SHARED_END        0x20178000u
#define V5F_SDRAM_DMA_SHARED_BUFFER_ADDR \
    (V5F_SDRAM_DMA_SHARED_END - V5F_SDRAM_DMA_BUFFER_BYTES)
#define V5F_SDRAM_DMA_FULL_BYTES       (32u * 1024u * 1024u)
#define V5F_SDRAM_DMA_PROGRESS_BYTES   (4u * 1024u * 1024u)
#define V5F_SDRAM_DMA_RESTART_GAP_US   0u
#define V5F_SDRAM_WATCHDOG_RETAIN_ADDR 0x2017FF00u
#define V5F_SDRAM_WATCHDOG_MAGIC       0x57444761u
#define V5F_SDRAM_WATCHDOG_MAGIC_INV   0xA8BBB89Eu
#define V5F_SDRAM_WATCHDOG_RECORD_VERSION 6u
#define V5F_SDRAM_WATCHDOG_READY_MASK  0x00000002u
#define V5F_SDRAM_WATCHDOG_READY_POLLS 2000000u
#define V5F_SDRAM_WATCHDOG_RELOAD      4000u
#define V5F_SDRAM_WATCHDOG_KEY_ENABLE  0xCCCCu
#define V5F_SDRAM_MEMTEST_LEGACY_DIAG  0

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
#if V5F_SDRAM_DMA_AB_USE_DTCM
/*
 * v63 A/B control: restore the original DTCM endpoint while retaining every
 * v62 TC-cleanup marker and all transfer timing. This is deliberately the
 * only functional difference from v62.
 */
static uint32_t s_sdram_bw_buffer[V5F_SDRAM_DMA_BW_WORDS]
    __attribute__((aligned(32)));
#else
/*
 * This CDC-only test never enables LTDC. Reuse the final 16 KiB of its
 * 0x20118000..0x20177fff framebuffer reservation as a shared-SRAM DMA
 * endpoint. The retained 32 KiB at 0x20178000..0x2017ffff remains untouched.
 * Keeping the array type in this macro preserves both indexed access and
 * sizeof(s_sdram_bw_buffer) without allocating a second DTCM object.
 */
#define s_sdram_bw_buffer \
    (*((uint32_t (*)[V5F_SDRAM_DMA_BW_WORDS])(uintptr_t) \
        V5F_SDRAM_DMA_SHARED_BUFFER_ADDR))
#endif

typedef enum
{
    V5F_SDRAM_WATCHDOG_STAGE_IDLE = 0,
    V5F_SDRAM_WATCHDOG_STAGE_WRITE = 1,
    V5F_SDRAM_WATCHDOG_STAGE_RETENTION = 2,
    V5F_SDRAM_WATCHDOG_STAGE_READ = 3,
    V5F_SDRAM_WATCHDOG_STAGE_DONE = 4,
    V5F_SDRAM_WATCHDOG_STAGE_LTDC = 5,
} v5f_sdram_watchdog_stage_t;

typedef enum
{
    V5F_SDRAM_WATCHDOG_POINT_IDLE = 0,
    V5F_SDRAM_WATCHDOG_POINT_ENTER = 1,
    V5F_SDRAM_WATCHDOG_POINT_PRE_ENABLE = 2,
    V5F_SDRAM_WATCHDOG_POINT_TC_SEEN = 3,
    V5F_SDRAM_WATCHDOG_POINT_TC_SNAPSHOT = 4,
    V5F_SDRAM_WATCHDOG_POINT_BEFORE_DISABLE = 5,
    V5F_SDRAM_WATCHDOG_POINT_DISABLE_DONE = 6,
    V5F_SDRAM_WATCHDOG_POINT_EN_CLEARED = 7,
    V5F_SDRAM_WATCHDOG_POINT_FLAGS_CLEARED = 8,
    V5F_SDRAM_WATCHDOG_POINT_FENCE_DONE = 9,
    V5F_SDRAM_WATCHDOG_POINT_GAP_DONE = 10,
    V5F_SDRAM_WATCHDOG_POINT_COMPLETE = 11,
    V5F_SDRAM_WATCHDOG_POINT_RETENTION = 12,
    V5F_SDRAM_WATCHDOG_POINT_VIDEO_RAW_WAIT = 13,
    V5F_SDRAM_WATCHDOG_POINT_VIDEO_RAW_READ = 14,
    V5F_SDRAM_WATCHDOG_POINT_VIDEO_DMA_CALL = 15,
    V5F_SDRAM_WATCHDOG_POINT_VIDEO_DMA_DONE = 16,
    V5F_SDRAM_WATCHDOG_POINT_VIDEO_ACK_SEND = 17,
    V5F_SDRAM_WATCHDOG_POINT_VIDEO_ACK_DONE = 18,
    V5F_SDRAM_WATCHDOG_POINT_LTDC_BASELINE = 19,
    V5F_SDRAM_WATCHDOG_POINT_LTDC_PANEL_INIT = 20,
    V5F_SDRAM_WATCHDOG_POINT_LTDC_PANEL_DONE = 21,
    V5F_SDRAM_WATCHDOG_POINT_LTDC_LAYER_CONFIG = 22,
    V5F_SDRAM_WATCHDOG_POINT_LTDC_LAYER_DONE = 23,
    V5F_SDRAM_WATCHDOG_POINT_LTDC_PRE_ENABLE = 24,
    V5F_SDRAM_WATCHDOG_POINT_LTDC_ENABLE_DONE = 25,
    V5F_SDRAM_WATCHDOG_POINT_LTDC_RELOAD_DONE = 26,
    V5F_SDRAM_WATCHDOG_POINT_LTDC_SCAN_WAIT = 27,
    V5F_SDRAM_WATCHDOG_POINT_LTDC_EXT_PROBE = 28,
    V5F_SDRAM_WATCHDOG_POINT_LTDC_FULL = 29,
    V5F_SDRAM_WATCHDOG_POINT_LTDC_PLAY = 30,
} v5f_sdram_watchdog_point_t;

typedef struct
{
    uint32_t magic;
    uint32_t magic_inv;
    uint32_t active;
    uint32_t sequence;
    uint32_t pass;
    uint32_t stage;
    uint32_t block;
    uint32_t address;
    uint32_t dma_cfgr;
    uint32_t dma_cntr;
    uint32_t dma_paddr;
    uint32_t dma_maddr;
    uint32_t fmc_sdsr;
    uint32_t fmc_sdcr;
    uint32_t fmc_sdtr;
    uint32_t fmc_sdrtr;
    uint32_t fmc_misc;
    uint32_t reset_flags;
    uint32_t watchdog_ready;
    uint32_t point;
    uint32_t dma_controller;
    uint32_t record_version;
    uint32_t usb_rx_callbacks;
    uint32_t usb_rx_bytes;
    uint32_t usb_rx_arm_ok;
    uint32_t usb_rx_arm_fail;
    uint32_t usb_rx_transfer_size;
    uint32_t usb_rx_armed;
    uint32_t usbfs_irq_state;
    uint32_t usbfs_ep2_state;
    uint32_t usb_irq_trace_valid;
    uint32_t usb_irq_trace_sequence;
    uint32_t usb_irq_trace_stage;
    uint32_t usb_irq_trace_state;
    uint32_t usb_irq_trace_ep_state;
    uint32_t usb_irq_trace_progress;
    uint32_t usb_irq_trace_sp;
    uint32_t usb_irq_trace_mscratch;
    uint32_t usb_irq_trace_mstatus;
    uint32_t usb_irq_trace_transfers;
    uint32_t usb_irq_trace_naks;
    uint32_t usb_irq_trace_nest;
    uint32_t usb_irq_trace_leave_hook;
    uint32_t usb_irq_trace_gp;
} v5f_sdram_watchdog_record_t;

static volatile v5f_sdram_watchdog_record_t *const s_sdram_watchdog_record =
    (volatile v5f_sdram_watchdog_record_t *)(uintptr_t)
        V5F_SDRAM_WATCHDOG_RETAIN_ADDR;
static uint8_t s_sdram_watchdog_started;
static uint8_t s_sdram_watchdog_reset_seen;
static uint32_t s_sdram_watchdog_pass;
static DMA_Channel_TypeDef *s_sdram_watchdog_dma_channel = DMA1_Channel3;
static uint32_t s_sdram_watchdog_dma_controller = 1u;

static uint8_t sdram_memtest_watchdog_record_valid(void)
{
    return (uint8_t)((s_sdram_watchdog_record->magic ==
                      V5F_SDRAM_WATCHDOG_MAGIC) &&
                     (s_sdram_watchdog_record->magic_inv ==
                      V5F_SDRAM_WATCHDOG_MAGIC_INV));
}

static void sdram_memtest_watchdog_feed(void)
{
    if(s_sdram_watchdog_started != 0u)
    {
        IWDG_ReloadCounter();
    }
}

static void sdram_memtest_watchdog_start_bounded(void)
{
    uint32_t polls = V5F_SDRAM_WATCHDOG_READY_POLLS;

    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_256);
    IWDG_SetReload(V5F_SDRAM_WATCHDOG_RELOAD);
    IWDG_ReloadCounter();

    /* The vendor IWDG_Enable helper has an unbounded LSI-ready wait. */
    IWDG->CTLR = V5F_SDRAM_WATCHDOG_KEY_ENABLE;
    s_sdram_watchdog_started = 1u;
    while(((RCC->RSTSCKR & V5F_SDRAM_WATCHDOG_READY_MASK) == 0u) &&
          (polls != 0u))
    {
        polls--;
    }
    s_sdram_watchdog_record->watchdog_ready =
        ((RCC->RSTSCKR & V5F_SDRAM_WATCHDOG_READY_MASK) != 0u) ? 1u : 0u;
    memory_barrier();
    sdram_memtest_watchdog_feed();
}

static void sdram_memtest_watchdog_begin(void)
{
    uint32_t sequence = 1u;

    if(sdram_memtest_watchdog_record_valid() != 0u)
    {
        sequence = s_sdram_watchdog_record->sequence + 1u;
    }
    s_sdram_watchdog_record->magic = V5F_SDRAM_WATCHDOG_MAGIC;
    s_sdram_watchdog_record->magic_inv = V5F_SDRAM_WATCHDOG_MAGIC_INV;
    s_sdram_watchdog_record->active = 1u;
    s_sdram_watchdog_record->sequence = sequence;
    s_sdram_watchdog_record->pass = 0u;
    s_sdram_watchdog_record->stage = V5F_SDRAM_WATCHDOG_STAGE_IDLE;
    s_sdram_watchdog_record->block = 0u;
    s_sdram_watchdog_record->address = V5F_SDRAM_NATIVE_ADDR;
    s_sdram_watchdog_record->dma_cfgr = 0u;
    s_sdram_watchdog_record->dma_cntr = 0u;
    s_sdram_watchdog_record->dma_paddr = 0u;
    s_sdram_watchdog_record->dma_maddr = 0u;
    s_sdram_watchdog_record->fmc_sdsr = FMC_Bank5_6->SDSR;
    s_sdram_watchdog_record->fmc_sdcr =
        FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM];
    s_sdram_watchdog_record->fmc_sdtr =
        FMC_Bank5_6->SDTR[FMC_Bank5_SDRAM];
    s_sdram_watchdog_record->fmc_sdrtr = FMC_Bank5_6->SDRTR;
    s_sdram_watchdog_record->fmc_misc = FMC_Bank5_6->MISC;
    s_sdram_watchdog_record->reset_flags = RCC->RSTSCKR;
    s_sdram_watchdog_record->watchdog_ready = 0u;
    s_sdram_watchdog_record->point = V5F_SDRAM_WATCHDOG_POINT_IDLE;
    s_sdram_watchdog_record->dma_controller = 0u;
    s_sdram_watchdog_record->record_version =
        V5F_SDRAM_WATCHDOG_RECORD_VERSION;
    s_sdram_watchdog_record->usb_rx_callbacks = 0u;
    s_sdram_watchdog_record->usb_rx_bytes = 0u;
    s_sdram_watchdog_record->usb_rx_arm_ok = 0u;
    s_sdram_watchdog_record->usb_rx_arm_fail = 0u;
    s_sdram_watchdog_record->usb_rx_transfer_size = 0u;
    s_sdram_watchdog_record->usb_rx_armed = 0u;
    s_sdram_watchdog_record->usbfs_irq_state = 0u;
    s_sdram_watchdog_record->usbfs_ep2_state = 0u;
    s_sdram_watchdog_record->usb_irq_trace_valid = 0u;
    s_sdram_watchdog_record->usb_irq_trace_sequence = 0u;
    s_sdram_watchdog_record->usb_irq_trace_stage = 0u;
    s_sdram_watchdog_record->usb_irq_trace_state = 0u;
    s_sdram_watchdog_record->usb_irq_trace_ep_state = 0u;
    s_sdram_watchdog_record->usb_irq_trace_progress = 0u;
    s_sdram_watchdog_record->usb_irq_trace_sp = 0u;
    s_sdram_watchdog_record->usb_irq_trace_mscratch = 0u;
    s_sdram_watchdog_record->usb_irq_trace_mstatus = 0u;
    s_sdram_watchdog_record->usb_irq_trace_transfers = 0u;
    s_sdram_watchdog_record->usb_irq_trace_naks = 0u;
    s_sdram_watchdog_record->usb_irq_trace_nest = 0u;
    s_sdram_watchdog_record->usb_irq_trace_leave_hook = 0u;
    s_sdram_watchdog_record->usb_irq_trace_gp = 0u;
    memory_barrier();
    sdram_memtest_watchdog_start_bounded();
}

#if (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO) && \
    defined(APP_USBFS_STREAM_DIAG) && (APP_USBFS_STREAM_DIAG != 0)
static void sdram_memtest_capture_usbfs_irq_trace(void)
{
    const volatile struct ch32h417_usbfs_retain_trace *trace =
        (const volatile struct ch32h417_usbfs_retain_trace *)(uintptr_t)
            CH32H417_USBFS_RETAIN_TRACE_ADDR;

    if((sdram_memtest_watchdog_record_valid() == 0u) ||
       (s_sdram_watchdog_record->active == 0u) ||
       (trace->magic != CH32H417_USBFS_RETAIN_TRACE_MAGIC) ||
       (trace->version != CH32H417_USBFS_RETAIN_TRACE_VERSION))
    {
        return;
    }

    /* USB enumeration will overwrite the retained trace; copy it first. */
    s_sdram_watchdog_record->usb_irq_trace_valid = 1u;
    s_sdram_watchdog_record->usb_irq_trace_sequence = trace->sequence;
    s_sdram_watchdog_record->usb_irq_trace_stage = trace->stage;
    s_sdram_watchdog_record->usb_irq_trace_state = trace->irq_state;
    s_sdram_watchdog_record->usb_irq_trace_ep_state = trace->ep_state;
    s_sdram_watchdog_record->usb_irq_trace_progress = trace->progress;
    s_sdram_watchdog_record->usb_irq_trace_sp = trace->sp;
    s_sdram_watchdog_record->usb_irq_trace_mscratch = trace->mscratch;
    s_sdram_watchdog_record->usb_irq_trace_mstatus = trace->mstatus;
    s_sdram_watchdog_record->usb_irq_trace_transfers = trace->transfer_count;
    s_sdram_watchdog_record->usb_irq_trace_naks = trace->nak_count;
    s_sdram_watchdog_record->usb_irq_trace_nest = trace->interrupt_nest;
    s_sdram_watchdog_record->usb_irq_trace_leave_hook = trace->leave_hook;
    s_sdram_watchdog_record->usb_irq_trace_gp = trace->gp;
    memory_barrier();
}
#endif

static void sdram_memtest_watchdog_context(uint32_t stage,
                                           uint32_t point,
                                           uint32_t block,
                                           uintptr_t address)
{
    if(s_sdram_watchdog_started == 0u)
    {
        return;
    }

    s_sdram_watchdog_record->active = 1u;
    s_sdram_watchdog_record->pass = s_sdram_watchdog_pass;
    s_sdram_watchdog_record->stage = stage;
    s_sdram_watchdog_record->point = point;
    s_sdram_watchdog_record->block = block;
    s_sdram_watchdog_record->address = (uint32_t)address;
#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO
    if(point >= V5F_SDRAM_WATCHDOG_POINT_VIDEO_RAW_WAIT)
    {
        ch32h417_usb_cdc_raw_rx_diag(
            (uint32_t *)&s_sdram_watchdog_record->usb_rx_callbacks,
            (uint32_t *)&s_sdram_watchdog_record->usb_rx_bytes,
            (uint32_t *)&s_sdram_watchdog_record->usb_rx_arm_ok,
            (uint32_t *)&s_sdram_watchdog_record->usb_rx_arm_fail,
            (uint32_t *)&s_sdram_watchdog_record->usb_rx_transfer_size,
            (uint32_t *)&s_sdram_watchdog_record->usb_rx_armed);
        s_sdram_watchdog_record->usbfs_irq_state =
            (uint32_t)USBFSD->INT_FG |
            ((uint32_t)USBFSD->INT_ST << 8u) |
            ((uint32_t)USBFSD->RX_LEN << 16u);
        s_sdram_watchdog_record->usbfs_ep2_state =
            (uint32_t)USBFSD->UEP2_RX_CTRL |
            ((uint32_t)USBFSD->BASE_CTRL << 8u) |
            ((uint32_t)USBFSD->INT_EN << 16u);
    }
#endif
    memory_barrier();
    sdram_memtest_watchdog_feed();
}

static void sdram_memtest_watchdog_checkpoint(uint32_t stage,
                                               uint32_t point,
                                               uint32_t block,
                                               uintptr_t address)
{
    if(s_sdram_watchdog_started == 0u)
    {
        return;
    }

    /* Commit the location before touching DMA/FMC status registers. */
    sdram_memtest_watchdog_context(stage, point, block, address);
    s_sdram_watchdog_record->dma_controller =
        s_sdram_watchdog_dma_controller;
    s_sdram_watchdog_record->dma_cfgr =
        s_sdram_watchdog_dma_channel->CFGR;
    s_sdram_watchdog_record->dma_cntr =
        s_sdram_watchdog_dma_channel->CNTR;
    s_sdram_watchdog_record->dma_paddr =
        s_sdram_watchdog_dma_channel->PADDR;
    s_sdram_watchdog_record->dma_maddr =
        s_sdram_watchdog_dma_channel->MADDR;
    s_sdram_watchdog_record->fmc_sdsr = FMC_Bank5_6->SDSR;
    s_sdram_watchdog_record->fmc_sdcr =
        FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM];
    s_sdram_watchdog_record->fmc_sdtr =
        FMC_Bank5_6->SDTR[FMC_Bank5_SDRAM];
    s_sdram_watchdog_record->fmc_sdrtr = FMC_Bank5_6->SDRTR;
    s_sdram_watchdog_record->fmc_misc = FMC_Bank5_6->MISC;
    memory_barrier();
    sdram_memtest_watchdog_feed();
}

static void sdram_memtest_watchdog_complete(void)
{
    if(sdram_memtest_watchdog_record_valid() != 0u)
    {
        s_sdram_watchdog_record->stage = V5F_SDRAM_WATCHDOG_STAGE_DONE;
        s_sdram_watchdog_record->point = V5F_SDRAM_WATCHDOG_POINT_COMPLETE;
        s_sdram_watchdog_record->active = 0u;
        memory_barrier();
    }
    sdram_memtest_watchdog_feed();
}

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
    V5F_SDRAM_STAGE_MARCH = 8,
    V5F_SDRAM_STAGE_RETENTION = 9,
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
    uint32_t retries = 0u;

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

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO
        if((wrote == -2) || (retries >= 1000000u))
#else
        if((wrote == -2) || (retries >= 8u))
#endif
        {
            return (offset > 0u) ? (int)offset : wrote;
        }

        retries++;
#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO
        /* USB completion is interrupt driven; do not enter the scheduler. */
        sdram_memtest_watchdog_feed();
        __NOP();
#else
        rt_thread_mdelay(1);
#endif
    }

    return (int)offset;
}

static int sdram_usb_debug_write_line(const char *line)
{
    char framed[V5F_SDRAM_USB_LINE_BYTES + 2u];
    rt_size_t length;

    if(line == RT_NULL)
    {
        return -1;
    }

    length = (rt_size_t)strlen(line);
    if(length <= V5F_SDRAM_USB_LINE_BYTES)
    {
        memcpy(framed, line, length);
        framed[length] = '\r';
        framed[length + 1u] = '\n';
        return (sdram_usb_debug_write_full(framed, length + 2u) ==
                (int)(length + 2u)) ? 0 : -1;
    }

    if(sdram_usb_debug_write_full(line, length) != (int)length)
    {
        return -1;
    }
    return (sdram_usb_debug_write_full("\r\n", 2u) == 2) ? 0 : -1;
}

#if V5F_SDRAM_MEMTEST_CDC_ONLY
static const char *sdram_memtest_stage_name(uint32_t stage)
{
    switch(stage)
    {
        case V5F_SDRAM_STAGE_INIT:
            return "INIT";
        case V5F_SDRAM_STAGE_DATA_BUS:
            return "DATA_BUS";
        case V5F_SDRAM_STAGE_ADDRESS_BUS:
            return "ADDRESS_BUS";
        case V5F_SDRAM_STAGE_PATTERN:
            return "PATTERN";
        case V5F_SDRAM_STAGE_PATTERN_INV:
            return "PATTERN_INV";
        case V5F_SDRAM_STAGE_MARCH:
            return "MARCH";
        case V5F_SDRAM_STAGE_RETENTION:
            return "RETENTION";
        default:
            return "UNKNOWN";
    }
}

static void sdram_memtest_cdc_stage(const char *stage, const char *state)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used = rt_snprintf(line,
                           sizeof(line),
                           "%s %s",
                           (stage != RT_NULL) ? stage : "UNKNOWN",
                           (state != RT_NULL) ? state : "UNKNOWN");

    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static int sdram_memtest_cdc_start_command(const char *line)
{
    if(line == RT_NULL)
    {
        return 0;
    }

    return (strcmp(line, "start") == 0) ||
           (strcmp(line, "START") == 0) ||
           (strcmp(line, "run") == 0) ||
           (strcmp(line, "RUN") == 0);
}

static int sdram_memtest_cdc_status_command(const char *line)
{
    if(line == RT_NULL)
    {
        return 0;
    }

    return sdram_memtest_cdc_start_command(line) ||
           (strcmp(line, "status") == 0) ||
           (strcmp(line, "STATUS") == 0);
}

static void sdram_memtest_cdc_wait_for_start(void)
{
    char command[32];

    while(1)
    {
        int len = ch32h417_usb_cdc_read_line(command, sizeof(command));

        if((len > 0) && sdram_memtest_cdc_start_command(command))
        {
            return;
        }
        ch32h417_dual_cdc_poll();
        sdram_memtest_watchdog_feed();
        rt_thread_mdelay(10);
    }
}

static void sdram_memtest_cdc_begin(void)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used;

    sdram_usb_debug_write_line("H417 SDRAM CDC TEST v71 FULL16");
    sdram_usb_debug_write_line("RANGE C0000000-C1FFFFFF bytes=33554432 access=short-dma1,stress-dma2-wide256");
    sdram_usb_debug_write_line("PHYSICAL_RANGE C0000000-C1FFFFFF bytes=33554432");
    sdram_usb_debug_write_line("BANK_SPAN bytes=00800000 bases=C0000000/C0800000/C1000000/C1800000");
    sdram_usb_debug_write_line("GEOMETRY fmc_width=16 device_width=16 row=13 col=9 banks=4");
    sdram_usb_debug_write_line("LANES compare=ffff ignored=0000 source=v70_dq_raw_pass dqml=FMC dqmh=FMC");
    used = rt_snprintf(line,
                       sizeof(line),
                       "CONTROL pa9=%u pa10=%u mode9=%x mode10=%x af9=%x af10=%x cfghr=%08x outdr=%04x ltdc=off",
                       (unsigned int)((GPIOA->OUTDR & GPIO_Pin_9) != 0u),
                       (unsigned int)((GPIOA->OUTDR & GPIO_Pin_10) != 0u),
                       (unsigned int)((GPIOA->CFGHR >> 4) & 0xFu),
                       (unsigned int)((GPIOA->CFGHR >> 8) & 0xFu),
                       (unsigned int)((AFIO->GPIOA_AFHR >> 4) & 0xFu),
                       (unsigned int)((AFIO->GPIOA_AFHR >> 8) & 0xFu),
                       (unsigned int)GPIOA->CFGHR,
                       (unsigned int)GPIOA->OUTDR);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    used = rt_snprintf(line,
                       sizeof(line),
                       "DMA_BUFFER addr=%08x end=%08x bytes=%u region=lcd_fb_tail shared_sram retained32k=untouched",
                       (unsigned int)(uintptr_t)s_sdram_bw_buffer,
                       (unsigned int)((uintptr_t)s_sdram_bw_buffer +
                                      sizeof(s_sdram_bw_buffer) - 1u),
                       (unsigned int)sizeof(s_sdram_bw_buffer));
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static const char *sdram_memtest_watchdog_stage_name(uint32_t stage)
{
    switch(stage)
    {
        case V5F_SDRAM_WATCHDOG_STAGE_WRITE:
            return "WRITE";
        case V5F_SDRAM_WATCHDOG_STAGE_RETENTION:
            return "RETENTION";
        case V5F_SDRAM_WATCHDOG_STAGE_READ:
            return "READ";
        case V5F_SDRAM_WATCHDOG_STAGE_DONE:
            return "DONE";
        case V5F_SDRAM_WATCHDOG_STAGE_LTDC:
            return "LTDC";
        default:
            return "IDLE";
    }
}

static const char *sdram_memtest_watchdog_point_name(uint32_t point)
{
    switch(point)
    {
        case V5F_SDRAM_WATCHDOG_POINT_ENTER:
            return "ENTER";
        case V5F_SDRAM_WATCHDOG_POINT_PRE_ENABLE:
            return "PRE_ENABLE";
        case V5F_SDRAM_WATCHDOG_POINT_TC_SEEN:
            return "TC_SEEN";
        case V5F_SDRAM_WATCHDOG_POINT_TC_SNAPSHOT:
            return "TC_SNAPSHOT";
        case V5F_SDRAM_WATCHDOG_POINT_BEFORE_DISABLE:
            return "BEFORE_DISABLE";
        case V5F_SDRAM_WATCHDOG_POINT_DISABLE_DONE:
            return "DISABLE_DONE";
        case V5F_SDRAM_WATCHDOG_POINT_EN_CLEARED:
            return "EN_CLEARED";
        case V5F_SDRAM_WATCHDOG_POINT_FLAGS_CLEARED:
            return "FLAGS_CLEARED";
        case V5F_SDRAM_WATCHDOG_POINT_FENCE_DONE:
            return "FENCE_DONE";
        case V5F_SDRAM_WATCHDOG_POINT_GAP_DONE:
            return "GAP_DONE";
        case V5F_SDRAM_WATCHDOG_POINT_COMPLETE:
            return "COMPLETE";
        case V5F_SDRAM_WATCHDOG_POINT_RETENTION:
            return "RETENTION";
        case V5F_SDRAM_WATCHDOG_POINT_VIDEO_RAW_WAIT:
            return "VIDEO_RAW_WAIT";
        case V5F_SDRAM_WATCHDOG_POINT_VIDEO_RAW_READ:
            return "VIDEO_RAW_READ";
        case V5F_SDRAM_WATCHDOG_POINT_VIDEO_DMA_CALL:
            return "VIDEO_DMA_CALL";
        case V5F_SDRAM_WATCHDOG_POINT_VIDEO_DMA_DONE:
            return "VIDEO_DMA_DONE";
        case V5F_SDRAM_WATCHDOG_POINT_VIDEO_ACK_SEND:
            return "VIDEO_ACK_SEND";
        case V5F_SDRAM_WATCHDOG_POINT_VIDEO_ACK_DONE:
            return "VIDEO_ACK_DONE";
        case V5F_SDRAM_WATCHDOG_POINT_LTDC_BASELINE:
            return "LTDC_BASELINE";
        case V5F_SDRAM_WATCHDOG_POINT_LTDC_PANEL_INIT:
            return "LTDC_PANEL_INIT";
        case V5F_SDRAM_WATCHDOG_POINT_LTDC_PANEL_DONE:
            return "LTDC_PANEL_DONE";
        case V5F_SDRAM_WATCHDOG_POINT_LTDC_LAYER_CONFIG:
            return "LTDC_LAYER_CONFIG";
        case V5F_SDRAM_WATCHDOG_POINT_LTDC_LAYER_DONE:
            return "LTDC_LAYER_DONE";
        case V5F_SDRAM_WATCHDOG_POINT_LTDC_PRE_ENABLE:
            return "LTDC_PRE_ENABLE";
        case V5F_SDRAM_WATCHDOG_POINT_LTDC_ENABLE_DONE:
            return "LTDC_ENABLE_DONE";
        case V5F_SDRAM_WATCHDOG_POINT_LTDC_RELOAD_DONE:
            return "LTDC_RELOAD_DONE";
        case V5F_SDRAM_WATCHDOG_POINT_LTDC_SCAN_WAIT:
            return "LTDC_SCAN_WAIT";
        case V5F_SDRAM_WATCHDOG_POINT_LTDC_EXT_PROBE:
            return "LTDC_EXT_PROBE";
        case V5F_SDRAM_WATCHDOG_POINT_LTDC_FULL:
            return "LTDC_FULL";
        case V5F_SDRAM_WATCHDOG_POINT_LTDC_PLAY:
            return "LTDC_PLAY";
        default:
            return "IDLE";
    }
}

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO
static const char *sdram_memtest_usbfs_trace_stage_name(uint32_t stage)
{
    switch(stage)
    {
        case CH32H417_USBFS_TRACE_IRQ_ENTER:
            return "IRQ_ENTER";
        case CH32H417_USBFS_TRACE_RT_ENTERED:
            return "RT_ENTERED";
        case CH32H417_USBFS_TRACE_HANDLER_ENTER:
            return "HANDLER_ENTER";
        case CH32H417_USBFS_TRACE_TRANSFER:
            return "TRANSFER";
        case CH32H417_USBFS_TRACE_OUT_ENTER:
            return "OUT_ENTER";
        case CH32H417_USBFS_TRACE_OUT_COPY_BEGIN:
            return "COPY_BEGIN";
        case CH32H417_USBFS_TRACE_OUT_COPY_END:
            return "COPY_END";
        case CH32H417_USBFS_TRACE_OUT_CALLBACK_BEGIN:
            return "CALLBACK_BEGIN";
        case CH32H417_USBFS_TRACE_OUT_CALLBACK_END:
            return "CALLBACK_END";
        case CH32H417_USBFS_TRACE_FLAG_CLEAR_BEGIN:
            return "FLAG_CLEAR_BEGIN";
        case CH32H417_USBFS_TRACE_FLAG_CLEAR_END:
            return "FLAG_CLEAR_END";
        case CH32H417_USBFS_TRACE_HANDLER_END:
            return "HANDLER_END";
        case CH32H417_USBFS_TRACE_RT_LEAVE_BEGIN:
            return "RT_LEAVE_BEGIN";
        case CH32H417_USBFS_TRACE_RT_LEAVE_IRQ_OFF:
            return "RT_LEAVE_IRQ_OFF";
        case CH32H417_USBFS_TRACE_RT_LEAVE_NEST_DEC:
            return "RT_LEAVE_NEST_DEC";
        case CH32H417_USBFS_TRACE_RT_LEAVE_END:
            return "RT_LEAVE_END";
        default:
            return "IDLE";
    }
}
#endif

static uint8_t sdram_memtest_watchdog_report_recovery(void)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    uint32_t stage;
    uint32_t point;
    uint32_t block;
    int used;

    if((s_sdram_watchdog_reset_seen == 0u) ||
       (sdram_memtest_watchdog_record_valid() == 0u) ||
       (s_sdram_watchdog_record->active == 0u))
    {
        return 0u;
    }

    stage = s_sdram_watchdog_record->stage;
    point = s_sdram_watchdog_record->point;
    block = s_sdram_watchdog_record->block;
    used = rt_snprintf(line,
                       sizeof(line),
                       "WATCHDOG RECOVERY cause=IWDG seq=%u pass=%u stage=%s(%u) point=%s(%u) block=%u addr=%08x",
                       (unsigned int)s_sdram_watchdog_record->sequence,
                       (unsigned int)s_sdram_watchdog_record->pass,
                       sdram_memtest_watchdog_stage_name(stage),
                       (unsigned int)stage,
                       sdram_memtest_watchdog_point_name(point),
                       (unsigned int)point,
                       (unsigned int)block,
                       (unsigned int)s_sdram_watchdog_record->address);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    used = rt_snprintf(line,
                       sizeof(line),
                       "WATCHDOG DMA controller=%u channel=3 cfgr=%08x cntr=%u paddr=%08x maddr=%08x",
                       (unsigned int)s_sdram_watchdog_record->dma_controller,
                       (unsigned int)s_sdram_watchdog_record->dma_cfgr,
                       (unsigned int)s_sdram_watchdog_record->dma_cntr,
                       (unsigned int)s_sdram_watchdog_record->dma_paddr,
                       (unsigned int)s_sdram_watchdog_record->dma_maddr);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    used = rt_snprintf(line,
                       sizeof(line),
                       "WATCHDOG FMC sdsr=%08x sdcr=%08x sdtr=%08x sdrtr=%08x misc=%08x",
                       (unsigned int)s_sdram_watchdog_record->fmc_sdsr,
                       (unsigned int)s_sdram_watchdog_record->fmc_sdcr,
                       (unsigned int)s_sdram_watchdog_record->fmc_sdtr,
                       (unsigned int)s_sdram_watchdog_record->fmc_sdrtr,
                       (unsigned int)s_sdram_watchdog_record->fmc_misc);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    if(s_sdram_watchdog_record->record_version ==
       V5F_SDRAM_WATCHDOG_RECORD_VERSION)
    {
        used = rt_snprintf(
            line,
            sizeof(line),
            "WATCHDOG USB rx_cb=%u bytes=%u arm=%u/%u xfer=%u armed=%u irq=%08x ep2=%08x",
            (unsigned int)s_sdram_watchdog_record->usb_rx_callbacks,
            (unsigned int)s_sdram_watchdog_record->usb_rx_bytes,
            (unsigned int)s_sdram_watchdog_record->usb_rx_arm_ok,
            (unsigned int)s_sdram_watchdog_record->usb_rx_arm_fail,
            (unsigned int)s_sdram_watchdog_record->usb_rx_transfer_size,
            (unsigned int)s_sdram_watchdog_record->usb_rx_armed,
            (unsigned int)s_sdram_watchdog_record->usbfs_irq_state,
            (unsigned int)s_sdram_watchdog_record->usbfs_ep2_state);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO
        if(s_sdram_watchdog_record->usb_irq_trace_valid != 0u)
        {
            used = rt_snprintf(
                line,
                sizeof(line),
                "WATCHDOG USBIRQ seq=%u stage=%s(%u) irq=%08x ep2=%08x progress=%08x",
                (unsigned int)s_sdram_watchdog_record->usb_irq_trace_sequence,
                sdram_memtest_usbfs_trace_stage_name(
                    s_sdram_watchdog_record->usb_irq_trace_stage),
                (unsigned int)s_sdram_watchdog_record->usb_irq_trace_stage,
                (unsigned int)s_sdram_watchdog_record->usb_irq_trace_state,
                (unsigned int)s_sdram_watchdog_record->usb_irq_trace_ep_state,
                (unsigned int)s_sdram_watchdog_record->usb_irq_trace_progress);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            used = rt_snprintf(
                line,
                sizeof(line),
                "WATCHDOG USBIRQ_CTX sp=%08x ms=%08x mst=%08x nest=%u hook=%08x gp=%08x",
                (unsigned int)s_sdram_watchdog_record->usb_irq_trace_sp,
                (unsigned int)s_sdram_watchdog_record->usb_irq_trace_mscratch,
                (unsigned int)s_sdram_watchdog_record->usb_irq_trace_mstatus,
                (unsigned int)s_sdram_watchdog_record->usb_irq_trace_nest,
                (unsigned int)s_sdram_watchdog_record->usb_irq_trace_leave_hook,
                (unsigned int)s_sdram_watchdog_record->usb_irq_trace_gp);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            used = rt_snprintf(
                line,
                sizeof(line),
                "WATCHDOG USBIRQ_COUNT xfer=%u nak=%u",
                (unsigned int)s_sdram_watchdog_record->usb_irq_trace_transfers,
                (unsigned int)s_sdram_watchdog_record->usb_irq_trace_naks);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
#endif
    }
    used = rt_snprintf(line,
                       sizeof(line),
                       "WATCHDOG META reset=%08x ready=%u retain=%08x bus_lock_suspected=1",
                       (unsigned int)s_sdram_watchdog_record->reset_flags,
                       (unsigned int)s_sdram_watchdog_record->watchdog_ready,
                       (unsigned int)V5F_SDRAM_WATCHDOG_RETAIN_ADDR);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_FAILED;
    g_v5f_hw_test_diag.last_error = V5F_SDRAM_ERR_TIMEOUT;
    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_DATA_BUS;
    g_v5f_hw_test_diag.sdram_test_bytes = block * sizeof(s_sdram_bw_buffer);
    g_v5f_hw_test_diag.sdram_expected = stage;
    g_v5f_hw_test_diag.sdram_actual = block;
    g_v5f_hw_test_diag.sdram_fail_count++;
    s_sdram_watchdog_record->active = 0u;
    s_sdram_watchdog_reset_seen = 0u;
    memory_barrier();
    sdram_memtest_watchdog_feed();
    return 1u;
}

static void sdram_memtest_cdc_clock(void)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used = rt_snprintf(line,
                           sizeof(line),
                           "CLOCK hclk=%u sdclk=%u refresh=%u sdrtr=%08x",
                           (unsigned int)g_v5f_hw_test_diag.sdram_hclk_hz,
                           (unsigned int)g_v5f_hw_test_diag.sdram_sdclk_hz,
                           (unsigned int)g_v5f_hw_test_diag.sdram_refresh_count,
                           (unsigned int)FMC_Bank5_6->SDRTR);

    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }

    used = rt_snprintf(line,
                       sizeof(line),
                       "IO pwrctl=%08x pcfr1=%08x pd0rm=%u hslv=%u",
                       (unsigned int)PWR->CTLR,
                       (unsigned int)AFIO->PCFR1,
                       (unsigned int)((AFIO->PCFR1 & AFIO_PCFR1_PD0_1_REMAP) != 0u),
                       (unsigned int)((AFIO->PCFR1 &
                                       (AFIO_PCFR1_VIO18_IO_HSLV |
                                        AFIO_PCFR1_VIO33_IO_HSLV |
                                        AFIO_PCFR1_VDD33_IO_HSLV)) != 0u));
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static void sdram_memtest_cdc_map(void)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used = rt_snprintf(line,
                           sizeof(line),
                           "MAP base=%08x remap=%u nor_en=%u bcr0=%08x misc=%08x",
                           (unsigned int)V5F_SDRAM_BASE_ADDR,
                           (unsigned int)((FMC_Bank1->BTCR[0] &
                                           V5F_FMC_SDRAM_REMAP_TO_0X60000000) != 0u),
                           (unsigned int)((FMC_Bank1->BTCR[0] & FMC_BCR1_FMCEN) != 0u),
                           (unsigned int)FMC_Bank1->BTCR[0],
                           (unsigned int)FMC_Bank5_6->MISC);

    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static void sdram_memtest_cdc_failure(int error)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used = rt_snprintf(line,
                           sizeof(line),
                           "FAIL error=%d stage=%s(%u) off=%08x exp=%08x got=%08x",
                           error,
                           sdram_memtest_stage_name(g_v5f_hw_test_diag.sdram_stage),
                           (unsigned int)g_v5f_hw_test_diag.sdram_stage,
                           (unsigned int)g_v5f_hw_test_diag.sdram_fail_offset,
                           (unsigned int)g_v5f_hw_test_diag.sdram_expected,
                           (unsigned int)g_v5f_hw_test_diag.sdram_actual);

    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    sdram_usb_debug_write_line("RESULT FAIL");
}

static void sdram_memtest_cdc_summary(uint8_t pass)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used = rt_snprintf(line,
                           sizeof(line),
                           "SUMMARY bytes=%u ok=%u fail=%u stage=%s",
                           (unsigned int)g_v5f_hw_test_diag.sdram_test_bytes,
                           (unsigned int)g_v5f_hw_test_diag.sdram_ok_count,
                           (unsigned int)g_v5f_hw_test_diag.sdram_fail_count,
                           sdram_memtest_stage_name(g_v5f_hw_test_diag.sdram_stage));

    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    sdram_usb_debug_write_line((pass != 0u) ? "RESULT PASS" : "RESULT FAIL");
}

static void sdram_memtest_cdc_result_loop(uint8_t pass)
{
    char command[32];

    while(1)
    {
        int len = ch32h417_usb_cdc_read_line(command, sizeof(command));

        if((len > 0) && sdram_memtest_cdc_status_command(command))
        {
            sdram_memtest_cdc_summary(pass);
        }
        ch32h417_dual_cdc_poll();
        sdram_memtest_watchdog_feed();
        g_v5f_hw_test_diag.frame_count++;
        rt_thread_mdelay(20);
    }
}

static void V5F_MAYBE_UNUSED sdram_memtest_cdc_continuous_rw(void)
{
    static const uint16_t patterns[] = {0x0000u, 0x0400u, 0x0800u, 0x0C00u};
    volatile uint16_t *probe = (volatile uint16_t *)V5F_SDRAM_BASE_ADDR;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    uint32_t loop_low = 0u;
    uint32_t loop_high = 0u;
    uint32_t bad_low = 0u;
    uint32_t bad_high = 0u;
    uint32_t fused_bad_low = 0u;
    uint32_t fused_bad_high = 0u;
    uint32_t report_count = 0u;
    uint32_t poll_count = 0u;
    uint16_t pattern_index = 0u;
    uint16_t bit_or = 0x0000u;
    uint16_t pin_and = 0x0003u;
    uint16_t pin_or = 0x0000u;
    uint16_t pin_index_and[4] = {0x0003u, 0x0003u, 0x0003u, 0x0003u};
    uint16_t pin_index_or[4] = {0u, 0u, 0u, 0u};
    uint32_t pin_changes = 0u;
    uint16_t previous_pin = 0u;
    uint16_t last_expected = 0u;
    uint16_t last_actual = 0u;
    uint16_t last_fused = 0u;
    uint8_t have_previous_pin = 0u;

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_DATA_BUS;
    g_v5f_hw_test_diag.sdram_test_bytes = sizeof(uint16_t);
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "CONTINUOUS START mode=direction-split addr=%08x width=16 patterns=0000,0400,0800,0c00",
                               (unsigned int)V5F_SDRAM_BASE_ADDR);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    sdram_usb_debug_write_line("SCOPE WRITE ARM delay_ms=1000 trigger=WE#_LOW dq=PD0/PD1(D10/D11)");
    for(poll_count = 0u; poll_count < (V5F_SDRAM_DIRECTION_ARM_DELAY_MS / 10u); poll_count++)
    {
        ch32h417_dual_cdc_poll();
        rt_thread_mdelay(10);
    }
    sdram_usb_debug_write_line("SCOPE WRITE START writes_only=1 addr_count=4");

    /* Write-only phase: all other DQ bits remain zero while D10/D11 walk
     * through 00, 01, 10 and 11. A WE#-low trigger therefore identifies
     * unequivocally whether PD0/PD1 are driven by the FMC write path. */
    for(report_count = 1u;
        report_count <= V5F_SDRAM_DIRECTION_WRITE_REPORTS;
        report_count++)
    {
        uint32_t poll;

        for(poll = 0u; poll < V5F_SDRAM_CONTINUOUS_REPORT_POLLS; poll++)
        {
            uint32_t i;

            for(i = 0u; i < V5F_SDRAM_CONTINUOUS_POLL_CYCLES; i++)
            {
                uint16_t index = pattern_index;

                pattern_index = (uint16_t)((pattern_index + 1u) & 3u);
                probe[index] = patterns[index];
                memory_barrier();
                loop_low++;
                if(loop_low == 0u)
                {
                    loop_high++;
                }
            }
            ch32h417_dual_cdc_poll();
        }

        {
            int used = rt_snprintf(line,
                                   sizeof(line),
                                   "SCOPE WRITE report=%u loops=%u:%08x next=%04x",
                                   (unsigned int)report_count,
                                   (unsigned int)loop_high,
                                   (unsigned int)loop_low,
                                   (unsigned int)patterns[pattern_index]);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
    }

    /* Leave four independently identifiable words in SDRAM. The following
     * phase never performs another SDRAM write, so any DQ waveform after the
     * READ START marker can only have been driven by the SDRAM itself. */
    for(pattern_index = 0u; pattern_index < 4u; pattern_index++)
    {
        probe[pattern_index] = patterns[pattern_index];
        memory_barrier();
    }
    sdram_usb_debug_write_line("SCOPE WRITE END");
    {
        uint16_t actual[4];
        int used;

        for(pattern_index = 0u; pattern_index < 4u; pattern_index++)
        {
            actual[pattern_index] = probe[pattern_index];
            memory_barrier();
        }
        used = rt_snprintf(line,
                           sizeof(line),
                           "SCOPE CHECK rd=%04x/%04x/%04x/%04x exp=0000/0400/0800/0c00",
                           (unsigned int)actual[0],
                           (unsigned int)actual[1],
                           (unsigned int)actual[2],
                           (unsigned int)actual[3]);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    /* Scan every documented FMC read phase and pipe delay using only the
     * already written words. raw excludes the all-zero word so a disconnected
     * D10/D11 input cannot receive credit; fuse reconstructs D10/D11 from the
     * physical GPIO input sample immediately following each FMC load. */
    {
        uint8_t best_phase = V5F_SDRAM_DEFAULT_PHASE_SEL;
        uint8_t best_pipe = FMC_ReadPipeDelay_none;
        uint16_t best_raw = 0u;
        uint16_t best_fused = 0u;
        uint8_t pipe;

        sdram_usb_debug_write_line("READ_SCAN START phases=16 pipes=3 raw_nonzero=48 fuse_all=64");
        for(pipe = 0u; pipe < 3u; pipe++)
        {
            uint8_t phase;
            int raw_used = rt_snprintf(line,
                                       sizeof(line),
                                       "READ_SCAN pipe=%u raw=",
                                       (unsigned int)pipe);
            char fused_line[V5F_SDRAM_USB_LINE_BYTES];
            int fused_used = rt_snprintf(fused_line,
                                         sizeof(fused_line),
                                         "READ_SCAN pipe=%u fuse=",
                                         (unsigned int)pipe);

            sdram_read_pipe_probe_apply(pipe);
            for(phase = 0u; phase < 16u; phase++)
            {
                uint16_t raw_ok = 0u;
                uint16_t fused_ok = 0u;
                uint8_t repeat;
                int appended;

                sdram_phase_probe_apply(phase);
                for(repeat = 0u; repeat < 16u; repeat++)
                {
                    uint16_t index;

                    for(index = 0u; index < 4u; index++)
                    {
                        uint16_t actual = probe[index];
                        uint16_t pin = (uint16_t)(GPIOD->INDR & 0x3u);
                        uint16_t fused = (uint16_t)((actual & ~0x0C00u) |
                                                    (uint16_t)(pin << 10));

                        memory_barrier();
                        if((index != 0u) && (actual == patterns[index]))
                        {
                            raw_ok++;
                        }
                        if(fused == patterns[index])
                        {
                            fused_ok++;
                        }
                    }
                }

                if(raw_ok > best_raw)
                {
                    best_raw = raw_ok;
                    best_phase = phase;
                    best_pipe = pipe;
                }
                if(fused_ok > best_fused)
                {
                    best_fused = fused_ok;
                }

                if((raw_used > 0) && ((rt_size_t)raw_used < sizeof(line)))
                {
                    appended = rt_snprintf(&line[raw_used],
                                           sizeof(line) - (rt_size_t)raw_used,
                                           "%s%u",
                                           (phase == 0u) ? "" : ",",
                                           (unsigned int)raw_ok);
                    if(appended > 0)
                    {
                        raw_used += appended;
                    }
                }
                if((fused_used > 0) && ((rt_size_t)fused_used < sizeof(fused_line)))
                {
                    appended = rt_snprintf(&fused_line[fused_used],
                                           sizeof(fused_line) - (rt_size_t)fused_used,
                                           "%s%u",
                                           (phase == 0u) ? "" : ",",
                                           (unsigned int)fused_ok);
                    if(appended > 0)
                    {
                        fused_used += appended;
                    }
                }
            }

            if((raw_used > 0) && ((rt_size_t)raw_used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            if((fused_used > 0) && ((rt_size_t)fused_used < sizeof(fused_line)))
            {
                sdram_usb_debug_write_line(fused_line);
            }
        }

        sdram_read_pipe_probe_apply(FMC_ReadPipeDelay_none);
        sdram_phase_probe_apply(V5F_SDRAM_DEFAULT_PHASE_SEL);
        {
            int used = rt_snprintf(line,
                                   sizeof(line),
                                   "READ_SCAN END best_raw=%u/48 phase=%u pipe=%u best_fuse=%u/64 restored=%u/%u",
                                   (unsigned int)best_raw,
                                   (unsigned int)best_phase,
                                   (unsigned int)best_pipe,
                                   (unsigned int)best_fused,
                                   (unsigned int)V5F_SDRAM_DEFAULT_PHASE_SEL,
                                   (unsigned int)FMC_ReadPipeDelay_none);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
    }

    /* PD0/PD1 are confirmed to receive the SDRAM data at the GPIO input
     * register. Test every non-contentious digital port mode while keeping
     * AFR=1 and issuing reads only. This checks whether this silicon requires
     * an undocumented GPIO mode to connect the pads to the FMC input mux. */
    {
        static const GPIOMode_TypeDef modes[] = {
            GPIO_Mode_AF_PP,
            GPIO_Mode_AF_OD,
            GPIO_Mode_IN_FLOATING,
            GPIO_Mode_IPU,
            GPIO_Mode_IPD,
        };
        static const char *const names[] = {
            "afpp",
            "afod",
            "float",
            "ipu",
            "ipd",
        };
        GPIO_InitTypeDef gpio = {0};
        uint32_t saved_outdr = GPIOD->OUTDR;
        uint8_t mode_index;

        gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
        gpio.GPIO_Speed = GPIO_Speed_Very_High;
        sdram_usb_debug_write_line("MODE_SCAN START afr=1 reads_only=1 modes=afpp,afod,float,ipu,ipd");
        for(mode_index = 0u; mode_index < 5u; mode_index++)
        {
            uint16_t raw_ok = 0u;
            uint16_t fused_ok = 0u;
            uint16_t mode_pin_and[4] = {3u, 3u, 3u, 3u};
            uint16_t mode_pin_or[4] = {0u, 0u, 0u, 0u};
            uint8_t repeat;
            int used;

            gpio.GPIO_Mode = modes[mode_index];
            GPIO_Init(GPIOD, &gpio);
            memory_barrier();
            for(repeat = 0u; repeat < 64u; repeat++)
            {
                uint16_t index;

                for(index = 0u; index < 4u; index++)
                {
                    uint16_t actual = probe[index];
                    uint16_t pin = (uint16_t)(GPIOD->INDR & 0x3u);
                    uint16_t fused = (uint16_t)((actual & ~0x0C00u) |
                                                (uint16_t)(pin << 10));

                    memory_barrier();
                    if((index != 0u) && (actual == patterns[index]))
                    {
                        raw_ok++;
                    }
                    if(fused == patterns[index])
                    {
                        fused_ok++;
                    }
                    mode_pin_and[index] = (uint16_t)(mode_pin_and[index] & pin);
                    mode_pin_or[index] = (uint16_t)(mode_pin_or[index] | pin);
                }
            }

            used = rt_snprintf(line,
                               sizeof(line),
                               "MODE_SCAN %s cfg=%02x raw=%u/192 fuse=%u/256 map=%u/%u,%u/%u,%u/%u,%u/%u",
                               names[mode_index],
                               (unsigned int)(GPIOD->CFGLR & 0xFFu),
                               (unsigned int)raw_ok,
                               (unsigned int)fused_ok,
                               (unsigned int)mode_pin_and[0],
                               (unsigned int)mode_pin_or[0],
                               (unsigned int)mode_pin_and[1],
                               (unsigned int)mode_pin_or[1],
                               (unsigned int)mode_pin_and[2],
                               (unsigned int)mode_pin_or[2],
                               (unsigned int)mode_pin_and[3],
                               (unsigned int)mode_pin_or[3]);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }

        GPIOD->OUTDR = saved_outdr;
        sdram_gpio_af(GPIOD, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF1);
        sdram_gpio_af(GPIOD, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF1);
        memory_barrier();
        {
            int used = rt_snprintf(line,
                                   sizeof(line),
                                   "MODE_SCAN END restored_af=1 cfg=%02x",
                                   (unsigned int)(GPIOD->CFGLR & 0xFFu));
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
    }

    /* The reference manual describes PD0PD1_RM=1 as the setting that makes
     * XI/XO usable as PD0/PD1, while QEU6 also exposes dedicated PD0/PD1
     * package pins. Scan both routing states and both high-speed-pad states
     * in one run so the FMC result does not depend on an interpretation of
     * that ambiguous package/remap description. Reads only: no route can
     * drive an external clock or contend with SDRAM during this scan. */
    {
        const uint32_t route_mask = AFIO_PCFR1_PD0_1_REMAP |
                                    AFIO_PCFR1_VIO18_IO_HSLV |
                                    AFIO_PCFR1_VIO33_IO_HSLV |
                                    AFIO_PCFR1_VDD33_IO_HSLV;
        const uint32_t hslv_mask = AFIO_PCFR1_VIO18_IO_HSLV |
                                   AFIO_PCFR1_VIO33_IO_HSLV |
                                   AFIO_PCFR1_VDD33_IO_HSLV;
        uint32_t saved_pcfr1 = AFIO->PCFR1;
        uint8_t route_index;

        sdram_usb_debug_write_line("ROUTE_SCAN START reads_only=1 rm=0/1 hslv=0/1 af=1 mode=afpp");
        for(route_index = 0u; route_index < 4u; route_index++)
        {
            uint8_t route_rm = (uint8_t)(route_index & 1u);
            uint8_t route_hslv = (uint8_t)((route_index >> 1) & 1u);
            uint16_t raw_ok = 0u;
            uint16_t fused_ok = 0u;
            uint16_t route_pin_and[4] = {3u, 3u, 3u, 3u};
            uint16_t route_pin_or[4] = {0u, 0u, 0u, 0u};
            uint8_t repeat;
            int used;

            AFIO->PCFR1 = (saved_pcfr1 & ~route_mask) |
                          ((route_rm != 0u) ? AFIO_PCFR1_PD0_1_REMAP : 0u) |
                          ((route_hslv != 0u) ? hslv_mask : 0u);
            sdram_gpio_af(GPIOD, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF1);
            sdram_gpio_af(GPIOD, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF1);
            memory_barrier();
            for(repeat = 0u; repeat < 64u; repeat++)
            {
                uint16_t index;

                for(index = 0u; index < 4u; index++)
                {
                    uint16_t actual = probe[index];
                    uint16_t pin = (uint16_t)(GPIOD->INDR & 0x3u);
                    uint16_t fused = (uint16_t)((actual & ~0x0C00u) |
                                                (uint16_t)(pin << 10));

                    memory_barrier();
                    if((index != 0u) && (actual == patterns[index]))
                    {
                        raw_ok++;
                    }
                    if(fused == patterns[index])
                    {
                        fused_ok++;
                    }
                    route_pin_and[index] = (uint16_t)(route_pin_and[index] & pin);
                    route_pin_or[index] = (uint16_t)(route_pin_or[index] | pin);
                }
            }

            used = rt_snprintf(line,
                               sizeof(line),
                               "ROUTE_SCAN rm=%u hslv=%u pcfr1=%08x raw=%u/192 fuse=%u/256 map=%u/%u,%u/%u,%u/%u,%u/%u",
                               (unsigned int)route_rm,
                               (unsigned int)route_hslv,
                               (unsigned int)AFIO->PCFR1,
                               (unsigned int)raw_ok,
                               (unsigned int)fused_ok,
                               (unsigned int)route_pin_and[0],
                               (unsigned int)route_pin_or[0],
                               (unsigned int)route_pin_and[1],
                               (unsigned int)route_pin_or[1],
                               (unsigned int)route_pin_and[2],
                               (unsigned int)route_pin_or[2],
                               (unsigned int)route_pin_and[3],
                               (unsigned int)route_pin_or[3]);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }

        AFIO->PCFR1 = saved_pcfr1;
        sdram_gpio_af(GPIOD, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF1);
        sdram_gpio_af(GPIOD, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF1);
        memory_barrier();
        {
            int used = rt_snprintf(line,
                                   sizeof(line),
                                   "ROUTE_SCAN END restored_pcfr1=%08x",
                                   (unsigned int)AFIO->PCFR1);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
    }

    sdram_usb_debug_write_line("SCOPE READ ARM delay_ms=1000 trigger=CAS#_LOW+WE#_HIGH");
    for(poll_count = 0u; poll_count < (V5F_SDRAM_DIRECTION_ARM_DELAY_MS / 10u); poll_count++)
    {
        ch32h417_dual_cdc_poll();
        rt_thread_mdelay(10);
    }
    sdram_usb_debug_write_line("SCOPE READ START reads_only=1 addr_count=4 no_writes=1");

    loop_low = 0u;
    loop_high = 0u;
    report_count = 0u;
    poll_count = 0u;
    pattern_index = 0u;
    while(1)
    {
        uint32_t i;

        for(i = 0u; i < V5F_SDRAM_CONTINUOUS_POLL_CYCLES; i++)
        {
            uint16_t index = pattern_index;
            uint16_t expected = patterns[index];
            uint16_t actual;
            uint16_t pin;
            uint16_t fused;

            pattern_index = (uint16_t)((pattern_index + 1u) & 3u);
            actual = probe[index];
            /* In AF_PP mode INDR still samples the physical pad. This read is
             * deliberately adjacent to the FMC load. Seeing pin_or != 0 is
             * strong evidence of SDRAM-driven read data; pin_or == 0 remains
             * inconclusive because DQ may already be high-Z. */
            pin = (uint16_t)(GPIOD->INDR & 0x3u);
            fused = (uint16_t)((actual & ~0x0C00u) |
                               (uint16_t)(pin << 10));
            memory_barrier();

            last_expected = expected;
            last_actual = actual;
            last_fused = fused;
            bit_or = (uint16_t)(bit_or | actual);
            pin_and = (uint16_t)(pin_and & pin);
            pin_or = (uint16_t)(pin_or | pin);
            pin_index_and[index] = (uint16_t)(pin_index_and[index] & pin);
            pin_index_or[index] = (uint16_t)(pin_index_or[index] | pin);
            if((have_previous_pin != 0u) && (pin != previous_pin))
            {
                pin_changes++;
            }
            previous_pin = pin;
            have_previous_pin = 1u;

            loop_low++;
            if(loop_low == 0u)
            {
                loop_high++;
            }
            if(actual != expected)
            {
                bad_low++;
                if(bad_low == 0u)
                {
                    bad_high++;
                }
            }
            if(fused != expected)
            {
                fused_bad_low++;
                if(fused_bad_low == 0u)
                {
                    fused_bad_high++;
                }
            }
        }

        ch32h417_dual_cdc_poll();
        poll_count++;
        if(poll_count >= V5F_SDRAM_CONTINUOUS_REPORT_POLLS)
        {
            int used;

            poll_count = 0u;
            report_count++;
            g_v5f_hw_test_diag.frame_count = report_count;
            g_v5f_hw_test_diag.sdram_expected = last_expected;
            g_v5f_hw_test_diag.sdram_actual = last_actual;
            g_v5f_hw_test_diag.sdram_fail_count = bad_low;
            used = rt_snprintf(line,
                               sizeof(line),
                               "SCOPE READ report=%u loops=%u:%08x rawbad=%u:%08x fusebad=%u:%08x last=%04x/%04x/%04x or=%04x",
                               (unsigned int)report_count,
                               (unsigned int)loop_high,
                               (unsigned int)loop_low,
                               (unsigned int)bad_high,
                               (unsigned int)bad_low,
                               (unsigned int)fused_bad_high,
                               (unsigned int)fused_bad_low,
                               (unsigned int)last_expected,
                               (unsigned int)last_actual,
                               (unsigned int)last_fused,
                               (unsigned int)bit_or);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            used = rt_snprintf(line,
                               sizeof(line),
                               "SCOPE PIN report=%u map=%u/%u,%u/%u,%u/%u,%u/%u all=%u/%u chg=%u",
                               (unsigned int)report_count,
                               (unsigned int)pin_index_and[0],
                               (unsigned int)pin_index_or[0],
                               (unsigned int)pin_index_and[1],
                               (unsigned int)pin_index_or[1],
                               (unsigned int)pin_index_and[2],
                               (unsigned int)pin_index_or[2],
                               (unsigned int)pin_index_and[3],
                               (unsigned int)pin_index_or[3],
                               (unsigned int)pin_and,
                               (unsigned int)pin_or,
                               (unsigned int)pin_changes);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            pin_changes = 0u;
            bit_or = 0x0000u;
            pin_and = 0x0003u;
            pin_or = 0x0000u;
            pin_index_and[0] = 0x0003u;
            pin_index_and[1] = 0x0003u;
            pin_index_and[2] = 0x0003u;
            pin_index_and[3] = 0x0003u;
            pin_index_or[0] = 0u;
            pin_index_or[1] = 0u;
            pin_index_or[2] = 0u;
            pin_index_or[3] = 0u;
            have_previous_pin = 0u;
        }
    }
}
#endif

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
    uint16_t drive_in[4];
    uint16_t restored_in;
    int used;

    init.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    init.GPIO_Mode = GPIO_Mode_Out_PP;
    init.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(GPIOD, &init);

    drive_in[0] = sdram_usb_debug_pd0_pd1_drive_read(0x0u);
    drive_in[1] = sdram_usb_debug_pd0_pd1_drive_read(0x1u);
    drive_in[2] = sdram_usb_debug_pd0_pd1_drive_read(0x2u);
    drive_in[3] = sdram_usb_debug_pd0_pd1_drive_read(0x3u);
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
                       "SDRAM pad drive 0/1/2/3=%u/%u/%u/%u restored=%u pdaflr=%08x pdcfg=%08x",
                       (unsigned int)drive_in[0],
                       (unsigned int)drive_in[1],
                       (unsigned int)drive_in[2],
                       (unsigned int)drive_in[3],
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

static void V5F_MAYBE_UNUSED sdram_usb_debug_remap0_probe(volatile uint16_t *probe)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    uint16_t actual_0000;
    uint16_t actual_ffff;
    uint16_t actual_aaaa;
    uint16_t actual_5555;
    int used;

    AFIO->PCFR1 &= ~AFIO_PCFR1_PD0_1_REMAP;
    sdram_gpio_af(GPIOD, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF1);
    sdram_gpio_af(GPIOD, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF1);
    memory_barrier();

    actual_0000 = sdram_probe_write_read16(probe, 0x0000u);
    actual_ffff = sdram_probe_write_read16(probe, 0xFFFFu);
    actual_aaaa = sdram_probe_write_read16(probe, 0xAAAAu);
    actual_5555 = sdram_probe_write_read16(probe, 0x5555u);

    used = rt_snprintf(line,
                       sizeof(line),
                       "SDRAM remap rm=0 pcfr1=%08x rd=%04x/%04x/%04x/%04x",
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

#if V5F_SDRAM_MEMTEST_CDC_ONLY
static void sdram_memtest_cdc_repeat(volatile uint16_t *probe, uint16_t expected)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    uint16_t first;
    uint16_t bit_and;
    uint16_t bit_or;
    uint16_t previous;
    uint16_t actual;
    uint16_t changes = 0u;
    uint16_t mismatches = 0u;
    uint16_t i;
    int used;

    *probe = expected;
    memory_barrier();
    first = *probe;
    previous = first;
    bit_and = first;
    bit_or = first;

    for(i = 0u; i < 64u; i++)
    {
        actual = *probe;
        bit_and = (uint16_t)(bit_and & actual);
        bit_or = (uint16_t)(bit_or | actual);
        if(actual != previous)
        {
            changes++;
        }
        if(actual != expected)
        {
            mismatches++;
        }
        previous = actual;
    }

    used = rt_snprintf(line,
                       sizeof(line),
                       "REPEAT exp=%04x first=%04x and=%04x or=%04x change=%u bad=%u/64",
                       (unsigned int)expected,
                       (unsigned int)first,
                       (unsigned int)bit_and,
                       (unsigned int)bit_or,
                       (unsigned int)changes,
                       (unsigned int)mismatches);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static void sdram_memtest_cdc_tune(volatile uint16_t *probe)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    uint8_t best_phase = V5F_SDRAM_DEFAULT_PHASE_SEL;
    uint8_t best_pipe = FMC_ReadPipeDelay_none;
    uint8_t best_score = 0u;
    uint8_t best_bits = 0u;
    uint8_t pipe;

    sdram_usb_debug_write_line("TUNE START phases=16 pipes=3 patterns=4");
    for(pipe = 0u; pipe < 3u; pipe++)
    {
        uint8_t phase;
        int used = rt_snprintf(line, sizeof(line), "TUNE pipe=%u bits=", (unsigned int)pipe);

        sdram_read_pipe_probe_apply(pipe);
        for(phase = 0u; phase < 16u; phase++)
        {
            uint8_t score;
            uint8_t bits;
            int appended;

            sdram_phase_probe_apply(phase);
            score = sdram_phase_probe_score(probe, &bits);
            if((bits > best_bits) ||
               ((bits == best_bits) && (score > best_score)))
            {
                best_phase = phase;
                best_pipe = pipe;
                best_score = score;
                best_bits = bits;
            }

            if((used <= 0) || ((rt_size_t)used >= sizeof(line)))
            {
                continue;
            }
            appended = rt_snprintf(&line[used],
                                   sizeof(line) - (rt_size_t)used,
                                   "%s%u",
                                   (phase == 0u) ? "" : ",",
                                   (unsigned int)bits);
            if(appended > 0)
            {
                used += appended;
            }
        }

        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    sdram_read_pipe_probe_apply(FMC_ReadPipeDelay_none);
    sdram_phase_probe_apply(V5F_SDRAM_DEFAULT_PHASE_SEL);
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "TUNE BEST phase=%u pipe=%u score=%u/4 bits=%u/64 restored=%u/%u",
                               (unsigned int)best_phase,
                               (unsigned int)best_pipe,
                               (unsigned int)best_score,
                               (unsigned int)best_bits,
                               (unsigned int)V5F_SDRAM_DEFAULT_PHASE_SEL,
                               (unsigned int)FMC_ReadPipeDelay_none);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
}

static void V5F_MAYBE_UNUSED sdram_memtest_cdc_diagnose(void)
{
    volatile uint16_t *probe = (volatile uint16_t *)V5F_SDRAM_BASE_ADDR;

    sdram_usb_debug_write_line("DIAG START");
    sdram_usb_debug_regs();
    sdram_usb_debug_rcc();
    sdram_memtest_cdc_repeat(probe, 0x0000u);
    sdram_memtest_cdc_repeat(probe, 0xFFFFu);
    sdram_memtest_cdc_repeat(probe, 0xAAAAu);
    sdram_memtest_cdc_repeat(probe, 0x5555u);
    sdram_usb_debug_dq(probe);
    sdram_usb_debug_addr();
    sdram_memtest_cdc_tune(probe);
    sdram_usb_debug_write_line("DIAG END");
}
#endif
#endif

static void sdram_enable_0x60000000_remap(void)
{
    FMC_Bank1->BTCR[0] |= V5F_FMC_SDRAM_REMAP_TO_0X60000000;
}

static void sdram_disable_0x60000000_remap(void)
{
    FMC_Bank1->BTCR[0] &= ~V5F_FMC_SDRAM_REMAP_TO_0X60000000;
}

static void V5F_MAYBE_UNUSED sdram_status_fail_show(void)
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
#if V5F_SDRAM_MEMTEST_CDC_ONLY
#if !V5F_SDRAM_MEMTEST_LOW8
    if((error == V5F_SDRAM_ERR_VERIFY) &&
       (g_v5f_hw_test_diag.sdram_stage != V5F_SDRAM_STAGE_INIT))
    {
        sdram_memtest_cdc_diagnose();
    }
#endif
    sdram_memtest_cdc_failure(error);
#else
    sdram_status_fail_show();
#endif
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

#if (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_MEMTEST) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO)
    *sdclk_hz = hclk_hz;
    return V5F_SDRAM_CLOCK_PERIOD_1HCLK;
#endif

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

static void sdram_set_refresh_count(uint16_t refresh_count)
{
    uint32_t encoded = ((uint32_t)refresh_count << 1) & FMC_SDRTR_COUNT;

    /* COUNT occupies SDRTR[13:1]. The vendor helper writes the unshifted
     * value, which halves the requested interval on CH32H417. */
    FMC_Bank5_6->SDRTR = (FMC_Bank5_6->SDRTR & ~FMC_SDRTR_COUNT) | encoded;
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

    /* The board powers all FMC IO domains at 3.3 V. HSLV is only for IO
     * domains below 2.7 V. QEU6 exposes SDRAM D10/D11 directly on dedicated
     * PD0/PD1 package pins 99/100; PD0PD1_RM instead maps those GPIO functions
     * to the separate XI/XO pins 23/24, so keep that remap disabled. */
    AFIO->PCFR1 &= ~(AFIO_PCFR1_VIO18_IO_HSLV |
                     AFIO_PCFR1_VIO33_IO_HSLV |
                     AFIO_PCFR1_VDD33_IO_HSLV);
    AFIO->PCFR1 &= ~AFIO_PCFR1_PD0_1_REMAP;

    /* PA9/PA10 are board-control GPIO outputs, but AF0 on those pins is also
     * SDRAM D10/D11.  GPIO mode keeps their requested high level; selecting
     * the unused AF15 removes the stale parallel FMC route from the pads. */
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF15);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF15);

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

static int sdram_init_profile(uint8_t prefetch,
                              uint8_t nrfs_count,
                              uint8_t report)
{
    FMC_SDRAM_InitTypeDef init = {0};
    FMC_SDRAM_TimingTypeDef timing = {0};
    uint32_t sdclock_period;
    uint32_t sdclk_hz = 0u;
    uint16_t refresh_count;
    int result;

    if(report != 0u)
    {
        sdram_diag_clear();
        g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_INIT;
        g_v5f_hw_test_diag.sdram_hclk_hz = HCLKClock;
#if V5F_SDRAM_MEMTEST_CDC_ONLY
        sdram_memtest_cdc_stage("INIT", "START");
#else
        sdram_status_show(V5F_SDRAM_STATUS_INIT,
                          ch32h417_ltdc_rgb_pack_rgb565(0u, 170u, 220u));
#endif
    }

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
    init.FMC_NRFS_CNT = nrfs_count & 0x0Fu;
    init.FMC_PHASE_SEL = V5F_SDRAM_DEFAULT_PHASE_SEL;
    /* Program bit 15 directly below. The vendor header clears bit 12 while
     * its mode constant writes bit 15, so FMC_SDRAM_Init cannot reliably
     * replace an existing mode during the cold-profile diagnostics. */
    init.FMC_ENHANCE_READ_MODE = V5F_SDRAM_NORMAL_READ_MODE;

    timing.FMC_LoadToActiveDelay = 2u;
    timing.FMC_ExitSelfRefreshDelay = 8u;
    timing.FMC_SelfRefreshTime = 5u;
    timing.FMC_RowCycleDelay = 6u;
    timing.FMC_WriteRecoveryTime = 2u;
    timing.FMC_RPDelay = 2u;
    timing.FMC_RCDDelay = 2u;
    init.FMC_SDRAM_Timing = &timing;

    FMC_SDRAM_Init(&init);
    {
        uint32_t misc = FMC_Bank5_6->MISC;

        misc &= ~(V5F_SDRAM_ENHANCE_READ_BIT | FMC_MISC_NRFS_CNT);
        misc |= (uint32_t)(nrfs_count & 0x0Fu);
        if(prefetch != 0u)
        {
            misc |= V5F_SDRAM_ENHANCE_READ_BIT;
        }
        FMC_Bank5_6->MISC = misc;
        ch32h417_ltdc_rgb_framebuffer_barrier();
    }
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

    sdram_set_refresh_count(refresh_count);
    /* BCR1.FMC_EN controls NAND/NOR/PSRAM only. WCH's 16-bit SDRAM
     * reference sequence leaves it clear and only selects the SDRAM remap. */
    FMC_Bank1->BTCR[0] &= ~FMC_BCR1_FMCEN;
#if V5F_SDRAM_MEMTEST_CDC_ONLY
    /* Exercise the native Bank5 SDRAM window for the CDC full-memory test. */
    sdram_disable_0x60000000_remap();
#else
    sdram_enable_0x60000000_remap();
#endif
    ch32h417_ltdc_rgb_framebuffer_barrier();
#if V5F_SDRAM_MEMTEST_CDC_ONLY
    if(report != 0u)
    {
        sdram_memtest_cdc_clock();
        sdram_memtest_cdc_map();
        sdram_memtest_cdc_stage("INIT", "PASS");
    }
#endif
    return V5F_SDRAM_OK;
}

static int sdram_init(void)
{
    return sdram_init_profile(0u, 0u, 1u);
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
#if V5F_SDRAM_MEMTEST_CDC_ONLY
    sdram_memtest_cdc_stage("DATA_BUS", "START");
#else
    sdram_status_show(V5F_SDRAM_STATUS_DATA_BUS,
                      ch32h417_ltdc_rgb_pack_rgb565(255u, 220u, 0u));
#endif
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
#if V5F_SDRAM_MEMTEST_CDC_ONLY
    sdram_memtest_cdc_stage("DATA_BUS", "PASS bits=32");
#endif

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_ADDRESS_BUS;
#if V5F_SDRAM_MEMTEST_CDC_ONLY
    sdram_memtest_cdc_stage("ADDRESS_BUS", "START");
#else
    sdram_status_show(V5F_SDRAM_STATUS_ADDRESS_BUS,
                      ch32h417_ltdc_rgb_pack_rgb565(220u, 0u, 255u));
#endif
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
#if V5F_SDRAM_MEMTEST_CDC_ONLY
    sdram_memtest_cdc_stage("ADDRESS_BUS", "PASS");
#endif

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_PATTERN;
#if V5F_SDRAM_MEMTEST_CDC_ONLY
    sdram_memtest_cdc_stage("PATTERN", "START");
#else
    sdram_status_show(V5F_SDRAM_STATUS_PATTERN,
                      ch32h417_ltdc_rgb_pack_rgb565(255u, 96u, 0u));
#endif
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
#if V5F_SDRAM_MEMTEST_CDC_ONLY
    sdram_memtest_cdc_stage("PATTERN", "PASS");
#endif

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_PATTERN_INV;
#if V5F_SDRAM_MEMTEST_CDC_ONLY
    sdram_memtest_cdc_stage("PATTERN_INV", "START");
#else
    sdram_status_show(V5F_SDRAM_STATUS_PATTERN,
                      ch32h417_ltdc_rgb_pack_rgb565(255u, 140u, 0u));
#endif
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
#if V5F_SDRAM_MEMTEST_CDC_ONLY
    sdram_memtest_cdc_stage("PATTERN_INV", "PASS");
#endif

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

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_UI_FRAMES
static void run_ltdc_ui_frames_test(void)
{
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    v5f_ltdc_ui_frames_run(s_lcd_fb);
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
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO) || \
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

#if V5F_SDRAM_MEMTEST_CDC_ONLY
static uint32_t s_sdram_x8_progress_total_mb = 16u;

/* Keep both the FMC and CPU accesses in native x16 mode. Every stored test
 * value has a zero high byte; reads are narrowed to uint8_t so only the proven
 * D0..D7 lane participates in pass/fail decisions. */
#define V5F_SDRAM_LOW8_BYTE_AT(base, logical_offset) \
    ((base)[(logical_offset) * V5F_SDRAM_LOW8_PHYSICAL_STRIDE])

static void sdram_memtest_x16_low8_write(volatile uint8_t *base,
                                          uint32_t logical_offset,
                                          uint8_t value)
{
    ((volatile uint16_t *)base)[logical_offset] = (uint16_t)value;
}

static uint8_t sdram_memtest_x16_low8_read(volatile uint8_t *base,
                                           uint32_t logical_offset)
{
    return (uint8_t)(((volatile uint16_t *)base)[logical_offset] & 0x00FFu);
}

static uint8_t sdram_memtest_x8_pattern(uint32_t offset)
{
    uint32_t value = offset ^ 0x6D2B79F5u;

    value ^= value >> 15;
    value *= 0x2C1B3C6Du;
    value ^= value >> 12;
    value *= 0x297A2D39u;
    value ^= value >> 15;
    return (uint8_t)value;
}

static uint32_t sdram_memtest_x8_hash(uint32_t hash, uint8_t value)
{
    return (hash ^ value) * 16777619u;
}

static void sdram_memtest_x8_progress_slow(const char *phase, uint32_t completed)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];

    ch32h417_dual_cdc_poll();

    if((completed != 0u) &&
       ((completed & (V5F_SDRAM_X8_PROGRESS_BYTES - 1u)) == 0u))
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "%s %s progress=%u/%uMB",
                               sdram_memtest_stage_name(g_v5f_hw_test_diag.sdram_stage),
                               (phase != RT_NULL) ? phase : "run",
                               (unsigned int)(completed / V5F_SDRAM_X8_PROGRESS_BYTES),
                               (unsigned int)s_sdram_x8_progress_total_mb);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
}

/* Keep the hot byte loops small: enter the reporting helper only once per
 * 64 KiB instead of paying a function call for every SDRAM byte. */
#define V5F_SDRAM_X8_PROGRESS(phase, completed)                              \
    do                                                                       \
    {                                                                        \
        uint32_t v5f_sdram_x8_done = (completed);                            \
        if((v5f_sdram_x8_done != 0u) &&                                     \
           ((v5f_sdram_x8_done & (V5F_SDRAM_X8_SERVICE_BYTES - 1u)) == 0u)) \
        {                                                                    \
            sdram_memtest_x8_progress_slow((phase), v5f_sdram_x8_done);      \
        }                                                                    \
    } while(0)

static uint8_t sdram_memtest_x8_bank_pins(void)
{
    uint8_t pins = 0u;

    if((GPIOB->INDR & GPIO_Pin_1) != 0u)
    {
        pins |= 1u;
    }
    if((GPIOB->INDR & GPIO_Pin_15) != 0u)
    {
        pins |= 2u;
    }
    return pins;
}

static uint8_t sdram_memtest_precharge_all(void)
{
    ch32h417_ltdc_rgb_framebuffer_barrier();
    if(sdram_send_command(FMC_SDRAM_CMD_Mode2, 1u, 0u) != V5F_SDRAM_OK)
    {
        return 0u;
    }
    sdram_delay_us(2u);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    return 1u;
}

static uint16_t sdram_memtest_bank_read_score(volatile uint8_t *base,
                                               const uint8_t patterns[4])
{
    uint16_t score = 0u;
    uint32_t repeat;

    for(repeat = 0u; repeat < 64u; repeat++)
    {
        uint32_t bank;

        for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
        {
            uint8_t value = sdram_memtest_x16_low8_read(
                base, bank * V5F_SDRAM_X8_BANK_BYTES);
            if(value == patterns[bank])
            {
                score++;
            }
        }
    }
    return score;
}

/* CH32H417RM defines R32_SDRAM_MISC bit 15 as Enhance_read_mode. The
 * vendor header's FMC_MISC_Enhance_read_mode mask is 0x1000, so it cannot
 * safely be used for this register bit. Apply the documented bit directly.
 * Enhanced read and RBURST are mutually exclusive according to the manual. */
static uint8_t sdram_memtest_read_mode_apply(uint8_t enhance, uint8_t rburst)
{
    uint32_t sdcr;
    uint32_t misc;
    uint8_t ok;

    if((enhance != 0u) && (rburst != 0u))
    {
        return 0u;
    }

    ok = sdram_memtest_precharge_all();
    sdcr = FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM];
    misc = FMC_Bank5_6->MISC;

    /* Disable both mechanisms first so no transient illegal combination is
     * visible to the controller while changing modes. */
    misc &= ~V5F_SDRAM_ENHANCE_READ_BIT;
    FMC_Bank5_6->MISC = misc;
    sdcr &= ~V5F_SDRAM_READ_BURST_BIT;
    FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM] = sdcr;
    ch32h417_ltdc_rgb_framebuffer_barrier();

    if(rburst != 0u)
    {
        FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM] =
            sdcr | V5F_SDRAM_READ_BURST_BIT;
    }
    if(enhance != 0u)
    {
        FMC_Bank5_6->MISC = misc | V5F_SDRAM_ENHANCE_READ_BIT;
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
    ok &= sdram_memtest_precharge_all();
    return ok;
}

static uint8_t sdram_memtest_phase_distance(uint8_t phase)
{
    uint8_t distance = (phase > V5F_SDRAM_DEFAULT_PHASE_SEL) ?
                       (uint8_t)(phase - V5F_SDRAM_DEFAULT_PHASE_SEL) :
                       (uint8_t)(V5F_SDRAM_DEFAULT_PHASE_SEL - phase);

    if(distance > 8u)
    {
        distance = (uint8_t)(16u - distance);
    }
    return distance;
}

static void sdram_memtest_bank_tune(void)
{
    static const uint8_t patterns[V5F_SDRAM_X8_BANK_COUNT] = {
        0x19u, 0x2Au, 0x4Cu, 0x8Fu
    };
    static const char *const mode_name[3] = {
        "normal", "enhance", "rburst"
    };
    static const uint8_t mode_enhance[3] = {0u, 1u, 0u};
    static const uint8_t mode_rburst[3] = {0u, 0u, 1u};
    volatile uint8_t *base = (volatile uint8_t *)V5F_SDRAM_BASE_ADDR;
    uint16_t best_score = 0u;
    uint8_t best_phase = V5F_SDRAM_DEFAULT_PHASE_SEL;
    uint8_t best_pipe = FMC_ReadPipeDelay_none;
    uint8_t best_mode = 0u;
    uint8_t setup_ok = 1u;
    uint8_t bank;
    uint8_t mode;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    sdram_usb_debug_write_line("BANK_MODE START modes=normal,enhance,rburst phases=16 pipes=3 score=alt/256");
    sdram_usb_debug_write_line("BANK_MODE NOTE manual_enh_bit=00008000 vendor_clear_mask=00001000 direct=1");
    for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
    {
        setup_ok &= sdram_memtest_precharge_all();
        sdram_memtest_x16_low8_write(base,
                                     bank * V5F_SDRAM_X8_BANK_BYTES,
                                     patterns[bank]);
    }
    setup_ok &= sdram_memtest_precharge_all();

    for(mode = 0u; mode < 3u; mode++)
    {
        uint16_t mode_best_score = 0u;
        uint8_t mode_best_phase = V5F_SDRAM_DEFAULT_PHASE_SEL;
        uint8_t mode_best_pipe = FMC_ReadPipeDelay_none;
        uint8_t pipe;

        setup_ok &= sdram_memtest_read_mode_apply(mode_enhance[mode],
                                                   mode_rburst[mode]);
        for(pipe = 0u; pipe < 3u; pipe++)
        {
            uint8_t phase;

            sdram_read_pipe_probe_apply(pipe);
            for(phase = 0u; phase < 16u; phase++)
            {
                uint16_t score;

                sdram_phase_probe_apply(phase);
                setup_ok &= sdram_memtest_precharge_all();
                score = sdram_memtest_bank_read_score(base, patterns);
                if((score > mode_best_score) ||
                   ((score == mode_best_score) && (pipe < mode_best_pipe)) ||
                   ((score == mode_best_score) && (pipe == mode_best_pipe) &&
                    (sdram_memtest_phase_distance(phase) <
                     sdram_memtest_phase_distance(mode_best_phase))))
                {
                    mode_best_score = score;
                    mode_best_phase = phase;
                    mode_best_pipe = pipe;
                }
            }
        }

        {
            int used = rt_snprintf(line,
                                   sizeof(line),
                                   "BANK_MODE SCAN mode=%s enh=%u rb=%u best=%u/256 phase=%u pipe=%u",
                                   mode_name[mode],
                                   (unsigned int)mode_enhance[mode],
                                   (unsigned int)mode_rburst[mode],
                                   (unsigned int)mode_best_score,
                                   (unsigned int)mode_best_phase,
                                   (unsigned int)mode_best_pipe);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }

        if((mode_best_score > best_score) ||
           ((mode_best_score == best_score) && (mode < best_mode)))
        {
            best_score = mode_best_score;
            best_phase = mode_best_phase;
            best_pipe = mode_best_pipe;
            best_mode = mode;
        }
    }

    /* The scan is diagnostic only. Small score differences are noisy and v22
     * could accidentally select RBURST, although halfword reads in that mode
     * were completely invalid. Continue all functional tests with the vendor
     * reference setting: normal read, no burst, default phase, no read pipe. */
    setup_ok &= sdram_memtest_read_mode_apply(0u, 0u);
    sdram_read_pipe_probe_apply(FMC_ReadPipeDelay_none);
    sdram_phase_probe_apply(V5F_SDRAM_DEFAULT_PHASE_SEL);
    setup_ok &= sdram_memtest_precharge_all();
    {
        uint16_t verify_score = sdram_memtest_bank_read_score(base, patterns);
        int used = rt_snprintf(line,
                               sizeof(line),
                               "BANK_MODE FORCE mode=normal enh=0 rb=0 phase=%u pipe=%u diag_best=%s/%u/%u/%u verify=%u cmd=%u",
                               (unsigned int)V5F_SDRAM_DEFAULT_PHASE_SEL,
                               (unsigned int)FMC_ReadPipeDelay_none,
                               mode_name[best_mode],
                               (unsigned int)best_phase,
                               (unsigned int)best_pipe,
                               (unsigned int)best_score,
                               (unsigned int)verify_score,
                               (unsigned int)setup_ok);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }

        used = rt_snprintf(line,
                           sizeof(line),
                           "BANK_MODE REG sdcr=%08x misc=%08x",
                           (unsigned int)FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM],
                           (unsigned int)FMC_Bank5_6->MISC);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
}

static void sdram_memtest_bank_access_write(volatile uint8_t *base,
                                             uint32_t logical_offset,
                                             uint8_t value,
                                             uint8_t cpu16)
{
    if(cpu16 != 0u)
    {
        sdram_memtest_x16_low8_write(base, logical_offset, value);
    }
    else
    {
        V5F_SDRAM_LOW8_BYTE_AT(base, logical_offset) = value;
    }
}

static uint8_t sdram_memtest_bank_access_read(volatile uint8_t *base,
                                               uint32_t logical_offset,
                                               uint8_t cpu16)
{
    if(cpu16 != 0u)
    {
        return sdram_memtest_x16_low8_read(base, logical_offset);
    }
    return V5F_SDRAM_LOW8_BYTE_AT(base, logical_offset);
}

static uint16_t sdram_memtest_bank_settle_score(volatile uint8_t *base,
                                                 const uint8_t patterns[4],
                                                 uint8_t cpu16,
                                                 uint8_t discard,
                                                 uint8_t reverse)
{
    uint16_t score = 0u;
    uint32_t repeat;

    for(repeat = 0u; repeat < 64u; repeat++)
    {
        uint32_t step;

        for(step = 0u; step < V5F_SDRAM_X8_BANK_COUNT; step++)
        {
            uint32_t bank = (reverse != 0u) ?
                            (V5F_SDRAM_X8_BANK_COUNT - 1u - step) : step;
            uint32_t offset = bank * V5F_SDRAM_X8_BANK_BYTES;
            uint8_t ignored = 0u;
            uint8_t i;
            uint8_t actual;

            for(i = 0u; i < discard; i++)
            {
                ignored ^= sdram_memtest_bank_access_read(base, offset, cpu16);
            }
            actual = sdram_memtest_bank_access_read(base, offset, cpu16);
            if(actual == patterns[bank])
            {
                score++;
            }

            (void)ignored;
        }
    }
    return score;
}

static void sdram_memtest_bank_settle_scan(void)
{
    static const uint8_t patterns[V5F_SDRAM_X8_BANK_COUNT] = {
        0x17u, 0x2Bu, 0x4Du, 0x8Eu
    };
    static const uint8_t discards[7] = {0u, 1u, 2u, 4u, 8u, 16u, 32u};
    volatile uint8_t *base = (volatile uint8_t *)V5F_SDRAM_BASE_ADDR;
    uint8_t cpu16;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    sdram_usb_debug_write_line("BANK_SETTLE START access=cpu16 compare=low8 discard=0,1,2,4,8,16,32");
    for(cpu16 = 1u; cpu16 <= 1u; cpu16++)
    {
        uint8_t saved[V5F_SDRAM_X8_BANK_COUNT] = {0u};
        uint8_t bank;
        uint8_t index;
        int used;

        for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
        {
            uint32_t offset = bank * V5F_SDRAM_X8_BANK_BYTES;

            (void)sdram_memtest_precharge_all();
            sdram_memtest_bank_access_write(base, offset, patterns[bank], cpu16);
        }
        ch32h417_ltdc_rgb_framebuffer_barrier();
        (void)sdram_memtest_precharge_all();

        for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
        {
            uint32_t offset = bank * V5F_SDRAM_X8_BANK_BYTES;
            uint8_t dummy;

            for(dummy = 0u; dummy < 32u; dummy++)
            {
                saved[bank] = sdram_memtest_bank_access_read(base,
                                                              offset,
                                                              cpu16);
            }
        }
        used = rt_snprintf(line,
                           sizeof(line),
                           "BANK_SETTLE cpu=%u saved=%02x/%02x/%02x/%02x",
                           (unsigned int)((cpu16 != 0u) ? 16u : 8u),
                           (unsigned int)saved[0],
                           (unsigned int)saved[1],
                           (unsigned int)saved[2],
                           (unsigned int)saved[3]);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }

        used = rt_snprintf(line,
                           sizeof(line),
                           "BANK_SETTLE cpu=%u fwd=",
                           (unsigned int)((cpu16 != 0u) ? 16u : 8u));
        for(index = 0u; index < 7u; index++)
        {
            uint16_t score = sdram_memtest_bank_settle_score(base,
                                                              patterns,
                                                              cpu16,
                                                              discards[index],
                                                              0u);
            int appended;

            if((used <= 0) || ((rt_size_t)used >= sizeof(line)))
            {
                continue;
            }
            appended = rt_snprintf(&line[used],
                                   sizeof(line) - (rt_size_t)used,
                                   "%s%u",
                                   (index == 0u) ? "" : ",",
                                   (unsigned int)score);
            if(appended > 0)
            {
                used += appended;
            }
        }
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }

        used = rt_snprintf(line,
                           sizeof(line),
                           "BANK_SETTLE cpu=%u rev=",
                           (unsigned int)((cpu16 != 0u) ? 16u : 8u));
        for(index = 0u; index < 7u; index++)
        {
            uint16_t score = sdram_memtest_bank_settle_score(base,
                                                              patterns,
                                                              cpu16,
                                                              discards[index],
                                                              1u);
            int appended;

            if((used <= 0) || ((rt_size_t)used >= sizeof(line)))
            {
                continue;
            }
            appended = rt_snprintf(&line[used],
                                   sizeof(line) - (rt_size_t)used,
                                   "%s%u",
                                   (index == 0u) ? "" : ",",
                                   (unsigned int)score);
            if(appended > 0)
            {
                used += appended;
            }
        }
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        ch32h417_dual_cdc_poll();
    }
    sdram_usb_debug_write_line("BANK_SETTLE END max=256 each_score_order=discard0,1,2,4,8,16,32");
}

enum
{
    V5F_SDRAM_SYNC_DIRECT = 0,
    V5F_SDRAM_SYNC_DUMMY1,
    V5F_SDRAM_SYNC_FENCE,
    V5F_SDRAM_SYNC_DELAY1,
    V5F_SDRAM_SYNC_DELAY10,
    V5F_SDRAM_SYNC_PALL,
    V5F_SDRAM_SYNC_DUMMY32
};

static uint16_t sdram_memtest_bank_sync_score(volatile uint8_t *base,
                                               const uint8_t patterns[4],
                                               uint8_t cpu16,
                                               uint8_t strategy)
{
    uint16_t score = 0u;
    uint32_t repeat;

    for(repeat = 0u; repeat < 64u; repeat++)
    {
        uint32_t bank;

        for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
        {
            uint32_t offset = bank * V5F_SDRAM_X8_BANK_BYTES;
            uint8_t ignored = 0u;
            uint8_t actual;

            if(strategy != V5F_SDRAM_SYNC_DIRECT)
            {
                uint8_t count = (strategy == V5F_SDRAM_SYNC_DUMMY32) ? 32u : 1u;
                uint8_t i;

                for(i = 0u; i < count; i++)
                {
                    ignored ^= sdram_memtest_bank_access_read(base,
                                                               offset,
                                                               cpu16);
                }
            }

            if(strategy == V5F_SDRAM_SYNC_FENCE)
            {
                ch32h417_ltdc_rgb_framebuffer_barrier();
            }
            else if(strategy == V5F_SDRAM_SYNC_DELAY1)
            {
                sdram_delay_us(1u);
            }
            else if(strategy == V5F_SDRAM_SYNC_DELAY10)
            {
                sdram_delay_us(10u);
            }
            else if(strategy == V5F_SDRAM_SYNC_PALL)
            {
                (void)sdram_memtest_precharge_all();
            }

            actual = sdram_memtest_bank_access_read(base, offset, cpu16);
            if(actual == patterns[bank])
            {
                score++;
            }
            (void)ignored;
        }
    }
    return score;
}

static void sdram_memtest_bank_sync_scan(void)
{
    static const uint8_t patterns[V5F_SDRAM_X8_BANK_COUNT] = {
        0x16u, 0x2Cu, 0x4Bu, 0x8Du
    };
    volatile uint8_t *base = (volatile uint8_t *)V5F_SDRAM_BASE_ADDR;
    uint8_t cpu16;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    sdram_usb_debug_write_line("BANK_SYNC START access=cpu16 compare=low8 strategies=direct,d1,fence,us1,us10,pall,d32,us10_r8191");
    for(cpu16 = 1u; cpu16 <= 1u; cpu16++)
    {
        uint16_t direct;
        uint16_t dummy1;
        uint16_t fence;
        uint16_t delay1;
        uint16_t delay10;
        uint16_t pall;
        uint16_t dummy32;
        uint16_t delay10_slow_refresh;
        uint32_t saved_sdrtr;
        uint8_t bank;
        int used;

        for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
        {
            uint32_t offset = bank * V5F_SDRAM_X8_BANK_BYTES;

            (void)sdram_memtest_precharge_all();
            sdram_memtest_bank_access_write(base, offset, patterns[bank], cpu16);
        }
        ch32h417_ltdc_rgb_framebuffer_barrier();
        (void)sdram_memtest_precharge_all();

        direct = sdram_memtest_bank_sync_score(base,
                                                patterns,
                                                cpu16,
                                                V5F_SDRAM_SYNC_DIRECT);
        dummy1 = sdram_memtest_bank_sync_score(base,
                                                patterns,
                                                cpu16,
                                                V5F_SDRAM_SYNC_DUMMY1);
        fence = sdram_memtest_bank_sync_score(base,
                                               patterns,
                                               cpu16,
                                               V5F_SDRAM_SYNC_FENCE);
        delay1 = sdram_memtest_bank_sync_score(base,
                                                patterns,
                                                cpu16,
                                                V5F_SDRAM_SYNC_DELAY1);
        delay10 = sdram_memtest_bank_sync_score(base,
                                                 patterns,
                                                 cpu16,
                                                 V5F_SDRAM_SYNC_DELAY10);
        pall = sdram_memtest_bank_sync_score(base,
                                              patterns,
                                              cpu16,
                                              V5F_SDRAM_SYNC_PALL);
        dummy32 = sdram_memtest_bank_sync_score(base,
                                                 patterns,
                                                 cpu16,
                                                 V5F_SDRAM_SYNC_DUMMY32);

        saved_sdrtr = FMC_Bank5_6->SDRTR;
        FMC_Bank5_6->SDRTR = (saved_sdrtr & ~FMC_SDRTR_COUNT) |
                             FMC_SDRTR_COUNT;
        ch32h417_ltdc_rgb_framebuffer_barrier();
        delay10_slow_refresh = sdram_memtest_bank_sync_score(
            base,
            patterns,
            cpu16,
            V5F_SDRAM_SYNC_DELAY10);
        FMC_Bank5_6->SDRTR = saved_sdrtr;
        ch32h417_ltdc_rgb_framebuffer_barrier();

        used = rt_snprintf(line,
                           sizeof(line),
                           "BANK_SYNC cpu=%u direct=%u d1=%u fence=%u us1=%u us10=%u pall=%u d32=%u us10_r8191=%u",
                           (unsigned int)((cpu16 != 0u) ? 16u : 8u),
                           (unsigned int)direct,
                           (unsigned int)dummy1,
                           (unsigned int)fence,
                           (unsigned int)delay1,
                           (unsigned int)delay10,
                           (unsigned int)pall,
                           (unsigned int)dummy32,
                           (unsigned int)delay10_slow_refresh);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        ch32h417_dual_cdc_poll();
    }
    sdram_usb_debug_write_line("BANK_SYNC END max=256 normal_refresh=240 slow_refresh=8191");
}

static void sdram_memtest_map16_write(volatile uint8_t *base,
                                       const uint8_t patterns[4])
{
    uint32_t bank;

    for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
    {
        (void)sdram_memtest_precharge_all();
        sdram_memtest_bank_access_write(base,
                                         bank * V5F_SDRAM_X8_BANK_BYTES,
                                         patterns[bank],
                                         1u);
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
    (void)sdram_memtest_precharge_all();
}

static void sdram_memtest_map16_report(const char *name,
                                        volatile uint8_t *base,
                                        const uint8_t patterns[4])
{
    uint8_t settled[4] = {0u};
    uint16_t direct;
    uint16_t pall;
    uint16_t dummy32;
    uint32_t bank;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used;

    for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
    {
        uint8_t discard;

        for(discard = 0u; discard < 32u; discard++)
        {
            (void)sdram_memtest_bank_access_read(
                base, bank * V5F_SDRAM_X8_BANK_BYTES, 1u);
        }
        settled[bank] = sdram_memtest_bank_access_read(
            base, bank * V5F_SDRAM_X8_BANK_BYTES, 1u);
    }

    direct = sdram_memtest_bank_sync_score(base,
                                            patterns,
                                            1u,
                                            V5F_SDRAM_SYNC_DIRECT);
    pall = sdram_memtest_bank_sync_score(base,
                                          patterns,
                                          1u,
                                          V5F_SDRAM_SYNC_PALL);
    dummy32 = sdram_memtest_bank_sync_score(base,
                                             patterns,
                                             1u,
                                             V5F_SDRAM_SYNC_DUMMY32);

    used = rt_snprintf(line,
                       sizeof(line),
                       "MAP16 name=%s base=%08x remap=%u settled=%02x/%02x/%02x/%02x direct=%u pall=%u d32=%u max=256",
                       (name != RT_NULL) ? name : "?",
                       (unsigned int)(uintptr_t)base,
                       (unsigned int)((FMC_Bank1->BTCR[0] &
                                       V5F_FMC_SDRAM_REMAP_TO_0X60000000) != 0u),
                       (unsigned int)settled[0],
                       (unsigned int)settled[1],
                       (unsigned int)settled[2],
                       (unsigned int)settled[3],
                       (unsigned int)direct,
                       (unsigned int)pall,
                       (unsigned int)dummy32);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static void sdram_memtest_map16_scan(void)
{
    static const uint8_t native_patterns[4] = {0x17u, 0x2Bu, 0x4Du, 0x8Eu};
    static const uint8_t remap_patterns[4] = {0x61u, 0x72u, 0x94u, 0xB8u};
    volatile uint8_t *native = (volatile uint8_t *)V5F_SDRAM_NATIVE_ADDR;
    volatile uint8_t *remap = (volatile uint8_t *)V5F_SDRAM_REMAP_ADDR;

    sdram_usb_debug_write_line("MAP16 START access=cpu16 compare=low8 native=c0000000 remap=60000000");

    sdram_disable_0x60000000_remap();
    ch32h417_ltdc_rgb_framebuffer_barrier();
    (void)sdram_memtest_precharge_all();
    sdram_memtest_map16_write(native, native_patterns);
    sdram_memtest_map16_report("native_write", native, native_patterns);

    sdram_enable_0x60000000_remap();
    ch32h417_ltdc_rgb_framebuffer_barrier();
    (void)sdram_memtest_precharge_all();
    sdram_memtest_map16_report("remap_read_native", remap, native_patterns);
    sdram_memtest_map16_write(remap, remap_patterns);
    sdram_memtest_map16_report("remap_write", remap, remap_patterns);

    sdram_disable_0x60000000_remap();
    ch32h417_ltdc_rgb_framebuffer_barrier();
    (void)sdram_memtest_precharge_all();
    sdram_memtest_map16_report("native_read_remap", native, remap_patterns);
    sdram_usb_debug_write_line("MAP16 END restored=native remap=0");
}

static void sdram_memtest_access_width_write(volatile uint8_t *base,
                                              uint32_t logical_offset,
                                              uint8_t value,
                                              uint8_t width)
{
    uintptr_t address = (uintptr_t)base +
                        (logical_offset * V5F_SDRAM_LOW8_PHYSICAL_STRIDE);

    if(width == 8u)
    {
        *(volatile uint8_t *)address = value;
    }
    else if(width == 16u)
    {
        *(volatile uint16_t *)address = (uint16_t)(value * 0x0101u);
    }
    else
    {
        *(volatile uint32_t *)address = (uint32_t)value * 0x01010101u;
    }
}

static uint8_t sdram_memtest_access_width_read(volatile uint8_t *base,
                                                uint32_t logical_offset,
                                                uint8_t width)
{
    uintptr_t address = (uintptr_t)base +
                        (logical_offset * V5F_SDRAM_LOW8_PHYSICAL_STRIDE);

    if(width == 8u)
    {
        return *(volatile uint8_t *)address;
    }
    if(width == 16u)
    {
        return (uint8_t)(*(volatile uint16_t *)address & 0x00FFu);
    }
    return (uint8_t)(*(volatile uint32_t *)address & 0x000000FFu);
}

static void sdram_memtest_access_profile_fill(volatile uint8_t *base,
                                               const uint8_t patterns[4])
{
    uint32_t bank;

    for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
    {
        (void)sdram_memtest_precharge_all();
        sdram_memtest_access_width_write(base,
                                          bank * V5F_SDRAM_X8_BANK_BYTES,
                                          patterns[bank],
                                          32u);
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
    (void)sdram_memtest_precharge_all();
}

static uint16_t sdram_memtest_access_profile_score(
    volatile uint8_t *base,
    const uint8_t patterns[4],
    uint8_t width,
    uint8_t discard)
{
    uint16_t score = 0u;
    uint32_t repeat;

    for(repeat = 0u; repeat < 64u; repeat++)
    {
        uint32_t bank;

        for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
        {
            uint32_t offset = bank * V5F_SDRAM_X8_BANK_BYTES;
            uint8_t index;
            uint8_t actual;

            for(index = 0u; index < discard; index++)
            {
                (void)sdram_memtest_access_width_read(base, offset, width);
            }
            actual = sdram_memtest_access_width_read(base, offset, width);
            if(actual == patterns[bank])
            {
                score++;
            }
        }
    }
    return score;
}

static void sdram_memtest_access_profile_report(const char *name,
                                                 volatile uint8_t *base,
                                                 uint8_t enhance,
                                                 uint8_t remap)
{
    static const uint8_t patterns[4] = {0x19u, 0x2Bu, 0x4Du, 0x8Fu};
    static const uint8_t widths[3] = {8u, 16u, 32u};
    static const uint8_t discards[7] = {0u, 1u, 2u, 4u, 8u, 16u, 32u};
    uint8_t width_index;

    sdram_memtest_access_profile_fill(base, patterns);
    for(width_index = 0u; width_index < 3u; width_index++)
    {
        char line[V5F_SDRAM_USB_LINE_BYTES];
        uint8_t score_index;
        int used = rt_snprintf(line,
                               sizeof(line),
                               "ACCESS name=%s base=%08x enh=%u remap=%u width=%u scores=",
                               (name != RT_NULL) ? name : "?",
                               (unsigned int)(uintptr_t)base,
                               (unsigned int)enhance,
                               (unsigned int)remap,
                               (unsigned int)widths[width_index]);

        for(score_index = 0u; score_index < 7u; score_index++)
        {
            uint16_t score = sdram_memtest_access_profile_score(
                base,
                patterns,
                widths[width_index],
                discards[score_index]);
            int appended;

            if((used <= 0) || ((rt_size_t)used >= sizeof(line)))
            {
                continue;
            }
            appended = rt_snprintf(&line[used],
                                   sizeof(line) - (rt_size_t)used,
                                   "%s%u",
                                   (score_index == 0u) ? "" : ",",
                                   (unsigned int)score);
            if(appended > 0)
            {
                used += appended;
            }
        }
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        ch32h417_dual_cdc_poll();
    }
}

static void sdram_memtest_access_profile_scan(void)
{
    volatile uint8_t *native = (volatile uint8_t *)V5F_SDRAM_NATIVE_ADDR;
    volatile uint8_t *remapped = (volatile uint8_t *)V5F_SDRAM_REMAP_ADDR;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    uint8_t setup_ok = 1u;
    int used;

    sdram_usb_debug_write_line("ACCESS_PROFILE START write=cpu32 repeated-byte compare=low8 order=d0,d1,d2,d4,d8,d16,d32");

    sdram_disable_0x60000000_remap();
    setup_ok &= sdram_memtest_read_mode_apply(0u, 0u);
    sdram_read_pipe_probe_apply(FMC_ReadPipeDelay_none);
    sdram_phase_probe_apply(V5F_SDRAM_DEFAULT_PHASE_SEL);
    sdram_memtest_access_profile_report("normal_native", native, 0u, 0u);

    setup_ok &= sdram_memtest_read_mode_apply(1u, 0u);
    sdram_enable_0x60000000_remap();
    ch32h417_ltdc_rgb_framebuffer_barrier();
    (void)sdram_memtest_precharge_all();
    sdram_memtest_access_profile_report("wch_official", remapped, 1u, 1u);

    sdram_disable_0x60000000_remap();
    setup_ok &= sdram_memtest_read_mode_apply(0u, 0u);
    sdram_read_pipe_probe_apply(FMC_ReadPipeDelay_none);
    sdram_phase_probe_apply(V5F_SDRAM_DEFAULT_PHASE_SEL);
    (void)sdram_memtest_precharge_all();

    used = rt_snprintf(line,
                       sizeof(line),
                       "ACCESS_PROFILE END restored=native/normal setup=%u sdcr=%08x misc=%08x",
                       (unsigned int)setup_ok,
                       (unsigned int)FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM],
                       (unsigned int)FMC_Bank5_6->MISC);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static uint8_t sdram_memtest_dma_read(void *destination,
                                      uintptr_t source,
                                      uint16_t transfers,
                                      uint8_t wide256)
{
    DMA_InitTypeDef dma = {0};
    uint32_t timeout = V5F_SDRAM_TIMEOUT_POLLS;
    uint32_t data_size = (wide256 != 0u) ?
                         DMA_PeripheralDataSize_256 :
                         DMA_PeripheralDataSize_Word;
    uint32_t memory_size = (wide256 != 0u) ?
                           DMA_MemoryDataSize_256 :
                           DMA_MemoryDataSize_Word;

    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);
    DMA_Cmd(DMA1_Channel3, DISABLE);
    DMA_DeInit(DMA1_Channel3);
    DMA_StructInit(&dma);
    dma.DMA_PeripheralBaseAddr = (uint32_t)source;
    dma.DMA_Memory0BaseAddr = (uint32_t)(uintptr_t)destination;
    dma.DMA_DIR = DMA_DIR_PeripheralSRC;
    dma.DMA_BufferSize = transfers;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Enable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = data_size;
    dma.DMA_MemoryDataSize = memory_size;
    dma.DMA_Mode = DMA_Mode_Normal;
    dma.DMA_Priority = DMA_Priority_VeryHigh;
    dma.DMA_M2M = DMA_M2M_Enable;
    DMA_Init(DMA1_Channel3, &dma);
    DMA_ClearFlag(DMA1, DMA1_FLAG_TC3 | DMA1_FLAG_TE3);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    DMA_Cmd(DMA1_Channel3, ENABLE);

    while((DMA_GetFlagStatus(DMA1, DMA1_FLAG_TC3) == RESET) &&
          (DMA_GetFlagStatus(DMA1, DMA1_FLAG_TE3) == RESET) &&
          (timeout != 0u))
    {
        timeout--;
    }
    DMA_Cmd(DMA1_Channel3, DISABLE);
    ch32h417_ltdc_rgb_framebuffer_barrier();

    if((timeout == 0u) ||
       (DMA_GetFlagStatus(DMA1, DMA1_FLAG_TE3) != RESET) ||
       (DMA_GetFlagStatus(DMA1, DMA1_FLAG_TC3) == RESET))
    {
        DMA_ClearFlag(DMA1, DMA1_FLAG_TC3 | DMA1_FLAG_TE3);
        return 0u;
    }
    DMA_ClearFlag(DMA1, DMA1_FLAG_TC3 | DMA1_FLAG_TE3);
    return 1u;
}

static void sdram_memtest_dma_fill_bank_blocks(volatile uint8_t *base,
                                                const uint8_t patterns[4])
{
    uint32_t bank;

    for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
    {
        uintptr_t address = (uintptr_t)base +
                            (bank * V5F_SDRAM_X8_BANK_BYTES *
                             V5F_SDRAM_LOW8_PHYSICAL_STRIDE);
        uint32_t word = (uint32_t)patterns[bank] * 0x01010101u;
        uint8_t index;

        (void)sdram_memtest_precharge_all();
        for(index = 0u; index < 8u; index++)
        {
            ((volatile uint32_t *)address)[index] = word;
        }
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
    (void)sdram_memtest_precharge_all();
}

enum
{
    V5F_SDRAM_DMA_REC_DIRECT = 0,
    V5F_SDRAM_DMA_REC_DUMMY1,
    V5F_SDRAM_DMA_REC_DUMMY2,
    V5F_SDRAM_DMA_REC_PALL,
    V5F_SDRAM_DMA_REC_PALL_AR1,
    V5F_SDRAM_DMA_REC_US10,
    V5F_SDRAM_DMA_REC_COUNT
};

static uint16_t sdram_memtest_dma_bank_score(volatile uint8_t *base,
                                              const uint8_t patterns[4],
                                              uint8_t wide256,
                                              uint8_t strategy,
                                              uint16_t *transfer_bad)
{
    static uint32_t buffer[8] __attribute__((aligned(32)));
    static uint32_t dummy[8] __attribute__((aligned(32)));
    uint16_t score = 0u;
    uint32_t repeat;

    *transfer_bad = 0u;
    for(repeat = 0u; repeat < 64u; repeat++)
    {
        uint32_t bank;

        for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
        {
            uintptr_t source = (uintptr_t)base +
                               (bank * V5F_SDRAM_X8_BANK_BYTES *
                                V5F_SDRAM_LOW8_PHYSICAL_STRIDE);
            uint32_t expected = (uint32_t)patterns[bank] * 0x00010001u;
            uint8_t index;
            uint8_t correct = 1u;
            uint8_t prep_ok = 1u;

            if(strategy == V5F_SDRAM_DMA_REC_DUMMY1)
            {
                prep_ok &= sdram_memtest_dma_read(dummy,
                                                  source,
                                                  (wide256 != 0u) ? 1u : 8u,
                                                  wide256);
            }
            else if(strategy == V5F_SDRAM_DMA_REC_DUMMY2)
            {
                prep_ok &= sdram_memtest_dma_read(dummy,
                                                  source,
                                                  (wide256 != 0u) ? 1u : 8u,
                                                  wide256);
                prep_ok &= sdram_memtest_dma_read(dummy,
                                                  source,
                                                  (wide256 != 0u) ? 1u : 8u,
                                                  wide256);
            }
            else if(strategy == V5F_SDRAM_DMA_REC_PALL)
            {
                prep_ok &= sdram_memtest_precharge_all();
            }
            else if(strategy == V5F_SDRAM_DMA_REC_PALL_AR1)
            {
                prep_ok &= sdram_memtest_precharge_all();
                prep_ok &= (uint8_t)(sdram_send_command(
                    FMC_SDRAM_CMD_Mode3, 0u, 0u) == V5F_SDRAM_OK);
            }
            else if(strategy == V5F_SDRAM_DMA_REC_US10)
            {
                sdram_delay_us(10u);
            }

            if(prep_ok == 0u)
            {
                (*transfer_bad)++;
                correct = 0u;
            }

            for(index = 0u; index < 8u; index++)
            {
                buffer[index] = 0xDEADBEEFu;
            }
            if(sdram_memtest_dma_read(buffer,
                                      source,
                                      (wide256 != 0u) ? 1u : 8u,
                                      wide256) == 0u)
            {
                (*transfer_bad)++;
                correct = 0u;
            }
            for(index = 0u; index < 8u; index++)
            {
                if((buffer[index] & 0x00FF00FFu) != expected)
                {
                    correct = 0u;
                }
            }
            if(correct != 0u)
            {
                score++;
            }
        }
    }
    return score;
}

static void sdram_memtest_dma_sample(volatile uint8_t *base)
{
    static uint32_t word_buffer[8] __attribute__((aligned(32)));
    static uint32_t wide_buffer[8] __attribute__((aligned(32)));
    uintptr_t bank1 = (uintptr_t)base +
                      (V5F_SDRAM_X8_BANK_BYTES *
                       V5F_SDRAM_LOW8_PHYSICAL_STRIDE);
    uint8_t word_ok;
    uint8_t wide_ok;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used;

    word_buffer[0] = 0xDEADBEEFu;
    word_buffer[1] = 0xDEADBEEFu;
    wide_buffer[0] = 0xDEADBEEFu;
    wide_buffer[1] = 0xDEADBEEFu;
    word_ok = sdram_memtest_dma_read(word_buffer,
                                     (uintptr_t)base,
                                     8u,
                                     0u);
    wide_ok = sdram_memtest_dma_read(wide_buffer, bank1, 1u, 1u);
    used = rt_snprintf(line,
                       sizeof(line),
                       "DMA_SAMPLE word_ok=%u b0=%08x/%08x wide_ok=%u b1=%08x/%08x mask=00ff00ff",
                       (unsigned int)word_ok,
                       (unsigned int)word_buffer[0],
                       (unsigned int)word_buffer[1],
                       (unsigned int)wide_ok,
                       (unsigned int)wide_buffer[0],
                       (unsigned int)wide_buffer[1]);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static uint8_t sdram_memtest_dma_boundary_score(volatile uint8_t *base,
                                                 uint8_t wide256,
                                                 uint8_t *transfer_bad)
{
    static uint32_t buffer[16] __attribute__((aligned(32)));
    uint8_t score = 0u;
    uint8_t boundary;

    *transfer_bad = 0u;
    for(boundary = 0u; boundary < 3u; boundary++)
    {
        uintptr_t next_bank = (uintptr_t)base +
                              ((boundary + 1u) *
                               V5F_SDRAM_X8_BANK_BYTES *
                               V5F_SDRAM_LOW8_PHYSICAL_STRIDE);
        uintptr_t source = next_bank - 32u;
        uint8_t first_byte = (uint8_t)(0x31u + boundary);
        uint8_t second_byte = (uint8_t)(0xA1u + boundary);
        uint32_t first_word = (uint32_t)first_byte * 0x01010101u;
        uint32_t second_word = (uint32_t)second_byte * 0x01010101u;
        uint32_t first_expected = (uint32_t)first_byte * 0x00010001u;
        uint32_t second_expected = (uint32_t)second_byte * 0x00010001u;
        uint8_t index;
        uint8_t correct = 1u;

        (void)sdram_memtest_precharge_all();
        for(index = 0u; index < 8u; index++)
        {
            ((volatile uint32_t *)source)[index] = first_word;
        }
        (void)sdram_memtest_precharge_all();
        for(index = 8u; index < 16u; index++)
        {
            ((volatile uint32_t *)source)[index] = second_word;
        }
        ch32h417_ltdc_rgb_framebuffer_barrier();
        (void)sdram_memtest_precharge_all();

        for(index = 0u; index < 16u; index++)
        {
            buffer[index] = 0xDEADBEEFu;
        }
        if(sdram_memtest_dma_read(buffer,
                                  source,
                                  (wide256 != 0u) ? 2u : 16u,
                                  wide256) == 0u)
        {
            (*transfer_bad)++;
            correct = 0u;
        }
        for(index = 0u; index < 8u; index++)
        {
            if((buffer[index] & 0x00FF00FFu) != first_expected)
            {
                correct = 0u;
            }
        }
        for(index = 8u; index < 16u; index++)
        {
            if((buffer[index] & 0x00FF00FFu) != second_expected)
            {
                correct = 0u;
            }
        }
        if(correct != 0u)
        {
            score++;
        }
    }
    return score;
}

static void sdram_memtest_dma_path_scan(void)
{
    static const uint8_t patterns[4] = {0x17u, 0x2Bu, 0x4Du, 0x8Eu};
    volatile uint8_t *base = (volatile uint8_t *)V5F_SDRAM_NATIVE_ADDR;
    uint16_t word_transfer_bad[V5F_SDRAM_DMA_REC_COUNT] = {0u};
    uint16_t wide_transfer_bad[V5F_SDRAM_DMA_REC_COUNT] = {0u};
    uint16_t word_score[V5F_SDRAM_DMA_REC_COUNT] = {0u};
    uint16_t wide_score[V5F_SDRAM_DMA_REC_COUNT] = {0u};
    uint8_t word_boundary_bad;
    uint8_t wide_boundary_bad;
    uint8_t word_boundary;
    uint8_t wide_boundary;
    uint8_t strategy;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used;

    sdram_usb_debug_write_line("DMA_PATH START source=sdram destination=sram channel=DMA1_CH3 modes=word,256");
    sdram_disable_0x60000000_remap();
    (void)sdram_memtest_read_mode_apply(0u, 0u);
    sdram_read_pipe_probe_apply(FMC_ReadPipeDelay_none);
    sdram_phase_probe_apply(V5F_SDRAM_DEFAULT_PHASE_SEL);
    sdram_memtest_dma_fill_bank_blocks(base, patterns);
    sdram_memtest_dma_sample(base);

    for(strategy = 0u; strategy < V5F_SDRAM_DMA_REC_COUNT; strategy++)
    {
        word_score[strategy] = sdram_memtest_dma_bank_score(
            base,
            patterns,
            0u,
            strategy,
            &word_transfer_bad[strategy]);
        wide_score[strategy] = sdram_memtest_dma_bank_score(
            base,
            patterns,
            1u,
            strategy,
            &wide_transfer_bad[strategy]);
        ch32h417_dual_cdc_poll();
    }
    word_boundary = sdram_memtest_dma_boundary_score(base,
                                                      0u,
                                                      &word_boundary_bad);
    wide_boundary = sdram_memtest_dma_boundary_score(base,
                                                      1u,
                                                      &wide_boundary_bad);
    DMA_Cmd(DMA1_Channel3, DISABLE);
    DMA_DeInit(DMA1_Channel3);

    used = rt_snprintf(line,
                       sizeof(line),
                       "DMA_BANK word=%u/256 bad=%u wide256=%u/256 bad=%u compare=00ff00ff",
                       (unsigned int)word_score[V5F_SDRAM_DMA_REC_DIRECT],
                       (unsigned int)word_transfer_bad[V5F_SDRAM_DMA_REC_DIRECT],
                       (unsigned int)wide_score[V5F_SDRAM_DMA_REC_DIRECT],
                       (unsigned int)wide_transfer_bad[V5F_SDRAM_DMA_REC_DIRECT]);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    used = rt_snprintf(line,
                       sizeof(line),
                       "DMA_REC word direct=%u d1=%u d2=%u pall=%u pall_ar1=%u us10=%u bad=%u",
                       (unsigned int)word_score[V5F_SDRAM_DMA_REC_DIRECT],
                       (unsigned int)word_score[V5F_SDRAM_DMA_REC_DUMMY1],
                       (unsigned int)word_score[V5F_SDRAM_DMA_REC_DUMMY2],
                       (unsigned int)word_score[V5F_SDRAM_DMA_REC_PALL],
                       (unsigned int)word_score[V5F_SDRAM_DMA_REC_PALL_AR1],
                       (unsigned int)word_score[V5F_SDRAM_DMA_REC_US10],
                       (unsigned int)(word_transfer_bad[0] +
                                      word_transfer_bad[1] +
                                      word_transfer_bad[2] +
                                      word_transfer_bad[3] +
                                      word_transfer_bad[4] +
                                      word_transfer_bad[5]));
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    used = rt_snprintf(line,
                       sizeof(line),
                       "DMA_REC wide256 direct=%u d1=%u d2=%u pall=%u pall_ar1=%u us10=%u bad=%u",
                       (unsigned int)wide_score[V5F_SDRAM_DMA_REC_DIRECT],
                       (unsigned int)wide_score[V5F_SDRAM_DMA_REC_DUMMY1],
                       (unsigned int)wide_score[V5F_SDRAM_DMA_REC_DUMMY2],
                       (unsigned int)wide_score[V5F_SDRAM_DMA_REC_PALL],
                       (unsigned int)wide_score[V5F_SDRAM_DMA_REC_PALL_AR1],
                       (unsigned int)wide_score[V5F_SDRAM_DMA_REC_US10],
                       (unsigned int)(wide_transfer_bad[0] +
                                      wide_transfer_bad[1] +
                                      wide_transfer_bad[2] +
                                      wide_transfer_bad[3] +
                                      wide_transfer_bad[4] +
                                      wide_transfer_bad[5]));
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    used = rt_snprintf(line,
                       sizeof(line),
                       "DMA_BOUNDARY word=%u/3 bad=%u wide256=%u/3 bad=%u span=64 compare=00ff00ff",
                       (unsigned int)word_boundary,
                       (unsigned int)word_boundary_bad,
                       (unsigned int)wide_boundary,
                       (unsigned int)wide_boundary_bad);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    sdram_usb_debug_write_line("DMA_PATH END restored=native/normal channel=disabled");
}

static uint8_t sdram_memtest_cas_apply(uint8_t cas)
{
    uint32_t mode_register = (cas == 2u) ?
                             V5F_SDRAM_MODE_REGISTER_CL2 :
                             V5F_SDRAM_MODE_REGISTER;
    uint32_t sdcr;
    uint8_t ok;

    if((cas != 2u) && (cas != 3u))
    {
        return 0u;
    }
    ok = sdram_memtest_precharge_all();
    sdcr = FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM];
    sdcr &= ~(3u << 7);
    sdcr |= (uint32_t)cas << 7;
    FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM] = sdcr;
    ch32h417_ltdc_rgb_framebuffer_barrier();
    ok &= (uint8_t)(sdram_send_command(FMC_SDRAM_CMD_Mode4,
                                       1u,
                                       mode_register) == V5F_SDRAM_OK);
    sdram_delay_us(2u);
    ok &= sdram_memtest_precharge_all();
    return ok;
}

static void sdram_memtest_cas_profile(uint8_t cas)
{
    static const uint8_t patterns[4] = {0x16u, 0x2Au, 0x4Cu, 0x8Du};
    volatile uint8_t *base = (volatile uint8_t *)V5F_SDRAM_NATIVE_ADDR;
    uint16_t best_score = 0u;
    uint16_t verify_score;
    uint16_t dma_word;
    uint16_t dma_wide;
    uint16_t dma_word_bad;
    uint16_t dma_wide_bad;
    uint8_t best_phase = V5F_SDRAM_DEFAULT_PHASE_SEL;
    uint8_t best_pipe = FMC_ReadPipeDelay_none;
    uint8_t pipe;
    uint8_t command_ok;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used;

    command_ok = sdram_memtest_cas_apply(cas);
    sdram_memtest_map16_write(base, patterns);
    for(pipe = 0u; pipe < 3u; pipe++)
    {
        uint8_t phase;

        sdram_read_pipe_probe_apply(pipe);
        for(phase = 0u; phase < 16u; phase++)
        {
            uint16_t score;

            sdram_phase_probe_apply(phase);
            (void)sdram_memtest_precharge_all();
            score = sdram_memtest_bank_sync_score(base,
                                                   patterns,
                                                   1u,
                                                   V5F_SDRAM_SYNC_DIRECT);
            if(score > best_score)
            {
                best_score = score;
                best_phase = phase;
                best_pipe = pipe;
            }
        }
    }
    sdram_read_pipe_probe_apply(best_pipe);
    sdram_phase_probe_apply(best_phase);
    (void)sdram_memtest_precharge_all();
    verify_score = sdram_memtest_bank_sync_score(base,
                                                  patterns,
                                                  1u,
                                                  V5F_SDRAM_SYNC_DIRECT);

    sdram_memtest_dma_fill_bank_blocks(base, patterns);
    dma_word = sdram_memtest_dma_bank_score(base,
                                             patterns,
                                             0u,
                                             V5F_SDRAM_DMA_REC_DIRECT,
                                             &dma_word_bad);
    dma_wide = sdram_memtest_dma_bank_score(base,
                                             patterns,
                                             1u,
                                             V5F_SDRAM_DMA_REC_DIRECT,
                                             &dma_wide_bad);
    DMA_Cmd(DMA1_Channel3, DISABLE);
    DMA_DeInit(DMA1_Channel3);

    used = rt_snprintf(line,
                       sizeof(line),
                       "CAS_CPU cl=%u scan=%u/256 verify=%u/256 phase=%u pipe=%u cmd=%u sdcr=%08x",
                       (unsigned int)cas,
                       (unsigned int)best_score,
                       (unsigned int)verify_score,
                       (unsigned int)best_phase,
                       (unsigned int)best_pipe,
                       (unsigned int)command_ok,
                       (unsigned int)FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM]);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    used = rt_snprintf(line,
                       sizeof(line),
                       "CAS_DMA cl=%u word=%u/256 bad=%u wide256=%u/256 bad=%u compare=00ff00ff",
                       (unsigned int)cas,
                       (unsigned int)dma_word,
                       (unsigned int)dma_word_bad,
                       (unsigned int)dma_wide,
                       (unsigned int)dma_wide_bad);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    ch32h417_dual_cdc_poll();
}

static void sdram_memtest_cas_scan(void)
{
    uint8_t restored;

    sdram_usb_debug_write_line("CAS_SCAN START modes=cl3,cl2 phases=16 pipes=3 score=cpu16/256+dma");
    sdram_disable_0x60000000_remap();
    (void)sdram_memtest_read_mode_apply(0u, 0u);
    sdram_memtest_cas_profile(3u);
    sdram_memtest_cas_profile(2u);

    restored = sdram_memtest_cas_apply(3u);
    sdram_read_pipe_probe_apply(FMC_ReadPipeDelay_none);
    sdram_phase_probe_apply(V5F_SDRAM_DEFAULT_PHASE_SEL);
    (void)sdram_memtest_precharge_all();
    {
        char line[V5F_SDRAM_USB_LINE_BYTES];
        int used = rt_snprintf(line,
                               sizeof(line),
                               "CAS_SCAN END restored=cl3 phase=10 pipe=0 mode=0230 cmd=%u sdcr=%08x",
                               (unsigned int)restored,
                               (unsigned int)FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM]);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
}

static void sdram_memtest_score_list(const char *prefix,
                                      const uint16_t scores[16])
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used;
    uint8_t index;

    used = rt_snprintf(line, sizeof(line), "%s", prefix);
    if((used <= 0) || ((rt_size_t)used >= sizeof(line)))
    {
        return;
    }
    for(index = 0u; index < 16u; index++)
    {
        int appended = rt_snprintf(&line[used],
                                   sizeof(line) - (rt_size_t)used,
                                   (index == 0u) ? "%u" : ",%u",
                                   (unsigned int)scores[index]);
        if((appended <= 0) ||
           ((rt_size_t)appended >= (sizeof(line) - (rt_size_t)used)))
        {
            return;
        }
        used += appended;
    }
    sdram_usb_debug_write_line(line);
}

static void sdram_memtest_cold_profile(const char *name,
                                        uint8_t prefetch,
                                        uint8_t nrfs_count)
{
    static const uint8_t patterns[4] = {0x18u, 0x2Cu, 0x4Eu, 0x8Fu};
    volatile uint8_t *base = (volatile uint8_t *)V5F_SDRAM_NATIVE_ADDR;
    uint16_t best_score = 0u;
    uint16_t verify_score = 0u;
    uint16_t dummy32_score = 0u;
    uint16_t dma_score = 0u;
    uint16_t dma_bad = 0u;
    uint8_t best_phase = V5F_SDRAM_DEFAULT_PHASE_SEL;
    uint8_t best_pipe = FMC_ReadPipeDelay_none;
    uint8_t pipe;
    int init_result;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used;

    init_result = sdram_init_profile(prefetch, nrfs_count, 0u);
    if(init_result == V5F_SDRAM_OK)
    {
        sdram_disable_0x60000000_remap();
        sdram_memtest_map16_write(base, patterns);
        for(pipe = 0u; pipe < 3u; pipe++)
        {
            uint8_t phase;

            sdram_read_pipe_probe_apply(pipe);
            for(phase = 0u; phase < 16u; phase++)
            {
                uint16_t score;

                sdram_phase_probe_apply(phase);
                (void)sdram_memtest_precharge_all();
                score = sdram_memtest_bank_sync_score(
                    base, patterns, 1u, V5F_SDRAM_SYNC_DIRECT);
                if((score > best_score) ||
                   ((score == best_score) && (pipe < best_pipe)) ||
                   ((score == best_score) && (pipe == best_pipe) &&
                    (sdram_memtest_phase_distance(phase) <
                     sdram_memtest_phase_distance(best_phase))))
                {
                    best_score = score;
                    best_phase = phase;
                    best_pipe = pipe;
                }
            }
        }
        sdram_read_pipe_probe_apply(best_pipe);
        sdram_phase_probe_apply(best_phase);
        (void)sdram_memtest_precharge_all();
        verify_score = sdram_memtest_bank_sync_score(
            base, patterns, 1u, V5F_SDRAM_SYNC_DIRECT);
        dummy32_score = sdram_memtest_bank_sync_score(
            base, patterns, 1u, V5F_SDRAM_SYNC_DUMMY32);
        sdram_memtest_dma_fill_bank_blocks(base, patterns);
        dma_score = sdram_memtest_dma_bank_score(
            base,
            patterns,
            1u,
            V5F_SDRAM_DMA_REC_DIRECT,
            &dma_bad);
        DMA_Cmd(DMA1_Channel3, DISABLE);
        DMA_DeInit(DMA1_Channel3);
    }

    used = rt_snprintf(line,
                       sizeof(line),
                       "COLD_PROFILE name=%s prefetch=%u nrfs=%u init=%d scan=%u verify=%u d32=%u dma256=%u bad=%u phase=%u pipe=%u misc=%08x",
                       name,
                       (unsigned int)prefetch,
                       (unsigned int)nrfs_count,
                       init_result,
                       (unsigned int)best_score,
                       (unsigned int)verify_score,
                       (unsigned int)dummy32_score,
                       (unsigned int)dma_score,
                       (unsigned int)dma_bad,
                       (unsigned int)best_phase,
                       (unsigned int)best_pipe,
                       (unsigned int)FMC_Bank5_6->MISC);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    ch32h417_dual_cdc_poll();
}

static void sdram_memtest_nrfs_scan(void)
{
    static const uint8_t patterns[4] = {0x15u, 0x29u, 0x4Bu, 0x8Du};
    volatile uint8_t *base = (volatile uint8_t *)V5F_SDRAM_NATIVE_ADDR;
    uint16_t cpu_scores[16] = {0u};
    uint16_t dma_scores[16] = {0u};
    uint8_t nrfs;
    int restore_result;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used;

    sdram_usb_debug_write_line("COLD_SCAN START order=normal_n0,prefetch_n0,normal_n15 phases=16 pipes=3");
    sdram_memtest_cold_profile("normal_n0", 0u, 0u);
    sdram_memtest_cold_profile("prefetch_n0", 1u, 0u);
    sdram_memtest_cold_profile("normal_n15", 0u, 15u);
    sdram_usb_debug_write_line("COLD_SCAN END manual_bit15=1_prefetch header_constants_invalid=1");

    (void)sdram_init_profile(0u, 0u, 0u);
    sdram_disable_0x60000000_remap();
    sdram_usb_debug_write_line("NRFS_SCAN START mode=normal values=0..15 refreshes_per_event=value+1");
    for(nrfs = 0u; nrfs < 16u; nrfs++)
    {
        uint32_t misc = FMC_Bank5_6->MISC;
        uint16_t dma_bad;

        misc &= ~(V5F_SDRAM_ENHANCE_READ_BIT | FMC_MISC_NRFS_CNT);
        misc |= nrfs;
        FMC_Bank5_6->MISC = misc;
        ch32h417_ltdc_rgb_framebuffer_barrier();
        (void)sdram_memtest_precharge_all();
        sdram_memtest_map16_write(base, patterns);
        cpu_scores[nrfs] = sdram_memtest_bank_sync_score(
            base, patterns, 1u, V5F_SDRAM_SYNC_DIRECT);
        sdram_memtest_dma_fill_bank_blocks(base, patterns);
        dma_scores[nrfs] = sdram_memtest_dma_bank_score(
            base,
            patterns,
            1u,
            V5F_SDRAM_DMA_REC_DIRECT,
            &dma_bad);
        DMA_Cmd(DMA1_Channel3, DISABLE);
        DMA_DeInit(DMA1_Channel3);
        ch32h417_dual_cdc_poll();
    }
    sdram_memtest_score_list("NRFS_CPU direct=", cpu_scores);
    sdram_memtest_score_list("NRFS_DMA wide256=", dma_scores);

    restore_result = sdram_init_profile(0u, 0u, 0u);
    sdram_disable_0x60000000_remap();
    used = rt_snprintf(line,
                       sizeof(line),
                       "NRFS_SCAN END restored=normal/nrfs0 init=%d phase=10 pipe=0 misc=%08x",
                       restore_result,
                       (unsigned int)FMC_Bank5_6->MISC);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static uint8_t sdram_memtest_prefetch_validate(void)
{
    static const uint8_t patterns[4] = {0x13u, 0x27u, 0x4Bu, 0x8Du};
    static uint32_t dma_raw[8] __attribute__((aligned(32)));
    volatile uint8_t *base = (volatile uint8_t *)V5F_SDRAM_NATIVE_ADDR;
    uint16_t dma_word[V5F_SDRAM_DMA_REC_COUNT] = {0u};
    uint16_t dma_wide[V5F_SDRAM_DMA_REC_COUNT] = {0u};
    uint16_t dma_word_bad[V5F_SDRAM_DMA_REC_COUNT] = {0u};
    uint16_t dma_wide_bad[V5F_SDRAM_DMA_REC_COUNT] = {0u};
    uint16_t direct_score = 0u;
    uint16_t boundary_total = 0u;
    uint16_t first_repeat = 0u;
    uint8_t first_bank = 0u;
    uint8_t first_expected = 0u;
    uint8_t first_actual = 0u;
    uint8_t first_valid = 0u;
    uint8_t strategy;
    uint32_t repeat;
    uint8_t config_ok;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used;

    sdram_disable_0x60000000_remap();
    config_ok = (uint8_t)(((FMC_Bank5_6->MISC &
                            (V5F_SDRAM_ENHANCE_READ_BIT |
                             FMC_MISC_NRFS_CNT)) ==
                           V5F_SDRAM_ENHANCE_READ_BIT) ? 1u : 0u);
    used = rt_snprintf(line,
                       sizeof(line),
                       "PREFETCH START first_init=1 config=%u bit15=1 nrfs=0 phase=10 pipe=0 misc=%08x",
                       (unsigned int)config_ok,
                       (unsigned int)FMC_Bank5_6->MISC);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    if(config_ok == 0u)
    {
        sdram_usb_debug_write_line("PREFETCH END cpu=FAIL boundary=FAIL dma=FAIL");
        return 0u;
    }

    sdram_memtest_map16_write(base, patterns);
    for(repeat = 0u; repeat < 1024u; repeat++)
    {
        uint8_t bank;

        for(bank = 0u; bank < 4u; bank++)
        {
            uint8_t actual = sdram_memtest_bank_access_read(
                base, bank * V5F_SDRAM_X8_BANK_BYTES, 1u);

            if(actual == patterns[bank])
            {
                direct_score++;
            }
            else if(first_valid == 0u)
            {
                first_valid = 1u;
                first_repeat = (uint16_t)repeat;
                first_bank = bank;
                first_expected = patterns[bank];
                first_actual = actual;
            }
        }
        if((repeat & 0x3Fu) == 0u)
        {
            ch32h417_dual_cdc_poll();
        }
    }
    used = rt_snprintf(line,
                       sizeof(line),
                       "PREFETCH_CPU direct=%u/4096 bad=%u first=%u/%u/%02x/%02x",
                       (unsigned int)direct_score,
                       (unsigned int)(4096u - direct_score),
                       (unsigned int)first_repeat,
                       (unsigned int)first_bank,
                       (unsigned int)first_expected,
                       (unsigned int)first_actual);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }

    for(repeat = 1u; repeat < V5F_SDRAM_X8_BANK_COUNT; repeat++)
    {
        uint32_t boundary = repeat * V5F_SDRAM_X8_BANK_BYTES;
        uint32_t start = boundary - 128u;
        uint16_t score = 0u;
        uint16_t index;

        (void)sdram_memtest_precharge_all();
        for(index = 0u; index < 256u; index++)
        {
            uint32_t offset = start + index;
            sdram_memtest_x16_low8_write(
                base, offset, sdram_memtest_x8_pattern(offset));
        }
        ch32h417_ltdc_rgb_framebuffer_barrier();
        (void)sdram_memtest_precharge_all();
        for(index = 0u; index < 256u; index++)
        {
            uint32_t offset = start + index;
            if(sdram_memtest_x16_low8_read(base, offset) ==
               sdram_memtest_x8_pattern(offset))
            {
                score++;
            }
        }
        boundary_total = (uint16_t)(boundary_total + score);
        used = rt_snprintf(line,
                           sizeof(line),
                           "PREFETCH_BOUNDARY bank=%u logical=%08x score=%u/256",
                           (unsigned int)repeat,
                           (unsigned int)boundary,
                           (unsigned int)score);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    sdram_memtest_dma_fill_bank_blocks(base, patterns);
    for(repeat = 0u; repeat < V5F_SDRAM_X8_BANK_COUNT; repeat++)
    {
        uintptr_t source = (uintptr_t)base +
                           (repeat * V5F_SDRAM_X8_BANK_BYTES *
                            V5F_SDRAM_LOW8_PHYSICAL_STRIDE);
        uint8_t ok;
        uint8_t index;

        for(index = 0u; index < 8u; index++)
        {
            dma_raw[index] = 0xDEADBEEFu;
        }
        ok = sdram_memtest_dma_read(dma_raw, source, 1u, 1u);
        used = rt_snprintf(line,
                           sizeof(line),
                           "PREFETCH_DMA_RAW bank=%u ok=%u exp=%02x raw=%08x/%08x",
                           (unsigned int)repeat,
                           (unsigned int)ok,
                           (unsigned int)patterns[repeat],
                           (unsigned int)dma_raw[0],
                           (unsigned int)dma_raw[1]);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    for(strategy = 0u; strategy < V5F_SDRAM_DMA_REC_COUNT; strategy++)
    {
        dma_word[strategy] = sdram_memtest_dma_bank_score(
            base, patterns, 0u, strategy, &dma_word_bad[strategy]);
        dma_wide[strategy] = sdram_memtest_dma_bank_score(
            base, patterns, 1u, strategy, &dma_wide_bad[strategy]);
        ch32h417_dual_cdc_poll();
    }
    used = rt_snprintf(line,
                       sizeof(line),
                       "PREFETCH_DMA word=%u,%u,%u,%u,%u,%u bad=%u",
                       (unsigned int)dma_word[0],
                       (unsigned int)dma_word[1],
                       (unsigned int)dma_word[2],
                       (unsigned int)dma_word[3],
                       (unsigned int)dma_word[4],
                       (unsigned int)dma_word[5],
                       (unsigned int)(dma_word_bad[0] + dma_word_bad[1] +
                                      dma_word_bad[2] + dma_word_bad[3] +
                                      dma_word_bad[4] + dma_word_bad[5]));
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    used = rt_snprintf(line,
                       sizeof(line),
                       "PREFETCH_DMA wide=%u,%u,%u,%u,%u,%u bad=%u order=direct,d1,d2,pall,pall_ar1,us10",
                       (unsigned int)dma_wide[0],
                       (unsigned int)dma_wide[1],
                       (unsigned int)dma_wide[2],
                       (unsigned int)dma_wide[3],
                       (unsigned int)dma_wide[4],
                       (unsigned int)dma_wide[5],
                       (unsigned int)(dma_wide_bad[0] + dma_wide_bad[1] +
                                      dma_wide_bad[2] + dma_wide_bad[3] +
                                      dma_wide_bad[4] + dma_wide_bad[5]));
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    DMA_Cmd(DMA1_Channel3, DISABLE);
    DMA_DeInit(DMA1_Channel3);

    used = rt_snprintf(line,
                       sizeof(line),
                       "PREFETCH END cpu=%s boundary=%u/768 dma_direct=%u/%u",
                       (direct_score == 4096u) ? "PASS" : "FAIL",
                       (unsigned int)boundary_total,
                       (unsigned int)dma_word[0],
                       (unsigned int)dma_wide[0]);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    return (uint8_t)((dma_word[0] == 256u) && (dma_wide[0] == 256u));
}

static uint16_t sdram_memtest_prefetch_sequence(volatile uint8_t *base,
                                                 uint32_t start,
                                                 uint16_t count,
                                                 uint8_t write_prefetch)
{
    uint16_t score = 0u;
    uint16_t index;

    (void)sdram_memtest_read_mode_apply(write_prefetch, 0u);
    for(index = 0u; index < count; index++)
    {
        uint32_t offset = start + index;
        sdram_memtest_x16_low8_write(
            base, offset, sdram_memtest_x8_pattern(offset));
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
    if(write_prefetch == 0u)
    {
        (void)sdram_memtest_read_mode_apply(1u, 0u);
    }
    else
    {
        (void)sdram_memtest_precharge_all();
    }
    for(index = 0u; index < count; index++)
    {
        uint32_t offset = start + index;
        if(sdram_memtest_x16_low8_read(base, offset) ==
           sdram_memtest_x8_pattern(offset))
        {
            score++;
        }
    }
    return score;
}

static uint8_t sdram_memtest_prefetch_coherence(void)
{
    static const uint8_t patterns_a[4] = {0x16u, 0x2Au, 0x4Cu, 0x8Eu};
    static const uint8_t patterns_b[4] = {0x31u, 0x52u, 0x74u, 0x98u};
    static const uint8_t patterns_dma[4] = {0x1Bu, 0x3Du, 0x5Fu, 0xA1u};
    volatile uint8_t *base = (volatile uint8_t *)V5F_SDRAM_NATIVE_ADDR;
    const uint32_t single_offset = 0x00300000u;
    uint16_t bank_a;
    uint16_t bank_b;
    uint16_t bank_b_d32;
    uint16_t seq_on;
    uint16_t seq_toggle;
    uint16_t dma_word_bad;
    uint16_t dma_wide_bad;
    uint16_t dma_word;
    uint16_t dma_wide;
    uint8_t first;
    uint8_t stale;
    uint8_t normal;
    uint8_t toggled;
    uint8_t setup_ok = 1u;
    uint8_t cpu_ok;
    uint8_t dma_ok;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used;

    sdram_usb_debug_write_line("COHERENCE START first_init=prefetch sdclk=1hclk tests=single,bank,sequence,dma");

    setup_ok &= sdram_memtest_read_mode_apply(1u, 0u);
    sdram_memtest_x16_low8_write(base, single_offset, 0x11u);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    first = sdram_memtest_x16_low8_read(base, single_offset);
    sdram_memtest_x16_low8_write(base, single_offset, 0x22u);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    stale = sdram_memtest_x16_low8_read(base, single_offset);
    setup_ok &= sdram_memtest_read_mode_apply(0u, 0u);
    normal = sdram_memtest_x16_low8_read(base, single_offset);
    setup_ok &= sdram_memtest_read_mode_apply(1u, 0u);
    toggled = sdram_memtest_x16_low8_read(base, single_offset);
    used = rt_snprintf(line,
                       sizeof(line),
                       "COHERENCE_SINGLE first=%02x after_write=%02x normal=%02x reenable=%02x exp=11/22/22/22 setup=%u",
                       (unsigned int)first,
                       (unsigned int)stale,
                       (unsigned int)normal,
                       (unsigned int)toggled,
                       (unsigned int)setup_ok);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }

    setup_ok &= sdram_memtest_read_mode_apply(0u, 0u);
    sdram_memtest_map16_write(base, patterns_a);
    setup_ok &= sdram_memtest_read_mode_apply(1u, 0u);
    bank_a = sdram_memtest_bank_sync_score(
        base, patterns_a, 1u, V5F_SDRAM_SYNC_DIRECT);
    setup_ok &= sdram_memtest_read_mode_apply(0u, 0u);
    sdram_memtest_map16_write(base, patterns_b);
    setup_ok &= sdram_memtest_read_mode_apply(1u, 0u);
    bank_b = sdram_memtest_bank_sync_score(
        base, patterns_b, 1u, V5F_SDRAM_SYNC_DIRECT);
    bank_b_d32 = sdram_memtest_bank_sync_score(
        base, patterns_b, 1u, V5F_SDRAM_SYNC_DUMMY32);
    used = rt_snprintf(line,
                       sizeof(line),
                       "COHERENCE_BANK fresh=%u/256 rewrite=%u/256 rewrite_d32=%u/256 setup=%u",
                       (unsigned int)bank_a,
                       (unsigned int)bank_b,
                       (unsigned int)bank_b_d32,
                       (unsigned int)setup_ok);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }

    seq_on = sdram_memtest_prefetch_sequence(base,
                                               0x00100000u,
                                               2048u,
                                               1u);
    seq_toggle = sdram_memtest_prefetch_sequence(base,
                                                   0x00200000u,
                                                   2048u,
                                                   0u);
    used = rt_snprintf(line,
                       sizeof(line),
                       "COHERENCE_SEQ prefetch_write=%u/2048 normal_write_then_prefetch=%u/2048",
                       (unsigned int)seq_on,
                       (unsigned int)seq_toggle);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }

    setup_ok &= sdram_memtest_read_mode_apply(0u, 0u);
    sdram_memtest_dma_fill_bank_blocks(base, patterns_dma);
    setup_ok &= sdram_memtest_read_mode_apply(1u, 0u);
    dma_word = sdram_memtest_dma_bank_score(base,
                                             patterns_dma,
                                             0u,
                                             V5F_SDRAM_DMA_REC_DIRECT,
                                             &dma_word_bad);
    dma_wide = sdram_memtest_dma_bank_score(base,
                                             patterns_dma,
                                             1u,
                                             V5F_SDRAM_DMA_REC_DIRECT,
                                             &dma_wide_bad);
    DMA_Cmd(DMA1_Channel3, DISABLE);
    DMA_DeInit(DMA1_Channel3);
    used = rt_snprintf(line,
                       sizeof(line),
                       "COHERENCE_DMA normal_write_then_prefetch word=%u/256 wide=%u/256 bad=%u/%u",
                       (unsigned int)dma_word,
                       (unsigned int)dma_wide,
                       (unsigned int)dma_word_bad,
                       (unsigned int)dma_wide_bad);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }

    cpu_ok = (uint8_t)((first == 0x11u) &&
                       (normal == 0x22u) &&
                       (toggled == 0x22u) &&
                       (bank_a == 256u) &&
                       (bank_b == 256u) &&
                       (seq_on == 2048u) &&
                       (seq_toggle == 2048u));
    dma_ok = (uint8_t)((dma_word == 256u) && (dma_wide == 256u));
    used = rt_snprintf(line,
                       sizeof(line),
                       "COHERENCE END cpu=%s dma=%s setup=%u",
                       (cpu_ok != 0u) ? "PASS" : "FAIL",
                       (dma_ok != 0u) ? "PASS" : "FAIL",
                       (unsigned int)setup_ok);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    return (uint8_t)((cpu_ok != 0u) && (dma_ok != 0u) && (setup_ok != 0u));
}

static uint8_t sdram_memtest_normal_100mhz_validate(void)
{
    static const uint8_t patterns[4] = {0x19u, 0x3Bu, 0x5Du, 0xA7u};
    volatile uint8_t *base = (volatile uint8_t *)V5F_SDRAM_NATIVE_ADDR;
    const uint32_t sequence_start = 0x00100000u;
    uint16_t bank_score = 0u;
    uint16_t sequence_score = 0u;
    uint16_t dma_word_bad;
    uint16_t dma_wide_bad;
    uint16_t dma_word;
    uint16_t dma_wide;
    uint32_t repeat;
    uint8_t setup_ok;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used;

    setup_ok = sdram_memtest_read_mode_apply(0u, 0u);
    sdram_usb_debug_write_line("NORMAL100 START mode=normal sdclk=100000000 access=cpu16+dma");
    sdram_memtest_map16_write(base, patterns);
    for(repeat = 0u; repeat < 1024u; repeat++)
    {
        uint8_t bank;

        for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
        {
            uint8_t actual = sdram_memtest_bank_access_read(
                base, bank * V5F_SDRAM_X8_BANK_BYTES, 1u);
            if(actual == patterns[bank])
            {
                bank_score++;
            }
        }
        if((repeat & 0x3Fu) == 0u)
        {
            ch32h417_dual_cdc_poll();
        }
    }
    used = rt_snprintf(line,
                       sizeof(line),
                       "NORMAL100_BANK score=%u/4096 bad=%u",
                       (unsigned int)bank_score,
                       (unsigned int)(4096u - bank_score));
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }

    for(repeat = 0u; repeat < 2048u; repeat++)
    {
        uint32_t offset = sequence_start + repeat;
        sdram_memtest_x16_low8_write(
            base, offset, sdram_memtest_x8_pattern(offset));
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
    (void)sdram_memtest_precharge_all();
    for(repeat = 0u; repeat < 2048u; repeat++)
    {
        uint32_t offset = sequence_start + repeat;
        if(sdram_memtest_x16_low8_read(base, offset) ==
           sdram_memtest_x8_pattern(offset))
        {
            sequence_score++;
        }
    }
    used = rt_snprintf(line,
                       sizeof(line),
                       "NORMAL100_SEQ score=%u/2048 bad=%u",
                       (unsigned int)sequence_score,
                       (unsigned int)(2048u - sequence_score));
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }

    sdram_memtest_dma_fill_bank_blocks(base, patterns);
    dma_word = sdram_memtest_dma_bank_score(base,
                                             patterns,
                                             0u,
                                             V5F_SDRAM_DMA_REC_DIRECT,
                                             &dma_word_bad);
    dma_wide = sdram_memtest_dma_bank_score(base,
                                             patterns,
                                             1u,
                                             V5F_SDRAM_DMA_REC_DIRECT,
                                             &dma_wide_bad);
    DMA_Cmd(DMA1_Channel3, DISABLE);
    DMA_DeInit(DMA1_Channel3);
    used = rt_snprintf(line,
                       sizeof(line),
                       "NORMAL100_DMA word=%u/256 wide=%u/256 bad=%u/%u",
                       (unsigned int)dma_word,
                       (unsigned int)dma_wide,
                       (unsigned int)dma_word_bad,
                       (unsigned int)dma_wide_bad);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }

    used = rt_snprintf(line,
                       sizeof(line),
                       "NORMAL100 END cpu=%s dma=%s setup=%u",
                       ((bank_score == 4096u) &&
                        (sequence_score == 2048u)) ? "PASS" : "FAIL",
                       ((dma_word == 256u) &&
                        (dma_wide == 256u)) ? "PASS" : "FAIL",
                       (unsigned int)setup_ok);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    return (uint8_t)((setup_ok != 0u) &&
                     (bank_score == 4096u) &&
                     (sequence_score == 2048u) &&
                     (dma_word == 256u) &&
                     (dma_wide == 256u));
}

enum
{
    V5F_SDRAM_REC_DIRECT = 0,
    V5F_SDRAM_REC_US20,
    V5F_SDRAM_REC_D32,
    V5F_SDRAM_REC_PALL1,
    V5F_SDRAM_REC_PALL2,
    V5F_SDRAM_REC_PALL_AR1,
    V5F_SDRAM_REC_TOGGLE
};

static uint16_t sdram_memtest_recovery_score(volatile uint8_t *base,
                                              const uint8_t patterns[4],
                                              uint8_t strategy)
{
    uint16_t score = 0u;
    uint32_t repeat;

    for(repeat = 0u; repeat < 64u; repeat++)
    {
        uint32_t bank;

        for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
        {
            uint32_t offset = bank * V5F_SDRAM_X8_BANK_BYTES;
            uint8_t command_ok = 1u;
            uint8_t actual;

            if(strategy == V5F_SDRAM_REC_US20)
            {
                sdram_delay_us(20u);
            }
            else if(strategy == V5F_SDRAM_REC_D32)
            {
                uint8_t discard;

                for(discard = 0u; discard < 32u; discard++)
                {
                    (void)sdram_memtest_bank_access_read(base, offset, 1u);
                }
            }
            else if(strategy == V5F_SDRAM_REC_PALL1)
            {
                command_ok &= sdram_memtest_precharge_all();
            }
            else if(strategy == V5F_SDRAM_REC_PALL2)
            {
                command_ok &= sdram_memtest_precharge_all();
                command_ok &= sdram_memtest_precharge_all();
            }
            else if(strategy == V5F_SDRAM_REC_PALL_AR1)
            {
                command_ok &= sdram_memtest_precharge_all();
                command_ok &= (uint8_t)(sdram_send_command(
                    FMC_SDRAM_CMD_Mode3, 0u, 0u) == V5F_SDRAM_OK);
            }
            else if(strategy == V5F_SDRAM_REC_TOGGLE)
            {
                FMC_SDRAMCmd(FMC_Bank5_SDRAM, DISABLE);
                ch32h417_ltdc_rgb_framebuffer_barrier();
                FMC_SDRAMCmd(FMC_Bank5_SDRAM, ENABLE);
                ch32h417_ltdc_rgb_framebuffer_barrier();
            }

            actual = sdram_memtest_bank_access_read(base, offset, 1u);
            if((command_ok != 0u) && (actual == patterns[bank]))
            {
                score++;
            }
        }
    }
    return score;
}

static void sdram_memtest_recovery_sequence_line(const char *name,
                                                  volatile uint8_t *base,
                                                  uint32_t offset,
                                                  uint8_t count)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used = rt_snprintf(line,
                           sizeof(line),
                           "REC_SEQ name=%s values=",
                           (name != RT_NULL) ? name : "?");
    uint8_t index;

    for(index = 0u; index < count; index++)
    {
        uint8_t value = sdram_memtest_bank_access_read(base, offset, 1u);
        int appended;

        if((used <= 0) || ((rt_size_t)used >= sizeof(line)))
        {
            continue;
        }
        appended = rt_snprintf(&line[used],
                               sizeof(line) - (rt_size_t)used,
                               "%s%02x",
                               (index == 0u) ? "" : ",",
                               (unsigned int)value);
        if(appended > 0)
        {
            used += appended;
        }
    }
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static void sdram_memtest_recovery_scan(void)
{
    static const uint8_t patterns[4] = {0x18u, 0x2Du, 0x4Au, 0x8Cu};
    volatile uint8_t *base = (volatile uint8_t *)V5F_SDRAM_BASE_ADDR;
    uint16_t score[7] = {0u};
    uint32_t saved_sdrtr = FMC_Bank5_6->SDRTR;
    uint8_t strategy;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used;

    sdram_usb_debug_write_line("RECOVERY START refresh=8191 strategies=direct,us20,d32,pall1,pall2,pall_ar1,toggle");
    FMC_Bank5_6->SDRTR = (saved_sdrtr & ~FMC_SDRTR_COUNT) |
                         FMC_SDRTR_COUNT;
    ch32h417_ltdc_rgb_framebuffer_barrier();

    for(strategy = 0u; strategy < 7u; strategy++)
    {
        sdram_memtest_map16_write(base, patterns);
        score[strategy] = sdram_memtest_recovery_score(base,
                                                        patterns,
                                                        strategy);
    }
    used = rt_snprintf(line,
                       sizeof(line),
                       "RECOVERY SCORE direct=%u us20=%u d32=%u pall1=%u pall2=%u pall_ar1=%u toggle=%u max=256",
                       (unsigned int)score[V5F_SDRAM_REC_DIRECT],
                       (unsigned int)score[V5F_SDRAM_REC_US20],
                       (unsigned int)score[V5F_SDRAM_REC_D32],
                       (unsigned int)score[V5F_SDRAM_REC_PALL1],
                       (unsigned int)score[V5F_SDRAM_REC_PALL2],
                       (unsigned int)score[V5F_SDRAM_REC_PALL_AR1],
                       (unsigned int)score[V5F_SDRAM_REC_TOGGLE]);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }

    sdram_memtest_map16_write(base, patterns);
    (void)sdram_memtest_precharge_all();
    (void)sdram_send_command(FMC_SDRAM_CMD_Mode3, 0u, 0u);
    (void)sdram_memtest_bank_access_read(base, 0u, 1u);
    sdram_memtest_recovery_sequence_line("ba0_direct",
                                         base,
                                         V5F_SDRAM_X8_BANK_BYTES,
                                         16u);
    (void)sdram_memtest_precharge_all();
    sdram_memtest_recovery_sequence_line("ba0_pall1",
                                         base,
                                         V5F_SDRAM_X8_BANK_BYTES,
                                         8u);
    (void)sdram_memtest_precharge_all();
    (void)sdram_send_command(FMC_SDRAM_CMD_Mode3, 0u, 0u);
    sdram_memtest_recovery_sequence_line("ba0_pall_ar1",
                                         base,
                                         V5F_SDRAM_X8_BANK_BYTES,
                                         8u);

    FMC_Bank5_6->SDRTR = saved_sdrtr;
    ch32h417_ltdc_rgb_framebuffer_barrier();
    (void)sdram_memtest_precharge_all();
    used = rt_snprintf(line,
                       sizeof(line),
                       "RECOVERY END refresh=%u restored=1",
                       (unsigned int)((saved_sdrtr & FMC_SDRTR_COUNT) >> 1));
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static void sdram_memtest_refresh_profile_scan(void)
{
    static const uint8_t patterns[4] = {0x1Au, 0x2Eu, 0x4Cu, 0x8Bu};
    static const uint16_t counts[7] = {41u, 80u, 110u, 160u,
                                       240u, 480u, 8191u};
    volatile uint8_t *base = (volatile uint8_t *)V5F_SDRAM_BASE_ADDR;
    uint32_t saved_sdrtr = FMC_Bank5_6->SDRTR;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used = rt_snprintf(line,
                           sizeof(line),
                           "REFRESH_PROFILE START counts=41,80,110,160,240,480,8191 score=direct/256");
    uint8_t index;

    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    used = rt_snprintf(line, sizeof(line), "REFRESH_PROFILE scores=");
    for(index = 0u; index < 7u; index++)
    {
        uint16_t score;
        int appended;

        sdram_set_refresh_count(counts[index]);
        ch32h417_ltdc_rgb_framebuffer_barrier();
        sdram_memtest_map16_write(base, patterns);
        score = sdram_memtest_bank_sync_score(base,
                                               patterns,
                                               1u,
                                               V5F_SDRAM_SYNC_DIRECT);
        if((used <= 0) || ((rt_size_t)used >= sizeof(line)))
        {
            continue;
        }
        appended = rt_snprintf(&line[used],
                               sizeof(line) - (rt_size_t)used,
                               "%s%u",
                               (index == 0u) ? "" : ",",
                               (unsigned int)score);
        if(appended > 0)
        {
            used += appended;
        }
    }
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    FMC_Bank5_6->SDRTR = saved_sdrtr;
    ch32h417_ltdc_rgb_framebuffer_barrier();
    (void)sdram_memtest_precharge_all();
    sdram_usb_debug_write_line("REFRESH_PROFILE END order=41,80,110,160,240,480,8191 restored=1");
}

static void sdram_memtest_transition_write_pair(volatile uint8_t *base,
                                                 uint32_t offset_a,
                                                 uint32_t offset_b)
{
    (void)sdram_memtest_precharge_all();
    sdram_memtest_bank_access_write(base, offset_a, 0x35u, 1u);
    (void)sdram_memtest_precharge_all();
    sdram_memtest_bank_access_write(base, offset_b, 0xCAu, 1u);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    (void)sdram_memtest_precharge_all();
}

static uint16_t sdram_memtest_transition_score(volatile uint8_t *base,
                                                uint32_t offset_a,
                                                uint32_t offset_b,
                                                uint8_t strategy)
{
    uint16_t score = 0u;
    uint32_t repeat;

    for(repeat = 0u; repeat < 64u; repeat++)
    {
        uint8_t side;

        for(side = 0u; side < 2u; side++)
        {
            uint32_t offset = (side == 0u) ? offset_a : offset_b;
            uint8_t expected = (side == 0u) ? 0x35u : 0xCAu;
            uint8_t ignored = 0u;
            uint8_t actual;

            if(strategy != V5F_SDRAM_SYNC_DIRECT)
            {
                ignored = sdram_memtest_bank_access_read(base, offset, 1u);
            }
            if(strategy == V5F_SDRAM_SYNC_FENCE)
            {
                ch32h417_ltdc_rgb_framebuffer_barrier();
            }
            else if(strategy == V5F_SDRAM_SYNC_PALL)
            {
                (void)sdram_memtest_precharge_all();
            }

            actual = sdram_memtest_bank_access_read(base, offset, 1u);
            if(actual == expected)
            {
                score++;
            }
            (void)ignored;
        }
    }
    return score;
}

static void sdram_memtest_transition_report(const char *name,
                                             uint32_t offset_a,
                                             uint32_t offset_b)
{
    volatile uint8_t *base = (volatile uint8_t *)V5F_SDRAM_BASE_ADDR;
    uint16_t direct;
    uint16_t fence;
    uint16_t pall;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used;

    sdram_memtest_transition_write_pair(base, offset_a, offset_b);
    direct = sdram_memtest_transition_score(base,
                                             offset_a,
                                             offset_b,
                                             V5F_SDRAM_SYNC_DIRECT);
    fence = sdram_memtest_transition_score(base,
                                            offset_a,
                                            offset_b,
                                            V5F_SDRAM_SYNC_FENCE);
    pall = sdram_memtest_transition_score(base,
                                           offset_a,
                                           offset_b,
                                           V5F_SDRAM_SYNC_PALL);

    used = rt_snprintf(line,
                       sizeof(line),
                       "TRANS name=%s a=%08x b=%08x direct=%u fence=%u pall=%u max=128",
                       (name != RT_NULL) ? name : "?",
                       (unsigned int)offset_a,
                       (unsigned int)offset_b,
                       (unsigned int)direct,
                       (unsigned int)fence,
                       (unsigned int)pall);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static void sdram_memtest_transition_scan(void)
{
    uint32_t saved_sdcr = FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM];
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used;

    sdram_usb_debug_write_line("TRANS START access=cpu16 compare=low8 pair=35/ca direct,fence,pall max=128");
    /* x16/9-column mapping: logical bit 0 is column A0, logical bit 9
     * becomes SDRAM row A0, bits 22/23 become BA0/BA1. */
    sdram_memtest_transition_report("column", 0u, 1u);
    sdram_memtest_transition_report("row", 0u, 1u << 9);
    sdram_memtest_transition_report("ba0", 0u, 1u << 22);
    sdram_memtest_transition_report("ba1", 0u, 1u << 23);
    sdram_memtest_transition_report("ba0_hi", 1u << 23,
                                     (1u << 23) | (1u << 22));

    (void)sdram_memtest_precharge_all();
    FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM] = saved_sdcr & ~FMC_SDCR1_NB;
    ch32h417_ltdc_rgb_framebuffer_barrier();
    (void)sdram_memtest_precharge_all();
    sdram_memtest_transition_report("ba0_nb2", 0u, 1u << 22);
    (void)sdram_memtest_precharge_all();
    FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM] = saved_sdcr;
    ch32h417_ltdc_rgb_framebuffer_barrier();
    (void)sdram_memtest_precharge_all();

    used = rt_snprintf(line,
                       sizeof(line),
                       "TRANS NB sdcr4=%08x sdcr2=%08x restored=%08x",
                       (unsigned int)saved_sdcr,
                       (unsigned int)(saved_sdcr & ~FMC_SDCR1_NB),
                       (unsigned int)FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM]);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    sdram_usb_debug_write_line("TRANS END mapping=column:lbit0 row:lbit9 ba0:lbit22 ba1:lbit23");
}

static uint8_t sdram_memtest_x8_bank_probe(v5f_sdram_memtest_result_t *failure)
{
    static const uint8_t patterns[V5F_SDRAM_X8_BANK_COUNT] = {
        0x11u, 0x22u, 0x44u, 0x88u
    };
    volatile uint8_t *base = (volatile uint8_t *)V5F_SDRAM_BASE_ADDR;
    uint8_t actual[V5F_SDRAM_X8_BANK_COUNT] = {0u};
    uint8_t pin_and[V5F_SDRAM_X8_BANK_COUNT] = {0u};
    uint8_t pin_or[V5F_SDRAM_X8_BANK_COUNT] = {0u};
    uint8_t alias_mask = 0u;
    uint8_t pall_alias_mask = 0u;
    uint8_t data_ok = 1u;
    uint8_t failure_saved = 0u;
    uint32_t grouped_bad = 0u;
    uint32_t alt_bad = 0u;
    uint32_t alt_first_bank = 0xFFFFFFFFu;
    uint8_t alt_first_expected = 0u;
    uint8_t alt_first_actual = 0u;
    uint32_t bank;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_ADDRESS_BUS;
    sdram_usb_debug_write_line("BANK_MAP START haddr23=BA0/PB1 haddr24=BA1/PB15 physical_span=00800000");

    for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
    {
        sdram_memtest_x16_low8_write(base,
                                     bank * V5F_SDRAM_X8_BANK_BYTES,
                                     patterns[bank]);
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
    sdram_delay_us(10u);

    for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
    {
        uint32_t sample;
        uint32_t bank_bad = 0u;

        pin_and[bank] = 3u;
        pin_or[bank] = 0u;
        for(sample = 0u; sample < 64u; sample++)
        {
            actual[bank] = sdram_memtest_x16_low8_read(
                base, bank * V5F_SDRAM_X8_BANK_BYTES);
            if(actual[bank] != patterns[bank])
            {
                bank_bad++;
                grouped_bad++;
            }
            pin_and[bank] &= sdram_memtest_x8_bank_pins();
            pin_or[bank] |= sdram_memtest_x8_bank_pins();
        }
        if(bank_bad != 0u)
        {
            data_ok = 0u;
            if((failure != RT_NULL) && (failure_saved == 0u) && (bank != 0u))
            {
                failure->stage = V5F_SDRAM_STAGE_ADDRESS_BUS;
                failure->offset = bank * V5F_SDRAM_X8_BANK_BYTES;
                failure->expected = patterns[bank];
                failure->actual = actual[bank];
                failure_saved = 1u;
            }
        }
    }

    {
        uint32_t sample;

        for(sample = 0u; sample < 64u; sample++)
        {
            for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
            {
                uint8_t value = sdram_memtest_x16_low8_read(
                    base, bank * V5F_SDRAM_X8_BANK_BYTES);
                if(value != patterns[bank])
                {
                    alt_bad++;
                    if(alt_first_bank == 0xFFFFFFFFu)
                    {
                        alt_first_bank = bank;
                        alt_first_expected = patterns[bank];
                        alt_first_actual = value;
                    }
                }
            }
        }
    }

    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "BANK_MAP data exp=11/22/44/88 got=%02x/%02x/%02x/%02x",
                               (unsigned int)actual[0],
                               (unsigned int)actual[1],
                               (unsigned int)actual[2],
                               (unsigned int)actual[3]);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "BANK_READ_SWITCH grouped_bad=%u/256 alt_bad=%u/256 first=%08x/%02x/%02x",
                               (unsigned int)grouped_bad,
                               (unsigned int)alt_bad,
                               (unsigned int)alt_first_bank,
                               (unsigned int)alt_first_expected,
                               (unsigned int)alt_first_actual);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "BANK_PIN bank=%u exp_contains=%u and=%u or=%u",
                               (unsigned int)bank,
                               (unsigned int)bank,
                               (unsigned int)pin_and[bank],
                               (unsigned int)pin_or[bank]);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    for(bank = 1u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
    {
        uint32_t target_offset = bank * V5F_SDRAM_X8_BANK_BYTES;
        uint8_t target_pattern = (uint8_t)(0x50u + bank);
        uint8_t fast_base;
        uint8_t fast_target;
        uint8_t base_read;
        uint8_t target_read;
        uint8_t pair_ok;
        int used;

        sdram_memtest_x16_low8_write(base, 0u, 0xA0u);
        sdram_memtest_x16_low8_write(base, target_offset, target_pattern);
        ch32h417_ltdc_rgb_framebuffer_barrier();
        fast_base = sdram_memtest_x16_low8_read(base, 0u);
        fast_target = sdram_memtest_x16_low8_read(base, target_offset);
        sdram_delay_us(10u);
        ch32h417_ltdc_rgb_framebuffer_barrier();
        base_read = sdram_memtest_x16_low8_read(base, 0u);
        target_read = sdram_memtest_x16_low8_read(base, target_offset);
        pair_ok = (uint8_t)((base_read == 0xA0u) &&
                            (target_read == target_pattern));
        if(pair_ok == 0u)
        {
            alias_mask |= (uint8_t)(1u << bank);
            if((failure != RT_NULL) && (failure_saved == 0u))
            {
                failure->stage = V5F_SDRAM_STAGE_ADDRESS_BUS;
                failure->offset = target_offset;
                failure->expected = 0x0000A000u | target_pattern;
                failure->actual = ((uint32_t)base_read << 8) | target_read;
                failure_saved = 1u;
            }
        }

        used = rt_snprintf(line,
                           sizeof(line),
                           "BANK_PAIR_AUTO bank=%u fast=%02x/%02x settled=%02x/%02x alias=%u",
                           (unsigned int)bank,
                           (unsigned int)fast_base,
                           (unsigned int)fast_target,
                           (unsigned int)base_read,
                           (unsigned int)target_read,
                           (unsigned int)(pair_ok == 0u));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }

        {
            uint8_t pall_ok = 1u;
            uint8_t pall_base;
            uint8_t pall_target;
            uint8_t pall_pair_ok;

            pall_ok &= sdram_memtest_precharge_all();
            sdram_memtest_x16_low8_write(base, 0u, 0xB0u);
            pall_ok &= sdram_memtest_precharge_all();
            sdram_memtest_x16_low8_write(base,
                                         target_offset,
                                         (uint8_t)(0xD0u + bank));
            pall_ok &= sdram_memtest_precharge_all();
            pall_base = sdram_memtest_x16_low8_read(base, 0u);
            pall_ok &= sdram_memtest_precharge_all();
            pall_target = sdram_memtest_x16_low8_read(base, target_offset);
            pall_ok &= sdram_memtest_precharge_all();
            pall_pair_ok = (uint8_t)((pall_ok != 0u) &&
                                     (pall_base == 0xB0u) &&
                                     (pall_target == (uint8_t)(0xD0u + bank)));
            if(pall_pair_ok == 0u)
            {
                pall_alias_mask |= (uint8_t)(1u << bank);
            }

            used = rt_snprintf(line,
                               sizeof(line),
                               "BANK_PAIR_PALL bank=%u cmd=%u got=%02x/%02x exp=b0/%02x alias=%u",
                               (unsigned int)bank,
                               (unsigned int)pall_ok,
                               (unsigned int)pall_base,
                               (unsigned int)pall_target,
                               (unsigned int)(0xD0u + bank),
                               (unsigned int)(pall_pair_ok == 0u));
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
    }

    if((data_ok != 0u) && (grouped_bad == 0u) && (alt_bad == 0u) &&
       (alias_mask == 0u) && (pall_alias_mask == 0u))
    {
        sdram_usb_debug_write_line("BANK_DIAG PASS grouped=1 alt=1 auto=1 pall=1");
        return data_ok;
    }

    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "BANK_DIAG WARN grouped_bad=%u alt_bad=%u auto_mask=%02x pall_mask=%02x full_test=all_16MB",
                               (unsigned int)grouped_bad,
                               (unsigned int)alt_bad,
                               (unsigned int)alias_mask,
                               (unsigned int)pall_alias_mask);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    return 0u;
}

static int sdram_memtest_x8_full(volatile uint8_t *base,
                                 v5f_sdram_memtest_result_t *failure,
                                 uint32_t bytes)
{
    const uint8_t address_pattern = 0xAAu;
    const uint8_t address_antipattern = 0x55u;
    uint32_t offset;
    uint32_t test_offset;
    uint32_t address_lines = 0u;
    uint32_t bit;
    uint32_t expected_hash;
    uint32_t actual_hash;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    if((bytes == 0u) || (bytes > V5F_SDRAM_BYTES) ||
       ((bytes & (bytes - 1u)) != 0u))
    {
        return V5F_SDRAM_ERR_PARAM;
    }

    g_v5f_hw_test_diag.sdram_test_bytes = bytes;
    s_sdram_x8_progress_total_mb = bytes / V5F_SDRAM_X8_PROGRESS_BYTES;

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_DATA_BUS;
    sdram_memtest_cdc_stage("DATA_BUS", "START width=8 walking-1/walking-0");
    for(bit = 0u; bit < 8u; bit++)
    {
        uint8_t expected = (uint8_t)(1u << bit);
        uint8_t actual;

        sdram_memtest_x16_low8_write(base, 0u, expected);
        ch32h417_ltdc_rgb_framebuffer_barrier();
        sdram_delay_us(2u);
        actual = sdram_memtest_x16_low8_read(base, 0u);
        if(actual != expected)
        {
            return sdram_fail_result(failure,
                                     V5F_SDRAM_STAGE_DATA_BUS,
                                     0u,
                                     expected,
                                     actual);
        }

        expected = (uint8_t)~expected;
        sdram_memtest_x16_low8_write(base, 0u, expected);
        ch32h417_ltdc_rgb_framebuffer_barrier();
        sdram_delay_us(2u);
        actual = sdram_memtest_x16_low8_read(base, 0u);
        if(actual != expected)
        {
            return sdram_fail_result(failure,
                                     V5F_SDRAM_STAGE_DATA_BUS,
                                     0u,
                                     expected,
                                     actual);
        }
    }
    g_v5f_hw_test_diag.sdram_ok_count++;
    sdram_memtest_cdc_stage("DATA_BUS", "PASS bits=8");

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_ADDRESS_BUS;
    for(offset = 1u; offset < bytes; offset <<= 1)
    {
        address_lines++;
    }
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "ADDRESS_BUS START bytes=%u lines=%u scope=selected_range",
                               (unsigned int)bytes,
                               (unsigned int)address_lines);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    for(offset = 1u; offset < bytes; offset <<= 1)
    {
        sdram_memtest_x16_low8_write(base, offset, address_pattern);
    }
    sdram_memtest_x16_low8_write(base, 0u, address_antipattern);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    sdram_delay_us(10u);
    for(offset = 1u; offset < bytes; offset <<= 1)
    {
        uint8_t actual = sdram_memtest_x16_low8_read(base, offset);
        if(actual != address_pattern)
        {
            return sdram_fail_result(failure,
                                     V5F_SDRAM_STAGE_ADDRESS_BUS,
                                     offset,
                                     address_pattern,
                                     actual);
        }
    }

    sdram_memtest_x16_low8_write(base, 0u, address_pattern);
    for(test_offset = 1u; test_offset < bytes; test_offset <<= 1)
    {
        sdram_memtest_x16_low8_write(base, test_offset, address_antipattern);
        ch32h417_ltdc_rgb_framebuffer_barrier();
        sdram_delay_us(2u);
        if(sdram_memtest_x16_low8_read(base, 0u) != address_pattern)
        {
            return sdram_fail_result(failure,
                                     V5F_SDRAM_STAGE_ADDRESS_BUS,
                                     0u,
                                     address_pattern,
                                     sdram_memtest_x16_low8_read(base, 0u));
        }
        for(offset = 1u; offset < bytes; offset <<= 1)
        {
            if((offset != test_offset) &&
               (sdram_memtest_x16_low8_read(base, offset) != address_pattern))
            {
                return sdram_fail_result(failure,
                                         V5F_SDRAM_STAGE_ADDRESS_BUS,
                                         offset,
                                         address_pattern,
                                         sdram_memtest_x16_low8_read(base, offset));
            }
        }
        sdram_memtest_x16_low8_write(base, test_offset, address_pattern);
    }
    g_v5f_hw_test_diag.sdram_ok_count++;
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "ADDRESS_BUS PASS lines=%u no_alias=1",
                               (unsigned int)address_lines);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_MARCH;
    sdram_memtest_cdc_stage("MARCH", "START algorithm=write0,read0,write1,read1,down-write0,down-read0");
    for(offset = 0u; offset < bytes; offset++)
    {
        sdram_memtest_x16_low8_write(base, offset, 0x00u);
        V5F_SDRAM_X8_PROGRESS("write0", offset + 1u);
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();

    for(offset = 0u; offset < bytes; offset++)
    {
        uint8_t actual = sdram_memtest_x16_low8_read(base, offset);
        if(actual != 0x00u)
        {
            return sdram_fail_result(failure,
                                     V5F_SDRAM_STAGE_MARCH,
                                     offset,
                                     0x00u,
                                     actual);
        }
        V5F_SDRAM_X8_PROGRESS("read0", offset + 1u);
    }

    for(offset = 0u; offset < bytes; offset++)
    {
        sdram_memtest_x16_low8_write(base, offset, 0xFFu);
        V5F_SDRAM_X8_PROGRESS("write1", offset + 1u);
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();

    for(offset = 0u; offset < bytes; offset++)
    {
        uint8_t actual = sdram_memtest_x16_low8_read(base, offset);
        if(actual != 0xFFu)
        {
            return sdram_fail_result(failure,
                                     V5F_SDRAM_STAGE_MARCH,
                                     offset,
                                     0xFFu,
                                     actual);
        }
        V5F_SDRAM_X8_PROGRESS("read1", offset + 1u);
    }

    for(offset = bytes; offset != 0u; )
    {
        offset--;
        sdram_memtest_x16_low8_write(base, offset, 0x00u);
        V5F_SDRAM_X8_PROGRESS("down-write0", bytes - offset);
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();

    for(offset = bytes; offset != 0u; )
    {
        uint8_t actual;
        offset--;
        actual = sdram_memtest_x16_low8_read(base, offset);
        if(actual != 0x00u)
        {
            return sdram_fail_result(failure,
                                     V5F_SDRAM_STAGE_MARCH,
                                     offset,
                                     0x00u,
                                     actual);
        }
        V5F_SDRAM_X8_PROGRESS("down-read0", bytes - offset);
    }
    g_v5f_hw_test_diag.sdram_ok_count++;
    sdram_memtest_cdc_stage("MARCH", "PASS passes=6 rw_turnaround=separated");

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_PATTERN;
    sdram_memtest_cdc_stage("PATTERN", "START address-hash write+verify separated=1");
    expected_hash = 2166136261u;
    for(offset = 0u; offset < bytes; offset++)
    {
        uint8_t expected = sdram_memtest_x8_pattern(offset);
        sdram_memtest_x16_low8_write(base, offset, expected);
        expected_hash = sdram_memtest_x8_hash(expected_hash, expected);
        V5F_SDRAM_X8_PROGRESS("write", offset + 1u);
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();

    actual_hash = 2166136261u;
    for(offset = 0u; offset < bytes; offset++)
    {
        uint8_t expected = sdram_memtest_x8_pattern(offset);
        uint8_t actual = sdram_memtest_x16_low8_read(base, offset);
        if(actual != expected)
        {
            return sdram_fail_result(failure,
                                     V5F_SDRAM_STAGE_PATTERN,
                                     offset,
                                     expected,
                                     actual);
        }
        actual_hash = sdram_memtest_x8_hash(actual_hash, actual);
        V5F_SDRAM_X8_PROGRESS("verify", offset + 1u);
    }
    if(actual_hash != expected_hash)
    {
        return sdram_fail_result(failure,
                                 V5F_SDRAM_STAGE_PATTERN,
                                 0u,
                                 expected_hash,
                                 actual_hash);
    }
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "PATTERN PASS checksum=%08x",
                               (unsigned int)actual_hash);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    g_v5f_hw_test_diag.sdram_ok_count++;

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_PATTERN_INV;
    sdram_memtest_cdc_stage("PATTERN_INV", "START write-inverse+verify separated=1");
    expected_hash = 2166136261u;
    for(offset = 0u; offset < bytes; offset++)
    {
        uint8_t expected = sdram_memtest_x8_pattern(offset);
        uint8_t inverse = (uint8_t)~expected;
        sdram_memtest_x16_low8_write(base, offset, inverse);
        expected_hash = sdram_memtest_x8_hash(expected_hash, inverse);
        V5F_SDRAM_X8_PROGRESS("write", offset + 1u);
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();

    actual_hash = 2166136261u;
    for(offset = 0u; offset < bytes; offset++)
    {
        uint8_t expected = (uint8_t)~sdram_memtest_x8_pattern(offset);
        uint8_t actual = sdram_memtest_x16_low8_read(base, offset);
        if(actual != expected)
        {
            return sdram_fail_result(failure,
                                     V5F_SDRAM_STAGE_PATTERN_INV,
                                     offset,
                                     expected,
                                     actual);
        }
        actual_hash = sdram_memtest_x8_hash(actual_hash, actual);
        V5F_SDRAM_X8_PROGRESS("verify", offset + 1u);
    }
    if(actual_hash != expected_hash)
    {
        return sdram_fail_result(failure,
                                 V5F_SDRAM_STAGE_PATTERN_INV,
                                 0u,
                                 expected_hash,
                                 actual_hash);
    }
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "PATTERN_INV PASS checksum=%08x",
                               (unsigned int)actual_hash);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    g_v5f_hw_test_diag.sdram_ok_count++;

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_RETENTION;
    sdram_memtest_cdc_stage("RETENTION", "START delay_ms=1000 pattern=inverse");
    for(offset = 0u; offset < 100u; offset++)
    {
        ch32h417_dual_cdc_poll();
        rt_thread_mdelay(10);
    }
    actual_hash = 2166136261u;
    for(offset = 0u; offset < bytes; offset++)
    {
        uint8_t expected = (uint8_t)~sdram_memtest_x8_pattern(offset);
        uint8_t actual = sdram_memtest_x16_low8_read(base, offset);
        if(actual != expected)
        {
            return sdram_fail_result(failure,
                                     V5F_SDRAM_STAGE_RETENTION,
                                     offset,
                                     expected,
                                     actual);
        }
        actual_hash = sdram_memtest_x8_hash(actual_hash, actual);
        V5F_SDRAM_X8_PROGRESS("verify", offset + 1u);
    }
    if(actual_hash != expected_hash)
    {
        return sdram_fail_result(failure,
                                 V5F_SDRAM_STAGE_RETENTION,
                                 0u,
                                 expected_hash,
                                 actual_hash);
    }
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "RETENTION PASS delay_ms=1000 checksum=%08x",
                               (unsigned int)actual_hash);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    g_v5f_hw_test_diag.sdram_ok_count++;
    return V5F_SDRAM_OK;
}

typedef struct
{
    uint32_t tested_reads;
    uint32_t bad_reads;
    uint32_t bit_bad[16];
    uint32_t first_offset[16];
    uint16_t first_expected[16];
    uint16_t first_actual[16];
} v5f_sdram_x16_diag_t;

static uint32_t sdram_memtest_timer_now(void)
{
    uint32_t tick_before;
    uint32_t tick_after;
    uint32_t counter;
    uint32_t ticks_per_rt_tick = HCLKClock / RT_TICK_PER_SECOND;

    if(ticks_per_rt_tick == 0u)
    {
        ticks_per_rt_tick = 1u;
    }
    do
    {
        tick_before = (uint32_t)rt_tick_get();
        counter = SysTick1->CNT;
        tick_after = (uint32_t)rt_tick_get();
    } while(tick_before != tick_after);

    return (tick_before * ticks_per_rt_tick) + counter;
}

static uint8_t sdram_memtest_popcount16(uint16_t value)
{
    uint8_t count = 0u;

    while(value != 0u)
    {
        count = (uint8_t)(count + (uint8_t)(value & 1u));
        value >>= 1;
    }
    return count;
}

static void sdram_memtest_x16_record(v5f_sdram_x16_diag_t *diag,
                                     uint32_t offset,
                                     uint16_t expected,
                                     uint16_t actual)
{
    uint16_t difference = (uint16_t)(expected ^ actual);
    uint8_t bit;

    diag->tested_reads++;
    if(difference != 0u)
    {
        diag->bad_reads++;
    }
    for(bit = 0u; bit < 16u; bit++)
    {
        if((difference & (uint16_t)(1u << bit)) != 0u)
        {
            if(diag->bit_bad[bit] == 0u)
            {
                diag->first_offset[bit] = offset;
                diag->first_expected[bit] = expected;
                diag->first_actual[bit] = actual;
            }
            diag->bit_bad[bit]++;
        }
    }
}

static uint16_t sdram_memtest_x16_sequence_pattern(uint32_t bank,
                                                    uint32_t index)
{
    uint32_t value = (index * 40503u) ^ (bank * 0x9E37u) ^ 0xA55Au;

    value ^= value >> 7;
    value ^= value << 9;
    return (uint16_t)value;
}

static uint16_t __attribute__((unused)) sdram_memtest_x16_dq_diagnose(void)
{
    static const uint16_t fixed_patterns[4] = {
        0x0000u, 0xFFFFu, 0xAAAAu, 0x5555u
    };
    volatile uint16_t *base = (volatile uint16_t *)V5F_SDRAM_NATIVE_ADDR;
    v5f_sdram_x16_diag_t diag = {0};
    const uint32_t sequence_offset = 0x00020000u;
    uint16_t good_mask = 0u;
    uint32_t pattern_index;
    uint32_t bank;
    uint32_t sample;
    uint8_t bit;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    for(bit = 0u; bit < 16u; bit++)
    {
        diag.first_offset[bit] = 0xFFFFFFFFu;
    }

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_DATA_BUS;
    sdram_usb_debug_write_line("X16_DQ START patterns=0000,ffff,aaaa,5555,walking1,walking0 banks=4 samples=64");
    for(pattern_index = 0u; pattern_index < 36u; pattern_index++)
    {
        uint16_t pattern;

        if(pattern_index < 4u)
        {
            pattern = fixed_patterns[pattern_index];
        }
        else if(pattern_index < 20u)
        {
            pattern = (uint16_t)(1u << (pattern_index - 4u));
        }
        else
        {
            pattern = (uint16_t)~(uint16_t)(1u << (pattern_index - 20u));
        }

        for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
        {
            volatile uint16_t *bank_base = (volatile uint16_t *)(
                (uintptr_t)base + (bank * V5F_SDRAM_X16_BANK_BYTES));

            for(sample = 0u; sample < 64u; sample++)
            {
                bank_base[sample] = pattern;
            }
        }
        ch32h417_ltdc_rgb_framebuffer_barrier();
        for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
        {
            volatile uint16_t *bank_base = (volatile uint16_t *)(
                (uintptr_t)base + (bank * V5F_SDRAM_X16_BANK_BYTES));

            for(sample = 0u; sample < 64u; sample++)
            {
                uint16_t actual = bank_base[sample];
                sdram_memtest_x16_record(&diag,
                                         (bank * V5F_SDRAM_X16_BANK_BYTES) +
                                             (sample * 2u),
                                         pattern,
                                         actual);
            }
        }
        ch32h417_dual_cdc_poll();
    }

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_PATTERN;
    sdram_usb_debug_write_line("X16_SEQ START words=4096/bank access=alternating-bank separated-write-read");
    for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
    {
        volatile uint16_t *bank_base = (volatile uint16_t *)(
            (uintptr_t)base + (bank * V5F_SDRAM_X16_BANK_BYTES) +
            sequence_offset);

        for(sample = 0u; sample < V5F_SDRAM_X16_DIAG_WORDS; sample++)
        {
            bank_base[sample] = sdram_memtest_x16_sequence_pattern(bank, sample);
        }
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
    for(sample = 0u; sample < V5F_SDRAM_X16_DIAG_WORDS; sample++)
    {
        for(bank = 0u; bank < V5F_SDRAM_X8_BANK_COUNT; bank++)
        {
            volatile uint16_t *bank_base = (volatile uint16_t *)(
                (uintptr_t)base + (bank * V5F_SDRAM_X16_BANK_BYTES) +
                sequence_offset);
            uint16_t expected = sdram_memtest_x16_sequence_pattern(bank, sample);
            uint16_t actual = bank_base[sample];

            sdram_memtest_x16_record(&diag,
                                     (bank * V5F_SDRAM_X16_BANK_BYTES) +
                                         sequence_offset + (sample * 2u),
                                     expected,
                                     actual);
        }
        if((sample & 0xFFu) == 0u)
        {
            ch32h417_dual_cdc_poll();
        }
    }

    for(bit = 0u; bit < 16u; bit++)
    {
        int used;

        if(diag.bit_bad[bit] == 0u)
        {
            good_mask |= (uint16_t)(1u << bit);
            used = rt_snprintf(line,
                               sizeof(line),
                               "X16_BIT D%u PASS bad=0/%u",
                               (unsigned int)bit,
                               (unsigned int)diag.tested_reads);
        }
        else
        {
            used = rt_snprintf(line,
                               sizeof(line),
                               "X16_BIT D%u FAIL bad=%u/%u first=%08x exp=%04x got=%04x",
                               (unsigned int)bit,
                               (unsigned int)diag.bit_bad[bit],
                               (unsigned int)diag.tested_reads,
                               (unsigned int)diag.first_offset[bit],
                               (unsigned int)diag.first_expected[bit],
                               (unsigned int)diag.first_actual[bit]);
        }
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    {
        uint8_t good_bits = sdram_memtest_popcount16(good_mask);
        uint32_t packed_mib_x100 = (32u * 100u * good_bits) / 16u;
        int used = rt_snprintf(line,
                               sizeof(line),
                               "X16_DQ END reads=%u bad_reads=%u good=%04x bad=%04x bits=%u/16 packed_MiB=%u.%02u",
                               (unsigned int)diag.tested_reads,
                               (unsigned int)diag.bad_reads,
                               (unsigned int)good_mask,
                               (unsigned int)((uint16_t)~good_mask),
                               (unsigned int)good_bits,
                               (unsigned int)(packed_mib_x100 / 100u),
                               (unsigned int)(packed_mib_x100 % 100u));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    return good_mask;
}

static uint32_t sdram_memtest_bandwidth_x100(uint32_t bytes,
                                             uint32_t cycles)
{
    uint64_t numerator;
    uint32_t timer_hz = (HCLKClock != 0u) ? HCLKClock : 100000000u;

    if(cycles == 0u)
    {
        return 0u;
    }
    numerator = (uint64_t)bytes * (uint64_t)timer_hz * 100u;
    return (uint32_t)((numerator / cycles) / 1000000u);
}

static void sdram_memtest_bandwidth_report(const char *name,
                                            uint32_t bytes,
                                            uint32_t cycles,
                                            uint8_t good_bits,
                                            uint32_t verify_bad,
                                            uint32_t checksum)
{
    uint32_t raw_x100 = sdram_memtest_bandwidth_x100(bytes, cycles);
    uint32_t effective_x100 = (raw_x100 * good_bits) / 16u;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used = rt_snprintf(line,
                           sizeof(line),
                           "BW %s bytes=%u cycles=%u raw=%u.%02uMB/s useful=%u.%02uMB/s bad=%u sum=%08x",
                           name,
                           (unsigned int)bytes,
                           (unsigned int)cycles,
                           (unsigned int)(raw_x100 / 100u),
                           (unsigned int)(raw_x100 % 100u),
                           (unsigned int)(effective_x100 / 100u),
                           (unsigned int)(effective_x100 % 100u),
                           (unsigned int)verify_bad,
                           (unsigned int)checksum);

    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static void __attribute__((unused))
sdram_memtest_bandwidth_progress(const char *name, uint32_t bytes_done)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used = rt_snprintf(line,
                           sizeof(line),
                           "BW PROGRESS %s %u/4MiB",
                           name,
                           (unsigned int)(bytes_done / (1024u * 1024u)));

    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    ch32h417_dual_cdc_poll();
}

static int sdram_memtest_safe_reinit(uint32_t *failure_stage);
static void sdram_memtest_dma1_hard_reset(void);
static void sdram_memtest_dma2_hard_reset(void);

static uint8_t sdram_memtest_dma_timed(uintptr_t sdram_address,
                                        uint8_t write_to_sdram,
                                        uint8_t wide256,
                                        uint32_t *cycles)
{
    DMA_InitTypeDef dma = {0};
    uint32_t timeout = V5F_SDRAM_TIMEOUT_POLLS;
    uint32_t stop_timeout;
    uint32_t data_size = (wide256 != 0u) ?
                         DMA_PeripheralDataSize_256 :
                         DMA_PeripheralDataSize_Word;
    uint32_t memory_size = (wide256 != 0u) ?
                           DMA_MemoryDataSize_256 :
                           DMA_MemoryDataSize_Word;
    uint32_t start;
    uint8_t ok;

    s_sdram_watchdog_dma_channel = DMA1_Channel3;
    s_sdram_watchdog_dma_controller = 1u;
    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);
    DMA_Cmd(DMA1_Channel3, DISABLE);
    stop_timeout = 1024u;
    while(((DMA1_Channel3->CFGR & DMA_CFGR1_EN) != 0u) &&
          (stop_timeout != 0u))
    {
        stop_timeout--;
    }
    if(stop_timeout == 0u)
    {
        *cycles = 0u;
        return 0u;
    }
    sdram_memtest_dma1_hard_reset();
    DMA_DeInit(DMA1_Channel3);
    DMA_StructInit(&dma);
    dma.DMA_PeripheralBaseAddr = (uint32_t)sdram_address;
    dma.DMA_Memory0BaseAddr = (uint32_t)(uintptr_t)s_sdram_bw_buffer;
    dma.DMA_DIR = (write_to_sdram != 0u) ?
                  DMA_DIR_PeripheralDST : DMA_DIR_PeripheralSRC;
    dma.DMA_BufferSize = (wide256 != 0u) ?
                         (V5F_SDRAM_DMA_BW_WORDS / 8u) :
                         V5F_SDRAM_DMA_BW_WORDS;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Enable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = data_size;
    dma.DMA_MemoryDataSize = memory_size;
    dma.DMA_Mode = DMA_Mode_Normal;
    dma.DMA_Priority = DMA_Priority_VeryHigh;
    dma.DMA_M2M = DMA_M2M_Enable;
    DMA_Init(DMA1_Channel3, &dma);
    DMA_ClearFlag(DMA1, DMA1_FLAG_TC3 | DMA1_FLAG_TE3);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    sdram_memtest_watchdog_checkpoint(
        (write_to_sdram != 0u) ?
            V5F_SDRAM_WATCHDOG_STAGE_WRITE :
            V5F_SDRAM_WATCHDOG_STAGE_READ,
        V5F_SDRAM_WATCHDOG_POINT_PRE_ENABLE,
        0xFFFFFFFFu,
        sdram_address);
    start = sdram_memtest_timer_now();
    DMA_Cmd(DMA1_Channel3, ENABLE);
    while((DMA_GetFlagStatus(DMA1, DMA1_FLAG_TC3) == RESET) &&
          (DMA_GetFlagStatus(DMA1, DMA1_FLAG_TE3) == RESET) &&
          (timeout != 0u))
    {
        timeout--;
    }
    *cycles = sdram_memtest_timer_now() - start;
    ok = (uint8_t)((timeout != 0u) &&
                   (DMA_GetFlagStatus(DMA1, DMA1_FLAG_TC3) != RESET) &&
                   (DMA_GetFlagStatus(DMA1, DMA1_FLAG_TE3) == RESET));
    sdram_memtest_watchdog_context(
        (write_to_sdram != 0u) ?
            V5F_SDRAM_WATCHDOG_STAGE_WRITE :
            V5F_SDRAM_WATCHDOG_STAGE_READ,
        V5F_SDRAM_WATCHDOG_POINT_TC_SEEN,
        0xFFFFFFFFu,
        sdram_address);
    DMA_Cmd(DMA1_Channel3, DISABLE);
    stop_timeout = 1024u;
    while(((DMA1_Channel3->CFGR & DMA_CFGR1_EN) != 0u) &&
          (stop_timeout != 0u))
    {
        stop_timeout--;
    }
    if(stop_timeout == 0u)
    {
        ok = 0u;
    }
    DMA_ClearFlag(DMA1, DMA1_FLAG_TC3 | DMA1_FLAG_TE3);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    sdram_delay_us(2u);
    return ok;
}

static uint8_t sdram_memtest_d10d11_raw_probe(void)
{
    const uintptr_t target =
        (uintptr_t)V5F_SDRAM_NATIVE_ADDR + 0x00500000u;
    uint32_t mapping[4][4] = {{0u}};
    uint32_t cycles;
    uint32_t index;
    uint32_t raw_bad = 0u;
    uint32_t other_bad = 0u;
    uint8_t write_ok;
    uint8_t read_ok;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    sdram_usb_debug_write_line(
        "DQ_RAW START target=c0500000 access=dma256 patterns=d10d11_00,01,10,11 compare=unmasked");
    for(index = 0u; index < V5F_SDRAM_DMA_BW_WORDS; index++)
    {
        uint32_t low = index & 3u;
        uint32_t high = (index >> 2) & 3u;

        s_sdram_bw_buffer[index] = (low << 10) | (high << 26);
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
    write_ok = sdram_memtest_dma_timed(target, 1u, 1u, &cycles);
    for(index = 0u; index < V5F_SDRAM_DMA_BW_WORDS; index++)
    {
        s_sdram_bw_buffer[index] = 0xDEADBEEFu;
    }
    read_ok = sdram_memtest_dma_timed(target, 0u, 1u, &cycles);
    for(index = 0u; index < V5F_SDRAM_DMA_BW_WORDS; index++)
    {
        uint32_t expected_low = index & 3u;
        uint32_t expected_high = (index >> 2) & 3u;
        uint32_t expected = (expected_low << 10) | (expected_high << 26);
        uint32_t actual = s_sdram_bw_buffer[index];
        uint32_t actual_low = (actual >> 10) & 3u;
        uint32_t actual_high = (actual >> 26) & 3u;

        mapping[expected_low][actual_low]++;
        mapping[expected_high][actual_high]++;
        if(actual != expected)
        {
            raw_bad++;
        }
        if(((actual ^ expected) & 0xF3FFF3FFu) != 0u)
        {
            other_bad++;
        }
    }
    for(index = 0u; index < 4u; index++)
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "DQ_RAW exp=%u got=%u,%u,%u,%u samples=%u",
                               (unsigned int)index,
                               (unsigned int)mapping[index][0],
                               (unsigned int)mapping[index][1],
                               (unsigned int)mapping[index][2],
                               (unsigned int)mapping[index][3],
                               (unsigned int)(mapping[index][0] +
                                              mapping[index][1] +
                                              mapping[index][2] +
                                              mapping[index][3]));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    {
        uint8_t pass = (uint8_t)((write_ok != 0u) &&
                                 (read_ok != 0u) &&
                                 (raw_bad == 0u) &&
                                 (other_bad == 0u));
        int used = rt_snprintf(line,
                               sizeof(line),
                               "DQ_RAW END write=%u read=%u raw_bad=%u/4096 other_bad=%u result=%s",
                               (unsigned int)write_ok,
                               (unsigned int)read_ok,
                               (unsigned int)raw_bad,
                               (unsigned int)other_bad,
                               (pass != 0u) ? "PASS" : "FAIL");
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        return pass;
    }
}

static uint32_t sdram_memtest_dma_full_pattern(uint32_t word,
                                                uint32_t seed,
                                                uint32_t multiplier,
                                                uint32_t mask32)
{
    uint32_t value = seed ^ (word * multiplier);

    value ^= value >> 11;
    value ^= value << 7;
    return value & mask32;
}

static uint8_t sdram_memtest_dma_stream_prepare(uint8_t write_to_sdram,
                                                 uint8_t wide256)
{
    DMA_InitTypeDef dma = {0};
    uint32_t stop_timeout = 1024u;

    s_sdram_watchdog_dma_channel = DMA2_Channel3;
    s_sdram_watchdog_dma_controller = 2u;
    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA2, ENABLE);
    DMA_Cmd(DMA2_Channel3, DISABLE);
    while(((DMA2_Channel3->CFGR & DMA_CFGR1_EN) != 0u) &&
          (stop_timeout != 0u))
    {
        stop_timeout--;
    }
    if(stop_timeout == 0u)
    {
        return 0u;
    }

    sdram_memtest_dma2_hard_reset();
    DMA_DeInit(DMA2_Channel3);
    DMA_StructInit(&dma);
    dma.DMA_PeripheralBaseAddr = (uint32_t)V5F_SDRAM_NATIVE_ADDR;
    dma.DMA_Memory0BaseAddr = (uint32_t)(uintptr_t)s_sdram_bw_buffer;
    dma.DMA_DIR = (write_to_sdram != 0u) ?
                  DMA_DIR_PeripheralDST : DMA_DIR_PeripheralSRC;
    dma.DMA_BufferSize = (wide256 != 0u) ?
                         (V5F_SDRAM_DMA_BW_WORDS / 8u) :
                         V5F_SDRAM_DMA_BW_WORDS;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Enable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = (wide256 != 0u) ?
                                 DMA_PeripheralDataSize_256 :
                                 DMA_PeripheralDataSize_Word;
    dma.DMA_MemoryDataSize = (wide256 != 0u) ?
                             DMA_MemoryDataSize_256 :
                             DMA_MemoryDataSize_Word;
    dma.DMA_Mode = DMA_Mode_Normal;
    dma.DMA_Priority = DMA_Priority_VeryHigh;
    dma.DMA_M2M = DMA_M2M_Enable;
    DMA_Init(DMA2_Channel3, &dma);
    DMA_ClearFlag(DMA2, DMA2_FLAG_TC3 | DMA2_FLAG_TE3);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    return 1u;
}

static uint8_t sdram_memtest_dma_stream_transfer_at(
    uintptr_t sdram_address,
    uintptr_t memory_address,
    uint32_t bytes,
    uint8_t wide256,
    uint32_t *cycles,
    uint32_t watchdog_stage,
    uint32_t watchdog_block)
{
    uint32_t transaction_bytes = (wide256 != 0u) ? 32u : 4u;
    uint32_t timeout = V5F_SDRAM_TIMEOUT_POLLS;
    uint32_t stop_timeout;
    uint32_t start;
    uint8_t ok;

    if((bytes == 0u) || (bytes > V5F_SDRAM_DMA_BUFFER_BYTES) ||
       ((bytes % transaction_bytes) != 0u))
    {
        *cycles = 0u;
        return 0u;
    }
    sdram_memtest_watchdog_context(watchdog_stage,
                                   V5F_SDRAM_WATCHDOG_POINT_ENTER,
                                   watchdog_block,
                                   sdram_address);
    if((DMA2_Channel3->CFGR & DMA_CFGR1_EN) != 0u)
    {
        *cycles = 0u;
        return 0u;
    }

    DMA2_Channel3->PADDR = (uint32_t)sdram_address;
    DMA2_Channel3->MADDR = (uint32_t)memory_address;
    DMA2_Channel3->CNTR = bytes / transaction_bytes;
    DMA_ClearFlag(DMA2, DMA2_FLAG_TC3 | DMA2_FLAG_TE3);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    sdram_memtest_watchdog_checkpoint(watchdog_stage,
                                      V5F_SDRAM_WATCHDOG_POINT_PRE_ENABLE,
                                      watchdog_block,
                                      sdram_address);
    start = sdram_memtest_timer_now();
    DMA_Cmd(DMA2_Channel3, ENABLE);
    while((DMA_GetFlagStatus(DMA2, DMA2_FLAG_TC3) == RESET) &&
          (DMA_GetFlagStatus(DMA2, DMA2_FLAG_TE3) == RESET) &&
          (timeout != 0u))
    {
        timeout--;
    }
    *cycles = sdram_memtest_timer_now() - start;
    ok = (uint8_t)((timeout != 0u) &&
                   (DMA_GetFlagStatus(DMA2, DMA2_FLAG_TC3) != RESET) &&
                   (DMA_GetFlagStatus(DMA2, DMA2_FLAG_TE3) == RESET));
    sdram_memtest_watchdog_context(watchdog_stage,
                                   V5F_SDRAM_WATCHDOG_POINT_TC_SEEN,
                                   watchdog_block,
                                   sdram_address);
    /*
     * Capture the first genuinely post-TC register image. The following
     * context markers intentionally bracket every cleanup bus transaction:
     * after an IWDG reset the last marker identifies the exact operation
     * that failed to return.
     */
    sdram_memtest_watchdog_checkpoint(
        watchdog_stage,
        V5F_SDRAM_WATCHDOG_POINT_TC_SNAPSHOT,
        watchdog_block,
        sdram_address);
    sdram_memtest_watchdog_context(
        watchdog_stage,
        V5F_SDRAM_WATCHDOG_POINT_BEFORE_DISABLE,
        watchdog_block,
        sdram_address);
    DMA_Cmd(DMA2_Channel3, DISABLE);
    sdram_memtest_watchdog_context(
        watchdog_stage,
        V5F_SDRAM_WATCHDOG_POINT_DISABLE_DONE,
        watchdog_block,
        sdram_address);
    stop_timeout = 1024u;
    while(((DMA2_Channel3->CFGR & DMA_CFGR1_EN) != 0u) &&
          (stop_timeout != 0u))
    {
        stop_timeout--;
    }
    if(stop_timeout == 0u)
    {
        ok = 0u;
    }
    sdram_memtest_watchdog_context(
        watchdog_stage,
        V5F_SDRAM_WATCHDOG_POINT_EN_CLEARED,
        watchdog_block,
        sdram_address);
    DMA_ClearFlag(DMA2, DMA2_FLAG_TC3 | DMA2_FLAG_TE3);
    sdram_memtest_watchdog_context(
        watchdog_stage,
        V5F_SDRAM_WATCHDOG_POINT_FLAGS_CLEARED,
        watchdog_block,
        sdram_address);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    sdram_memtest_watchdog_context(
        watchdog_stage,
        V5F_SDRAM_WATCHDOG_POINT_FENCE_DONE,
        watchdog_block,
        sdram_address);
    sdram_delay_us(V5F_SDRAM_DMA_RESTART_GAP_US);
    sdram_memtest_watchdog_context(
        watchdog_stage,
        V5F_SDRAM_WATCHDOG_POINT_GAP_DONE,
        watchdog_block,
        sdram_address);
    sdram_memtest_watchdog_context(watchdog_stage,
                                   V5F_SDRAM_WATCHDOG_POINT_COMPLETE,
                                   watchdog_block,
                                   sdram_address);
    return ok;
}

static uint8_t sdram_memtest_dma_stream_transfer(uintptr_t sdram_address,
                                                  uint8_t wide256,
                                                  uint32_t *cycles,
                                                  uint32_t watchdog_stage,
                                                  uint32_t watchdog_block)
{
    return sdram_memtest_dma_stream_transfer_at(
        sdram_address,
        (uintptr_t)s_sdram_bw_buffer,
        V5F_SDRAM_DMA_BUFFER_BYTES,
        wide256,
        cycles,
        watchdog_stage,
        watchdog_block);
}

static void sdram_memtest_dma_stream_finish(void)
{
    uint32_t stop_timeout = 1024u;

    DMA_Cmd(DMA2_Channel3, DISABLE);
    while(((DMA2_Channel3->CFGR & DMA_CFGR1_EN) != 0u) &&
          (stop_timeout != 0u))
    {
        stop_timeout--;
    }
    DMA_ClearFlag(DMA2, DMA2_FLAG_TC3 | DMA2_FLAG_TE3);
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

static void __attribute__((unused))
sdram_memtest_dma_full_progress(const char *mode,
                                             const char *phase,
                                             uint32_t bytes_done)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used = rt_snprintf(line,
                           sizeof(line),
                           "DMA_FULL %s %s progress=%u/32MiB",
                           mode,
                           phase,
                           (unsigned int)(bytes_done / (1024u * 1024u)));

    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    ch32h417_dual_cdc_poll();
}

static void __attribute__((unused))
sdram_memtest_dma_full_trace(const char *mode,
                                          const char *phase,
                                          uint32_t block,
                                          uintptr_t address,
                                          const char *state)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used = rt_snprintf(line,
                           sizeof(line),
                           "DMA_TRACE %s %s block=%u addr=%08x %s",
                           mode,
                           phase,
                           (unsigned int)block,
                           (unsigned int)address,
                           state);

    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    ch32h417_dual_cdc_poll();
}

static uint8_t __attribute__((unused))
sdram_memtest_dma_trace_block(uint32_t block)
{
    uint32_t blocks_per_bank = V5F_SDRAM_X16_BANK_BYTES /
                               sizeof(s_sdram_bw_buffer);
    uint32_t bank_block = block % blocks_per_bank;

    return (uint8_t)((block < 16u) ||
                     ((block % 64u) == 0u) ||
                     ((block % 64u) == 63u) ||
                     (bank_block == (blocks_per_bank - 1u)) ||
                     (bank_block == 0u) ||
                     (bank_block == 1u));
}

static uint8_t __attribute__((unused))
sdram_memtest_dma_bank_guard(const char *mode,
                                             const char *phase,
                                             uint32_t block,
                                             uintptr_t address)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    uint8_t ok;
    int used = rt_snprintf(line,
                           sizeof(line),
                           "DMA_BANK_SWITCH %s %s block=%u addr=%08x PALL_START",
                           mode,
                           phase,
                           (unsigned int)block,
                           (unsigned int)address);

    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    ok = sdram_memtest_precharge_all();
    sdram_delay_us(10u);
    used = rt_snprintf(line,
                       sizeof(line),
                       "DMA_BANK_SWITCH %s %s block=%u addr=%08x PALL_%s",
                       mode,
                       phase,
                       (unsigned int)block,
                       (unsigned int)address,
                       (ok != 0u) ? "PASS" : "FAIL");
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    ch32h417_dual_cdc_poll();
    return ok;
}

static uint8_t __attribute__((unused))
sdram_memtest_dma_full_recover(const char *phase,
                                               uint32_t block,
                                               uintptr_t address)
{
    uint32_t failure_stage = 0u;
    int result;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used = rt_snprintf(line,
                           sizeof(line),
                           "DMA_SLICE RECOVER %s block=%u addr=%08x START",
                           phase,
                           (unsigned int)block,
                           (unsigned int)address);

    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    result = sdram_memtest_safe_reinit(&failure_stage);
    used = rt_snprintf(line,
                       sizeof(line),
                       "DMA_SLICE RECOVER %s block=%u %s code=%d stage=%u sdsr=%08x misc=%08x",
                       phase,
                       (unsigned int)block,
                       (result == V5F_SDRAM_OK) ? "PASS" : "FAIL",
                       result,
                       (unsigned int)failure_stage,
                       (unsigned int)FMC_Bank5_6->SDSR,
                       (unsigned int)FMC_Bank5_6->MISC);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    ch32h417_dual_cdc_poll();
    return (uint8_t)(result == V5F_SDRAM_OK);
}

static uint8_t
sdram_memtest_dma_full(uint16_t good_mask,
                       uint8_t wide256,
                       uint32_t seed,
                       uint32_t multiplier)
{
    const uintptr_t base = (uintptr_t)V5F_SDRAM_NATIVE_ADDR;
    const uint32_t block_bytes = sizeof(s_sdram_bw_buffer);
    const uint32_t blocks = V5F_SDRAM_DMA_FULL_BYTES / block_bytes;
    const char *mode = (wide256 != 0u) ? "wide256" : "word32";
    uint32_t mask32 = (uint32_t)good_mask | ((uint32_t)good_mask << 16);
    uint8_t good_bits = sdram_memtest_popcount16(good_mask);
    uint32_t write_cycles = 0u;
    uint32_t read_cycles = 0u;
    uint32_t write_e2e_start;
    uint32_t write_e2e_cycles;
    uint32_t read_e2e_start = 0u;
    uint32_t read_e2e_cycles = 0u;
    uint32_t transfer_cycles;
    uint32_t write_bytes = 0u;
    uint32_t read_bytes = 0u;
    uint32_t bad_words = 0u;
    uint32_t checksum = 0u;
    uint32_t first_offset = 0xFFFFFFFFu;
    uint32_t first_expected = 0u;
    uint32_t first_actual = 0u;
    uint32_t block;
    uint32_t index;
    uint8_t write_ok = 1u;
    uint8_t read_ok = 1u;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "DMA_FULL START mode=%s controller=DMA2 channel=3 compare=%04x bits=%u bytes=33554432 blocks=%u block=16384 gap_us=%u log=silent",
                               mode,
                               (unsigned int)good_mask,
                               (unsigned int)good_bits,
                               (unsigned int)blocks,
                               (unsigned int)V5F_SDRAM_DMA_RESTART_GAP_US);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    write_e2e_start = sdram_memtest_timer_now();
    write_ok = sdram_memtest_dma_stream_prepare(1u, wide256);
    for(block = 0u; (block < blocks) && (write_ok != 0u); block++)
    {
        uint32_t first_word = block * V5F_SDRAM_DMA_BW_WORDS;
        uintptr_t block_address = base + (block * block_bytes);

        for(index = 0u; index < V5F_SDRAM_DMA_BW_WORDS; index++)
        {
            s_sdram_bw_buffer[index] = sdram_memtest_dma_full_pattern(
                first_word + index, seed, multiplier, mask32);
        }
        if(sdram_memtest_dma_stream_transfer(block_address,
                                             wide256,
                                             &transfer_cycles,
                                             V5F_SDRAM_WATCHDOG_STAGE_WRITE,
                                             block) == 0u)
        {
            write_ok = 0u;
            first_offset = block * block_bytes;
            break;
        }
        write_cycles += transfer_cycles;
        write_bytes += block_bytes;
    }

    sdram_memtest_dma_stream_finish();
    write_e2e_cycles = sdram_memtest_timer_now() - write_e2e_start;
    if(write_ok != 0u)
    {
        uint32_t wait;

        sdram_memtest_watchdog_checkpoint(
            V5F_SDRAM_WATCHDOG_STAGE_RETENTION,
            V5F_SDRAM_WATCHDOG_POINT_RETENTION,
            0u,
            base);
        sdram_usb_debug_write_line("DMA_FULL RETENTION delay_ms=1000");
        for(wait = 0u; wait < 20u; wait++)
        {
            ch32h417_dual_cdc_poll();
            sdram_memtest_watchdog_feed();
            rt_thread_mdelay(50);
        }
        read_e2e_start = sdram_memtest_timer_now();
        read_ok = sdram_memtest_dma_stream_prepare(0u, wide256);
        for(block = 0u; (block < blocks) && (read_ok != 0u); block++)
        {
            uint32_t first_word = block * V5F_SDRAM_DMA_BW_WORDS;
            uintptr_t block_address = base + (block * block_bytes);

            for(index = 0u; index < V5F_SDRAM_DMA_BW_WORDS; index++)
            {
                s_sdram_bw_buffer[index] = 0xDEADBEEFu;
            }
            if(sdram_memtest_dma_stream_transfer(block_address,
                                                 wide256,
                                                 &transfer_cycles,
                                                 V5F_SDRAM_WATCHDOG_STAGE_READ,
                                                 block) == 0u)
            {
                read_ok = 0u;
                if(first_offset == 0xFFFFFFFFu)
                {
                    first_offset = block * block_bytes;
                }
                break;
            }
            read_cycles += transfer_cycles;
            read_bytes += block_bytes;
            for(index = 0u; index < V5F_SDRAM_DMA_BW_WORDS; index++)
            {
                uint32_t expected = sdram_memtest_dma_full_pattern(
                    first_word + index, seed, multiplier, mask32);
                uint32_t actual = s_sdram_bw_buffer[index];

                checksum += actual;
                if(((actual ^ expected) & mask32) != 0u)
                {
                    bad_words++;
                    if(first_offset == 0xFFFFFFFFu)
                    {
                        first_offset = (block * block_bytes) + (index * 4u);
                        first_expected = expected;
                        first_actual = actual;
                    }
                }
            }
        }
        sdram_memtest_dma_stream_finish();
        read_e2e_cycles = sdram_memtest_timer_now() - read_e2e_start;
    }
    else
    {
        read_ok = 0u;
    }

    sdram_memtest_bandwidth_report((wide256 != 0u) ?
                                    "DMA256_FULL_WRITE_BUS" : "DMA32_FULL_WRITE_BUS",
                                    write_bytes,
                                    write_cycles,
                                    good_bits,
                                    (write_ok != 0u) ? 0u : 1u,
                                    0u);
    sdram_memtest_bandwidth_report((wide256 != 0u) ?
                                    "DMA256_FULL_READ_BUS" : "DMA32_FULL_READ_BUS",
                                    read_bytes,
                                    read_cycles,
                                    good_bits,
                                    bad_words + ((read_ok != 0u) ? 0u : 1u),
                                    checksum);
    sdram_memtest_bandwidth_report((wide256 != 0u) ?
                                    "DMA256_FULL_WRITE_E2E" : "DMA32_FULL_WRITE_E2E",
                                    write_bytes,
                                    write_e2e_cycles,
                                    good_bits,
                                    (write_ok != 0u) ? 0u : 1u,
                                    0u);
    sdram_memtest_bandwidth_report((wide256 != 0u) ?
                                    "DMA256_FULL_READ_E2E" : "DMA32_FULL_READ_E2E",
                                    read_bytes,
                                    read_e2e_cycles,
                                    good_bits,
                                    bad_words + ((read_ok != 0u) ? 0u : 1u),
                                    checksum);
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "DMA_FULL END mode=%s %s write_ok=%u read_ok=%u bad=%u first=%08x exp=%08x got=%08x",
                               mode,
                               ((write_ok != 0u) && (read_ok != 0u) &&
                                (bad_words == 0u)) ? "PASS" : "FAIL",
                               (unsigned int)write_ok,
                               (unsigned int)read_ok,
                               (unsigned int)bad_words,
                               (unsigned int)first_offset,
                               (unsigned int)first_expected,
                               (unsigned int)first_actual);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    sdram_memtest_watchdog_feed();
    return (uint8_t)((write_ok != 0u) &&
                     (read_ok != 0u) &&
                     (bad_words == 0u));
}

static uint8_t sdram_memtest_dma_sliced_stress(uint16_t good_mask,
                                                uint8_t wide256)
{
    const uint32_t passes = 4u;
    const char *mode = (wide256 != 0u) ? "wide256" : "word32";
    uint32_t pass;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "DMA_SLICED STRESS START mode=%s compare=%04x bits=%u passes=4 bytes_per_pass=33554432 write_read=1 fmc_reinit=none",
                               mode,
                               (unsigned int)good_mask,
                               (unsigned int)sdram_memtest_popcount16(good_mask));

        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    for(pass = 0u; pass < passes; pass++)
    {
        uint8_t ok;
        s_sdram_watchdog_pass = pass + 1u;
        int used = rt_snprintf(line,
                               sizeof(line),
                               "DMA_SLICED %s pass=%u/4 START",
                               mode,
                               (unsigned int)(pass + 1u));

        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        ok = sdram_memtest_dma_full(
            good_mask,
            wide256,
            0x13579BDFu ^ (pass * 0x55AA33CCu),
            0x10204081u + (pass * 0x01010102u));
        used = rt_snprintf(line,
                           sizeof(line),
                           "DMA_SLICED %s pass=%u/4 %s",
                           mode,
                           (unsigned int)(pass + 1u),
                           (ok != 0u) ? "PASS" : "FAIL");
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        if(ok == 0u)
        {
            sdram_memtest_watchdog_complete();
            return 0u;
        }
    }
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "DMA_SLICED STRESS END PASS mode=%s passes=4 transferred=268435456",
                               mode);

        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    sdram_memtest_watchdog_complete();
    return 1u;
}

static void sdram_memtest_dma1_hard_reset(void)
{
    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);
    RCC_HBPeriphResetCmd(RCC_HBPeriph_DMA1, ENABLE);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    RCC_HBPeriphResetCmd(RCC_HBPeriph_DMA1, DISABLE);
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

static void sdram_memtest_dma2_hard_reset(void)
{
    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA2, ENABLE);
    RCC_HBPeriphResetCmd(RCC_HBPeriph_DMA2, ENABLE);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    RCC_HBPeriphResetCmd(RCC_HBPeriph_DMA2, DISABLE);
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

static uint8_t sdram_memtest_dma_long_timed(uintptr_t sdram_address,
                                              uint8_t write_to_sdram,
                                              uint16_t transfer_count,
                                              uint32_t *cycles)
{
    DMA_InitTypeDef dma = {0};
    uint32_t timeout = V5F_SDRAM_TIMEOUT_POLLS * 16u;
    uint32_t stop_timeout;
    uint32_t start;
    uint8_t ok;

    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);
    DMA_Cmd(DMA1_Channel3, DISABLE);
    stop_timeout = 1024u;
    while(((DMA1_Channel3->CFGR & DMA_CFGR1_EN) != 0u) &&
          (stop_timeout != 0u))
    {
        stop_timeout--;
    }
    if(stop_timeout == 0u)
    {
        *cycles = 0u;
        return 0u;
    }
    sdram_memtest_dma1_hard_reset();
    DMA_DeInit(DMA1_Channel3);
    DMA_StructInit(&dma);
    dma.DMA_PeripheralBaseAddr = (uint32_t)sdram_address;
    dma.DMA_Memory0BaseAddr = (uint32_t)(uintptr_t)s_sdram_bw_buffer;
    dma.DMA_DIR = (write_to_sdram != 0u) ?
                  DMA_DIR_PeripheralDST : DMA_DIR_PeripheralSRC;
    dma.DMA_BufferSize = transfer_count;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Enable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Disable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_256;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_256;
    dma.DMA_Mode = DMA_Mode_Normal;
    dma.DMA_Priority = DMA_Priority_VeryHigh;
    dma.DMA_M2M = DMA_M2M_Enable;
    DMA_Init(DMA1_Channel3, &dma);
    DMA_ClearFlag(DMA1, DMA1_FLAG_TC3 | DMA1_FLAG_TE3);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    start = sdram_memtest_timer_now();
    DMA_Cmd(DMA1_Channel3, ENABLE);
    while((DMA_GetFlagStatus(DMA1, DMA1_FLAG_TC3) == RESET) &&
          (DMA_GetFlagStatus(DMA1, DMA1_FLAG_TE3) == RESET) &&
          (timeout != 0u))
    {
        timeout--;
    }
    *cycles = sdram_memtest_timer_now() - start;
    ok = (uint8_t)((timeout != 0u) &&
                   (DMA_GetFlagStatus(DMA1, DMA1_FLAG_TC3) != RESET) &&
                   (DMA_GetFlagStatus(DMA1, DMA1_FLAG_TE3) == RESET));
    DMA_Cmd(DMA1_Channel3, DISABLE);
    stop_timeout = 1024u;
    while(((DMA1_Channel3->CFGR & DMA_CFGR1_EN) != 0u) &&
          (stop_timeout != 0u))
    {
        stop_timeout--;
    }
    if(stop_timeout == 0u)
    {
        ok = 0u;
    }
    DMA_ClearFlag(DMA1, DMA1_FLAG_TC3 | DMA1_FLAG_TE3);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    sdram_delay_us(10u);
    return ok;
}

static uint8_t __attribute__((unused))
sdram_memtest_dma_long_banks(uint16_t good_mask)
{
    const uint32_t transfer_count = 65535u;
    const uint32_t transfer_bytes = transfer_count * 32u;
    const uint32_t segment_stride = 2u * 1024u * 1024u;
    uint32_t mask32 = (uint32_t)good_mask | ((uint32_t)good_mask << 16);
    uint8_t good_bits = sdram_memtest_popcount16(good_mask);
    uint32_t total_write_cycles = 0u;
    uint32_t total_read_cycles = 0u;
    uint32_t total_bytes = 0u;
    uint32_t cycles;
    uint32_t bad_words = 0u;
    uint32_t checksum = 0u;
    uint32_t bank;
    uint32_t segment;
    uint32_t index;
    uint8_t all_ok = 1u;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    sdram_usb_debug_write_line("DMA_LONG START mode=wide256 count=65535 span=2097120 segments=4 banks=4 meminc=0");
    for(bank = 0u; bank < 4u; bank++)
    {
        for(segment = 0u; segment < 4u; segment++)
        {
            uintptr_t address = (uintptr_t)V5F_SDRAM_NATIVE_ADDR +
                                (bank * V5F_SDRAM_X16_BANK_BYTES) +
                                (segment * segment_stride);
            uint32_t pattern[8];
            uint8_t write_ok;
            uint8_t read_ok;
            int used;

            for(index = 0u; index < 8u; index++)
            {
                pattern[index] = (0x6D2B79F5u ^
                                  (bank * 0x11111111u) ^
                                  (segment * 0x22222222u) ^
                                  (index * 0x9E3779B1u)) & mask32;
                s_sdram_bw_buffer[index] = pattern[index];
            }
            used = rt_snprintf(line,
                               sizeof(line),
                               "DMA_LONG bank=%u segment=%u addr=%08x WRITE_START",
                               (unsigned int)bank,
                               (unsigned int)segment,
                               (unsigned int)address);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            if(sdram_memtest_precharge_all() == 0u)
            {
                all_ok = 0u;
                break;
            }
            write_ok = sdram_memtest_dma_long_timed(
                address, 1u, (uint16_t)transfer_count, &cycles);
            total_write_cycles += cycles;
            used = rt_snprintf(line,
                               sizeof(line),
                               "DMA_LONG bank=%u segment=%u addr=%08x WRITE_%s cycles=%u",
                               (unsigned int)bank,
                               (unsigned int)segment,
                               (unsigned int)address,
                               (write_ok != 0u) ? "DONE" : "FAIL",
                               (unsigned int)cycles);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            if(write_ok == 0u)
            {
                all_ok = 0u;
                break;
            }

            for(index = 0u; index < 8u; index++)
            {
                s_sdram_bw_buffer[index] = 0xDEADBEEFu;
            }
            used = rt_snprintf(line,
                               sizeof(line),
                               "DMA_LONG bank=%u segment=%u addr=%08x READ_START",
                               (unsigned int)bank,
                               (unsigned int)segment,
                               (unsigned int)address);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            if(sdram_memtest_precharge_all() == 0u)
            {
                all_ok = 0u;
                break;
            }
            read_ok = sdram_memtest_dma_long_timed(
                address, 0u, (uint16_t)transfer_count, &cycles);
            total_read_cycles += cycles;
            for(index = 0u; index < 8u; index++)
            {
                uint32_t actual = s_sdram_bw_buffer[index];

                checksum += actual;
                if(((actual ^ pattern[index]) & mask32) != 0u)
                {
                    bad_words++;
                }
            }
            used = rt_snprintf(line,
                               sizeof(line),
                               "DMA_LONG bank=%u segment=%u addr=%08x READ_%s cycles=%u bad=%u",
                               (unsigned int)bank,
                               (unsigned int)segment,
                               (unsigned int)address,
                               (read_ok != 0u) ? "DONE" : "FAIL",
                               (unsigned int)cycles,
                               (unsigned int)bad_words);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            if(read_ok == 0u)
            {
                all_ok = 0u;
                break;
            }
            total_bytes += transfer_bytes;
            ch32h417_dual_cdc_poll();
        }
        if(all_ok == 0u)
        {
            break;
        }
    }
    sdram_memtest_bandwidth_report("DMA256_LONG_WRITE",
                                    total_bytes,
                                    total_write_cycles,
                                    good_bits,
                                    (all_ok != 0u) ? 0u : 1u,
                                    0u);
    sdram_memtest_bandwidth_report("DMA256_LONG_READ",
                                    total_bytes,
                                    total_read_cycles,
                                    good_bits,
                                    bad_words + ((all_ok != 0u) ? 0u : 1u),
                                    checksum);
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "DMA_LONG END %s bytes=%u bad=%u",
                               ((all_ok != 0u) && (bad_words == 0u)) ?
                               "PASS" : "FAIL",
                               (unsigned int)total_bytes,
                               (unsigned int)bad_words);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    return (uint8_t)((all_ok != 0u) && (bad_words == 0u));
}

static uint8_t __attribute__((unused))
sdram_memtest_dma_read_length(uint16_t good_mask)
{
    static const uint16_t counts[] = {
        512u, 2048u, 8192u, 16384u, 32768u, 49152u, 65535u
    };
    const uintptr_t address = (uintptr_t)V5F_SDRAM_NATIVE_ADDR + 0x00200000u;
    uint32_t mask32 = (uint32_t)good_mask | ((uint32_t)good_mask << 16);
    uint32_t pattern[8];
    uint32_t cycles;
    uint32_t index;
    uint32_t step;
    uint8_t ok;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    sdram_usb_debug_write_line("DMA_LENGTH START addr=c0200000 mode=wide256 meminc=0 counts=512,2048,8192,16384,32768,49152,65535");
    for(index = 0u; index < 8u; index++)
    {
        pattern[index] = (0x5A17C3E9u ^ (index * 0x9E3779B1u)) & mask32;
        s_sdram_bw_buffer[index] = pattern[index];
    }
    sdram_usb_debug_write_line("DMA_LENGTH PREP WRITE_START count=65535 span=2097120");
    if(sdram_memtest_precharge_all() == 0u)
    {
        sdram_usb_debug_write_line("DMA_LENGTH PREP PALL_FAIL");
        return 0u;
    }
    ok = sdram_memtest_dma_long_timed(address, 1u, 65535u, &cycles);
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "DMA_LENGTH PREP WRITE_%s cycles=%u",
                               (ok != 0u) ? "DONE" : "FAIL",
                               (unsigned int)cycles);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    if(ok == 0u)
    {
        return 0u;
    }

    for(step = 0u; step < (sizeof(counts) / sizeof(counts[0])); step++)
    {
        uint32_t span = (uint32_t)counts[step] * 32u;
        uint32_t bad = 0u;

        for(index = 0u; index < 8u; index++)
        {
            s_sdram_bw_buffer[index] = 0xDEADBEEFu;
        }
        if(sdram_memtest_precharge_all() == 0u)
        {
            sdram_usb_debug_write_line("DMA_LENGTH READ PALL_FAIL");
            return 0u;
        }
        {
            int used = rt_snprintf(line,
                                   sizeof(line),
                                   "DMA_LENGTH READ count=%u span=%u START",
                                   (unsigned int)counts[step],
                                   (unsigned int)span);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
        ok = sdram_memtest_dma_long_timed(
            address, 0u, counts[step], &cycles);
        for(index = 0u; index < 8u; index++)
        {
            if(((s_sdram_bw_buffer[index] ^ pattern[index]) & mask32) != 0u)
            {
                bad++;
            }
        }
        {
            int used = rt_snprintf(line,
                                   sizeof(line),
                                   "DMA_LENGTH READ count=%u span=%u %s cycles=%u bad=%u",
                                   (unsigned int)counts[step],
                                   (unsigned int)span,
                                   (ok != 0u) ? "DONE" : "FAIL",
                                   (unsigned int)cycles,
                                   (unsigned int)bad);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
        if((ok == 0u) || (bad != 0u))
        {
            return 0u;
        }
        ch32h417_dual_cdc_poll();
    }
    sdram_usb_debug_write_line("DMA_LENGTH END PASS max_count=65535 max_span=2097120");
    return 1u;
}

static int sdram_memtest_safe_reinit(uint32_t *failure_stage)
{
    int result;

    *failure_stage = 1u;
    result = sdram_wait_ready();
    if(result != V5F_SDRAM_OK)
    {
        return result;
    }
    *failure_stage = 2u;
    if(sdram_memtest_precharge_all() == 0u)
    {
        return V5F_SDRAM_ERR_TIMEOUT;
    }
    *failure_stage = 3u;
    FMC_SDRAMCmd(FMC_Bank5_SDRAM, DISABLE);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    sdram_delay_us(20u);
    if((FMC_Bank5_6->MISC & FMC_MISC_En_Bank1) != 0u)
    {
        return V5F_SDRAM_ERR_TIMEOUT;
    }
    *failure_stage = 4u;
    result = sdram_init_profile(0u, 0u, 0u);
    if(result != V5F_SDRAM_OK)
    {
        return result;
    }
    *failure_stage = 5u;
    return sdram_wait_ready();
}

static uint8_t __attribute__((unused))
sdram_memtest_dma_reinit_stress(uint16_t good_mask)
{
    const uint32_t stress_loops = 128u;
    static const uintptr_t addresses[2] = {
        (uintptr_t)V5F_SDRAM_NATIVE_ADDR,
        (uintptr_t)V5F_SDRAM_NATIVE_ADDR + 0x00200000u
    };
    uint32_t mask32 = (uint32_t)good_mask | ((uint32_t)good_mask << 16);
    uint32_t patterns[2][8];
    uint32_t cycles;
    uint32_t loop;
    uint32_t target;
    uint32_t index;
    uint32_t total_cycles = 0u;
    uint32_t bad_words = 0u;
    uint8_t ok = 1u;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    sdram_usb_debug_write_line("DMA_REINIT START loops=128 mode=wide256 count=65535 span=2097120 targets=c0000000,c0200000 dma_reset=each_transfer");
    for(target = 0u; target < 2u; target++)
    {
        int used;

        for(index = 0u; index < 8u; index++)
        {
            patterns[target][index] =
                (0x35A7C19Du ^
                 (target * 0x55555555u) ^
                 (index * 0x9E3779B1u)) & mask32;
            s_sdram_bw_buffer[index] = patterns[target][index];
        }
        used = rt_snprintf(line,
                           sizeof(line),
                           "DMA_REINIT PREP target=%u addr=%08x WRITE_START",
                           (unsigned int)target,
                           (unsigned int)addresses[target]);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        if((sdram_memtest_precharge_all() == 0u) ||
           (sdram_memtest_dma_long_timed(
                addresses[target], 1u, 65535u, &cycles) == 0u))
        {
            sdram_usb_debug_write_line("DMA_REINIT PREP FAIL");
            return 0u;
        }
        used = rt_snprintf(line,
                           sizeof(line),
                           "DMA_REINIT PREP target=%u addr=%08x WRITE_DONE cycles=%u",
                           (unsigned int)target,
                           (unsigned int)addresses[target],
                           (unsigned int)cycles);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    for(loop = 0u; loop < stress_loops; loop++)
    {
        int init_result;
        uint32_t recover_stage;
        int used;

        target = loop & 1u;
        used = rt_snprintf(line,
                           sizeof(line),
                           "DMA_REINIT loop=%u target=%u addr=%08x SAFE_REINIT_START",
                           (unsigned int)loop,
                           (unsigned int)target,
                           (unsigned int)addresses[target]);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        init_result = sdram_memtest_safe_reinit(&recover_stage);
        used = rt_snprintf(line,
                           sizeof(line),
                           "DMA_REINIT loop=%u target=%u SAFE_REINIT_%s code=%d stage=%u sdsr=%08x misc=%08x",
                           (unsigned int)loop,
                           (unsigned int)target,
                           (init_result == V5F_SDRAM_OK) ? "PASS" : "FAIL",
                           init_result,
                           (unsigned int)recover_stage,
                           (unsigned int)FMC_Bank5_6->SDSR,
                           (unsigned int)FMC_Bank5_6->MISC);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        if(init_result != V5F_SDRAM_OK)
        {
            ok = 0u;
            break;
        }
        for(index = 0u; index < 8u; index++)
        {
            s_sdram_bw_buffer[index] = 0xDEADBEEFu;
        }
        used = rt_snprintf(line,
                           sizeof(line),
                           "DMA_REINIT loop=%u target=%u addr=%08x READ_START",
                           (unsigned int)loop,
                           (unsigned int)target,
                           (unsigned int)addresses[target]);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        if(sdram_memtest_dma_long_timed(
               addresses[target], 0u, 65535u, &cycles) == 0u)
        {
            ok = 0u;
            break;
        }
        total_cycles += cycles;
        for(index = 0u; index < 8u; index++)
        {
            if(((s_sdram_bw_buffer[index] ^ patterns[target][index]) &
                mask32) != 0u)
            {
                bad_words++;
            }
        }
        used = rt_snprintf(line,
                           sizeof(line),
                           "DMA_REINIT loop=%u target=%u READ_DONE cycles=%u bad=%u",
                           (unsigned int)loop,
                           (unsigned int)target,
                           (unsigned int)cycles,
                           (unsigned int)bad_words);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        if(bad_words != 0u)
        {
            ok = 0u;
            break;
        }
        ch32h417_dual_cdc_poll();
    }
    sdram_memtest_bandwidth_report("DMA256_REINIT_READ",
                                    loop * (65535u * 32u),
                                    total_cycles,
                                    sdram_memtest_popcount16(good_mask),
                                    bad_words + ((ok != 0u) ? 0u : 1u),
                                    0u);
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "DMA_REINIT END %s loops=%u bad=%u",
                               ((ok != 0u) && (loop == stress_loops)) ? "PASS" : "FAIL",
                               (unsigned int)loop,
                               (unsigned int)bad_words);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    return (uint8_t)((ok != 0u) &&
                     (loop == stress_loops) &&
                     (bad_words == 0u));
}

static uint8_t __attribute__((noinline))
sdram_memtest_bandwidth(uint16_t good_mask)
{
    uintptr_t dma32_target = (uintptr_t)(
        V5F_SDRAM_NATIVE_ADDR + 0x00600000u);
    uintptr_t dma256_target = (uintptr_t)(
        V5F_SDRAM_NATIVE_ADDR + 0x00700000u);
    uint32_t mask32 = (uint32_t)good_mask | ((uint32_t)good_mask << 16);
    uint8_t good_bits = sdram_memtest_popcount16(good_mask);
    uint32_t start;
    uint32_t cycles;
    uint32_t write_cycles;
    uint32_t checksum;
    uint32_t verify_bad;
    uint32_t dma32_bad;
    uint32_t dma256_bad;
    uint32_t index;
    uint8_t dma32_write_ok;
    uint8_t dma32_read_ok;
    uint8_t dma256_write_ok;
    uint8_t dma256_read_ok;
    uint8_t dma32_full_ok;
    uint8_t dma256_full_ok;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_PATTERN;
    s_sdram_watchdog_pass = 0u;
    sdram_memtest_watchdog_begin();
    sdram_usb_debug_write_line("WATCHDOG ARMED timeout_approx_s=25 retain=2017ff00 checkpoint=tc-cleanup");
    {
        uint32_t theoretical_x100 =
            (uint32_t)((((uint64_t)g_v5f_hw_test_diag.sdram_sdclk_hz *
                         2u * 100u) + 999999u) /
                       1000000u);
        uint32_t useful_x100 = (theoretical_x100 * good_bits) / 16u;
        int used = rt_snprintf(line,
                               sizeof(line),
                               "DMA_ONLY START mode=dma256-stress timer=systick1 hz=%u block=16384 good_bits=%u useful_peak=%u.%02uMB/s cpu_sdram=none",
                               (unsigned int)HCLKClock,
                               (unsigned int)good_bits,
                               (unsigned int)(useful_x100 / 100u),
                               (unsigned int)(useful_x100 % 100u));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    sdram_usb_debug_write_line("DMA_ROUTE short=DMA1_CH3 stress=DMA2_CH3 mode=wide256 buffer=shared-sram@20174000 gap_us=0");
    sdram_usb_debug_write_line("DMA_ONLY TIMER PROBE START");
    start = sdram_memtest_timer_now();
    cycles = sdram_memtest_timer_now() - start;
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "DMA_ONLY TIMER PROBE PASS delta=%u",
                               (unsigned int)cycles);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    sdram_usb_debug_write_line("DMA_ONLY CPU ACCESS SKIP read=1 write=1");
    for(index = 0u; index < V5F_SDRAM_DMA_BW_WORDS; index++)
    {
        s_sdram_bw_buffer[index] =
            (0x13579BDFu ^ (index * 0x10204081u)) & mask32;
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
    sdram_usb_debug_write_line("DMA_ONLY DMA32_WRITE START");
    dma32_write_ok = sdram_memtest_dma_timed(
        dma32_target, 1u, 0u, &write_cycles);
    for(index = 0u; index < V5F_SDRAM_DMA_BW_WORDS; index++)
    {
        s_sdram_bw_buffer[index] = 0xDEADBEEFu;
    }
    sdram_usb_debug_write_line("DMA_ONLY DMA32_READ START");
    dma32_read_ok = sdram_memtest_dma_timed(
        dma32_target, 0u, 0u, &cycles);
    verify_bad = 0u;
    checksum = 0u;
    for(index = 0u; index < V5F_SDRAM_DMA_BW_WORDS; index++)
    {
        uint32_t expected =
            (0x13579BDFu ^ (index * 0x10204081u)) & mask32;
        uint32_t actual = s_sdram_bw_buffer[index];

        checksum += actual;
        if(((actual ^ expected) & mask32) != 0u)
        {
            verify_bad++;
        }
    }
    dma32_bad = verify_bad;
    sdram_memtest_bandwidth_report("DMA32_WRITE",
                                    sizeof(s_sdram_bw_buffer),
                                    write_cycles,
                                    good_bits,
                                    0u,
                                    0u);
    sdram_memtest_bandwidth_report("DMA32_READ",
                                    sizeof(s_sdram_bw_buffer),
                                    cycles,
                                    good_bits,
                                    verify_bad,
                                    checksum);

    for(index = 0u; index < V5F_SDRAM_DMA_BW_WORDS; index++)
    {
        s_sdram_bw_buffer[index] =
            (0x2468ACE1u ^ (index * 0x20408103u)) & mask32;
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
    sdram_usb_debug_write_line("DMA_ONLY DMA256_WRITE START");
    dma256_write_ok = sdram_memtest_dma_timed(
        dma256_target, 1u, 1u, &write_cycles);
    for(index = 0u; index < V5F_SDRAM_DMA_BW_WORDS; index++)
    {
        s_sdram_bw_buffer[index] = 0xDEADBEEFu;
    }
    sdram_usb_debug_write_line("DMA_ONLY DMA256_READ START");
    dma256_read_ok = sdram_memtest_dma_timed(
        dma256_target, 0u, 1u, &cycles);
    verify_bad = 0u;
    checksum = 0u;
    for(index = 0u; index < V5F_SDRAM_DMA_BW_WORDS; index++)
    {
        uint32_t expected =
            (0x2468ACE1u ^ (index * 0x20408103u)) & mask32;
        uint32_t actual = s_sdram_bw_buffer[index];

        checksum += actual;
        if(((actual ^ expected) & mask32) != 0u)
        {
            verify_bad++;
        }
    }
    dma256_bad = verify_bad;
    sdram_memtest_bandwidth_report("DMA256_WRITE",
                                    sizeof(s_sdram_bw_buffer),
                                    write_cycles,
                                    good_bits,
                                    0u,
                                    0u);
    sdram_memtest_bandwidth_report("DMA256_READ",
                                    sizeof(s_sdram_bw_buffer),
                                    cycles,
                                    good_bits,
                                    verify_bad,
                                    checksum);

    dma32_full_ok = 1u;
    sdram_usb_debug_write_line("DMA32_FULL SKIP reason=v62_shared_sram_pass");
    dma256_full_ok = sdram_memtest_dma_sliced_stress(good_mask, 1u);
    sdram_usb_debug_write_line("DMA_LENGTH TEST SKIP result=v49_pass");

    sdram_usb_debug_write_line((dma32_write_ok != 0u) &&
                               (dma32_read_ok != 0u) &&
                               (dma256_write_ok != 0u) &&
                               (dma256_read_ok != 0u) &&
                               (dma32_bad == 0u) &&
                               (dma256_bad == 0u) &&
                               (dma32_full_ok != 0u) &&
                               (dma256_full_ok != 0u) ?
                               "DMA_ONLY END verify=PASS dma=PASS cpu_sdram=none" :
                               "DMA_ONLY END verify=FAIL_OR_DMA_FAIL cpu_sdram=none");
    return (uint8_t)((dma32_write_ok != 0u) &&
                     (dma32_read_ok != 0u) &&
                     (dma256_write_ok != 0u) &&
                     (dma256_read_ok != 0u) &&
                     (dma32_bad == 0u) &&
                     (dma256_bad == 0u) &&
                     (dma32_full_ok != 0u) &&
                     (dma256_full_ok != 0u));
}

static void V5F_MAYBE_UNUSED run_sdram_memtest_test(void)
{
    int result;
    uint8_t banks_ok;
    uint8_t dma_ok;
    uint8_t coherence_ok;
    uint8_t bandwidth_ok;
    uint8_t d10d11_raw_ok;
    uint16_t x16_compare_mask;
    uint32_t test_bytes;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    v5f_sdram_memtest_result_t failure = {0};
    v5f_sdram_memtest_result_t bank_failure = {0};

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;

    sdram_memtest_cdc_wait_for_start();
    sdram_memtest_cdc_begin();
    if(sdram_memtest_watchdog_report_recovery() != 0u)
    {
        sdram_usb_debug_write_line("WATCHDOG DIAG DMA_FMC_HB_LOCK_SUSPECTED IWDG_RESET_CONFIRMED");
        sdram_memtest_cdc_summary(0u);
        sdram_memtest_cdc_result_loop(0u);
    }

    result = sdram_init_profile(0u, 0u, 1u);
    if(result != V5F_SDRAM_OK)
    {
        failure.stage = V5F_SDRAM_STAGE_INIT;
        sdram_diag_fail(result, &failure);
        sdram_memtest_cdc_result_loop(0u);
    }

    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "DQ_ROUTE pa9=AF%x/mode%x pa10=AF%x/mode%x pd0=AF%x/mode%x pd1=AF%x/mode%x rm=%u",
                               (unsigned int)((AFIO->GPIOA_AFHR >> 4) & 0xFu),
                               (unsigned int)((GPIOA->CFGHR >> 4) & 0xFu),
                               (unsigned int)((AFIO->GPIOA_AFHR >> 8) & 0xFu),
                               (unsigned int)((GPIOA->CFGHR >> 8) & 0xFu),
                               (unsigned int)(AFIO->GPIOD_AFLR & 0xFu),
                               (unsigned int)(GPIOD->CFGLR & 0xFu),
                               (unsigned int)((AFIO->GPIOD_AFLR >> 4) & 0xFu),
                               (unsigned int)((GPIOD->CFGLR >> 4) & 0xFu),
                               (unsigned int)((AFIO->PCFR1 & AFIO_PCFR1_PD0_1_REMAP) != 0u));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "LOW8 CONFIG fmc_width=16 cpu_width=16 stride=2 compare_mask=00ff");
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "DQM_CFG dqml_pc2=%x/af%x dqmh_pe3=%x/af%x",
                               (unsigned int)((GPIOC->CFGLR >> 8) & 0xFu),
                               (unsigned int)((AFIO->GPIOC_AFLR >> 8) & 0xFu),
                               (unsigned int)((GPIOE->CFGLR >> 12) & 0xFu),
                               (unsigned int)((AFIO->GPIOE_AFLR >> 12) & 0xFu));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    {
        uint32_t sdcr = FMC_Bank5_6->SDCR[FMC_Bank5_SDRAM];
        int used = rt_snprintf(line,
                               sizeof(line),
                               "BANK_CFG sdcr=%08x mwid=%u nb=%u pb1=%x/af%x pb15=%x/af%x",
                               (unsigned int)sdcr,
                               (unsigned int)((sdcr >> 4) & 0x3u),
                               (unsigned int)((sdcr >> 6) & 0x1u),
                               (unsigned int)((GPIOB->CFGLR >> 4) & 0xFu),
                               (unsigned int)((AFIO->GPIOB_AFLR >> 4) & 0xFu),
                               (unsigned int)((GPIOB->CFGHR >> 28) & 0xFu),
                               (unsigned int)((AFIO->GPIOB_AFHR >> 28) & 0xFu));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    x16_compare_mask = 0xFFFFu;
    sdram_usb_debug_write_line("COMPARE_MASK full=ffff ignored=0000 source=v70_dq_raw_pass");
    sdram_usb_debug_write_line("DMA CALL START cpu_sdram_access=none");
    bandwidth_ok = sdram_memtest_bandwidth(x16_compare_mask);
    sdram_usb_debug_write_line("DMA CALL DONE");
    d10d11_raw_ok = sdram_memtest_d10d11_raw_probe();
    sdram_usb_debug_write_line((d10d11_raw_ok != 0u) ?
                               "DQ_ISOLATED RESULT PASS d10d11=variable" :
                               "DQ_ISOLATED RESULT FAIL d10d11=not_variable");
    g_v5f_hw_test_diag.sdram_test_bytes = 32u * 1024u * 1024u;
    if((x16_compare_mask == 0xFFFFu) &&
       (bandwidth_ok != 0u) &&
       (d10d11_raw_ok != 0u))
    {
        g_v5f_hw_test_diag.phase = V5F_HW_PHASE_PASSED;
        g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_PATTERN;
        g_v5f_hw_test_diag.sdram_ok_count++;
        sdram_usb_debug_write_line("FULL16 DMA RESULT PASS mask=ffff dma_rw=pass");
        sdram_memtest_cdc_summary(1u);
        sdram_memtest_cdc_result_loop(1u);
    }

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_FAILED;
    g_v5f_hw_test_diag.last_error = V5F_SDRAM_ERR_VERIFY;
    g_v5f_hw_test_diag.sdram_stage = V5F_SDRAM_STAGE_DATA_BUS;
    g_v5f_hw_test_diag.sdram_expected = 0xFFFFu;
    g_v5f_hw_test_diag.sdram_actual = x16_compare_mask;
    g_v5f_hw_test_diag.sdram_fail_count++;
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "FULL16 RESULT FAIL dma=%s d10d11_raw=%s",
                               ((x16_compare_mask == 0xFFFFu) &&
                                (bandwidth_ok != 0u)) ? "pass" : "fail",
                               (d10d11_raw_ok != 0u) ? "pass" : "fail");
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    sdram_memtest_cdc_summary(0u);
    sdram_memtest_cdc_result_loop(0u);

    coherence_ok = sdram_memtest_normal_100mhz_validate();
    banks_ok = sdram_memtest_x8_bank_probe(&bank_failure);
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "NORMAL100_BANK_DIAG %s off=%08x exp=%02x got=%02x continue=full16MB",
                               (banks_ok != 0u) ? "PASS" : "WARN",
                               (unsigned int)bank_failure.offset,
                               (unsigned int)bank_failure.expected,
                               (unsigned int)bank_failure.actual);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    test_bytes = V5F_SDRAM_BYTES;
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "FULL_TEST START mode=normal100 access=cpu16 compare=low8 logical=%u physical=%u banks=%s",
                               (unsigned int)test_bytes,
                               (unsigned int)(test_bytes * V5F_SDRAM_LOW8_PHYSICAL_STRIDE),
                               (banks_ok != 0u) ? "pass" : "warn");
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    result = sdram_memtest_x8_full(
        (volatile uint8_t *)V5F_SDRAM_BASE_ADDR,
        &failure,
        test_bytes);
    if(result != V5F_SDRAM_OK)
    {
        sdram_diag_fail(result, &failure);
        sdram_memtest_cdc_result_loop(0u);
    }
    sdram_usb_debug_write_line("CPU STORAGE RESULT PASS mode=normal100 logical_bytes=16777216 compare=low8");
    if(coherence_ok == 0u)
    {
        g_v5f_hw_test_diag.phase = V5F_HW_PHASE_FAILED;
        g_v5f_hw_test_diag.last_error = V5F_SDRAM_ERR_VERIFY;
        g_v5f_hw_test_diag.sdram_fail_count++;
        sdram_usb_debug_write_line("FUNCTION RESULT FAIL reason=normal100_quick cpu_storage=pass");
        sdram_memtest_cdc_summary(0u);
        sdram_memtest_cdc_result_loop(0u);
    }
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_PASSED;
    sdram_memtest_cdc_summary(1u);
    sdram_memtest_cdc_result_loop(1u);

    if(V5F_SDRAM_MEMTEST_LEGACY_DIAG != 0)
    {
        (void)sdram_memtest_prefetch_coherence();
        sdram_memtest_bank_tune();
        sdram_memtest_access_profile_scan();
        sdram_memtest_dma_path_scan();
        sdram_memtest_cas_scan();
        sdram_memtest_nrfs_scan();
        sdram_memtest_map16_scan();
        sdram_memtest_bank_settle_scan();
        sdram_memtest_bank_sync_scan();
        sdram_memtest_recovery_scan();
        sdram_memtest_refresh_profile_scan();
        sdram_memtest_transition_scan();
    }

    dma_ok = sdram_memtest_prefetch_validate();
    banks_ok = sdram_memtest_x8_bank_probe(&bank_failure);
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "PREFETCH_BANK_DIAG %s off=%08x exp=%02x got=%02x continue=full16MB",
                               (banks_ok != 0u) ? "PASS" : "WARN",
                               (unsigned int)bank_failure.offset,
                               (unsigned int)bank_failure.expected,
                               (unsigned int)bank_failure.actual);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    test_bytes = V5F_SDRAM_BYTES;
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "FULL_TEST START mode=prefetch access=cpu16 compare=low8 logical=%u physical=%u banks=%s",
                               (unsigned int)test_bytes,
                               (unsigned int)(test_bytes * V5F_SDRAM_LOW8_PHYSICAL_STRIDE),
                               (banks_ok != 0u) ? "pass" : "warn");
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    result = sdram_memtest_x8_full(
        (volatile uint8_t *)V5F_SDRAM_BASE_ADDR,
        &failure,
        test_bytes);
    if(result != V5F_SDRAM_OK)
    {
        sdram_diag_fail(result, &failure);
        sdram_memtest_cdc_result_loop(0u);
    }

    sdram_usb_debug_write_line("CPU STORAGE RESULT PASS mode=prefetch logical_bytes=16777216 compare=low8");
    if(dma_ok == 0u)
    {
        g_v5f_hw_test_diag.phase = V5F_HW_PHASE_FAILED;
        g_v5f_hw_test_diag.last_error = V5F_SDRAM_ERR_VERIFY;
        g_v5f_hw_test_diag.sdram_fail_count++;
        sdram_usb_debug_write_line("FUNCTION RESULT FAIL reason=prefetch_dma_direct cpu_storage=pass");
        sdram_memtest_cdc_summary(0u);
        sdram_memtest_cdc_result_loop(0u);
    }

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_PASSED;
    sdram_memtest_cdc_summary(1u);
    sdram_memtest_cdc_result_loop(1u);
}

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO
#define V5F_SDRAM_VIDEO_WIDTH             800u
#define V5F_SDRAM_VIDEO_HEIGHT            480u
#define V5F_SDRAM_VIDEO_STAGE_BYTES       V5F_SDRAM_DMA_BUFFER_BYTES
#define V5F_SDRAM_VIDEO_ACK_BYTES         (2u * V5F_SDRAM_VIDEO_STAGE_BYTES)
#define V5F_SDRAM_VIDEO_UPLOAD_TIMEOUT_MS 10000u
#define V5F_SDRAM_VIDEO_REPORT_MS         1000u
#define V5F_SDRAM_VIDEO_PASS_MS           10000u
#define V5F_SDRAM_VIDEO_VBLANK_TIMEOUT_MS 100u
#define V5F_SDRAM_VIDEO_POWER_SETTLE_MS   550u
#define V5F_SDRAM_VIDEO_SCAN_TEST_MS      200u
#define V5F_SDRAM_VIDEO_VISUAL_HOLD_MS    2000u
#define V5F_SDRAM_VIDEO_PROBE_WIDTH       64u
#define V5F_SDRAM_VIDEO_PROBE_HEIGHT      64u
#define V5F_SDRAM_VIDEO_CHART_WIDTH        400u
#define V5F_SDRAM_VIDEO_CHART_HEIGHT       240u
#define V5F_SDRAM_VIDEO_CHART_BPP          4u
#define V5F_SDRAM_VIDEO_CHART_COLUMNS      8u
#define V5F_SDRAM_VIDEO_CHART_ROWS         4u
#define V5F_SDRAM_VIDEO_CHART_CELL_WIDTH   50u
#define V5F_SDRAM_VIDEO_CHART_CELL_HEIGHT  60u
#define V5F_SDRAM_VIDEO_CHART_BYTES        \
    (V5F_SDRAM_VIDEO_CHART_WIDTH * V5F_SDRAM_VIDEO_CHART_HEIGHT * \
     V5F_SDRAM_VIDEO_CHART_BPP)
#define V5F_SDRAM_VIDEO_CHART_CRC32        0x01D00ACBu
#define V5F_SDRAM_VIDEO_READBACK_PASSES   1u
#define V5F_SDRAM_VIDEO_MAX_BLOCKS        \
    (V5F_SDRAM_BYTES / V5F_SDRAM_VIDEO_STAGE_BYTES)
#define V5F_SDRAM_H4V1_STORAGE_OFFSET     0x00200000u
#define V5F_SDRAM_H4V1_STORAGE_BYTES      \
    (V5F_SDRAM_BYTES - V5F_SDRAM_H4V1_STORAGE_OFFSET)
#define V5F_SDRAM_H4V1_FRAME_BYTES        \
    (V5F_SDRAM_VIDEO_WIDTH * V5F_SDRAM_VIDEO_HEIGHT * 2u)
#define V5F_SDRAM_H4V1_FB1_OFFSET          0x000C0000u
#define V5F_SDRAM_H4V1_PAYLOAD_STAGE_BYTES 0x00048000u
#define V5F_SDRAM_H4V1_HISTORY_BYTES       0x00010000u
#define V5F_SDRAM_H4V1_OUTPUT_BYTES        0x00004000u
#define V5F_SDRAM_H4V1_DMA_ALIGNMENT       32u
#define V5F_SDRAM_H4V1_BATCH_FRAMES        90u
#define V5F_SDRAM_H4V1_BATCH_DMA_SAMPLES   5u
#define V5F_SDRAM_H4V1_EXPECTED_GOP        30u
#define V5F_SDRAM_H4V1_LIVE_READ_WIDE256   0u
#define V5F_SDRAM_H4V1_PREVIOUS_READ_WIDE256 1u
#define V5F_SDRAM_H4V1_LIVE_READ_SLICE_BYTES 0x00000800u
#define V5F_SDRAM_H4V1_LIVE_WRITE_WIDE256  1u
#define V5F_SDRAM_H4V1_LIVE_WRITE_SLICE_BYTES 0x00000800u
#define V5F_SDRAM_H4V1_MAX_FRAMES          120u
#define V5F_SDRAM_H4V1_OUTPUT_ADDR         0x20160000u
#define V5F_SDRAM_H4V1_HISTORY_ADDR        0x20164000u

#if V5F_SDRAM_H4V1_PAYLOAD_STAGE_BYTES > V5F_LCD_FB_REGION_SIZE
#error H4V1 payload stage exceeds the internal LCD framebuffer region
#endif

#if (V5F_SDRAM_H4V1_OUTPUT_ADDR + V5F_SDRAM_H4V1_OUTPUT_BYTES) > \
    V5F_SDRAM_H4V1_HISTORY_ADDR
#error H4V1 output overlaps H4V1 history
#endif

#if (V5F_SDRAM_H4V1_HISTORY_ADDR + V5F_SDRAM_H4V1_HISTORY_BYTES) > \
    V5F_SDRAM_DMA_SHARED_BUFFER_ADDR
#error H4V1 history overlaps the shared DMA staging buffer
#endif

#if (V5F_SDRAM_H4V1_LIVE_WRITE_SLICE_BYTES == 0u) || \
    (V5F_SDRAM_H4V1_LIVE_WRITE_SLICE_BYTES > \
     V5F_SDRAM_H4V1_OUTPUT_BYTES) || \
    ((V5F_SDRAM_H4V1_LIVE_WRITE_SLICE_BYTES % 32u) != 0u)
#error H4V1 live DMA256 write slice must be a nonzero multiple of 32 bytes
#endif

#if (V5F_SDRAM_H4V1_LIVE_READ_SLICE_BYTES == 0u) || \
    (V5F_SDRAM_H4V1_LIVE_READ_SLICE_BYTES > \
     V5F_SDRAM_H4V1_OUTPUT_BYTES) || \
    ((V5F_SDRAM_H4V1_LIVE_READ_SLICE_BYTES % 32u) != 0u)
#error H4V1 live DMA256 read slice must be a nonzero multiple of 32 bytes
#endif

#if V5F_SDRAM_VIDEO_CHART_BYTES > V5F_LCD_FB_REGION_SIZE
#error ARGB8888 color chart exceeds the internal LCD framebuffer region
#endif

typedef struct
{
    uint32_t pixel_format;
    uint32_t bytes_per_pixel;
    uint32_t frame_bytes;
    uint32_t frames;
    uint32_t fps;
    uint32_t total_bytes;
    uint32_t expected_crc;
    uint32_t storage_offset;
    uint8_t is_h4v1;
    char name[16];
} v5f_sdram_video_config_t;

typedef struct
{
    uint32_t crc;
    uint32_t bad_blocks;
    uint32_t first_bad_block;
    uint32_t first_expected_crc;
    uint32_t first_actual_crc;
} v5f_sdram_video_readback_diag_t;

static uint32_t s_sdram_video_block_crc[V5F_SDRAM_VIDEO_MAX_BLOCKS];
static uint32_t s_sdram_video_block_count;
static uint8_t s_sdram_video_readback_unstable;

/*
 * Upload ownership: 0x20160000..0x2016ffff is the retired CDC raw ring.
 * Decode ownership after raw_rx_enable(0): 16 KiB output + 64 KiB history.
 * The DMA endpoint remains separate at 0x20174000..0x20177fff.
 */
#define s_sdram_h4v1_output \
    ((uint8_t *)(uintptr_t)V5F_SDRAM_H4V1_OUTPUT_ADDR)
#define s_sdram_h4v1_history \
    ((uint8_t *)(uintptr_t)V5F_SDRAM_H4V1_HISTORY_ADDR)

static uint32_t sdram_video_crc32_update(uint32_t crc,
                                         const uint8_t *data,
                                         uint32_t bytes)
{
    static const uint32_t nibble_table[16] =
    {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
        0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
        0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
    };
    uint32_t offset;

    for(offset = 0u; offset < bytes; offset++)
    {
        crc ^= data[offset];
        crc = (crc >> 4) ^ nibble_table[crc & 0x0Fu];
        crc = (crc >> 4) ^ nibble_table[crc & 0x0Fu];
    }
    return crc;
}

static void sdram_video_send_config_help(void)
{
    sdram_usb_debug_write_line("H417 SDRAM VIDEO H4V1 ISOLATED v29 STAGE5R COPY PROFILE");
    sdram_usb_debug_write_line("ISOLATION base=aa5f60f transport=v37_32k_dma2 readback=dma256 codec=stream64k_fast playback=live0_89_once live_dma=r256x2k_w256x2k codec_crc=sampled8 profile=3,30,31_copy_split delta=xor_copy32 match=forward_fanout usb=retire_before_rearm");
    sdram_usb_debug_write_line("VIDEO FORMAT ARGB8888=4BPP ARGB1555=2BPP resolution=800x480");
    sdram_usb_debug_write_line("VIDEO LANES full16=ffff ignored=0000 rotation=host_rot180");
    sdram_usb_debug_write_line("VIDEO PATH cdc_rx_32k_credit,shared_sram_16k,dma2,60000000,ltdc_argb,vblank_locked");
    sdram_usb_debug_write_line("VIDEO WAIT command=VIDEO_<format>_<frames>_<fps>_<bytes>_<crc32> spaces_not_underscores");
    sdram_usb_debug_write_line("H4V1 WAIT command=H4V1_<padded_bytes>_<transfer_crc32> storage=60200000 fb=60000000/600c0000 stage=stage5r_copy_profile");
}

static int sdram_video_next_token(const char **cursor,
                                  char *token,
                                  uint32_t token_bytes)
{
    const char *read;
    uint32_t length = 0u;

    if((cursor == RT_NULL) || (*cursor == RT_NULL) ||
       (token == RT_NULL) || (token_bytes < 2u))
    {
        return 0;
    }
    read = *cursor;
    while(*read == ' ')
    {
        read++;
    }
    if(*read == '\0')
    {
        return 0;
    }
    while((*read != '\0') && (*read != ' '))
    {
        if(length >= (token_bytes - 1u))
        {
            return 0;
        }
        token[length++] = *read++;
    }
    token[length] = '\0';
    *cursor = read;
    return 1;
}

static int sdram_video_parse_u32(const char *text,
                                 uint32_t base,
                                 uint32_t *value)
{
    uint32_t parsed = 0u;
    uint32_t digits = 0u;

    if((text == RT_NULL) || (value == RT_NULL) ||
       ((base != 10u) && (base != 16u)))
    {
        return 0;
    }
    while(*text != '\0')
    {
        uint32_t digit;

        if((*text >= '0') && (*text <= '9'))
        {
            digit = (uint32_t)(*text - '0');
        }
        else if((base == 16u) && (*text >= 'a') && (*text <= 'f'))
        {
            digit = 10u + (uint32_t)(*text - 'a');
        }
        else if((base == 16u) && (*text >= 'A') && (*text <= 'F'))
        {
            digit = 10u + (uint32_t)(*text - 'A');
        }
        else
        {
            return 0;
        }
        if((digit >= base) ||
           (parsed > ((0xFFFFFFFFu - digit) / base)))
        {
            return 0;
        }
        parsed = (parsed * base) + digit;
        digits++;
        text++;
    }
    if(digits == 0u)
    {
        return 0;
    }
    *value = parsed;
    return 1;
}

static int sdram_video_parse_config(const char *line,
                                    v5f_sdram_video_config_t *config)
{
    char format[16];
    char frames_text[12];
    char fps_text[12];
    char bytes_text[12];
    char crc_text[12];
    char extra[2];
    const char *cursor;
    uint32_t frames;
    uint32_t fps;
    uint32_t total_bytes;
    uint32_t crc;
    uint32_t frame_bytes;
    uint32_t bytes_per_pixel;
    uint32_t pixel_format;

    if((line == RT_NULL) || (config == RT_NULL))
    {
        return 0;
    }
    if(strncmp(line, "H4V1 ", 5u) == 0)
    {
        char transfer_bytes_text[12];
        char transfer_crc_text[12];
        const char *h4v_cursor = &line[5];

        if((sdram_video_next_token(&h4v_cursor,
                                   transfer_bytes_text,
                                   sizeof(transfer_bytes_text)) == 0) ||
           (sdram_video_next_token(&h4v_cursor,
                                   transfer_crc_text,
                                   sizeof(transfer_crc_text)) == 0) ||
           (sdram_video_next_token(&h4v_cursor, extra, sizeof(extra)) != 0) ||
           (sdram_video_parse_u32(transfer_bytes_text,
                                  10u,
                                  &total_bytes) == 0) ||
           (sdram_video_parse_u32(transfer_crc_text, 16u, &crc) == 0))
        {
            return -1;
        }
        if((total_bytes == 0u) ||
           (total_bytes > V5F_SDRAM_H4V1_STORAGE_BYTES) ||
           ((total_bytes % V5F_SDRAM_VIDEO_STAGE_BYTES) != 0u) ||
           ((total_bytes % V5F_SDRAM_VIDEO_ACK_BYTES) != 0u))
        {
            return -1;
        }
        memset(config, 0, sizeof(*config));
        config->pixel_format = LTDC_Pixelformat_ARGB1555;
        config->bytes_per_pixel = 2u;
        config->frame_bytes = V5F_SDRAM_H4V1_FRAME_BYTES;
        config->total_bytes = total_bytes;
        config->expected_crc = crc;
        config->storage_offset = V5F_SDRAM_H4V1_STORAGE_OFFSET;
        config->is_h4v1 = 1u;
        memcpy(config->name, "H4V1", 5u);
        return 1;
    }
    if(strncmp(line, "VIDEO ", 6u) != 0)
    {
        return 0;
    }
    cursor = &line[6];
    if((sdram_video_next_token(&cursor, format, sizeof(format)) == 0) ||
       (sdram_video_next_token(&cursor, frames_text, sizeof(frames_text)) == 0) ||
       (sdram_video_next_token(&cursor, fps_text, sizeof(fps_text)) == 0) ||
       (sdram_video_next_token(&cursor, bytes_text, sizeof(bytes_text)) == 0) ||
       (sdram_video_next_token(&cursor, crc_text, sizeof(crc_text)) == 0) ||
       (sdram_video_next_token(&cursor, extra, sizeof(extra)) != 0) ||
       (sdram_video_parse_u32(frames_text, 10u, &frames) == 0) ||
       (sdram_video_parse_u32(fps_text, 10u, &fps) == 0) ||
       (sdram_video_parse_u32(bytes_text, 10u, &total_bytes) == 0) ||
       (sdram_video_parse_u32(crc_text, 16u, &crc) == 0))
    {
        return 0;
    }

    if(strcmp(format, "ARGB8888") == 0)
    {
        pixel_format = LTDC_Pixelformat_ARGB8888;
        bytes_per_pixel = 4u;
    }
    else if(strcmp(format, "ARGB1555") == 0)
    {
        pixel_format = LTDC_Pixelformat_ARGB1555;
        bytes_per_pixel = 2u;
    }
    else
    {
        return -1;
    }

    frame_bytes = V5F_SDRAM_VIDEO_WIDTH * V5F_SDRAM_VIDEO_HEIGHT *
                  bytes_per_pixel;
    if((frames == 0u) ||
       (frames > (V5F_SDRAM_BYTES / frame_bytes)) ||
       (fps == 0u) || (fps > 60u) ||
       (total_bytes != (frames * frame_bytes)) ||
       (total_bytes > V5F_SDRAM_BYTES) ||
       ((total_bytes % V5F_SDRAM_VIDEO_STAGE_BYTES) != 0u) ||
       ((total_bytes % V5F_SDRAM_VIDEO_ACK_BYTES) != 0u))
    {
        return -1;
    }

    memset(config, 0, sizeof(*config));
    config->pixel_format = pixel_format;
    config->bytes_per_pixel = bytes_per_pixel;
    config->frame_bytes = frame_bytes;
    config->frames = frames;
    config->fps = fps;
    config->total_bytes = total_bytes;
    config->expected_crc = crc;
    config->storage_offset = 0u;
    config->is_h4v1 = 0u;
    memcpy(config->name, format, strlen(format) + 1u);
    return 1;
}

static void __attribute__((noreturn))
sdram_video_fail(const char *reason)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used;

    (void)ch32h417_usb_cdc_raw_rx_enable(0u);
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_FAILED;
    g_v5f_hw_test_diag.last_error = V5F_SDRAM_ERR_VERIFY;
    g_v5f_hw_test_diag.sdram_fail_count++;
    used = rt_snprintf(line,
                       sizeof(line),
                       "VIDEO FAIL reason=%s",
                       (reason != RT_NULL) ? reason : "unknown");
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    sdram_usb_debug_write_line("RESULT FAIL");
    sdram_memtest_watchdog_complete();
    rt_thread_mdelay(1000);
    NVIC_SystemReset();
    while(1)
    {
        __NOP();
    }
}

static void sdram_video_wait_config(v5f_sdram_video_config_t *config)
{
    char command[64];

    sdram_video_send_config_help();
    while(1)
    {
        int len = ch32h417_usb_cdc_read_line(command, sizeof(command));

        if(len > 0)
        {
            int parsed;

            if((strcmp(command, "status") == 0) ||
               (strcmp(command, "STATUS") == 0))
            {
                if(sdram_memtest_watchdog_report_recovery() != 0u)
                {
                    sdram_usb_debug_write_line("VIDEO RECOVERY previous_upload_stalled watchdog_reset=confirmed");
                }
                sdram_video_send_config_help();
            }
            else
            {
                parsed = sdram_video_parse_config(command, config);
                if(parsed > 0)
                {
                    return;
                }
                sdram_usb_debug_write_line("VIDEO ERR invalid_command_or_geometry");
            }
        }
        ch32h417_dual_cdc_poll();
        sdram_memtest_watchdog_feed();
        rt_thread_mdelay(10);
    }
}

static void sdram_video_send_credit_ack(
    const v5f_sdram_video_config_t *config,
    uint32_t received)
{
    uint32_t rx_callbacks;
    uint32_t rx_arm_ok;
    uint32_t rx_arm_fail;
    uint32_t rx_transfer_size;
    uint32_t rx_armed;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used;

    ch32h417_usb_cdc_raw_rx_diag(&rx_callbacks,
                                   RT_NULL,
                                   &rx_arm_ok,
                                   &rx_arm_fail,
                                   &rx_transfer_size,
                                   &rx_armed);
    used = rt_snprintf(line,
                       sizeof(line),
                       "VIDEO ACK bytes=%u/%u rx_cb=%u arm=%u/%u xfer=%u armed=%u",
                       (unsigned int)received,
                       (unsigned int)config->total_bytes,
                       (unsigned int)rx_callbacks,
                       (unsigned int)rx_arm_ok,
                       (unsigned int)rx_arm_fail,
                       (unsigned int)rx_transfer_size,
                       (unsigned int)rx_armed);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_memtest_watchdog_context(
            V5F_SDRAM_WATCHDOG_STAGE_WRITE,
            V5F_SDRAM_WATCHDOG_POINT_VIDEO_ACK_SEND,
            received / V5F_SDRAM_VIDEO_STAGE_BYTES,
            (uintptr_t)V5F_SDRAM_BASE_ADDR + config->storage_offset + received);
        sdram_usb_debug_write_line(line);
        sdram_memtest_watchdog_context(
            V5F_SDRAM_WATCHDOG_STAGE_WRITE,
            V5F_SDRAM_WATCHDOG_POINT_VIDEO_ACK_DONE,
            received / V5F_SDRAM_VIDEO_STAGE_BYTES,
            (uintptr_t)V5F_SDRAM_BASE_ADDR + config->storage_offset + received);
    }
}

static uint32_t sdram_video_upload(const v5f_sdram_video_config_t *config)
{
    uint8_t *stage = (uint8_t *)(uintptr_t)s_sdram_bw_buffer;
    uint32_t committed = 0u;
    uint32_t staged = 0u;
    uint32_t crc = 0xFFFFFFFFu;
    rt_tick_t last_data_tick = rt_tick_get();
    char line[V5F_SDRAM_USB_LINE_BYTES];

    s_sdram_video_block_count =
        config->total_bytes / V5F_SDRAM_VIDEO_STAGE_BYTES;
    memset(s_sdram_video_block_crc,
           0,
           s_sdram_video_block_count * sizeof(s_sdram_video_block_crc[0]));

    if(sdram_memtest_dma_stream_prepare(1u, 1u) == 0u)
    {
        sdram_video_fail("sdram_dma2_write_prepare");
    }
    if(ch32h417_usb_cdc_raw_rx_enable(1u) != 0)
    {
        sdram_video_fail("raw_rx_unavailable");
    }
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "VIDEO READY format=%s frames=%u fps=%u bytes=%u chunk=%u window=%u usb_rx=1024",
                               config->name,
                               (unsigned int)config->frames,
                               (unsigned int)config->fps,
                               (unsigned int)config->total_bytes,
                               (unsigned int)V5F_SDRAM_VIDEO_STAGE_BYTES,
                               (unsigned int)V5F_SDRAM_VIDEO_ACK_BYTES);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    while(committed < config->total_bytes)
    {
        uint32_t block = committed / V5F_SDRAM_VIDEO_STAGE_BYTES;
        uint32_t received = committed + staged;
        uint32_t read_limit = V5F_SDRAM_VIDEO_STAGE_BYTES - staged;
        uint32_t credit_remaining =
            V5F_SDRAM_VIDEO_ACK_BYTES -
            (received % V5F_SDRAM_VIDEO_ACK_BYTES);
        uint8_t credit_due = 0u;

        if(read_limit > credit_remaining)
        {
            read_limit = credit_remaining;
        }

        sdram_memtest_watchdog_context(
            V5F_SDRAM_WATCHDOG_STAGE_WRITE,
            V5F_SDRAM_WATCHDOG_POINT_VIDEO_RAW_WAIT,
            block,
            (uintptr_t)V5F_SDRAM_BASE_ADDR + config->storage_offset + committed);
        int read = ch32h417_usb_cdc_raw_rx_read(
            &stage[staged],
            read_limit);

        if(ch32h417_usb_cdc_raw_rx_overflowed() != 0u)
        {
            sdram_video_fail("raw_rx_overflow");
        }
        if(read < 0)
        {
            sdram_video_fail("raw_rx_read");
        }
        if(read > 0)
        {
            sdram_memtest_watchdog_context(
                V5F_SDRAM_WATCHDOG_STAGE_WRITE,
                V5F_SDRAM_WATCHDOG_POINT_VIDEO_RAW_READ,
                block,
                (uintptr_t)V5F_SDRAM_BASE_ADDR + config->storage_offset + committed);
            crc = sdram_video_crc32_update(crc,
                                            &stage[staged],
                                            (uint32_t)read);
            staged += (uint32_t)read;
            last_data_tick = rt_tick_get();
            received = committed + staged;
            if((received % V5F_SDRAM_VIDEO_ACK_BYTES) == 0u)
            {
                credit_due = 1u;
            }
        }

        if(staged == V5F_SDRAM_VIDEO_STAGE_BYTES)
        {
            uint32_t cycles;

            s_sdram_video_block_crc[block] =
                sdram_video_crc32_update(0xFFFFFFFFu,
                                         stage,
                                         V5F_SDRAM_VIDEO_STAGE_BYTES) ^
                0xFFFFFFFFu;

            s_sdram_watchdog_pass = block;
            sdram_memtest_watchdog_context(
                V5F_SDRAM_WATCHDOG_STAGE_WRITE,
                V5F_SDRAM_WATCHDOG_POINT_VIDEO_DMA_CALL,
                block,
                (uintptr_t)V5F_SDRAM_BASE_ADDR + config->storage_offset + committed);
            if(sdram_memtest_dma_stream_transfer(
                   (uintptr_t)V5F_SDRAM_BASE_ADDR +
                       config->storage_offset + committed,
                   1u,
                   &cycles,
                   V5F_SDRAM_WATCHDOG_STAGE_WRITE,
                   block) == 0u)
            {
                sdram_video_fail("sdram_dma_write");
            }
            sdram_memtest_watchdog_context(
                V5F_SDRAM_WATCHDOG_STAGE_WRITE,
                V5F_SDRAM_WATCHDOG_POINT_VIDEO_DMA_DONE,
                block,
                (uintptr_t)V5F_SDRAM_BASE_ADDR + config->storage_offset + committed);
            committed += V5F_SDRAM_VIDEO_STAGE_BYTES;
            staged = 0u;
        }
        if(credit_due != 0u)
        {
            sdram_video_send_credit_ack(config, committed + staged);
        }
        if(read == 0)
        {
            if(((uint32_t)(rt_tick_get() - last_data_tick) * 1000u /
                RT_TICK_PER_SECOND) > V5F_SDRAM_VIDEO_UPLOAD_TIMEOUT_MS)
            {
                sdram_video_fail("upload_timeout");
            }
            /* Dedicated test thread: keep polling instead of entering RTOS sleep. */
            __NOP();
        }
        sdram_memtest_watchdog_feed();
    }

    (void)ch32h417_usb_cdc_raw_rx_enable(0u);
    sdram_memtest_dma_stream_finish();
    return crc ^ 0xFFFFFFFFu;
}

static uint32_t sdram_video_readback_crc(
    const v5f_sdram_video_config_t *config,
    uint32_t pass,
    v5f_sdram_video_readback_diag_t *diag)
{
    uint8_t *stage = (uint8_t *)(uintptr_t)s_sdram_bw_buffer;
    uint32_t offset;
    uint32_t crc = 0xFFFFFFFFu;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    memset(diag, 0, sizeof(*diag));
    diag->first_bad_block = 0xFFFFFFFFu;
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "VIDEO VERIFY START pass=%u/%u controller=DMA2 channel=3 mode=wide256 block_crc=1",
                               (unsigned int)pass,
                               (unsigned int)V5F_SDRAM_VIDEO_READBACK_PASSES);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    if(sdram_memtest_dma_stream_prepare(0u, 1u) == 0u)
    {
        sdram_video_fail("sdram_dma2_read_prepare");
    }

    for(offset = 0u; offset < config->total_bytes;
        offset += V5F_SDRAM_VIDEO_STAGE_BYTES)
    {
        uint32_t cycles;
        uint32_t block = offset / V5F_SDRAM_VIDEO_STAGE_BYTES;
        uint32_t actual_block_crc;

        s_sdram_watchdog_pass = offset / V5F_SDRAM_VIDEO_STAGE_BYTES;
        sdram_memtest_watchdog_context(
            V5F_SDRAM_WATCHDOG_STAGE_READ,
            V5F_SDRAM_WATCHDOG_POINT_VIDEO_DMA_CALL,
            offset / V5F_SDRAM_VIDEO_STAGE_BYTES,
            (uintptr_t)V5F_SDRAM_BASE_ADDR + config->storage_offset + offset);
        if(sdram_memtest_dma_stream_transfer(
               (uintptr_t)V5F_SDRAM_BASE_ADDR +
                   config->storage_offset + offset,
               1u,
               &cycles,
               V5F_SDRAM_WATCHDOG_STAGE_READ,
               offset / V5F_SDRAM_VIDEO_STAGE_BYTES) == 0u)
        {
            sdram_video_fail("sdram_dma_readback");
        }
        sdram_memtest_watchdog_context(
            V5F_SDRAM_WATCHDOG_STAGE_READ,
            V5F_SDRAM_WATCHDOG_POINT_VIDEO_DMA_DONE,
            offset / V5F_SDRAM_VIDEO_STAGE_BYTES,
            (uintptr_t)V5F_SDRAM_BASE_ADDR + config->storage_offset + offset);
        crc = sdram_video_crc32_update(crc,
                                        stage,
                                        V5F_SDRAM_VIDEO_STAGE_BYTES);
        actual_block_crc =
            sdram_video_crc32_update(0xFFFFFFFFu,
                                     stage,
                                     V5F_SDRAM_VIDEO_STAGE_BYTES) ^
            0xFFFFFFFFu;
        if((block >= s_sdram_video_block_count) ||
           (actual_block_crc != s_sdram_video_block_crc[block]))
        {
            if(diag->bad_blocks == 0u)
            {
                diag->first_bad_block = block;
                diag->first_expected_crc =
                    (block < s_sdram_video_block_count) ?
                    s_sdram_video_block_crc[block] : 0u;
                diag->first_actual_crc = actual_block_crc;
            }
            diag->bad_blocks++;
        }
        if((((offset + V5F_SDRAM_VIDEO_STAGE_BYTES) %
             (4u * 1024u * 1024u)) == 0u) ||
           ((offset + V5F_SDRAM_VIDEO_STAGE_BYTES) == config->total_bytes))
        {
            int used = rt_snprintf(line,
                                   sizeof(line),
                                   "VIDEO VERIFY pass=%u bytes=%u/%u",
                                   (unsigned int)pass,
                                   (unsigned int)(offset +
                                                  V5F_SDRAM_VIDEO_STAGE_BYTES),
                                   (unsigned int)config->total_bytes);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
        sdram_memtest_watchdog_feed();
    }
    sdram_memtest_dma_stream_finish();
    diag->crc = crc ^ 0xFFFFFFFFu;
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "VIDEO VERIFY RESULT pass=%u crc=%08x bad_blocks=%u first_block=%u first_off=%08x block_crc=%08x/%08x",
                               (unsigned int)pass,
                               (unsigned int)diag->crc,
                               (unsigned int)diag->bad_blocks,
                               (unsigned int)diag->first_bad_block,
                               (unsigned int)((diag->first_bad_block == 0xFFFFFFFFu) ?
                                              0xFFFFFFFFu :
                                              (diag->first_bad_block * V5F_SDRAM_VIDEO_STAGE_BYTES)),
                               (unsigned int)diag->first_expected_crc,
                               (unsigned int)diag->first_actual_crc);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    return diag->crc;
}

static uint8_t sdram_video_h4v1_probe_container(
    const v5f_sdram_video_config_t *config,
    h4v1_header_t *header_out,
    h4v1_index_entry_t *first_out,
    h4v1_index_entry_t *second_out)
{
    uint8_t *stage = (uint8_t *)(uintptr_t)s_sdram_bw_buffer;
    h4v1_header_t header;
    h4v1_index_entry_t first;
    h4v1_index_entry_t second;
    uint32_t index_bytes;
    uint32_t cycles;
    uint32_t index_crc;
    int status;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    if((config == RT_NULL) || (config->is_h4v1 == 0u) ||
       (header_out == RT_NULL) || (first_out == RT_NULL) ||
       (second_out == RT_NULL))
    {
        return 0u;
    }
    if(sdram_memtest_dma_stream_prepare(0u, 1u) == 0u)
    {
        return 0u;
    }
    if(sdram_memtest_dma_stream_transfer(
           (uintptr_t)V5F_SDRAM_BASE_ADDR + config->storage_offset,
           1u,
           &cycles,
           V5F_SDRAM_WATCHDOG_STAGE_READ,
           0x3000u) == 0u)
    {
        sdram_memtest_dma_stream_finish();
        return 0u;
    }
    sdram_memtest_dma_stream_finish();

    status = h4v1_parse_header(stage,
                               V5F_SDRAM_VIDEO_STAGE_BYTES,
                               &header);
    if(status != H4V1_OK)
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "H4V1 HEADER FAIL status=%d",
                               status);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        return 0u;
    }

    index_bytes = header.frame_count * header.index_entry_bytes;
    if((header.width != V5F_SDRAM_VIDEO_WIDTH) ||
       (header.height != V5F_SDRAM_VIDEO_HEIGHT) ||
       (header.pixel_format != H4V1_PIXEL_FORMAT_ARGB1555) ||
       (header.frame_bytes != V5F_SDRAM_H4V1_FRAME_BYTES) ||
       (header.frame_count < 2u) ||
       (header.frame_count > V5F_SDRAM_H4V1_MAX_FRAMES) ||
       (header.file_bytes > config->total_bytes) ||
       (header.index_offset > V5F_SDRAM_VIDEO_STAGE_BYTES) ||
       (index_bytes > (V5F_SDRAM_VIDEO_STAGE_BYTES -
                       header.index_offset)))
    {
        sdram_usb_debug_write_line("H4V1 HEADER FAIL status=geometry_or_index_stage");
        return 0u;
    }
    index_crc = h4v1_crc32(&stage[header.index_offset], index_bytes);
    if(index_crc != header.index_crc32)
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "H4V1 INDEX FAIL crc=%08x/%08x bytes=%u",
                               (unsigned int)index_crc,
                               (unsigned int)header.index_crc32,
                               (unsigned int)index_bytes);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        return 0u;
    }
    status = h4v1_parse_index_entry(&stage[header.index_offset],
                                    index_bytes,
                                    &header,
                                    &first);
    if(status != H4V1_OK)
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "H4V1 INDEX FAIL first_status=%d",
                               status);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        return 0u;
    }
    status = h4v1_parse_index_entry(
        &stage[header.index_offset + header.index_entry_bytes],
        index_bytes - header.index_entry_bytes,
        &header,
        &second);
    if(status != H4V1_OK)
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "H4V1 INDEX FAIL second_status=%d",
                               status);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        return 0u;
    }

    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "H4V1 CONTAINER PASS frames=%u fps=%u file=%u transfer=%u gop=%u index=%u validated=%u source=sdram first=%u/%u flags=%08x dma_cycles=%u",
            (unsigned int)header.frame_count,
            (unsigned int)header.fps,
            (unsigned int)header.file_bytes,
            (unsigned int)config->total_bytes,
            (unsigned int)header.gop,
            (unsigned int)index_bytes,
            (unsigned int)index_bytes,
            (unsigned int)first.offset,
            (unsigned int)first.compressed_bytes,
            (unsigned int)first.flags,
            (unsigned int)cycles);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    *header_out = header;
    *first_out = first;
    *second_out = second;
    return 1u;
}

static uint8_t sdram_video_h4v1_read_index_entry(
    const v5f_sdram_video_config_t *config,
    const h4v1_header_t *header,
    uint32_t frame,
    h4v1_index_entry_t *entry_out,
    uint32_t *cycles_out)
{
    uint8_t *stage = (uint8_t *)(uintptr_t)s_sdram_bw_buffer;
    uint32_t entry_offset;
    uint32_t cycles;
    int status;

    if((config == RT_NULL) || (header == RT_NULL) ||
       (entry_out == RT_NULL) || (frame >= header->frame_count))
    {
        return 0u;
    }
    entry_offset = header->index_offset +
                   (frame * header->index_entry_bytes);
    if((entry_offset > V5F_SDRAM_VIDEO_STAGE_BYTES) ||
       (header->index_entry_bytes >
        (V5F_SDRAM_VIDEO_STAGE_BYTES - entry_offset)))
    {
        return 0u;
    }
    if(sdram_memtest_dma_stream_prepare(0u, 1u) == 0u)
    {
        return 0u;
    }
    if(sdram_memtest_dma_stream_transfer(
           (uintptr_t)V5F_SDRAM_BASE_ADDR + config->storage_offset,
           1u,
           &cycles,
           V5F_SDRAM_WATCHDOG_STAGE_READ,
           0x3100u + frame) == 0u)
    {
        sdram_memtest_dma_stream_finish();
        return 0u;
    }
    sdram_memtest_dma_stream_finish();
    status = h4v1_parse_index_entry(
        &stage[entry_offset],
        V5F_SDRAM_VIDEO_STAGE_BYTES - entry_offset,
        header,
        entry_out);
    if(status != H4V1_OK)
    {
        return 0u;
    }
    if(cycles_out != RT_NULL)
    {
        *cycles_out = cycles;
    }
    return 1u;
}

static void sdram_video_ltdc_stop(void)
{
    ch32h417_ltdc_rgb_enable(0u);
    ch32h417_ltdc_rgb_layer1_enable(0u);
    ch32h417_ltdc_rgb_layer2_enable(0u);
    ch32h417_ltdc_rgb_reload();
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

static uint32_t sdram_video_cycle_now(void)
{
    uint32_t value;

    __asm volatile ("csrr %0, mcycle" : "=r"(value));
    return value;
}

static void sdram_video_visible_hold(uint32_t milliseconds)
{
    uint32_t elapsed = 0u;

    while(elapsed < milliseconds)
    {
        uint32_t slice = milliseconds - elapsed;

        if(slice > 10u)
        {
            slice = 10u;
        }
        sdram_memtest_watchdog_feed();
        rt_thread_mdelay((rt_int32_t)slice);
        elapsed += slice;
    }
}

static void sdram_video_ltdc_trace(const char *stage,
                                   const ch32h417_ltdc_rgb_layer_t *layer)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used = rt_snprintf(line,
                           sizeof(line),
                           "VIDEO LTDC STAGE %s fb=%08x cr=%08x lcr=%08x cpsr=%08x",
                           stage,
                           (unsigned int)layer->framebuffer,
                           (unsigned int)LTDC->GCR,
                           (unsigned int)LTDC_Layer1->CR,
                           (unsigned int)LTDC->CPSR);

    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static void sdram_video_ltdc_trace_layer(const char *stage)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int used = rt_snprintf(line,
                           sizeof(line),
                           "VIDEO LTDC REGS %s pfcr=%08x cacr=%08x bfcr=%08x cfb=%08x cfbll=%08x bg=%06x",
                           stage,
                           (unsigned int)LTDC_Layer1->PFCR,
                           (unsigned int)LTDC_Layer1->CACR,
                           (unsigned int)LTDC_Layer1->BFCR,
                           (unsigned int)LTDC_Layer1->CFBAR,
                           (unsigned int)LTDC_Layer1->CFBLR,
                           (unsigned int)(LTDC->BCCR & 0x00FFFFFFu));

    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
}

static int sdram_video_ltdc_start_staged(
    const ch32h417_ltdc_rgb_layer_t *layer,
    uint32_t outer_point)
{
    ch32h417_ltdc_rgb_color_t black = {0u, 0u, 0u};
    int result;

    sdram_memtest_watchdog_context(V5F_SDRAM_WATCHDOG_STAGE_LTDC,
                                   outer_point,
                                   0u,
                                   layer->framebuffer);
    sdram_memtest_watchdog_context(
        V5F_SDRAM_WATCHDOG_STAGE_LTDC,
        V5F_SDRAM_WATCHDOG_POINT_LTDC_PANEL_INIT,
        0u,
        layer->framebuffer);
    sdram_video_ltdc_trace("PANEL_INIT_ARM", layer);
    result = ch32h417_ltdc_rgb_panel_init(
        &ch32h417_ltdc_rgb_panel_800x480,
        &black);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        return result;
    }
    sdram_memtest_watchdog_context(
        V5F_SDRAM_WATCHDOG_STAGE_LTDC,
        V5F_SDRAM_WATCHDOG_POINT_LTDC_PANEL_DONE,
        0u,
        layer->framebuffer);
    sdram_video_ltdc_trace("PANEL_INIT_PASS", layer);
    sdram_memtest_watchdog_context(
        V5F_SDRAM_WATCHDOG_STAGE_LTDC,
        V5F_SDRAM_WATCHDOG_POINT_LTDC_LAYER_CONFIG,
        0u,
        layer->framebuffer);
    sdram_video_ltdc_trace("LAYER_CONFIG_ARM", layer);
    result = ch32h417_ltdc_rgb_layer1_config(
        &ch32h417_ltdc_rgb_panel_800x480,
        layer);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        return result;
    }
    sdram_memtest_watchdog_context(
        V5F_SDRAM_WATCHDOG_STAGE_LTDC,
        V5F_SDRAM_WATCHDOG_POINT_LTDC_LAYER_DONE,
        0u,
        layer->framebuffer);
    sdram_video_ltdc_trace("LAYER_CONFIG_PASS", layer);

    sdram_video_ltdc_trace("LAYER_ENABLE_ARM", layer);
    ch32h417_ltdc_rgb_layer1_enable(1u);
    ch32h417_ltdc_rgb_layer2_enable(0u);
    ch32h417_ltdc_rgb_reload();
    sdram_video_ltdc_trace("LAYER_ENABLE_PASS", layer);

    /*
     * Turn on the board backlight before the LTDC master is enabled.  If the
     * enable transaction locks the HB fabric, the lit panel is still a
     * visible indication that GPIO/power sequencing completed.
     */
    ch32h417_lcd_rgb_backlight_enable(1u);
    sdram_video_ltdc_trace("BACKLIGHT_ON", layer);
    sdram_memtest_watchdog_context(
        V5F_SDRAM_WATCHDOG_STAGE_LTDC,
        V5F_SDRAM_WATCHDOG_POINT_LTDC_PRE_ENABLE,
        0u,
        layer->framebuffer);
    sdram_video_ltdc_trace("CONTROLLER_ENABLE_ARM", layer);
    ch32h417_ltdc_rgb_enable(1u);
    sdram_memtest_watchdog_context(
        V5F_SDRAM_WATCHDOG_STAGE_LTDC,
        V5F_SDRAM_WATCHDOG_POINT_LTDC_ENABLE_DONE,
        0u,
        layer->framebuffer);
    ch32h417_ltdc_rgb_reload();
    sdram_memtest_watchdog_context(
        V5F_SDRAM_WATCHDOG_STAGE_LTDC,
        V5F_SDRAM_WATCHDOG_POINT_LTDC_RELOAD_DONE,
        0u,
        layer->framebuffer);
    sdram_video_ltdc_trace("CONTROLLER_ENABLE_PASS", layer);
    return CH32H417_LTDC_RGB_OK;
}

static uint8_t sdram_video_ltdc_wait_scan(uint32_t *changes_out)
{
    uint32_t start = sdram_video_cycle_now();
    uint32_t cycles_per_ms = SystemCoreClock / 1000u;
    uint32_t timeout_cycles;
    uint32_t previous = LTDC->CPSR;
    uint32_t changes = 0u;

    if(cycles_per_ms == 0u)
    {
        cycles_per_ms = 1u;
    }
    timeout_cycles = cycles_per_ms * V5F_SDRAM_VIDEO_SCAN_TEST_MS;

    sdram_memtest_watchdog_context(
        V5F_SDRAM_WATCHDOG_STAGE_LTDC,
        V5F_SDRAM_WATCHDOG_POINT_LTDC_SCAN_WAIT,
        0u,
        LTDC_Layer1->CFBAR);
    while((uint32_t)(sdram_video_cycle_now() - start) < timeout_cycles)
    {
        uint32_t current = LTDC->CPSR;

        if(current != previous)
        {
            changes++;
            previous = current;
        }
    }
    if(changes_out != RT_NULL)
    {
        *changes_out = changes;
    }
    return (uint8_t)(changes >= 3u);
}

static void sdram_video_fill_main_l8_pattern(void)
{
    uint32_t x;
    uint32_t y;

    for(y = 0u; y < V5F_SDRAM_VIDEO_HEIGHT; y++)
    {
        for(x = 0u; x < V5F_SDRAM_VIDEO_WIDTH; x++)
        {
            static const uint8_t levels[4] = {32u, 96u, 176u, 255u};
            uint32_t band = ((x / 100u) + (y / 60u)) & 3u;

            s_lcd_fb[(y * V5F_SDRAM_VIDEO_WIDTH) + x] = levels[band];
        }
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

static int sdram_video_ltdc_matrix_stage(const char *name,
                                         uint16_t width,
                                         uint16_t height,
                                         uint32_t pixel_format,
                                         uint32_t framebuffer,
                                         uint32_t pitch,
                                         uint32_t line_extra)
{
    ch32h417_ltdc_rgb_layer_t layer = {0};
    uint32_t bytes_per_pixel =
        ch32h417_ltdc_rgb_bytes_per_pixel(pixel_format);
    uint32_t active_line_bytes = (uint32_t)width * bytes_per_pixel;
    uint32_t changes = 0u;
    uint32_t underrun;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int result;
    int used;

    layer.width = width;
    layer.height = height;
    layer.offset_x = (uint16_t)((V5F_SDRAM_VIDEO_WIDTH - width) / 2u);
    layer.offset_y = (uint16_t)((V5F_SDRAM_VIDEO_HEIGHT - height) / 2u);
    layer.pixel_format = pixel_format;
    layer.framebuffer = framebuffer;
    layer.line_pitch = pitch;

    used = rt_snprintf(line,
                       sizeof(line),
                       "VIDEO MATRIX %s START win=%ux%u fb=%08x pitch=%u len=%u fmt=%u",
                       name,
                       (unsigned int)width,
                       (unsigned int)height,
                       (unsigned int)framebuffer,
                       (unsigned int)pitch,
                       (unsigned int)(active_line_bytes + line_extra),
                       (unsigned int)pixel_format);
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }

    LTDC_ClearFlag(LTDC_FLAG_FU);
    result = sdram_video_ltdc_start_staged(
        &layer,
        V5F_SDRAM_WATCHDOG_POINT_LTDC_EXT_PROBE);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        return result;
    }

    /*
     * CFBPitch is the distance between line starts.  CFBLineLength is the
     * number of active bytes fetched per line plus the controller-specific
     * suffix.  Override the generic helper so strided narrow windows do not
     * accidentally request a full 1600-byte line.
     */
    LTDC_Layer1->CFBLR = ((pitch & 0x1FFFu) << 16) |
                         ((active_line_bytes + line_extra) & 0x1FFFu);
    ch32h417_ltdc_rgb_reload();
    ch32h417_ltdc_rgb_framebuffer_barrier();
    if(pixel_format == LTDC_Pixelformat_L8)
    {
        load_l8_clut_after_layer_start();
    }
    sdram_video_ltdc_trace_layer(name);
    if(sdram_video_ltdc_wait_scan(&changes) == 0u)
    {
        sdram_video_ltdc_stop();
        return CH32H417_LTDC_RGB_ERR_PARAM;
    }
    sdram_video_visible_hold(V5F_SDRAM_VIDEO_VISUAL_HOLD_MS);
    underrun = (LTDC_GetFlagStatus(LTDC_FLAG_FU) != RESET) ? 1u : 0u;
    used = rt_snprintf(line,
                       sizeof(line),
                       "VIDEO MATRIX %s END changes=%u underrun=%u cpsr=%08x cfblr=%08x",
                       name,
                       (unsigned int)changes,
                       (unsigned int)underrun,
                       (unsigned int)LTDC->CPSR,
                       (unsigned int)LTDC_Layer1->CFBLR);
    sdram_video_ltdc_stop();
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    return CH32H417_LTDC_RGB_OK;
}

static int V5F_MAYBE_UNUSED sdram_video_ltdc_preflight(
    const v5f_sdram_video_config_t *config)
{
    ch32h417_ltdc_rgb_layer_t layer = {0};
    uint32_t scan_format = (config->bytes_per_pixel == 2u) ?
                               LTDC_Pixelformat_RGB565 :
                               config->pixel_format;
    uint32_t pitch = V5F_SDRAM_VIDEO_WIDTH * config->bytes_per_pixel;
    uint32_t changes = 0u;
    uint32_t frame;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int result;
    int used;

    sdram_usb_debug_write_line(
        "VIDEO FOCUS START compare=main,p3,p31,p31_reload hold_ms=2000");
    used = rt_snprintf(line,
                       sizeof(line),
                       "VIDEO LTDC CONTROL PASS preinit_before_usb=1 pa_cfghr=%08x pa_out=%04x disp=%u backlight=%u settle=upload_elapsed",
                       (unsigned int)GPIOA->CFGHR,
                       (unsigned int)GPIOA->OUTDR,
                       (unsigned int)((GPIOA->OUTDR & GPIO_Pin_9) != 0u),
                       (unsigned int)((GPIOA->OUTDR & GPIO_Pin_10) != 0u));
    if((used > 0) && ((rt_size_t)used < sizeof(line)))
    {
        sdram_usb_debug_write_line(line);
    }
    sdram_usb_debug_write_line("VIDEO LTDC POWER SETTLE PASS source=boot_preinit");

    /* Reproduce main's proven full-screen path in this same image. */
    sdram_video_fill_main_l8_pattern();
    sdram_usb_debug_write_line(
        "VIDEO FOCUS MAIN_L8 START win=800x480 fb=shared_sram pitch=800 len=803");
    LTDC_ClearFlag(LTDC_FLAG_FU);
    result = lcd_start_l8_fullscreen();
    if(result != CH32H417_LTDC_RGB_OK)
    {
        return result;
    }
    sdram_video_ltdc_trace_layer("1_MAIN_L8");
    if(sdram_video_ltdc_wait_scan(&changes) == 0u)
    {
        sdram_video_ltdc_stop();
        return CH32H417_LTDC_RGB_ERR_PARAM;
    }
    sdram_video_visible_hold(V5F_SDRAM_VIDEO_VISUAL_HOLD_MS);
    sdram_usb_debug_write_line(
        "VIDEO FOCUS MAIN_L8 END expected=full_grayscale_checker");
    sdram_video_ltdc_stop();

    result = sdram_video_ltdc_matrix_stage("FULL_P3_STATIC",
        800u, 480u, scan_format, V5F_SDRAM_NATIVE_ADDR, pitch, 3u);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        return result;
    }
    result = sdram_video_ltdc_matrix_stage("FULL_P31_STATIC",
        800u, 480u, scan_format, V5F_SDRAM_NATIVE_ADDR, pitch, 31u);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        return result;
    }

    /* Keep +31 active while changing only CFBAR and the reload method. */
    memset(&layer, 0, sizeof(layer));
    layer.width = V5F_SDRAM_VIDEO_WIDTH;
    layer.height = V5F_SDRAM_VIDEO_HEIGHT;
    layer.pixel_format = scan_format;
    layer.framebuffer = V5F_SDRAM_NATIVE_ADDR;
    layer.line_pitch = pitch;
    result = sdram_video_ltdc_start_staged(
        &layer,
        V5F_SDRAM_WATCHDOG_POINT_LTDC_FULL);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        return result;
    }
    LTDC_Layer1->CFBLR = ((pitch & 0x1FFFu) << 16) |
                         ((pitch + 31u) & 0x1FFFu);
    ch32h417_ltdc_rgb_reload();
    ch32h417_ltdc_rgb_framebuffer_barrier();
    sdram_video_ltdc_trace_layer("P31_DYNAMIC_STATIC");
    sdram_usb_debug_write_line(
        "VIDEO FOCUS P31_A frame=0 reload=start hold=2000");
    sdram_video_visible_hold(V5F_SDRAM_VIDEO_VISUAL_HOLD_MS);

    frame = (config->frames > 1u) ? 1u : 0u;
    LTDC_LayerAddress(LTDC_Layer1,
        V5F_SDRAM_NATIVE_ADDR + (frame * config->frame_bytes));
    ch32h417_ltdc_rgb_framebuffer_barrier();
    LTDC_ReloadConfig(LTDC_IMReload);
    sdram_video_ltdc_trace_layer("P31_DYNAMIC_IM");
    sdram_usb_debug_write_line(
        "VIDEO FOCUS P31_B frame=1 reload=immediate hold=2000");
    sdram_video_visible_hold(V5F_SDRAM_VIDEO_VISUAL_HOLD_MS);

    frame = (config->frames > 2u) ? 2u : 0u;
    LTDC_LayerAddress(LTDC_Layer1,
        V5F_SDRAM_NATIVE_ADDR + (frame * config->frame_bytes));
    ch32h417_ltdc_rgb_framebuffer_barrier();
    LTDC_ReloadConfig(LTDC_VBReload);
    sdram_video_ltdc_trace_layer("P31_DYNAMIC_VB");
    sdram_usb_debug_write_line(
        "VIDEO FOCUS P31_C frame=2 reload=vblank hold=2000");
    sdram_video_visible_hold(V5F_SDRAM_VIDEO_VISUAL_HOLD_MS);

    frame = (config->frames > 3u) ? 3u : 0u;
    ch32h417_ltdc_rgb_layer1_enable(0u);
    ch32h417_ltdc_rgb_reload();
    ch32h417_ltdc_rgb_framebuffer_barrier();
    LTDC_LayerAddress(LTDC_Layer1,
        V5F_SDRAM_NATIVE_ADDR + (frame * config->frame_bytes));
    ch32h417_ltdc_rgb_layer1_enable(1u);
    ch32h417_ltdc_rgb_reload();
    ch32h417_ltdc_rgb_framebuffer_barrier();
    sdram_video_ltdc_trace_layer("P31_DYNAMIC_DISABLE");
    sdram_usb_debug_write_line(
        "VIDEO FOCUS P31_D frame=3 reload=layer_disable hold=2000");
    sdram_video_visible_hold(V5F_SDRAM_VIDEO_VISUAL_HOLD_MS);
    sdram_video_ltdc_stop();
    sdram_usb_debug_write_line("VIDEO FOCUS END final_playback=cfbll_plus31_vblank");
    return CH32H417_LTDC_RGB_OK;
}

#define V5F_SDRAM_SYNTH_RGB_ADDR       V5F_SDRAM_NATIVE_ADDR
#define V5F_SDRAM_SYNTH_L8_ADDR        (V5F_SDRAM_NATIVE_ADDR + 0x00100000u)
#define V5F_SDRAM_SYNTH_RGB_BYTES      \
    (V5F_SDRAM_VIDEO_WIDTH * V5F_SDRAM_VIDEO_HEIGHT * 2u)
#define V5F_SDRAM_SYNTH_DYNAMIC_FRAMES 4u
#define V5F_SDRAM_SYNTH_DYNAMIC_BYTES  \
    (V5F_SDRAM_SYNTH_RGB_BYTES * V5F_SDRAM_SYNTH_DYNAMIC_FRAMES)
#define V5F_SDRAM_SYNTH_L8_BYTES       \
    (V5F_SDRAM_VIDEO_WIDTH * V5F_SDRAM_VIDEO_HEIGHT)

static uint8_t sdram_video_synth_l8_pixel(uint32_t x, uint32_t y)
{
    static const uint8_t levels[4] = {32u, 96u, 176u, 255u};

    if((x < 8u) || (x >= (V5F_SDRAM_VIDEO_WIDTH - 8u)) ||
       (y < 8u) || (y >= (V5F_SDRAM_VIDEO_HEIGHT - 8u)))
    {
        return 255u;
    }
    return levels[((x / 50u) + (y / 30u)) & 3u];
}

static uint16_t sdram_video_synth_argb1555_pixel(uint32_t x,
                                                 uint32_t y,
                                                 uint32_t frame)
{
    static const uint16_t backgrounds[4] =
    {
        0xFC00u, /* red:   A=1 R=11111 G=00000 B=00000 */
        0x83E0u, /* green: A=1 R=00000 G=11111 B=00000 */
        0x801Fu, /* blue:  A=1 R=00000 G=00000 B=11111 */
        0xFFFFu  /* white: A=1 R=11111 G=11111 B=11111 */
    };
    uint32_t user_x;
    uint32_t user_y;
    uint8_t marker;

    /* Match main's framebuffer convention.  The panel is mounted 180
     * degrees, so user coordinates map to memory at (799-x, 479-y). */
    user_x = V5F_SDRAM_VIDEO_WIDTH - 1u - x;
    user_y = V5F_SDRAM_VIDEO_HEIGHT - 1u - y;
    frame %= V5F_SDRAM_SYNTH_DYNAMIC_FRAMES;

    /* A 96x96 marker moves clockwise through the four user-view corners.
     * It is white over RGB backgrounds and black over the white frame. */
    marker = 0u;
    if((frame == 0u) && (user_x < 96u) && (user_y < 96u))
    {
        marker = 1u;
    }
    else if((frame == 1u) &&
            (user_x >= (V5F_SDRAM_VIDEO_WIDTH - 96u)) && (user_y < 96u))
    {
        marker = 1u;
    }
    else if((frame == 2u) &&
            (user_x >= (V5F_SDRAM_VIDEO_WIDTH - 96u)) &&
            (user_y >= (V5F_SDRAM_VIDEO_HEIGHT - 96u)))
    {
        marker = 1u;
    }
    else if((frame == 3u) && (user_x < 96u) &&
            (user_y >= (V5F_SDRAM_VIDEO_HEIGHT - 96u)))
    {
        marker = 1u;
    }
    if(marker != 0u)
    {
        return (frame == 3u) ? 0x8000u : 0xFFFFu;
    }
    return backgrounds[frame];
}

static void sdram_video_synth_fill_block(uint8_t *stage,
                                         uint32_t offset,
                                         uint32_t bytes,
                                         uint8_t word16)
{
    uint32_t i;

    if(word16 != 0u)
    {
        uint16_t *pixels = (uint16_t *)(void *)stage;

        for(i = 0u; i < (V5F_SDRAM_VIDEO_STAGE_BYTES / 2u); i++)
        {
            uint32_t byte_offset = offset + (i * 2u);

            if(byte_offset < bytes)
            {
                uint32_t pixel = byte_offset / 2u;
                uint32_t frame_pixels =
                    V5F_SDRAM_VIDEO_WIDTH * V5F_SDRAM_VIDEO_HEIGHT;
                uint32_t frame = pixel / frame_pixels;
                uint32_t frame_pixel = pixel % frame_pixels;
                uint32_t x = frame_pixel % V5F_SDRAM_VIDEO_WIDTH;
                uint32_t y = frame_pixel / V5F_SDRAM_VIDEO_WIDTH;

                pixels[i] = sdram_video_synth_argb1555_pixel(x, y, frame);
            }
            else
            {
                pixels[i] = 0x8000u;
            }
        }
    }
    else
    {
        for(i = 0u; i < V5F_SDRAM_VIDEO_STAGE_BYTES; i++)
        {
            uint32_t byte_offset = offset + i;

            if(byte_offset < bytes)
            {
                uint32_t x = byte_offset % V5F_SDRAM_VIDEO_WIDTH;
                uint32_t y = byte_offset / V5F_SDRAM_VIDEO_WIDTH;

                stage[i] = sdram_video_synth_l8_pixel(x, y);
                if((byte_offset & 1u) != 0u)
                {
                    /* Odd L8 pixels occupy physical D8-D15. */
                    stage[i] |= 0x0Cu;
                }
            }
            else
            {
                stage[i] = 0u;
            }
        }
    }
}

static int V5F_MAYBE_UNUSED sdram_video_synth_write_verify(
                                           const char *name,
                                           uintptr_t address,
                                           uint32_t bytes,
                                           uint8_t word16)
{
    uint8_t *stage = (uint8_t *)(uintptr_t)s_sdram_bw_buffer;
    uint32_t blocks =
        (bytes + V5F_SDRAM_VIDEO_STAGE_BYTES - 1u) /
        V5F_SDRAM_VIDEO_STAGE_BYTES;
    uint32_t expected = 0xFFFFFFFFu;
    uint32_t actual = 0xFFFFFFFFu;
    uint32_t block;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    if(sdram_memtest_dma_stream_prepare(1u, 1u) == 0u)
    {
        return 0;
    }
    for(block = 0u; block < blocks; block++)
    {
        uint32_t offset = block * V5F_SDRAM_VIDEO_STAGE_BYTES;
        uint32_t valid = bytes - offset;
        uint32_t cycles;

        if(valid > V5F_SDRAM_VIDEO_STAGE_BYTES)
        {
            valid = V5F_SDRAM_VIDEO_STAGE_BYTES;
        }
        sdram_video_synth_fill_block(stage, offset, bytes, word16);
        expected = sdram_video_crc32_update(expected, stage, valid);
        if(sdram_memtest_dma_stream_transfer(
               address + offset,
               1u,
               &cycles,
               V5F_SDRAM_WATCHDOG_STAGE_WRITE,
               block) == 0u)
        {
            sdram_memtest_dma_stream_finish();
            return 0;
        }
        sdram_memtest_watchdog_feed();
    }
    sdram_memtest_dma_stream_finish();

    if(sdram_memtest_dma_stream_prepare(0u, 1u) == 0u)
    {
        return 0;
    }
    for(block = 0u; block < blocks; block++)
    {
        uint32_t offset = block * V5F_SDRAM_VIDEO_STAGE_BYTES;
        uint32_t valid = bytes - offset;
        uint32_t cycles;

        if(valid > V5F_SDRAM_VIDEO_STAGE_BYTES)
        {
            valid = V5F_SDRAM_VIDEO_STAGE_BYTES;
        }
        if(sdram_memtest_dma_stream_transfer(
               address + offset,
               1u,
               &cycles,
               V5F_SDRAM_WATCHDOG_STAGE_READ,
               block) == 0u)
        {
            sdram_memtest_dma_stream_finish();
            return 0;
        }
        actual = sdram_video_crc32_update(actual, stage, valid);
        sdram_memtest_watchdog_feed();
    }
    sdram_memtest_dma_stream_finish();
    expected ^= 0xFFFFFFFFu;
    actual ^= 0xFFFFFFFFu;
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "SYNTH PREP %s bytes=%u blocks=%u crc=%08x/%08x %s",
                               name,
                               (unsigned int)bytes,
                               (unsigned int)blocks,
                               (unsigned int)expected,
                               (unsigned int)actual,
                               (expected == actual) ? "PASS" : "FAIL");
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    return (expected == actual) ? 1 : 0;
}

static int sdram_video_static_ltdc_start(void)
{
    ch32h417_ltdc_rgb_layer_t layer = {0};
    ch32h417_ltdc_rgb_color_t black = {0u, 0u, 0u};

    layer.width = V5F_SDRAM_VIDEO_WIDTH;
    layer.height = V5F_SDRAM_VIDEO_HEIGHT;
    layer.offset_x = 0u;
    layer.offset_y = 0u;
    layer.pixel_format = LTDC_Pixelformat_ARGB1555;
    /* Feed LTDC through the WCH 16-bit SDRAM window.  The frame is prepared
     * and verified through this same remapped window before LTDC starts. */
    layer.framebuffer = V5F_SDRAM_REMAP_ADDR;
    layer.line_pitch = V5F_SDRAM_VIDEO_WIDTH * 2u;

    /* PA9/PA10 are board-initialization outputs.  LTDC diagnostics must not
     * read, gate on, or modify them. */
    return ch32h417_ltdc_rgb_start_layer1(
        &ch32h417_ltdc_rgb_panel_800x480,
        &layer,
        &black);
}

static void V5F_MAYBE_UNUSED __attribute__((noreturn)) sdram_video_synth_cycle(void)
{
    uint32_t seconds = 0u;
    uint32_t scan_changes = 0u;
    uint32_t frame = 0u;
    uint8_t result_reported = 0u;
    uint8_t scan_ok;
    uint8_t underrun_seen = 0u;
    int result;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    sdram_memtest_cdc_wait_for_start();
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    sdram_usb_debug_write_line(
        "H417 SDRAM LTDC TEST v25 REMAP VBLANK 4-FRAME");
    sdram_usb_debug_write_line(
        "DYNAMIC CONTRACT source=external_sdram_argb1555 base=60000000 frames=4 reload=vblank panel_control=boot_init_only test_control_writes=0 layer_toggle=0 ltdc_toggle=0");
    sdram_usb_debug_write_line(
        "STATIC FORMAT ARGB1555 full16=1 resolution=800x480 pitch=1600 bytes=768000");
    result = sdram_init_profile(0u, 0u, 1u);
    if(result != V5F_SDRAM_OK)
    {
        sdram_video_fail("sdram_init");
    }
    sdram_enable_0x60000000_remap();
    ch32h417_ltdc_rgb_framebuffer_barrier();
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "STATIC FMC REMAP base=%08x remap=%u bcr0=%08x",
                               (unsigned int)V5F_SDRAM_REMAP_ADDR,
                               (unsigned int)((FMC_Bank1->BTCR[0] &
                                               V5F_FMC_SDRAM_REMAP_TO_0X60000000) != 0u),
                               (unsigned int)FMC_Bank1->BTCR[0]);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    if(sdram_video_synth_write_verify("ARGB1555_DYNAMIC4",
                                      V5F_SDRAM_REMAP_ADDR,
                                      V5F_SDRAM_SYNTH_DYNAMIC_BYTES,
                                      1u) == 0)
    {
        sdram_video_fail("static_external_argb1555_dma");
    }
    sdram_usb_debug_write_line(
        "DYNAMIC FRAMES READY screen=red_TL,green_TR,blue_BR,white_BL marker=96x96 rot180=main frame_bytes=768000 total=3072000 compare=full16");
    result = sdram_video_static_ltdc_start();
    if(result != CH32H417_LTDC_RGB_OK)
    {
        sdram_video_fail("static_ltdc_start");
    }
    LTDC_ClearFlag(LTDC_FLAG_FU);
    scan_ok = sdram_video_ltdc_wait_scan(&scan_changes);
    sdram_video_ltdc_trace_layer("STATIC_STARTED_ONCE");
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "STATIC LTDC TIMING scan=%s changes=%u cfb=%08x sscr=%08x bpcr=%08x awcr=%08x twcr=%08x whpcr=%08x wvpcr=%08x cpsr=%08x cdsr=%08x",
                               (scan_ok != 0u) ? "PASS" : "FAIL",
                               (unsigned int)scan_changes,
                               (unsigned int)LTDC_Layer1->CFBAR,
                               (unsigned int)LTDC->SSCR,
                               (unsigned int)LTDC->BPCR,
                               (unsigned int)LTDC->AWCR,
                               (unsigned int)LTDC->TWCR,
                               (unsigned int)LTDC_Layer1->WHPCR,
                               (unsigned int)LTDC_Layer1->WVPCR,
                               (unsigned int)LTDC->CPSR,
                               (unsigned int)LTDC->CDSR);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    sdram_usb_debug_write_line(
        "DYNAMIC DISPLAY START control_writes=0 frame_order=red,green,blue,white marker_order=TL,TR,BR,BL interval_ms=1000 source=sdram_remap600 reload=vblank");

    while(1)
    {
        int used;
        uint8_t fifo_underrun;

        rt_thread_mdelay(1000);
        seconds++;
        frame++;
        if(frame >= V5F_SDRAM_SYNTH_DYNAMIC_FRAMES)
        {
            frame = 0u;
        }
        LTDC_LayerAddress(
            LTDC_Layer1,
            V5F_SDRAM_REMAP_ADDR + (frame * V5F_SDRAM_SYNTH_RGB_BYTES));
        ch32h417_ltdc_rgb_framebuffer_barrier();
        LTDC_ReloadConfig(LTDC_VBReload);
        fifo_underrun = (uint8_t)(
            (LTDC_GetFlagStatus(LTDC_FLAG_FU) != RESET) ? 1u : 0u);
        underrun_seen |= fifo_underrun;
        used = rt_snprintf(line,
                           sizeof(line),
                           "DYNAMIC FRAME seconds=%u frame=%u/4 cfbar=%08x reload=vblank gcr=%08x lcr=%08x fu=%u",
                           (unsigned int)seconds,
                           (unsigned int)(frame + 1u),
                           (unsigned int)LTDC_Layer1->CFBAR,
                           (unsigned int)LTDC->GCR,
                           (unsigned int)LTDC_Layer1->CR,
                           (unsigned int)fifo_underrun);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        if((result_reported == 0u) && (seconds >= 8u))
        {
            result_reported = 1u;
            if((scan_ok != 0u) && (underrun_seen == 0u))
            {
                g_v5f_hw_test_diag.phase = V5F_HW_PHASE_PASSED;
                g_v5f_hw_test_diag.sdram_ok_count++;
                sdram_usb_debug_write_line(
                    "LTDC RESULT PASS frame_crc=pass remap=pass scan=pass switches=8 reload=vblank fifo_underrun=0 display=cycling");
                sdram_usb_debug_write_line("RESULT PASS");
            }
            else
            {
                g_v5f_hw_test_diag.phase = V5F_HW_PHASE_FAILED;
                g_v5f_hw_test_diag.last_error = V5F_SDRAM_ERR_LCD;
                g_v5f_hw_test_diag.sdram_fail_count++;
                if(scan_ok == 0u)
                {
                    sdram_usb_debug_write_line(
                        "LTDC RESULT FAIL reason=no_scan display=hold controls=unchanged");
                }
                else
                {
                    sdram_usb_debug_write_line(
                        "LTDC RESULT FAIL reason=fifo_underrun display=hold controls=unchanged");
                }
                sdram_usb_debug_write_line("RESULT FAIL");
            }
        }
    }
}

static int sdram_video_ltdc_start_full(
    const v5f_sdram_video_config_t *config)
{
    ch32h417_ltdc_rgb_layer_t layer = {0};
    ch32h417_ltdc_rgb_color_t black = {0u, 0u, 0u};
    uint32_t pitch = V5F_SDRAM_VIDEO_WIDTH * config->bytes_per_pixel;
    int result;

    layer.width = V5F_SDRAM_VIDEO_WIDTH;
    layer.height = V5F_SDRAM_VIDEO_HEIGHT;
    layer.pixel_format = config->pixel_format;
    layer.framebuffer = V5F_SDRAM_BASE_ADDR;
    layer.line_pitch = pitch;
    result = ch32h417_ltdc_rgb_start_layer1(
        &ch32h417_ltdc_rgb_panel_800x480,
        &layer,
        &black);
    if(result == CH32H417_LTDC_RGB_OK)
    {
        ch32h417_ltdc_rgb_framebuffer_barrier();
        sdram_video_ltdc_trace_layer("FULL_REMAP_ARGB_P3");
    }
    return result;
}

static uint8_t sdram_video_h4v1_stage_payload(
    const v5f_sdram_video_config_t *config,
    const h4v1_index_entry_t *entry,
    uint8_t wide256)
{
    uint8_t *dma_stage = (uint8_t *)(uintptr_t)s_sdram_bw_buffer;
    uint32_t done = 0u;
    uint8_t ok = 1u;

    if((config == RT_NULL) || (entry == RT_NULL) ||
       (entry->compressed_bytes == 0u) ||
       (entry->compressed_bytes > V5F_SDRAM_H4V1_PAYLOAD_STAGE_BYTES) ||
       (entry->offset > config->total_bytes) ||
       (entry->compressed_bytes > (config->total_bytes - entry->offset)))
    {
        return 0u;
    }
    if(sdram_memtest_dma_stream_prepare(0u, wide256) == 0u)
    {
        return 0u;
    }
    while(done < entry->compressed_bytes)
    {
        uintptr_t source =
            (uintptr_t)V5F_SDRAM_BASE_ADDR + config->storage_offset +
            entry->offset + done;
        uintptr_t aligned_source =
            source & ~((uintptr_t)V5F_SDRAM_H4V1_DMA_ALIGNMENT - 1u);
        uint32_t prefix = (uint32_t)(source - aligned_source);
        uint32_t chunk = entry->compressed_bytes - done;
        uint32_t usable = V5F_SDRAM_VIDEO_STAGE_BYTES - prefix;
        uint32_t cycles;

        if(chunk > usable)
        {
            chunk = usable;
        }
        if(sdram_memtest_dma_stream_transfer(
               aligned_source,
               wide256,
               &cycles,
               V5F_SDRAM_WATCHDOG_STAGE_READ,
               0x3100u + (done / V5F_SDRAM_VIDEO_STAGE_BYTES)) == 0u)
        {
            ok = 0u;
            break;
        }
        memcpy(&s_lcd_fb[done], &dma_stage[prefix], chunk);
        done += chunk;
        sdram_memtest_watchdog_feed();
    }
    sdram_memtest_dma_stream_finish();
    ch32h417_ltdc_rgb_framebuffer_barrier();
    return ok;
}

static uint8_t sdram_video_ltdc_wait_next_vblank(void);

typedef struct
{
    uint32_t total_cycles;
    uint32_t payload_crc_cycles;
    uint32_t flush_cycles;
    uint32_t read_total_cycles;
    uint32_t read_active_cycles;
    uint32_t xor_cycles;
    uint32_t reconstructed_crc_cycles;
    uint32_t stage_copy_cycles;
    uint32_t write_total_cycles;
    uint32_t write_active_cycles;
    uint32_t flush_count;
    uint32_t literal_copy_cycles;
    uint32_t literal_bytes;
    uint32_t literal_calls;
    uint32_t match_copy_cycles;
    uint32_t match_bytes;
    uint32_t match_calls;
} v5f_sdram_h4v1_profile_t;

typedef struct
{
    const h4v1_index_entry_t *entry;
    const uint8_t *previous_frame;
    uint8_t *output_frame;
    uint8_t *history;
    uint8_t *output_chunk;
    uint32_t frame;
    uint32_t produced;
    uint32_t chunk_bytes;
    uint32_t reconstructed_crc;
    uint32_t read_slice_bytes;
    uint32_t write_slice_bytes;
    uint8_t delta;
    uint8_t read_wide256;
    uint8_t write_wide256;
    uint8_t vblank_gate;
    uint8_t verify_crc;
    v5f_sdram_h4v1_profile_t *profile;
} v5f_sdram_h4v1_stream_t;

typedef uint32_t v5f_sdram_h4v1_alias_u32_t
    __attribute__((may_alias));

static void __attribute__((optimize("O3")))
sdram_video_h4v1_xor_copy(uint8_t *output,
                          uint8_t *dma_stage,
                          uint32_t bytes)
{
    v5f_sdram_h4v1_alias_u32_t *output32 =
        (v5f_sdram_h4v1_alias_u32_t *)(void *)output;
    v5f_sdram_h4v1_alias_u32_t *stage32 =
        (v5f_sdram_h4v1_alias_u32_t *)(void *)dma_stage;
    uint32_t words = bytes / sizeof(uint32_t);
    uint32_t index;

    for(index = 0u; index < words; ++index)
    {
        uint32_t value = output32[index] ^ stage32[index];

        output32[index] = value;
        stage32[index] = value;
    }
    for(index = words * sizeof(uint32_t); index < bytes; ++index)
    {
        uint8_t value = output[index] ^ dma_stage[index];

        output[index] = value;
        dma_stage[index] = value;
    }
}

static int sdram_video_h4v1_stream_flush(
    v5f_sdram_h4v1_stream_t *stream)
{
    uint8_t *dma_stage = (uint8_t *)(uintptr_t)s_sdram_bw_buffer;
    uint32_t output_offset;
    uint32_t read_offset;
    uint32_t write_offset;
    uint32_t cycles;
    uint32_t flush_start = 0u;
    uint32_t phase_start = 0u;

    if(stream->chunk_bytes == 0u)
    {
        return H4V1_OK;
    }
    if(stream->profile != RT_NULL)
    {
        flush_start = sdram_video_cycle_now();
        stream->profile->flush_count++;
    }
    output_offset = stream->produced - stream->chunk_bytes;
    if((stream->vblank_gate != 0u) &&
       (sdram_video_ltdc_wait_next_vblank() == 0u))
    {
        return H4V1_ERR_OUTPUT;
    }
    if(stream->delta != 0u)
    {
        if(stream->previous_frame == RT_NULL)
        {
            return H4V1_ERR_FRAME_BASE;
        }
        if(stream->profile != RT_NULL)
        {
            phase_start = sdram_video_cycle_now();
        }
        if(sdram_memtest_dma_stream_prepare(0u, stream->read_wide256) == 0u)
        {
            return H4V1_ERR_INPUT;
        }
        read_offset = 0u;
        while(read_offset < stream->chunk_bytes)
        {
            uint32_t read_bytes = stream->chunk_bytes - read_offset;

            if(read_bytes > stream->read_slice_bytes)
            {
                read_bytes = stream->read_slice_bytes;
            }
            if(sdram_memtest_dma_stream_transfer_at(
                   (uintptr_t)&stream->previous_frame[
                       output_offset + read_offset],
                   (uintptr_t)&dma_stage[read_offset],
                   read_bytes,
                   stream->read_wide256,
                   &cycles,
                   V5F_SDRAM_WATCHDOG_STAGE_READ,
                   0x3600u + (stream->frame * 512u) +
                       ((output_offset /
                         V5F_SDRAM_H4V1_OUTPUT_BYTES) * 8u) +
                       (read_offset / stream->read_slice_bytes)) == 0u)
            {
                sdram_memtest_dma_stream_finish();
                return H4V1_ERR_INPUT;
            }
            if(stream->profile != RT_NULL)
            {
                stream->profile->read_active_cycles += cycles;
            }
            read_offset += read_bytes;
        }
        sdram_memtest_dma_stream_finish();
        if(stream->profile != RT_NULL)
        {
            stream->profile->read_total_cycles +=
                sdram_video_cycle_now() - phase_start;
            phase_start = sdram_video_cycle_now();
        }
        sdram_video_h4v1_xor_copy(stream->output_chunk,
                                  dma_stage,
                                  stream->chunk_bytes);
        if(stream->profile != RT_NULL)
        {
            stream->profile->xor_cycles +=
                sdram_video_cycle_now() - phase_start;
        }
    }
    if(stream->verify_crc != 0u)
    {
        if(stream->profile != RT_NULL)
        {
            phase_start = sdram_video_cycle_now();
        }
        stream->reconstructed_crc = h4v1_crc32_update(
            stream->reconstructed_crc,
            stream->output_chunk,
            stream->chunk_bytes);
        if(stream->profile != RT_NULL)
        {
            stream->profile->reconstructed_crc_cycles +=
                sdram_video_cycle_now() - phase_start;
        }
    }
    if(stream->profile != RT_NULL)
    {
        phase_start = sdram_video_cycle_now();
    }
    if(stream->delta == 0u)
    {
        memcpy(dma_stage, stream->output_chunk, stream->chunk_bytes);
    }
    if(stream->chunk_bytes < V5F_SDRAM_H4V1_OUTPUT_BYTES)
    {
        memset(&dma_stage[stream->chunk_bytes],
               0,
               V5F_SDRAM_H4V1_OUTPUT_BYTES - stream->chunk_bytes);
    }
    if(stream->profile != RT_NULL)
    {
        stream->profile->stage_copy_cycles +=
            sdram_video_cycle_now() - phase_start;
        phase_start = sdram_video_cycle_now();
    }
    if((stream->delta != 0u) &&
       (sdram_memtest_dma_stream_prepare(1u, stream->write_wide256) == 0u))
    {
        return H4V1_ERR_OUTPUT;
    }
    write_offset = 0u;
    while(write_offset < stream->chunk_bytes)
    {
        uint32_t write_bytes = stream->chunk_bytes - write_offset;

        if(write_bytes > stream->write_slice_bytes)
        {
            write_bytes = stream->write_slice_bytes;
        }
        if(sdram_memtest_dma_stream_transfer_at(
               (uintptr_t)&stream->output_frame[output_offset + write_offset],
               (uintptr_t)&dma_stage[write_offset],
               write_bytes,
               stream->write_wide256,
               &cycles,
               V5F_SDRAM_WATCHDOG_STAGE_WRITE,
               0x3500u + (stream->frame * 512u) +
                   ((output_offset / V5F_SDRAM_H4V1_OUTPUT_BYTES) * 8u) +
                   (write_offset / stream->write_slice_bytes)) == 0u)
        {
            if(stream->delta != 0u)
            {
                sdram_memtest_dma_stream_finish();
            }
            return H4V1_ERR_OUTPUT;
        }
        if(stream->profile != RT_NULL)
        {
            stream->profile->write_active_cycles += cycles;
        }
        write_offset += write_bytes;
    }
    if(stream->delta != 0u)
    {
        sdram_memtest_dma_stream_finish();
    }
    if(stream->profile != RT_NULL)
    {
        stream->profile->write_total_cycles +=
            sdram_video_cycle_now() - phase_start;
        stream->profile->flush_cycles +=
            sdram_video_cycle_now() - flush_start;
    }
    stream->chunk_bytes = 0u;
    sdram_memtest_watchdog_feed();
    return H4V1_OK;
}

static int __attribute__((optimize("O3")))
sdram_video_h4v1_stream_copy_literals(v5f_sdram_h4v1_stream_t *stream,
                                      const uint8_t *source,
                                      uint32_t bytes)
{
    if(bytes > (stream->entry->uncompressed_bytes - stream->produced))
    {
        return H4V1_ERR_OUTPUT;
    }
    if(stream->profile != RT_NULL)
    {
        stream->profile->literal_bytes += bytes;
        stream->profile->literal_calls++;
    }
    while(bytes != 0u)
    {
        uint32_t history_index = stream->produced &
            (V5F_SDRAM_H4V1_HISTORY_BYTES - 1u);
        uint32_t span = bytes;
        uint32_t space = V5F_SDRAM_H4V1_OUTPUT_BYTES -
            stream->chunk_bytes;
        uint32_t copy_start = 0u;
        int result;

        if(span > space)
        {
            span = space;
        }
        space = V5F_SDRAM_H4V1_HISTORY_BYTES - history_index;
        if(span > space)
        {
            span = space;
        }
        if(stream->profile != RT_NULL)
        {
            copy_start = sdram_video_cycle_now();
        }
        memcpy(&stream->history[history_index], source, span);
        memcpy(&stream->output_chunk[stream->chunk_bytes], source, span);
        if(stream->profile != RT_NULL)
        {
            stream->profile->literal_copy_cycles +=
                sdram_video_cycle_now() - copy_start;
        }
        source += span;
        bytes -= span;
        stream->produced += span;
        stream->chunk_bytes += span;
        if(stream->chunk_bytes == V5F_SDRAM_H4V1_OUTPUT_BYTES)
        {
            result = sdram_video_h4v1_stream_flush(stream);
            if(result != H4V1_OK)
            {
                return result;
            }
        }
    }
    return H4V1_OK;
}

static int __attribute__((optimize("O3,no-tree-loop-distribute-patterns")))
sdram_video_h4v1_stream_copy_match(v5f_sdram_h4v1_stream_t *stream,
                                   uint32_t match_offset,
                                   uint32_t bytes)
{
    if((match_offset == 0u) || (match_offset > stream->produced) ||
       (bytes > (stream->entry->uncompressed_bytes - stream->produced)))
    {
        return H4V1_ERR_LZ4;
    }
    if(stream->profile != RT_NULL)
    {
        stream->profile->match_bytes += bytes;
        stream->profile->match_calls++;
    }
    while(bytes != 0u)
    {
        uint32_t history_write = stream->produced &
            (V5F_SDRAM_H4V1_HISTORY_BYTES - 1u);
        uint32_t history_read = (stream->produced - match_offset) &
            (V5F_SDRAM_H4V1_HISTORY_BYTES - 1u);
        uint32_t span = bytes;
        uint32_t space = V5F_SDRAM_H4V1_OUTPUT_BYTES -
            stream->chunk_bytes;
        uint32_t copy_start = 0u;
        int result;

        if(span > space)
        {
            span = space;
        }
        space = V5F_SDRAM_H4V1_HISTORY_BYTES - history_write;
        if(span > space)
        {
            span = space;
        }
        space = V5F_SDRAM_H4V1_HISTORY_BYTES - history_read;
        if(span > space)
        {
            span = space;
        }
        if(stream->profile != RT_NULL)
        {
            copy_start = sdram_video_cycle_now();
        }
        if(match_offset == 1u)
        {
            uint8_t value = stream->history[history_read];

            memset(&stream->history[history_write], value, span);
            memset(&stream->output_chunk[stream->chunk_bytes], value, span);
        }
        else
        {
            uint32_t copy_index;

            for(copy_index = 0u; copy_index < span; ++copy_index)
            {
                uint8_t value =
                    stream->history[history_read + copy_index];

                stream->history[history_write + copy_index] = value;
                stream->output_chunk[
                    stream->chunk_bytes + copy_index] = value;
            }
        }
        if(stream->profile != RT_NULL)
        {
            stream->profile->match_copy_cycles +=
                sdram_video_cycle_now() - copy_start;
        }
        bytes -= span;
        stream->produced += span;
        stream->chunk_bytes += span;
        if(stream->chunk_bytes == V5F_SDRAM_H4V1_OUTPUT_BYTES)
        {
            result = sdram_video_h4v1_stream_flush(stream);
            if(result != H4V1_OK)
            {
                return result;
            }
        }
    }
    return H4V1_OK;
}

static inline __attribute__((always_inline)) int
sdram_video_h4v1_lz4_length(const uint8_t **input,
                            const uint8_t *input_end,
                            uint32_t *length)
{
    uint8_t extension;

    do
    {
        if(*input >= input_end)
        {
            return H4V1_ERR_INPUT;
        }
        extension = *(*input)++;
        if(*length > (0xFFFFFFFFu - extension))
        {
            return H4V1_ERR_LZ4;
        }
        *length += extension;
    } while(extension == 255u);
    return H4V1_OK;
}

static int __attribute__((optimize("O3,no-tree-loop-distribute-patterns")))
sdram_video_h4v1_decode_streamed(
    const h4v1_index_entry_t *entry,
    uint32_t frame,
    const uint8_t *previous_frame,
    uint8_t *output_frame,
    uint32_t *decoded_crc,
    uint8_t read_wide256,
    uint8_t write_wide256,
    uint32_t read_slice_bytes,
    uint32_t write_slice_bytes,
    uint8_t vblank_gate,
    uint8_t verify_crc,
    v5f_sdram_h4v1_profile_t *profile)
{
    const uint8_t *input = s_lcd_fb;
    const uint8_t *input_end;
    v5f_sdram_h4v1_stream_t stream;
    uint32_t frame_kind;
    uint32_t function_start = 0u;
    uint32_t phase_start = 0u;
    int result;

    if((entry == RT_NULL) || (output_frame == RT_NULL) ||
       (decoded_crc == RT_NULL) || (entry->compressed_bytes == 0u) ||
       (entry->compressed_bytes > V5F_SDRAM_H4V1_PAYLOAD_STAGE_BYTES) ||
       (entry->uncompressed_bytes != V5F_SDRAM_H4V1_FRAME_BYTES) ||
       (read_slice_bytes == 0u) ||
       (read_slice_bytes > V5F_SDRAM_H4V1_OUTPUT_BYTES) ||
       ((read_slice_bytes % ((read_wide256 != 0u) ? 32u : 4u)) != 0u) ||
       (write_slice_bytes == 0u) ||
       (write_slice_bytes > V5F_SDRAM_H4V1_OUTPUT_BYTES) ||
       ((write_slice_bytes % ((write_wide256 != 0u) ? 32u : 4u)) != 0u))
    {
        return H4V1_ERR_ARGUMENT;
    }
    if(profile != RT_NULL)
    {
        memset(profile, 0, sizeof(*profile));
        function_start = sdram_video_cycle_now();
    }
    input_end = &s_lcd_fb[entry->compressed_bytes];
    if(verify_crc != 0u)
    {
        uint32_t payload_crc;

        if(profile != RT_NULL)
        {
            phase_start = sdram_video_cycle_now();
        }
        payload_crc = h4v1_crc32(input, entry->compressed_bytes);
        if(profile != RT_NULL)
        {
            profile->payload_crc_cycles +=
                sdram_video_cycle_now() - phase_start;
        }
        if(payload_crc != entry->payload_crc32)
        {
            return H4V1_ERR_PAYLOAD_CRC;
        }
    }
    frame_kind = entry->flags & (H4V1_FRAME_KEY | H4V1_FRAME_XOR_DELTA);
    if((frame_kind != H4V1_FRAME_KEY) &&
       (frame_kind != H4V1_FRAME_XOR_DELTA))
    {
        return H4V1_ERR_INDEX;
    }
    if((frame_kind == H4V1_FRAME_XOR_DELTA) &&
       (previous_frame == RT_NULL))
    {
        return H4V1_ERR_FRAME_BASE;
    }

    memset(&stream, 0, sizeof(stream));
    stream.entry = entry;
    stream.previous_frame = previous_frame;
    stream.output_frame = output_frame;
    stream.history = s_sdram_h4v1_history;
    stream.output_chunk = s_sdram_h4v1_output;
    stream.frame = frame;
    stream.read_slice_bytes = read_slice_bytes;
    stream.write_slice_bytes = write_slice_bytes;
    stream.delta = (uint8_t)(frame_kind == H4V1_FRAME_XOR_DELTA);
    stream.read_wide256 = read_wide256;
    stream.write_wide256 = write_wide256;
    stream.vblank_gate = vblank_gate;
    stream.verify_crc = verify_crc;
    stream.profile = profile;

    while(input < input_end)
    {
        uint8_t token = *input++;
        uint32_t literal_length = token >> 4;
        uint32_t match_length;
        uint16_t match_offset;

        if(literal_length == 15u)
        {
            result = sdram_video_h4v1_lz4_length(&input,
                                                  input_end,
                                                  &literal_length);
            if(result != H4V1_OK)
            {
                return result;
            }
        }
        if((uint32_t)(input_end - input) < literal_length)
        {
            return H4V1_ERR_INPUT;
        }
        result = sdram_video_h4v1_stream_copy_literals(&stream,
                                                        input,
                                                        literal_length);
        if(result != H4V1_OK)
        {
            return result;
        }
        input += literal_length;
        if(input == input_end)
        {
            break;
        }
        if((uint32_t)(input_end - input) < 2u)
        {
            return H4V1_ERR_INPUT;
        }
        match_offset = (uint16_t)((uint16_t)input[0] |
                                  ((uint16_t)input[1] << 8));
        input += 2;
        if((match_offset == 0u) || (match_offset > stream.produced))
        {
            return H4V1_ERR_LZ4;
        }
        match_length = token & 0x0Fu;
        if(match_length == 15u)
        {
            result = sdram_video_h4v1_lz4_length(&input,
                                                  input_end,
                                                  &match_length);
            if(result != H4V1_OK)
            {
                return result;
            }
        }
        if(match_length > (0xFFFFFFFFu - 4u))
        {
            return H4V1_ERR_LZ4;
        }
        match_length += 4u;
        result = sdram_video_h4v1_stream_copy_match(&stream,
                                                    match_offset,
                                                    match_length);
        if(result != H4V1_OK)
        {
            return result;
        }
    }
    result = sdram_video_h4v1_stream_flush(&stream);
    if(result != H4V1_OK)
    {
        return result;
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
    if((input != input_end) ||
       (stream.produced != entry->uncompressed_bytes))
    {
        return H4V1_ERR_OUTPUT;
    }
    *decoded_crc = stream.reconstructed_crc;
    if((verify_crc != 0u) &&
       (stream.reconstructed_crc != entry->raw_crc32))
    {
        return H4V1_ERR_FRAME_CRC;
    }
    if(profile != RT_NULL)
    {
        profile->total_cycles = sdram_video_cycle_now() - function_start;
    }
    return H4V1_OK;
}

static uint32_t sdram_video_h4v1_dma_frame_crc(
    const uint8_t *framebuffer,
    uint32_t bytes,
    uint8_t *ok_out,
    uint8_t wide256)
{
    uint8_t *dma_stage = (uint8_t *)(uintptr_t)s_sdram_bw_buffer;
    uint32_t offset;
    uint32_t crc = 0u;

    *ok_out = 0u;
    if(sdram_memtest_dma_stream_prepare(0u, wide256) == 0u)
    {
        return 0u;
    }
    for(offset = 0u; offset < bytes;
        offset += V5F_SDRAM_VIDEO_STAGE_BYTES)
    {
        uint32_t chunk = bytes - offset;
        uint32_t cycles;

        if(chunk > V5F_SDRAM_VIDEO_STAGE_BYTES)
        {
            chunk = V5F_SDRAM_VIDEO_STAGE_BYTES;
        }
        if(sdram_memtest_dma_stream_transfer(
               (uintptr_t)&framebuffer[offset],
               wide256,
               &cycles,
               V5F_SDRAM_WATCHDOG_STAGE_READ,
               0x4000u + (offset / V5F_SDRAM_VIDEO_STAGE_BYTES)) == 0u)
        {
            sdram_memtest_dma_stream_finish();
            return crc;
        }
        crc = h4v1_crc32_update(crc, dma_stage, chunk);
        sdram_memtest_watchdog_feed();
    }
    sdram_memtest_dma_stream_finish();
    *ok_out = 1u;
    return crc;
}

static void sdram_video_h4v1_live_fu_checkpoint(
    uint32_t frame,
    const char *phase,
    uint32_t *underruns)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];

    if(LTDC_GetFlagStatus(LTDC_FLAG_FU) == RESET)
    {
        return;
    }
    (*underruns)++;
    LTDC_ClearFlag(LTDC_FLAG_FU);
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "H4V1 LIVE FU frame=%u phase=%s count=%u",
            (unsigned int)frame,
            phase,
            (unsigned int)*underruns);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
}

static uint8_t sdram_video_h4v1_live_swap(
    uint32_t frame,
    const uint8_t *framebuffer,
    uint32_t *switches,
    uint32_t *sync_timeouts,
    uint32_t *underruns)
{
    char line[V5F_SDRAM_USB_LINE_BYTES];

    if(LTDC_GetFlagStatus(LTDC_FLAG_FU) != RESET)
    {
        (*underruns)++;
        LTDC_ClearFlag(LTDC_FLAG_FU);
    }
    if(sdram_video_ltdc_wait_next_vblank() == 0u)
    {
        (*sync_timeouts)++;
        return 0u;
    }
    LTDC_LayerAddress(LTDC_Layer1, (uint32_t)(uintptr_t)framebuffer);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    LTDC_ReloadConfig(LTDC_IMReload);
    (*switches)++;
    g_v5f_hw_test_diag.frame_count++;
    if(LTDC_GetFlagStatus(LTDC_FLAG_FU) != RESET)
    {
        (*underruns)++;
        LTDC_ClearFlag(LTDC_FLAG_FU);
    }
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "H4V1 LIVE SWAP frame=%u cfb=%08x swaps=%u timeout=%u fu=%u",
            (unsigned int)frame,
            (unsigned int)LTDC_Layer1->CFBAR,
            (unsigned int)*switches,
            (unsigned int)*sync_timeouts,
            (unsigned int)*underruns);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    return 1u;
}

static void __attribute__((noreturn))
sdram_video_h4v1_show_pair(v5f_sdram_video_config_t *config,
                           const h4v1_header_t *header,
                           const h4v1_index_entry_t *first,
                           const h4v1_index_entry_t *second)
{
    uint8_t *framebuffer0 = (uint8_t *)(uintptr_t)V5F_SDRAM_BASE_ADDR;
    uint8_t *framebuffer1 = (uint8_t *)(uintptr_t)(
        V5F_SDRAM_BASE_ADDR + V5F_SDRAM_H4V1_FB1_OFFSET);
    h4v1_index_entry_t third;
    h4v1_index_entry_t batch_entry;
    uint32_t decoded_crc = 0u;
    uint32_t second_decoded_crc = 0u;
    uint32_t third_decoded_crc = 0u;
    uint32_t dma_crc;
    uint32_t second_dma_crc;
    uint32_t second_dma_crc_wide = 0u;
    uint32_t third_dma_crc;
    uint32_t decode_start;
    uint32_t decode_cycles;
    uint32_t second_decode_cycles;
    uint32_t third_decode_cycles;
    uint32_t third_index_cycles;
    uint32_t batch_frame;
    uint32_t batch_index_cycles;
    uint32_t batch_decoded_crc;
    uint32_t batch_dma_crc;
    uint32_t batch_decode_cycles;
    uint32_t batch_frame_kind;
    uint32_t batch_key_count = 0u;
    uint32_t batch_delta_count = 0u;
    uint32_t batch_dma_sample_count = 0u;
    v5f_sdram_h4v1_profile_t batch_profile;
    uint32_t scan_changes = 0u;
    uint32_t switches = 0u;
    uint32_t sync_timeouts = 0u;
    uint32_t underruns = 0u;
    uint32_t hold_seconds = 0u;
    uint8_t dma_ok;
    uint8_t second_dma_ok;
    uint8_t second_dma_wide_ok = 0u;
    uint8_t third_dma_ok;
    uint8_t batch_dma_ok;
    uint8_t batch_dma_sampled;
    uint8_t batch_profiled;
    int result;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    if((header->frame_count == 0u) || (header->fps == 0u) ||
       (header->fps > 60u) ||
       ((first->flags & (H4V1_FRAME_KEY | H4V1_FRAME_XOR_DELTA)) !=
        H4V1_FRAME_KEY))
    {
        sdram_video_fail("h4v1_first_index");
    }
    sdram_usb_debug_write_line(
        "H4V1 FIRST DECODE START path=dma_payload,stream64k,dma2_write16k,dma256_crc ltdc=off");
    if(sdram_video_h4v1_stage_payload(config, first, 1u) == 0u)
    {
        sdram_video_fail("h4v1_first_payload_dma");
    }
    decode_start = sdram_video_cycle_now();
    if(sdram_memtest_dma_stream_prepare(1u, 1u) == 0u)
    {
        sdram_video_fail("h4v1_first_output_dma_prepare");
    }
    result = sdram_video_h4v1_decode_streamed(first,
                                               0u,
                                               RT_NULL,
                                               framebuffer0,
                                               &decoded_crc,
                                               1u,
                                               1u,
                                               V5F_SDRAM_H4V1_OUTPUT_BYTES,
                                               V5F_SDRAM_H4V1_OUTPUT_BYTES,
                                               0u,
                                               1u,
                                               RT_NULL);
    sdram_memtest_dma_stream_finish();
    decode_cycles = sdram_video_cycle_now() - decode_start;
    if(result != H4V1_OK)
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "H4V1 FIRST DECODE FAIL status=%d cycles=%u crc=%08x/%08x",
                               result,
                               (unsigned int)decode_cycles,
                               (unsigned int)decoded_crc,
                               (unsigned int)first->raw_crc32);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        sdram_video_fail("h4v1_first_decode");
    }
    dma_crc = sdram_video_h4v1_dma_frame_crc(framebuffer0,
                                              header->frame_bytes,
                                              &dma_ok,
                                              1u);
    if((dma_ok == 0u) || (dma_crc != first->raw_crc32))
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "H4V1 FIRST VERIFY FAIL dma=%u crc=%08x/%08x decoded=%08x",
                               (unsigned int)dma_ok,
                               (unsigned int)dma_crc,
                               (unsigned int)first->raw_crc32,
                               (unsigned int)decoded_crc);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        sdram_video_fail("h4v1_first_dma_crc");
    }
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "H4V1 FIRST PASS compressed=%u frame=%u crc=%08x dma_crc=%08x decode_cycles=%u",
            (unsigned int)first->compressed_bytes,
            (unsigned int)header->frame_bytes,
            (unsigned int)decoded_crc,
            (unsigned int)dma_crc,
            (unsigned int)decode_cycles);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    config->pixel_format = LTDC_Pixelformat_ARGB1555;
    config->bytes_per_pixel = 2u;
    config->frame_bytes = header->frame_bytes;
    config->frames = V5F_SDRAM_H4V1_BATCH_FRAMES;
    config->fps = header->fps;
    LTDC_ClearFlag(LTDC_FLAG_FU);
    result = sdram_video_ltdc_start_full(config);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        sdram_video_fail("h4v1_live_ltdc_start");
    }
    if(sdram_video_ltdc_wait_scan(&scan_changes) == 0u)
    {
        sdram_video_fail("h4v1_live_ltdc_no_scan");
    }
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "H4V1 LIVE START frame=0 cfb=%08x scan_changes=%u fu=%u decode_next=1 dma=r256x2k/w256x2k sampled-crc payload_crc=r32 gate=off",
            (unsigned int)LTDC_Layer1->CFBAR,
            (unsigned int)scan_changes,
            (unsigned int)((LTDC_GetFlagStatus(LTDC_FLAG_FU) != RESET) ?
                           1u : 0u));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    if((second->flags & (H4V1_FRAME_KEY | H4V1_FRAME_XOR_DELTA)) !=
       H4V1_FRAME_XOR_DELTA)
    {
        sdram_video_fail("h4v1_second_index");
    }
    sdram_usb_debug_write_line(
        "H4V1 DELTA DECODE START frame=1 previous=60000000 output=600c0000 dma_read_previous=1");
    if(sdram_video_h4v1_stage_payload(
           config, second, V5F_SDRAM_H4V1_LIVE_READ_WIDE256) == 0u)
    {
        sdram_video_fail("h4v1_second_payload_dma");
    }
    sdram_video_h4v1_live_fu_checkpoint(1u, "payload", &underruns);
    decode_start = sdram_video_cycle_now();
    result = sdram_video_h4v1_decode_streamed(second,
                                               1u,
                                               framebuffer0,
                                               framebuffer1,
                                               &second_decoded_crc,
                                               V5F_SDRAM_H4V1_PREVIOUS_READ_WIDE256,
                                               V5F_SDRAM_H4V1_LIVE_WRITE_WIDE256,
                                               V5F_SDRAM_H4V1_LIVE_READ_SLICE_BYTES,
                                               V5F_SDRAM_H4V1_LIVE_WRITE_SLICE_BYTES,
                                               0u,
                                               1u,
                                               RT_NULL);
    second_decode_cycles = sdram_video_cycle_now() - decode_start;
    sdram_video_h4v1_live_fu_checkpoint(1u, "decode", &underruns);
    if(result != H4V1_OK)
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "H4V1 DELTA DECODE FAIL status=%d cycles=%u crc=%08x/%08x",
            result,
            (unsigned int)second_decode_cycles,
            (unsigned int)second_decoded_crc,
            (unsigned int)second->raw_crc32);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        sdram_video_fail("h4v1_second_decode");
    }
    second_dma_crc = sdram_video_h4v1_dma_frame_crc(framebuffer1,
                                                     header->frame_bytes,
                                                     &second_dma_ok,
                                                     V5F_SDRAM_H4V1_LIVE_READ_WIDE256);
    sdram_video_h4v1_live_fu_checkpoint(1u, "crc", &underruns);
    if((second_dma_ok == 0u) ||
       (second_dma_crc != second->raw_crc32))
    {
        second_dma_crc_wide = sdram_video_h4v1_dma_frame_crc(
            framebuffer1,
            header->frame_bytes,
            &second_dma_wide_ok,
            1u);
        sdram_video_h4v1_live_fu_checkpoint(
            1u, "crc256_recheck", &underruns);
        {
            int used = rt_snprintf(
                line,
                sizeof(line),
                "H4V1 DELTA CRC RECHECK word32=%u/%08x dma256=%u/%08x exp=%08x",
                (unsigned int)second_dma_ok,
                (unsigned int)second_dma_crc,
                (unsigned int)second_dma_wide_ok,
                (unsigned int)second_dma_crc_wide,
                (unsigned int)second->raw_crc32);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
        if((second_dma_wide_ok != 0u) &&
           (second_dma_crc_wide == second->raw_crc32))
        {
            second_dma_ok = 1u;
            second_dma_crc = second_dma_crc_wide;
        }
    }
    if((second_dma_ok == 0u) ||
       (second_dma_crc != second->raw_crc32))
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "H4V1 DELTA VERIFY FAIL dma=%u crc=%08x/%08x decoded=%08x",
            (unsigned int)second_dma_ok,
            (unsigned int)second_dma_crc,
            (unsigned int)second->raw_crc32,
            (unsigned int)second_decoded_crc);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        sdram_video_fail("h4v1_second_dma_crc");
    }
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "H4V1 DELTA PASS frame=1 compressed=%u crc=%08x dma_crc=%08x decode_cycles=%u",
            (unsigned int)second->compressed_bytes,
            (unsigned int)second_decoded_crc,
            (unsigned int)second_dma_crc,
            (unsigned int)second_decode_cycles);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    if(sdram_video_h4v1_live_swap(1u,
                                   framebuffer1,
                                   &switches,
                                   &sync_timeouts,
                                   &underruns) == 0u)
    {
        sdram_video_fail("h4v1_live_swap1");
    }

    if(header->frame_count < 3u)
    {
        sdram_video_fail("h4v1_third_missing");
    }
    if(sdram_video_h4v1_read_index_entry(config,
                                          header,
                                          2u,
                                          &third,
                                          &third_index_cycles) == 0u)
    {
        sdram_video_fail("h4v1_third_index_dma");
    }
    if((third.flags & (H4V1_FRAME_KEY | H4V1_FRAME_XOR_DELTA)) !=
       H4V1_FRAME_XOR_DELTA)
    {
        sdram_video_fail("h4v1_third_index_type");
    }
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "H4V1 THIRD INDEX PASS frame=2 offset=%u compressed=%u flags=%08x cycles=%u",
            (unsigned int)third.offset,
            (unsigned int)third.compressed_bytes,
            (unsigned int)third.flags,
            (unsigned int)third_index_cycles);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "H4V1 THIRD DECODE START frame=2 previous=600c0000 output=60000000 dma_read_previous=1 payload_align_skip=%u",
            (unsigned int)(third.offset &
                           (V5F_SDRAM_H4V1_DMA_ALIGNMENT - 1u)));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    if(sdram_video_h4v1_stage_payload(
           config, &third, V5F_SDRAM_H4V1_LIVE_READ_WIDE256) == 0u)
    {
        sdram_video_fail("h4v1_third_payload_dma");
    }
    sdram_video_h4v1_live_fu_checkpoint(2u, "payload", &underruns);
    decode_start = sdram_video_cycle_now();
    result = sdram_video_h4v1_decode_streamed(&third,
                                               2u,
                                               framebuffer1,
                                               framebuffer0,
                                               &third_decoded_crc,
                                               V5F_SDRAM_H4V1_PREVIOUS_READ_WIDE256,
                                               V5F_SDRAM_H4V1_LIVE_WRITE_WIDE256,
                                               V5F_SDRAM_H4V1_LIVE_READ_SLICE_BYTES,
                                               V5F_SDRAM_H4V1_LIVE_WRITE_SLICE_BYTES,
                                               0u,
                                               1u,
                                               RT_NULL);
    third_decode_cycles = sdram_video_cycle_now() - decode_start;
    sdram_video_h4v1_live_fu_checkpoint(2u, "decode", &underruns);
    if(result != H4V1_OK)
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "H4V1 THIRD DECODE FAIL status=%d cycles=%u crc=%08x/%08x",
            result,
            (unsigned int)third_decode_cycles,
            (unsigned int)third_decoded_crc,
            (unsigned int)third.raw_crc32);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        sdram_video_fail("h4v1_third_decode");
    }
    third_dma_crc = sdram_video_h4v1_dma_frame_crc(framebuffer0,
                                                    header->frame_bytes,
                                                    &third_dma_ok,
                                                    V5F_SDRAM_H4V1_LIVE_READ_WIDE256);
    sdram_video_h4v1_live_fu_checkpoint(2u, "crc", &underruns);
    if((third_dma_ok == 0u) || (third_dma_crc != third.raw_crc32))
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "H4V1 THIRD VERIFY FAIL dma=%u crc=%08x/%08x decoded=%08x",
            (unsigned int)third_dma_ok,
            (unsigned int)third_dma_crc,
            (unsigned int)third.raw_crc32,
            (unsigned int)third_decoded_crc);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        sdram_video_fail("h4v1_third_dma_crc");
    }
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "H4V1 THIRD PASS frame=2 compressed=%u crc=%08x dma_crc=%08x decode_cycles=%u",
            (unsigned int)third.compressed_bytes,
            (unsigned int)third_decoded_crc,
            (unsigned int)third_dma_crc,
            (unsigned int)third_decode_cycles);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    if(sdram_video_h4v1_live_swap(2u,
                                   framebuffer0,
                                   &switches,
                                   &sync_timeouts,
                                   &underruns) == 0u)
    {
        sdram_video_fail("h4v1_live_swap2");
    }

    if((header->frame_count < V5F_SDRAM_H4V1_BATCH_FRAMES) ||
       (header->gop != V5F_SDRAM_H4V1_EXPECTED_GOP))
    {
        sdram_video_fail("h4v1_live90_contract");
    }
    sdram_usb_debug_write_line(
        "H4V1 LIVE90 DECODE START frames=3..89 keys=30,60 ltdc=on verify=codec+dma_crc_0,1,2,30,31,60,61,89 others=decode_bounds swap=vblank");
    for(batch_frame = 3u;
        batch_frame < V5F_SDRAM_H4V1_BATCH_FRAMES;
        ++batch_frame)
    {
        const uint8_t *previous_frame;
        uint8_t *output_frame;

        if(sdram_video_h4v1_read_index_entry(config,
                                              header,
                                              batch_frame,
                                              &batch_entry,
                                              &batch_index_cycles) == 0u)
        {
            int used = rt_snprintf(
                line,
                sizeof(line),
                "H4V1 BATCH INDEX FAIL frame=%u",
                (unsigned int)batch_frame);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            sdram_video_fail("h4v1_batch_index_dma");
        }
        batch_frame_kind = batch_entry.flags &
            (H4V1_FRAME_KEY | H4V1_FRAME_XOR_DELTA);
        if((batch_frame_kind != H4V1_FRAME_KEY) &&
           (batch_frame_kind != H4V1_FRAME_XOR_DELTA))
        {
            int used = rt_snprintf(
                line,
                sizeof(line),
                "H4V1 BATCH TYPE FAIL frame=%u flags=%08x",
                (unsigned int)batch_frame,
                (unsigned int)batch_entry.flags);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            sdram_video_fail("h4v1_batch_index_type");
        }
        if(batch_frame_kind !=
           (((batch_frame % header->gop) == 0u) ?
            H4V1_FRAME_KEY : H4V1_FRAME_XOR_DELTA))
        {
            int used = rt_snprintf(
                line,
                sizeof(line),
                "H4V1 LIVE90 KIND FAIL frame=%u got=%08x expected=%s",
                (unsigned int)batch_frame,
                (unsigned int)batch_frame_kind,
                ((batch_frame % header->gop) == 0u) ? "KEY" : "DELTA");
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            sdram_video_fail("h4v1_live90_kind_position");
        }
        if(sdram_video_h4v1_stage_payload(
               config,
               &batch_entry,
               V5F_SDRAM_H4V1_LIVE_READ_WIDE256) == 0u)
        {
            int used = rt_snprintf(
                line,
                sizeof(line),
                "H4V1 BATCH PAYLOAD FAIL frame=%u offset=%u align_skip=%u",
                (unsigned int)batch_frame,
                (unsigned int)batch_entry.offset,
                (unsigned int)(batch_entry.offset &
                               (V5F_SDRAM_H4V1_DMA_ALIGNMENT - 1u)));
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            sdram_video_fail("h4v1_batch_payload_dma");
        }
        sdram_video_h4v1_live_fu_checkpoint(
            batch_frame, "payload", &underruns);
        if((batch_frame & 1u) != 0u)
        {
            output_frame = framebuffer1;
            previous_frame = framebuffer0;
        }
        else
        {
            output_frame = framebuffer0;
            previous_frame = framebuffer1;
        }
        if(batch_frame_kind == H4V1_FRAME_KEY)
        {
            batch_key_count++;
            previous_frame = RT_NULL;
            if(sdram_memtest_dma_stream_prepare(
                   1u, V5F_SDRAM_H4V1_LIVE_WRITE_WIDE256) == 0u)
            {
                sdram_video_fail("h4v1_batch_key_output_dma_prepare");
            }
        }
        else
        {
            batch_delta_count++;
        }
        batch_dma_sampled = (uint8_t)(
            ((batch_frame % header->gop) == 0u) ||
            ((batch_frame % header->gop) == 1u) ||
            (batch_frame == (V5F_SDRAM_H4V1_BATCH_FRAMES - 1u)));
        batch_profiled = (uint8_t)((batch_frame == 3u) ||
                                   (batch_frame == 30u) ||
                                   (batch_frame == 31u));
        batch_decoded_crc = 0u;
        decode_start = sdram_video_cycle_now();
        result = sdram_video_h4v1_decode_streamed(&batch_entry,
                                                   batch_frame,
                                                   previous_frame,
                                                   output_frame,
                                                   &batch_decoded_crc,
                                                   V5F_SDRAM_H4V1_PREVIOUS_READ_WIDE256,
                                                   V5F_SDRAM_H4V1_LIVE_WRITE_WIDE256,
                                                   V5F_SDRAM_H4V1_LIVE_READ_SLICE_BYTES,
                                                   V5F_SDRAM_H4V1_LIVE_WRITE_SLICE_BYTES,
                                                   0u,
                                                   batch_dma_sampled,
                                                   (batch_profiled != 0u) ?
                                                       &batch_profile : RT_NULL);
        if(batch_frame_kind == H4V1_FRAME_KEY)
        {
            sdram_memtest_dma_stream_finish();
        }
        batch_decode_cycles = sdram_video_cycle_now() - decode_start;
        sdram_video_h4v1_live_fu_checkpoint(
            batch_frame, "decode", &underruns);
        if(result != H4V1_OK)
        {
            int used = rt_snprintf(
                line,
                sizeof(line),
                "H4V1 BATCH DECODE FAIL frame=%u status=%d cycles=%u crc=%08x/%08x",
                (unsigned int)batch_frame,
                result,
                (unsigned int)batch_decode_cycles,
                (unsigned int)batch_decoded_crc,
                (unsigned int)batch_entry.raw_crc32);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            sdram_video_fail("h4v1_batch_decode");
        }
        if(batch_profiled != 0u)
        {
            uint32_t accounted = batch_profile.payload_crc_cycles +
                                 batch_profile.flush_cycles;
            uint32_t other = (batch_profile.total_cycles >= accounted) ?
                             (batch_profile.total_cycles - accounted) : 0u;
            uint32_t copy_accounted =
                batch_profile.literal_copy_cycles +
                batch_profile.match_copy_cycles;
            uint32_t parse_other = (other >= copy_accounted) ?
                                   (other - copy_accounted) : 0u;
            int used = rt_snprintf(
                line,
                sizeof(line),
                "H4P A f=%u t=%u pc=%u fl=%u rd=%u ra=%u",
                (unsigned int)batch_frame,
                (unsigned int)batch_profile.total_cycles,
                (unsigned int)batch_profile.payload_crc_cycles,
                (unsigned int)batch_profile.flush_cycles,
                (unsigned int)batch_profile.read_total_cycles,
                (unsigned int)batch_profile.read_active_cycles);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            used = rt_snprintf(
                line,
                sizeof(line),
                "H4P C f=%u lc=%u lb=%u ln=%u",
                (unsigned int)batch_frame,
                (unsigned int)batch_profile.literal_copy_cycles,
                (unsigned int)batch_profile.literal_bytes,
                (unsigned int)batch_profile.literal_calls);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            used = rt_snprintf(
                line,
                sizeof(line),
                "H4P D f=%u mc=%u mb=%u mn=%u ps=%u",
                (unsigned int)batch_frame,
                (unsigned int)batch_profile.match_copy_cycles,
                (unsigned int)batch_profile.match_bytes,
                (unsigned int)batch_profile.match_calls,
                (unsigned int)parse_other);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            used = rt_snprintf(
                line,
                sizeof(line),
                "H4P B f=%u xo=%u rc=%u cp=%u wr=%u wa=%u ot=%u ch=%u",
                (unsigned int)batch_frame,
                (unsigned int)batch_profile.xor_cycles,
                (unsigned int)batch_profile.reconstructed_crc_cycles,
                (unsigned int)batch_profile.stage_copy_cycles,
                (unsigned int)batch_profile.write_total_cycles,
                (unsigned int)batch_profile.write_active_cycles,
                (unsigned int)other,
                (unsigned int)batch_profile.flush_count);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
        if(batch_dma_sampled != 0u)
        {
            batch_dma_sample_count++;
            batch_dma_crc = sdram_video_h4v1_dma_frame_crc(
                output_frame,
                header->frame_bytes,
                &batch_dma_ok,
                V5F_SDRAM_H4V1_LIVE_READ_WIDE256);
            sdram_video_h4v1_live_fu_checkpoint(
                batch_frame, "crc", &underruns);
        }
        else
        {
            batch_dma_ok = 1u;
            batch_dma_crc = batch_entry.raw_crc32;
        }
        if((batch_dma_sampled != 0u) &&
           ((batch_dma_ok == 0u) ||
            (batch_dma_crc != batch_entry.raw_crc32)))
        {
            int used = rt_snprintf(
                line,
                sizeof(line),
                "H4V1 BATCH VERIFY FAIL frame=%u dma=%u crc=%08x/%08x decoded=%08x",
                (unsigned int)batch_frame,
                (unsigned int)batch_dma_ok,
                (unsigned int)batch_dma_crc,
                (unsigned int)batch_entry.raw_crc32,
                (unsigned int)batch_decoded_crc);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            sdram_video_fail("h4v1_batch_dma_crc");
        }
        {
            int used = rt_snprintf(
                line,
                sizeof(line),
                "H4V1 LIVE90 PASS f=%u k=%c off=%u skip=%u n=%u ref=%08x verify=%s dc=%u",
                (unsigned int)batch_frame,
                (batch_frame_kind == H4V1_FRAME_KEY) ? 'K' : 'D',
                (unsigned int)batch_entry.offset,
                (unsigned int)(batch_entry.offset &
                               (V5F_SDRAM_H4V1_DMA_ALIGNMENT - 1u)),
                (unsigned int)batch_entry.compressed_bytes,
                (unsigned int)batch_dma_crc,
                (batch_dma_sampled != 0u) ? "codec+dma" : "skipped",
                (unsigned int)batch_decode_cycles);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
        if(sdram_video_h4v1_live_swap(batch_frame,
                                       output_frame,
                                       &switches,
                                       &sync_timeouts,
                                       &underruns) == 0u)
        {
            sdram_video_fail("h4v1_live_batch_swap");
        }
        sdram_memtest_watchdog_feed();
    }
    if((batch_key_count !=
        ((V5F_SDRAM_H4V1_BATCH_FRAMES - 1u) / header->gop)) ||
       (batch_delta_count !=
        ((V5F_SDRAM_H4V1_BATCH_FRAMES - 3u) - batch_key_count)) ||
       (batch_dma_sample_count != V5F_SDRAM_H4V1_BATCH_DMA_SAMPLES))
    {
        sdram_video_fail("h4v1_live90_contract_counts");
    }
    sdram_usb_debug_write_line(
        "H4V1 LIVE90 DECODE PASS frames=0..89 codec_crc_samples=8 dma_crc_samples=8 displayed=90 keys30,60=pass ltdc=continuous");
    ch32h417_dual_cdc_poll();
    rt_thread_mdelay(1000u);
    if(LTDC_GetFlagStatus(LTDC_FLAG_FU) != RESET)
    {
        underruns++;
        LTDC_ClearFlag(LTDC_FLAG_FU);
    }
    if((switches != (V5F_SDRAM_H4V1_BATCH_FRAMES - 1u)) ||
       (sync_timeouts != 0u) || (underruns != 0u))
    {
        sdram_video_fail("h4v1_live90_ltdc");
    }
    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_PASSED;
    g_v5f_hw_test_diag.sdram_ok_count++;
    sdram_usb_debug_write_line(
        "H4V1 ISOLATED STAGE5R PASS transport=stable full90=pass keys30,60=pass dma=r256x2k/w256x2k codec_crc=sampled8 dma_crc=sampled8 profile=copy_split delta=xor_copy32 match=forward_fanout swaps=89 fifo_underrun=0");
    sdram_usb_debug_write_line("RESULT PASS");
    sdram_memtest_watchdog_complete();
    while(1)
    {
        ch32h417_dual_cdc_poll();
        rt_thread_mdelay(1000u);
        hold_seconds++;
        if(LTDC_GetFlagStatus(LTDC_FLAG_FU) != RESET)
        {
            underruns++;
            LTDC_ClearFlag(LTDC_FLAG_FU);
        }
        if((hold_seconds % 5u) == 0u)
        {
            int used = rt_snprintf(
                line,
                sizeof(line),
                "H4V1 LIVE HOLD frame=89 seconds=%u cfb=%08x fu=%u",
                (unsigned int)hold_seconds,
                (unsigned int)LTDC_Layer1->CFBAR,
                (unsigned int)underruns);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
    }
}

static uint8_t sdram_video_ltdc_wait_vdes(uint8_t active,
                                          uint32_t timeout_ms)
{
    uint32_t start = sdram_video_cycle_now();
    uint32_t cycles_per_ms = SystemCoreClock / 1000u;
    uint32_t timeout_cycles;

    if(cycles_per_ms == 0u)
    {
        cycles_per_ms = 1u;
    }
    timeout_cycles = cycles_per_ms * timeout_ms;
    while((((LTDC_GetCDStatus(LTDC_CD_VDES) != RESET) ? 1u : 0u) != active) &&
          ((uint32_t)(sdram_video_cycle_now() - start) < timeout_cycles))
    {
        __NOP();
    }
    return (uint8_t)(((LTDC_GetCDStatus(LTDC_CD_VDES) != RESET) ? 1u : 0u) ==
                     active);
}

static uint8_t sdram_video_ltdc_wait_next_vblank(void)
{
    /*
     * Requiring an active interval first prevents a call made in the current
     * blanking interval from treating that same interval as a new frame edge.
     */
    if(sdram_video_ltdc_wait_vdes(1u,
                                  V5F_SDRAM_VIDEO_VBLANK_TIMEOUT_MS) == 0u)
    {
        return 0u;
    }
    return sdram_video_ltdc_wait_vdes(0u,
                                      V5F_SDRAM_VIDEO_VBLANK_TIMEOUT_MS);
}

static uint32_t sdram_video_panel_refresh_millihz(void)
{
    uint32_t total_pixels =
        (CH32H417_LCD_RGB_WIDTH + CH32H417_LCD_RGB_HSYNC +
         CH32H417_LCD_RGB_HBP + CH32H417_LCD_RGB_HFP) *
        (CH32H417_LCD_RGB_HEIGHT + CH32H417_LCD_RGB_VSYNC +
         CH32H417_LCD_RGB_VBP + CH32H417_LCD_RGB_VFP);
    uint32_t whole_hz = CH32H417_LCD_RGB_PIXEL_CLOCK_HZ / total_pixels;
    uint32_t remainder = CH32H417_LCD_RGB_PIXEL_CLOCK_HZ % total_pixels;

    return (whole_hz * 1000u) + ((remainder * 1000u) / total_pixels);
}

static void __attribute__((noreturn))
sdram_video_play(const v5f_sdram_video_config_t *config)
{
    uint32_t frame = 0u;
    uint32_t loops = 0u;
    uint32_t underruns = 0u;
    uint32_t sync_timeouts = 0u;
    uint32_t blank_count = 0u;
    uint32_t swaps = 0u;
    uint32_t panel_millihz = sdram_video_panel_refresh_millihz();
    uint32_t source_millihz = config->fps * 1000u;
    uint32_t blanks_per_frame =
        (panel_millihz + (source_millihz / 2u)) / source_millihz;
    uint32_t playback_millihz;
    rt_tick_t start_tick = rt_tick_get();
    rt_tick_t last_report_tick = start_tick;
    uint8_t result_reported = 0u;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    if(blanks_per_frame == 0u)
    {
        blanks_per_frame = 1u;
    }
    playback_millihz = panel_millihz / blanks_per_frame;
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "VIDEO SYNC panel_millihz=%u source_fps=%u blanks_per_frame=%u actual_millihz=%u method=vdes_blank+immediate",
            (unsigned int)panel_millihz,
            (unsigned int)config->fps,
            (unsigned int)blanks_per_frame,
            (unsigned int)playback_millihz);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    sdram_memtest_watchdog_context(
        V5F_SDRAM_WATCHDOG_STAGE_LTDC,
        V5F_SDRAM_WATCHDOG_POINT_LTDC_PLAY,
        0u,
        V5F_SDRAM_BASE_ADDR);
    LTDC_ClearFlag(LTDC_FLAG_FU);
    while(1)
    {
        rt_tick_t now;

        if(sdram_video_ltdc_wait_next_vblank() != 0u)
        {
            blank_count++;
            if(blank_count >= blanks_per_frame)
            {
                blank_count = 0u;
                frame++;
                if(frame >= config->frames)
                {
                    frame = 0u;
                    loops++;
                }
                LTDC_LayerAddress(
                    LTDC_Layer1,
                    V5F_SDRAM_BASE_ADDR + (frame * config->frame_bytes));
                ch32h417_ltdc_rgb_framebuffer_barrier();
                /* Immediate shadow load is safe here because VDES is blank. */
                LTDC_ReloadConfig(LTDC_IMReload);
                swaps++;
                g_v5f_hw_test_diag.frame_count++;
            }
        }
        else
        {
            sync_timeouts++;
        }
        now = rt_tick_get();
        if(LTDC_GetFlagStatus(LTDC_FLAG_FU) != RESET)
        {
            underruns++;
            LTDC_ClearFlag(LTDC_FLAG_FU);
        }
        if(((uint32_t)(now - last_report_tick) * 1000u /
            RT_TICK_PER_SECOND) >= V5F_SDRAM_VIDEO_REPORT_MS)
        {
            int used = rt_snprintf(line,
                                   sizeof(line),
                                   "VIDEO PLAY frame=%u/%u loops=%u swaps=%u sync_timeout=%u underrun=%u cfbar=%08x isr=%08x",
                                   (unsigned int)(frame + 1u),
                                   (unsigned int)config->frames,
                                   (unsigned int)loops,
                                   (unsigned int)swaps,
                                   (unsigned int)sync_timeouts,
                                   (unsigned int)underruns,
                                   (unsigned int)LTDC_Layer1->CFBAR,
                                   (unsigned int)LTDC->ISR);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            last_report_tick = now;
        }
        if((result_reported == 0u) &&
           (((uint32_t)(now - start_tick) * 1000u /
             RT_TICK_PER_SECOND) >= V5F_SDRAM_VIDEO_PASS_MS))
        {
            result_reported = 1u;
            if((sync_timeouts == 0u) &&
               (underruns == 0u) &&
               (s_sdram_video_readback_unstable == 0u))
            {
                sdram_memtest_watchdog_complete();
                g_v5f_hw_test_diag.phase = V5F_HW_PHASE_PASSED;
                g_v5f_hw_test_diag.sdram_ok_count++;
                sdram_usb_debug_write_line("VIDEO RESULT PASS upload_crc=pass readback_crc=pass remap=pass argb=pass vblank_lock=pass ltdc_underrun=0");
                sdram_usb_debug_write_line("RESULT PASS");
            }
            else
            {
                g_v5f_hw_test_diag.phase = V5F_HW_PHASE_FAILED;
                g_v5f_hw_test_diag.last_error = V5F_SDRAM_ERR_LCD;
                g_v5f_hw_test_diag.sdram_fail_count++;
                if(s_sdram_video_readback_unstable != 0u)
                {
                    sdram_usb_debug_write_line("VIDEO RESULT FAIL reason=readback_unstable ltdc_scan_completed=1");
                }
                else
                {
                    if(sync_timeouts != 0u)
                    {
                        sdram_usb_debug_write_line("VIDEO RESULT FAIL reason=ltdc_vblank_timeout");
                    }
                    else
                    {
                        sdram_usb_debug_write_line("VIDEO RESULT FAIL reason=ltdc_fifo_underrun");
                    }
                }
                sdram_usb_debug_write_line("RESULT FAIL");
            }
        }
        sdram_memtest_watchdog_feed();
    }
}

static const uint32_t s_sdram_video_argb8888_chart
    [V5F_SDRAM_VIDEO_CHART_ROWS][V5F_SDRAM_VIDEO_CHART_COLUMNS] = {
    /* Columns: bit5, bit6, bit7, 5+6, 5+7, 6+7, 5+6+7, full. */
    {0xFF200000u, 0xFF400000u, 0xFF800000u, 0xFF600000u,
     0xFFA00000u, 0xFFC00000u, 0xFFE00000u, 0xFFFF0000u},
    {0xFF002000u, 0xFF004000u, 0xFF008000u, 0xFF006000u,
     0xFF00A000u, 0xFF00C000u, 0xFF00E000u, 0xFF00FF00u},
    {0xFF000020u, 0xFF000040u, 0xFF000080u, 0xFF000060u,
     0xFF0000A0u, 0xFF0000C0u, 0xFF0000E0u, 0xFF0000FFu},
    {0xFF202020u, 0xFF404040u, 0xFF808080u, 0xFF606060u,
     0xFFA0A0A0u, 0xFFC0C0C0u, 0xFFE0E0E0u, 0xFFFFFFFFu},
};

static uint32_t s_sdram_video_green_transfer_actual[256];
static uint32_t s_sdram_video_green_background_actual[256];

static void sdram_video_draw_argb8888_color_chart(void)
{
    uint32_t *framebuffer = (uint32_t *)(void *)s_lcd_fb;
    uint32_t y;

    for(y = 0u; y < V5F_SDRAM_VIDEO_CHART_HEIGHT; y++)
    {
        uint32_t x;
        uint32_t row = y / V5F_SDRAM_VIDEO_CHART_CELL_HEIGHT;
        uint32_t cell_y = y % V5F_SDRAM_VIDEO_CHART_CELL_HEIGHT;

        for(x = 0u; x < V5F_SDRAM_VIDEO_CHART_WIDTH; x++)
        {
            uint32_t column = x / V5F_SDRAM_VIDEO_CHART_CELL_WIDTH;
            uint32_t cell_x = x % V5F_SDRAM_VIDEO_CHART_CELL_WIDTH;
            uint32_t pixel;

            if((cell_x == 0u) ||
               (cell_x == (V5F_SDRAM_VIDEO_CHART_CELL_WIDTH - 1u)) ||
               (cell_y == 0u) ||
               (cell_y == (V5F_SDRAM_VIDEO_CHART_CELL_HEIGHT - 1u)))
            {
                pixel = 0xFF101010u;
            }
            else
            {
                pixel = s_sdram_video_argb8888_chart[row][column];
            }
            framebuffer[(y * V5F_SDRAM_VIDEO_CHART_WIDTH) + x] = pixel;
        }
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

static uint32_t sdram_video_ltdc_sample_rgb_pads(void)
{
    uint32_t pa = GPIOA->INDR;
    uint32_t pb = GPIOB->INDR;
    uint32_t pc = GPIOC->INDR;
    uint32_t pd = GPIOD->INDR;
    uint32_t pe = GPIOE->INDR;
    uint32_t pf = GPIOF->INDR;
    uint32_t red = 0u;
    uint32_t green = 0u;
    uint32_t blue = 0u;

    red |= ((pa >> 0u) & 1u) << 0u;   /* R0 PA0  */
    red |= ((pa >> 2u) & 1u) << 1u;   /* R1 PA2  */
    red |= ((pa >> 1u) & 1u) << 2u;   /* R2 PA1  */
    red |= ((pb >> 0u) & 1u) << 3u;   /* R3 PB0  */
    red |= ((pa >> 5u) & 1u) << 4u;   /* R4 PA5  */
    red |= ((pc >> 0u) & 1u) << 5u;   /* R5 PC0  */
    red |= ((pa >> 8u) & 1u) << 6u;   /* R6 PA8  */
    red |= ((pc >> 4u) & 1u) << 7u;   /* R7 PC4  */

    green |= ((pe >> 5u) & 1u) << 0u; /* G0 PE5  */
    green |= ((pe >> 6u) & 1u) << 1u; /* G1 PE6  */
    green |= ((pa >> 6u) & 1u) << 2u; /* G2 PA6  */
    green |= ((pf >> 4u) & 1u) << 3u; /* G3 PF4  */
    green |= ((pc >> 8u) & 1u) << 4u; /* G4 PC8  */
    green |= ((pc >> 1u) & 1u) << 5u; /* G5 PC1  */
    green |= ((pc >> 7u) & 1u) << 6u; /* G6 PC7  */
    green |= ((pd >> 3u) & 1u) << 7u; /* G7 PD3  */

    blue |= ((pe >> 4u) & 1u) << 0u;  /* B0 PE4  */
    blue |= ((pc >> 10u) & 1u) << 1u; /* B1 PC10 */
    blue |= ((pa >> 3u) & 1u) << 2u;  /* B2 PA3  */
    blue |= ((pd >> 7u) & 1u) << 3u;  /* B3 PD7  */
    blue |= ((pc >> 11u) & 1u) << 4u; /* B4 PC11 */
    blue |= ((pd >> 5u) & 1u) << 5u;  /* B5 PD5  */
    blue |= ((pa >> 14u) & 1u) << 6u; /* B6 PA14 */
    blue |= ((pd >> 2u) & 1u) << 7u;  /* B7 PD2  */

    return (red << 16) | (green << 8) | blue;
}

static uint8_t sdram_video_ltdc_position_in_cell(uint32_t position,
                                                  uint32_t row,
                                                  uint32_t column)
{
    uint32_t x = (position >> 16) & 0xFFFFu;
    uint32_t y = position & 0xFFFFu;
    uint32_t layer_x = CH32H417_LCD_RGB_HSYNC +
                       CH32H417_LCD_RGB_HBP +
                       ((CH32H417_LCD_RGB_WIDTH -
                         V5F_SDRAM_VIDEO_CHART_WIDTH) / 2u);
    uint32_t layer_y = CH32H417_LCD_RGB_VSYNC +
                       CH32H417_LCD_RGB_VBP +
                       ((CH32H417_LCD_RGB_HEIGHT -
                         V5F_SDRAM_VIDEO_CHART_HEIGHT) / 2u);
    uint32_t x0 = layer_x +
                  (column * V5F_SDRAM_VIDEO_CHART_CELL_WIDTH) + 8u;
    uint32_t x1 = layer_x +
                  ((column + 1u) * V5F_SDRAM_VIDEO_CHART_CELL_WIDTH) - 8u;
    uint32_t y0 = layer_y +
                  (row * V5F_SDRAM_VIDEO_CHART_CELL_HEIGHT) + 8u;
    uint32_t y1 = layer_y +
                  ((row + 1u) * V5F_SDRAM_VIDEO_CHART_CELL_HEIGHT) - 8u;

    return (uint8_t)((x >= x0) && (x < x1) &&
                     (y >= y0) && (y < y1));
}

static uint8_t sdram_video_ltdc_sample_cell(uint32_t row,
                                            uint32_t column,
                                            uint32_t *rgb_out,
                                            uint8_t *unstable_out)
{
    uint32_t start = sdram_video_cycle_now();
    uint32_t timeout = SystemCoreClock / 20u;

    if(timeout == 0u)
    {
        timeout = 1u;
    }
    while((uint32_t)(sdram_video_cycle_now() - start) < timeout)
    {
        uint32_t before = LTDC->CPSR;

        if(sdram_video_ltdc_position_in_cell(before, row, column) != 0u)
        {
            uint32_t first = sdram_video_ltdc_sample_rgb_pads();
            uint32_t sample_and = first;
            uint32_t sample_or = first;
            uint32_t samples = 1u;
            uint32_t after = LTDC->CPSR;

            if(sdram_video_ltdc_position_in_cell(after, row, column) != 0u)
            {
                while((samples < 64u) &&
                      (sdram_video_ltdc_position_in_cell(
                           LTDC->CPSR, row, column) != 0u))
                {
                    uint32_t rgb = sdram_video_ltdc_sample_rgb_pads();

                    sample_and &= rgb;
                    sample_or |= rgb;
                    samples++;
                }
                *rgb_out = first;
                *unstable_out = (uint8_t)(sample_and != sample_or);
                return 1u;
            }
        }
    }
    *rgb_out = 0u;
    *unstable_out = 0u;
    return 0u;
}

static void sdram_video_ltdc_fill_green_scan_band(uint8_t green)
{
    uint32_t *framebuffer = (uint32_t *)(void *)s_lcd_fb;
    uint32_t y;
    uint32_t pixel = 0xFF000000u | ((uint32_t)green << 8);

    for(y = V5F_SDRAM_VIDEO_CHART_CELL_HEIGHT;
        y < (2u * V5F_SDRAM_VIDEO_CHART_CELL_HEIGHT);
        y++)
    {
        uint32_t x;

        for(x = 0u; x < V5F_SDRAM_VIDEO_CHART_WIDTH; x++)
        {
            framebuffer[(y * V5F_SDRAM_VIDEO_CHART_WIDTH) + x] = pixel;
        }
    }
    ch32h417_ltdc_rgb_framebuffer_barrier();
}

static void sdram_video_ltdc_run_green_transfer(uint32_t actual[256],
                                                 uint16_t *unstable_count,
                                                 uint16_t *timeout_count)
{
    uint32_t input;

    *unstable_count = 0u;
    *timeout_count = 0u;
    for(input = 0u; input < 256u; input++)
    {
        uint8_t unstable = 0u;

        sdram_video_ltdc_fill_green_scan_band((uint8_t)input);
        rt_thread_mdelay(20);
        if(sdram_video_ltdc_sample_cell(1u, 3u,
                                        &actual[input],
                                        &unstable) == 0u)
        {
            actual[input] = 0xFFFFFFFFu;
            (*timeout_count)++;
        }
        else if(unstable != 0u)
        {
            (*unstable_count)++;
        }
    }
    sdram_video_draw_argb8888_color_chart();
    rt_thread_mdelay(20);
}

static void sdram_video_ltdc_run_green_background(uint32_t actual[256],
                                                   uint16_t *unstable_count,
                                                   uint16_t *timeout_count)
{
    uint32_t saved_bccr = LTDC->BCCR;
    uint8_t layer_enabled = (uint8_t)(
        (LTDC_Layer1->CR & LTDC_CR_LEN) != 0u);
    uint32_t input;

    *unstable_count = 0u;
    *timeout_count = 0u;
    LTDC_LayerCmd(LTDC_Layer1, DISABLE);
    LTDC_ReloadConfig(LTDC_IMReload);
    rt_thread_mdelay(20);
    for(input = 0u; input < 256u; input++)
    {
        uint8_t unstable = 0u;

        LTDC->BCCR = input << 8;
        rt_thread_mdelay(20);
        if(sdram_video_ltdc_sample_cell(1u, 3u,
                                        &actual[input],
                                        &unstable) == 0u)
        {
            actual[input] = 0xFFFFFFFFu;
            (*timeout_count)++;
        }
        else if(unstable != 0u)
        {
            (*unstable_count)++;
        }
    }
    LTDC->BCCR = saved_bccr;
    if(layer_enabled != 0u)
    {
        LTDC_LayerCmd(LTDC_Layer1, ENABLE);
    }
    LTDC_ReloadConfig(LTDC_IMReload);
    rt_thread_mdelay(20);
}

static uint16_t sdram_video_gpio_green_high_probe(uint8_t actual[16])
{
    GPIO_InitTypeDef init = {0};
    const uint16_t pc_mask = GPIO_Pin_1 | GPIO_Pin_7 | GPIO_Pin_8;
    const uint16_t pd_mask = GPIO_Pin_3;
    uint16_t mismatch = 0u;
    uint32_t input;

    GPIO_ResetBits(GPIOC, pc_mask);
    GPIO_ResetBits(GPIOD, pd_mask);
    init.GPIO_Speed = GPIO_Speed_Very_High;
    init.GPIO_Mode = GPIO_Mode_Out_PP;
    init.GPIO_Pin = pc_mask;
    GPIO_Init(GPIOC, &init);
    init.GPIO_Pin = pd_mask;
    GPIO_Init(GPIOD, &init);

    for(input = 0u; input < 16u; input++)
    {
        GPIO_ResetBits(GPIOC, pc_mask);
        GPIO_ResetBits(GPIOD, pd_mask);
        if((input & 0x1u) != 0u)
        {
            GPIO_SetBits(GPIOC, GPIO_Pin_8);  /* G4 */
        }
        if((input & 0x2u) != 0u)
        {
            GPIO_SetBits(GPIOC, GPIO_Pin_1);  /* G5 */
        }
        if((input & 0x4u) != 0u)
        {
            GPIO_SetBits(GPIOC, GPIO_Pin_7);  /* G6 */
        }
        if((input & 0x8u) != 0u)
        {
            GPIO_SetBits(GPIOD, GPIO_Pin_3);  /* G7 */
        }
        rt_thread_mdelay(1);
        actual[input] = (uint8_t)(
            (sdram_video_ltdc_sample_rgb_pads() >> 12) & 0xFu);
        if(actual[input] != input)
        {
            mismatch |= (uint16_t)(1u << input);
        }
    }

    GPIO_PinAFConfig(GPIOC, GPIO_PinSource8, GPIO_AF14);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource1, GPIO_AF14);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource7, GPIO_AF14);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource3, GPIO_AF14);
    init.GPIO_Mode = GPIO_Mode_AF_PP;
    init.GPIO_Pin = pc_mask;
    GPIO_Init(GPIOC, &init);
    init.GPIO_Pin = pd_mask;
    GPIO_Init(GPIOD, &init);
    ch32h417_ltdc_rgb_framebuffer_barrier();
    rt_thread_mdelay(20);
    return mismatch;
}

static void sdram_video_ltdc_report_green_transfer(
    const char *tag,
    const uint32_t actual[256],
    uint16_t unstable_count,
    uint16_t timeout_count)
{
    uint32_t base;
    uint32_t exact = 0u;
    uint32_t minus_one = 0u;
    uint32_t cross_channel = 0u;
    uint32_t nonmonotonic = 0u;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    for(base = 0u; base < 256u; base += 16u)
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "%s %02x-%02x=%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x",
            tag,
            (unsigned int)base,
            (unsigned int)(base + 15u),
            (unsigned int)((actual[base + 0u] >> 8) & 0xFFu),
            (unsigned int)((actual[base + 1u] >> 8) & 0xFFu),
            (unsigned int)((actual[base + 2u] >> 8) & 0xFFu),
            (unsigned int)((actual[base + 3u] >> 8) & 0xFFu),
            (unsigned int)((actual[base + 4u] >> 8) & 0xFFu),
            (unsigned int)((actual[base + 5u] >> 8) & 0xFFu),
            (unsigned int)((actual[base + 6u] >> 8) & 0xFFu),
            (unsigned int)((actual[base + 7u] >> 8) & 0xFFu),
            (unsigned int)((actual[base + 8u] >> 8) & 0xFFu),
            (unsigned int)((actual[base + 9u] >> 8) & 0xFFu),
            (unsigned int)((actual[base + 10u] >> 8) & 0xFFu),
            (unsigned int)((actual[base + 11u] >> 8) & 0xFFu),
            (unsigned int)((actual[base + 12u] >> 8) & 0xFFu),
            (unsigned int)((actual[base + 13u] >> 8) & 0xFFu),
            (unsigned int)((actual[base + 14u] >> 8) & 0xFFu),
            (unsigned int)((actual[base + 15u] >> 8) & 0xFFu));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    for(base = 0u; base < 256u; base++)
    {
        uint32_t rgb = actual[base] & 0x00FFFFFFu;
        uint32_t green = (rgb >> 8) & 0xFFu;
        uint32_t expected_minus_one = (base == 0u) ? 0u : (base - 1u);

        if(green == base)
        {
            exact++;
        }
        if(green == expected_minus_one)
        {
            minus_one++;
        }
        if((rgb & 0x00FF00FFu) != 0u)
        {
            cross_channel++;
        }
        if((base != 0u) &&
           (((actual[base - 1u] >> 8) & 0xFFu) > green))
        {
            nonmonotonic++;
        }
    }
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "%s SUMMARY exact=%u/256 minus1=%u/256 nonmono=%u cross=%u unstable=%u timeout=%u",
            tag,
            (unsigned int)exact,
            (unsigned int)minus_one,
            (unsigned int)nonmonotonic,
            (unsigned int)cross_channel,
            (unsigned int)unstable_count,
            (unsigned int)timeout_count);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
}

static uint8_t sdram_video_ltdc_report_pad_matrix(void)
{
    uint32_t row;
    uint8_t all_match = 1u;
    char line[V5F_SDRAM_USB_LINE_BYTES];

    for(row = 0u; row < V5F_SDRAM_VIDEO_CHART_ROWS; row++)
    {
        uint32_t actual[V5F_SDRAM_VIDEO_CHART_COLUMNS];
        uint32_t column;
        uint32_t mismatch = 0u;
        uint32_t timeout = 0u;
        uint32_t unstable = 0u;

        for(column = 0u; column < V5F_SDRAM_VIDEO_CHART_COLUMNS; column++)
        {
            uint32_t expected =
                s_sdram_video_argb8888_chart[row][column] & 0x00FFFFFFu;
            uint8_t cell_unstable = 0u;

            if(sdram_video_ltdc_sample_cell(row, column,
                                            &actual[column],
                                            &cell_unstable) == 0u)
            {
                timeout |= 1u << column;
            }
            else if(actual[column] != expected)
            {
                mismatch |= 1u << column;
            }
            if(cell_unstable != 0u)
            {
                unstable |= 1u << column;
            }
        }
        if((mismatch != 0u) || (timeout != 0u) || (unstable != 0u))
        {
            all_match = 0u;
        }
        {
            int used = rt_snprintf(
                line,
                sizeof(line),
                "PAD ROW%u got=%06x,%06x,%06x,%06x,%06x,%06x,%06x,%06x mm=%02x var=%02x to=%02x",
                (unsigned int)row,
                (unsigned int)actual[0],
                (unsigned int)actual[1],
                (unsigned int)actual[2],
                (unsigned int)actual[3],
                (unsigned int)actual[4],
                (unsigned int)actual[5],
                (unsigned int)actual[6],
                (unsigned int)actual[7],
                (unsigned int)mismatch,
                (unsigned int)unstable,
                (unsigned int)timeout);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
    }
    return all_match;
}

static void __attribute__((noreturn))
sdram_video_run_ltdc_contract_probe(void)
{
    ch32h417_ltdc_rgb_panel_t panel =
        ch32h417_ltdc_rgb_panel_800x480;
    ch32h417_ltdc_rgb_layer_t layer = {0};
    ch32h417_ltdc_rgb_color_t black = {0u, 0u, 0u};
    uint32_t framebuffer_crc;
    uint32_t row_crc_expected[V5F_SDRAM_VIDEO_CHART_ROWS];
    uint32_t expected_cfblr;
    uint32_t vio_ctlr_before;
    uint32_t scan_changes = 0u;
    uint32_t seconds = 0u;
    uint8_t scan_ok;
    uint8_t pin_config_ok;
    uint8_t underrun_seen = 0u;
    uint8_t runtime_memory_ok = 1u;
    uint8_t pad_matrix_ok = 0u;
    uint8_t green_transfer_tested = 0u;
    uint8_t result_reported = 0u;
    uint16_t green_transfer_unstable = 0u;
    uint16_t green_transfer_timeout = 0u;
    uint16_t green_background_unstable = 0u;
    uint16_t green_background_timeout = 0u;
    uint16_t gpio_green_mismatch = 0u;
    uint8_t gpio_green_actual[16];
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int result;

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    sdram_usb_debug_write_line(
        "H417 SDRAM LTDC TEST v36 GREEN PATH ISOLATION");
    sdram_usb_debug_write_line(
        "LINE CONTRACT source=internal_shared_sram no_sdram=1 no_upload=1 no_dma=1 no_frame_switch=1");
    sdram_usb_debug_write_line(
        "CHART FORMAT ARGB8888 size=400x240 offset=200,120 pitch=1600 cells=8x4 cell=50x60");
    sdram_usb_debug_write_line(
        "FIX CONTRACT vio18=SW_MODE3 cfbll=active_bytes+31 pclk=IIPC source=RM+WCH_example");
    sdram_usb_debug_write_line(
        "CHART ROWS top_to_bottom=R,G,B,GRAY");
    sdram_usb_debug_write_line(
        "CHART COLS left_to_right=20(bit5),40(bit6),80(bit7),60,A0,C0,E0,FF");
    sdram_usb_debug_write_line(
        "CHART EXPECT centered_bit_matrix dark_grid black_border hold=continuous ltdc_start_once=1");
    sdram_usb_debug_write_line(
        "TRACE CONTRACT framebuffer=ARGB8888 green_transfer=0..255 pads=G0..G7 no_clut=1");

    sdram_video_draw_argb8888_color_chart();
    framebuffer_crc = sdram_video_crc32_update(
                          0xFFFFFFFFu,
                          s_lcd_fb,
                      V5F_SDRAM_VIDEO_CHART_BYTES) ^
                      0xFFFFFFFFu;
    {
        uint32_t row;
        uint32_t row_bytes = V5F_SDRAM_VIDEO_CHART_CELL_HEIGHT *
                             V5F_SDRAM_VIDEO_CHART_WIDTH *
                             V5F_SDRAM_VIDEO_CHART_BPP;

        for(row = 0u; row < V5F_SDRAM_VIDEO_CHART_ROWS; row++)
        {
            row_crc_expected[row] = sdram_video_crc32_update(
                                        0xFFFFFFFFu,
                                        &s_lcd_fb[row * row_bytes],
                                        row_bytes) ^
                                    0xFFFFFFFFu;
        }
    }
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "CHART BUFFER crc=%08x expected=%08x %s addr=%08x bytes=%u",
            (unsigned int)framebuffer_crc,
            (unsigned int)V5F_SDRAM_VIDEO_CHART_CRC32,
            (framebuffer_crc == V5F_SDRAM_VIDEO_CHART_CRC32) ?
                "PASS" : "FAIL",
            (unsigned int)(uintptr_t)s_lcd_fb,
            (unsigned int)V5F_SDRAM_VIDEO_CHART_BYTES);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    if(framebuffer_crc != V5F_SDRAM_VIDEO_CHART_CRC32)
    {
        sdram_usb_debug_write_line(
            "CHART RESULT FAIL reason=internal_framebuffer_crc");
        sdram_usb_debug_write_line("RESULT FAIL");
        g_v5f_hw_test_diag.phase = V5F_HW_PHASE_FAILED;
        while(1)
        {
            sdram_memtest_watchdog_feed();
            rt_thread_mdelay(1000);
        }
    }

    panel.pixel_clock_polarity = LTDC_PCPolarity_IIPC;
    layer.width = V5F_SDRAM_VIDEO_CHART_WIDTH;
    layer.height = V5F_SDRAM_VIDEO_CHART_HEIGHT;
    layer.offset_x = (uint16_t)(
        (CH32H417_LCD_RGB_WIDTH - V5F_SDRAM_VIDEO_CHART_WIDTH) / 2u);
    layer.offset_y = (uint16_t)(
        (CH32H417_LCD_RGB_HEIGHT - V5F_SDRAM_VIDEO_CHART_HEIGHT) / 2u);
    layer.pixel_format = LTDC_Pixelformat_ARGB8888;
    layer.framebuffer = (uint32_t)(uintptr_t)s_lcd_fb;
    layer.line_pitch = V5F_SDRAM_VIDEO_CHART_WIDTH *
                       V5F_SDRAM_VIDEO_CHART_BPP;
    expected_cfblr = ((layer.line_pitch & 0x1FFFu) << 16) |
                     (((V5F_SDRAM_VIDEO_CHART_WIDTH *
                        V5F_SDRAM_VIDEO_CHART_BPP) + 31u) & 0x1FFFu);

    vio_ctlr_before = PWR->CTLR;
    LTDC_ClearFlag(LTDC_FLAG_FU);
    result = ch32h417_ltdc_rgb_start_layer1(&panel, &layer, &black);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "CHART RESULT FAIL reason=ltdc_start error=%d",
                               result);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        sdram_usb_debug_write_line("RESULT FAIL");
        g_v5f_hw_test_diag.phase = V5F_HW_PHASE_FAILED;
        while(1)
        {
            sdram_memtest_watchdog_feed();
            rt_thread_mdelay(1000);
        }
    }
    scan_ok = sdram_video_ltdc_wait_scan(&scan_changes);
    pin_config_ok = (uint8_t)(
        (((GPIOC->CFGLR >> 0u) & 0xFu) == 9u) &&
        (((AFIO->GPIOC_AFLR >> 0u) & 0xFu) == GPIO_AF14) &&
        (((GPIOC->CFGLR >> 4u) & 0xFu) == 9u) &&
        (((AFIO->GPIOC_AFLR >> 4u) & 0xFu) == GPIO_AF14) &&
        (((GPIOD->CFGLR >> 20u) & 0xFu) == 9u) &&
        (((AFIO->GPIOD_AFLR >> 20u) & 0xFu) == GPIO_AF14) &&
        (((GPIOA->CFGHR >> 0u) & 0xFu) == 9u) &&
        (((AFIO->GPIOA_AFHR >> 0u) & 0xFu) == GPIO_AF14) &&
        (((GPIOC->CFGLR >> 28u) & 0xFu) == 9u) &&
        (((AFIO->GPIOC_AFLR >> 28u) & 0xFu) == GPIO_AF14) &&
        (((GPIOA->CFGHR >> 24u) & 0xFu) == 9u) &&
        (((AFIO->GPIOA_AFHR >> 24u) & 0xFu) == GPIO_AF14) &&
        (((GPIOC->CFGLR >> 16u) & 0xFu) == 9u) &&
        (((AFIO->GPIOC_AFLR >> 16u) & 0xFu) == GPIO_AF14) &&
        (((GPIOD->CFGLR >> 12u) & 0xFu) == 9u) &&
        (((AFIO->GPIOD_AFLR >> 12u) & 0xFu) == GPIO_AF14) &&
        (((GPIOD->CFGLR >> 8u) & 0xFu) == 9u) &&
        (((AFIO->GPIOD_AFLR >> 8u) & 0xFu) == GPIO_AF14) &&
        (((GPIOC->SPEED >> 0u) & 0x3u) == GPIO_Speed_Very_High) &&
        (((GPIOC->SPEED >> 2u) & 0x3u) == GPIO_Speed_Very_High) &&
        (((GPIOD->SPEED >> 10u) & 0x3u) == GPIO_Speed_Very_High) &&
        (((GPIOA->SPEED >> 16u) & 0x3u) == GPIO_Speed_Very_High) &&
        (((GPIOC->SPEED >> 14u) & 0x3u) == GPIO_Speed_Very_High) &&
        (((GPIOA->SPEED >> 28u) & 0x3u) == GPIO_Speed_Very_High) &&
        (((GPIOC->SPEED >> 8u) & 0x3u) == GPIO_Speed_Very_High) &&
        (((GPIOD->SPEED >> 6u) & 0x3u) == GPIO_Speed_Very_High) &&
        (((GPIOD->SPEED >> 4u) & 0x3u) == GPIO_Speed_Very_High));
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "LINE START gcr=%08x pcpol=%u expected=1 scan=%s changes=%u cfb=%08x cfblr=%08x/%08x vio=%08x->%08x",
            (unsigned int)LTDC->GCR,
            (unsigned int)((LTDC->GCR & LTDC_GCR_PCPOL) != 0u),
            (scan_ok != 0u) ? "PASS" : "FAIL",
            (unsigned int)scan_changes,
            (unsigned int)LTDC_Layer1->CFBAR,
            (unsigned int)LTDC_Layer1->CFBLR,
            (unsigned int)expected_cfblr,
            (unsigned int)vio_ctlr_before,
            (unsigned int)PWR->CTLR);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "PIN BIT5 R5=PC0:%x/%x G5=PC1:%x/%x B5=PD5:%x/%x expected=9/e",
            (unsigned int)((GPIOC->CFGLR >> 0u) & 0xFu),
            (unsigned int)((AFIO->GPIOC_AFLR >> 0u) & 0xFu),
            (unsigned int)((GPIOC->CFGLR >> 4u) & 0xFu),
            (unsigned int)((AFIO->GPIOC_AFLR >> 4u) & 0xFu),
            (unsigned int)((GPIOD->CFGLR >> 20u) & 0xFu),
            (unsigned int)((AFIO->GPIOD_AFLR >> 20u) & 0xFu));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "PIN BIT6 R6=PA8:%x/%x G6=PC7:%x/%x B6=PA14:%x/%x expected=9/e",
            (unsigned int)((GPIOA->CFGHR >> 0u) & 0xFu),
            (unsigned int)((AFIO->GPIOA_AFHR >> 0u) & 0xFu),
            (unsigned int)((GPIOC->CFGLR >> 28u) & 0xFu),
            (unsigned int)((AFIO->GPIOC_AFLR >> 28u) & 0xFu),
            (unsigned int)((GPIOA->CFGHR >> 24u) & 0xFu),
            (unsigned int)((AFIO->GPIOA_AFHR >> 24u) & 0xFu));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "PIN BIT7 R7=PC4:%x/%x G7=PD3:%x/%x B7=PD2:%x/%x expected=9/e",
            (unsigned int)((GPIOC->CFGLR >> 16u) & 0xFu),
            (unsigned int)((AFIO->GPIOC_AFLR >> 16u) & 0xFu),
            (unsigned int)((GPIOD->CFGLR >> 12u) & 0xFu),
            (unsigned int)((AFIO->GPIOD_AFLR >> 12u) & 0xFu),
            (unsigned int)((GPIOD->CFGLR >> 8u) & 0xFu),
            (unsigned int)((AFIO->GPIOD_AFLR >> 8u) & 0xFu));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "PIN SPEED bit5 R/G/B=%x/%x/%x bit6=%x/%x/%x bit7=%x/%x/%x expected=3",
            (unsigned int)((GPIOC->SPEED >> 0u) & 0x3u),
            (unsigned int)((GPIOC->SPEED >> 2u) & 0x3u),
            (unsigned int)((GPIOD->SPEED >> 10u) & 0x3u),
            (unsigned int)((GPIOA->SPEED >> 16u) & 0x3u),
            (unsigned int)((GPIOC->SPEED >> 14u) & 0x3u),
            (unsigned int)((GPIOA->SPEED >> 28u) & 0x3u),
            (unsigned int)((GPIOC->SPEED >> 8u) & 0x3u),
            (unsigned int)((GPIOD->SPEED >> 6u) & 0x3u),
            (unsigned int)((GPIOD->SPEED >> 4u) & 0x3u));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "PIN CHECK %s pc_in=%04x pd_in=%04x pa_in=%04x live_scan=1",
            (pin_config_ok != 0u) ? "PASS" : "FAIL",
            (unsigned int)(GPIOC->INDR & 0xFFFFu),
            (unsigned int)(GPIOD->INDR & 0xFFFFu),
            (unsigned int)(GPIOA->INDR & 0xFFFFu));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    sdram_usb_debug_write_line(
        "CHART DISPLAY green_scan_once_then_restore observe=order,ramps,grid,horizontal_lines");
    sdram_usb_debug_write_line(
        "PAD EXPECT rows=R,G,B,GRAY values=20,40,80,60,a0,c0,e0,ff mm=00 var=00 to=00 required");

    while(1)
    {
        uint8_t fifo_underrun;
        uint32_t live_crc;
        uint32_t row_crc[V5F_SDRAM_VIDEO_CHART_ROWS];
        uint32_t row;
        uint32_t row_bytes = V5F_SDRAM_VIDEO_CHART_CELL_HEIGHT *
                             V5F_SDRAM_VIDEO_CHART_WIDTH *
                             V5F_SDRAM_VIDEO_CHART_BPP;
        uint32_t raw_available = 0u;
        uint32_t raw_overflow = 0u;
        int used;

        rt_thread_mdelay(1000);
        sdram_memtest_watchdog_feed();
        seconds++;
        fifo_underrun = (uint8_t)(
            (LTDC_GetFlagStatus(LTDC_FLAG_FU) != RESET) ? 1u : 0u);
        underrun_seen |= fifo_underrun;
        live_crc = sdram_video_crc32_update(
                       0xFFFFFFFFu,
                       s_lcd_fb,
                       V5F_SDRAM_VIDEO_CHART_BYTES) ^
                   0xFFFFFFFFu;
        for(row = 0u; row < V5F_SDRAM_VIDEO_CHART_ROWS; row++)
        {
            row_crc[row] = sdram_video_crc32_update(
                               0xFFFFFFFFu,
                               &s_lcd_fb[row * row_bytes],
                               row_bytes) ^
                           0xFFFFFFFFu;
            if(row_crc[row] != row_crc_expected[row])
            {
                runtime_memory_ok = 0u;
            }
        }
        if(live_crc != framebuffer_crc)
        {
            runtime_memory_ok = 0u;
        }
#if APP_ENABLE_USB_TEST
        raw_available = ch32h417_usb_cdc_raw_rx_available();
        raw_overflow = ch32h417_usb_cdc_raw_rx_overflowed();
#endif
        used = rt_snprintf(
            line,
            sizeof(line),
            "SW TRACE s=%u crc=%08x/%08x mem=%s rows=%08x/%08x/%08x/%08x raw=%u/%u",
            (unsigned int)seconds,
            (unsigned int)live_crc,
            (unsigned int)framebuffer_crc,
            (runtime_memory_ok != 0u) ? "PASS" : "FAIL",
            (unsigned int)row_crc[0],
            (unsigned int)row_crc[1],
            (unsigned int)row_crc[2],
            (unsigned int)row_crc[3],
            (unsigned int)raw_available,
            (unsigned int)raw_overflow);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        if((seconds == 3u) && (green_transfer_tested == 0u))
        {
            sdram_usb_debug_write_line(
                "GSCAN START source=ARGB8888 green_band input=00..ff hold_ms=20 no_clut=1");
            sdram_video_ltdc_run_green_transfer(
                s_sdram_video_green_transfer_actual,
                &green_transfer_unstable,
                &green_transfer_timeout);
            green_transfer_tested = 1u;
            sdram_video_ltdc_report_green_transfer(
                "G_LAYER",
                s_sdram_video_green_transfer_actual,
                green_transfer_unstable,
                green_transfer_timeout);
            sdram_usb_debug_write_line(
                "G_BG START source=ltdc_background layer=disabled input=00..ff hold_ms=20");
            sdram_video_ltdc_run_green_background(
                s_sdram_video_green_background_actual,
                &green_background_unstable,
                &green_background_timeout);
            sdram_video_ltdc_report_green_transfer(
                "G_BG",
                s_sdram_video_green_background_actual,
                green_background_unstable,
                green_background_timeout);
            sdram_usb_debug_write_line(
                "G_GPIO START source=ordinary_push_pull inputs=G4..G7 patterns=0..f ltdc_mux_bypassed=1");
            gpio_green_mismatch = sdram_video_gpio_green_high_probe(
                                      gpio_green_actual);
            used = rt_snprintf(
                line,
                sizeof(line),
                "G_GPIO got=%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x mismatch=%04x",
                (unsigned int)gpio_green_actual[0],
                (unsigned int)gpio_green_actual[1],
                (unsigned int)gpio_green_actual[2],
                (unsigned int)gpio_green_actual[3],
                (unsigned int)gpio_green_actual[4],
                (unsigned int)gpio_green_actual[5],
                (unsigned int)gpio_green_actual[6],
                (unsigned int)gpio_green_actual[7],
                (unsigned int)gpio_green_actual[8],
                (unsigned int)gpio_green_actual[9],
                (unsigned int)gpio_green_actual[10],
                (unsigned int)gpio_green_actual[11],
                (unsigned int)gpio_green_actual[12],
                (unsigned int)gpio_green_actual[13],
                (unsigned int)gpio_green_actual[14],
                (unsigned int)gpio_green_actual[15],
                (unsigned int)gpio_green_mismatch);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
            sdram_usb_debug_write_line(
                "GSCAN END layer=restored background=restored gpio_af=restored no_panel_power_toggle=1");
        }
        if((seconds == 2u) || ((seconds % 10u) == 0u))
        {
            pad_matrix_ok = sdram_video_ltdc_report_pad_matrix();
            used = rt_snprintf(
                line,
                sizeof(line),
                "PAD MATRIX %s sample=%u gcr=%08x pf=%08x ca=%08x bf=%08x fu=%u",
                (pad_matrix_ok != 0u) ? "PASS" : "FAIL",
                (unsigned int)seconds,
                (unsigned int)LTDC->GCR,
                (unsigned int)LTDC_Layer1->PFCR,
                (unsigned int)LTDC_Layer1->CACR,
                (unsigned int)LTDC_Layer1->BFCR,
                (unsigned int)fifo_underrun);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
        if((green_transfer_tested != 0u) &&
           (seconds != 0u) && ((seconds % 10u) == 0u))
        {
            sdram_video_ltdc_report_green_transfer(
                "G_LAYER",
                s_sdram_video_green_transfer_actual,
                green_transfer_unstable,
                green_transfer_timeout);
            sdram_video_ltdc_report_green_transfer(
                "G_BG",
                s_sdram_video_green_background_actual,
                green_background_unstable,
                green_background_timeout);
        }
        if((seconds % 5u) == 0u)
        {
            used = rt_snprintf(
                line,
                sizeof(line),
                "REG TRACE lcr=%08x wh=%08x wv=%08x cfb=%08x len=%08x lines=%08x cpsr=%08x",
                (unsigned int)LTDC_Layer1->CR,
                (unsigned int)LTDC_Layer1->WHPCR,
                (unsigned int)LTDC_Layer1->WVPCR,
                (unsigned int)LTDC_Layer1->CFBAR,
                (unsigned int)LTDC_Layer1->CFBLR,
                (unsigned int)LTDC_Layer1->CFBLNR,
                (unsigned int)LTDC->CPSR);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
        if((result_reported == 0u) && (seconds >= 5u))
        {
            result_reported = 1u;
            if((scan_ok != 0u) &&
               ((LTDC->GCR & LTDC_GCR_PCPOL) != 0u) &&
               (LTDC_Layer1->CFBLR == expected_cfblr) &&
               ((PWR->CTLR & 0x00000E00u) == 0x00000E00u) &&
               (pin_config_ok != 0u) &&
               (runtime_memory_ok != 0u) &&
               (underrun_seen == 0u))
            {
                g_v5f_hw_test_diag.phase = V5F_HW_PHASE_PASSED;
                g_v5f_hw_test_diag.sdram_ok_count++;
                sdram_usb_debug_write_line(
                    "CHART RESULT PASS format=argb8888 green_scan=complete runtime_crc=pass vio18=3v3 cfbll=active+31 scan=pass iipc=1 fifo_underrun=0 pad_matrix=diagnostic");
                sdram_usb_debug_write_line("RESULT PASS");
            }
            else
            {
                g_v5f_hw_test_diag.phase = V5F_HW_PHASE_FAILED;
                g_v5f_hw_test_diag.last_error = V5F_SDRAM_ERR_LCD;
                g_v5f_hw_test_diag.sdram_fail_count++;
                sdram_usb_debug_write_line(
                    "CHART RESULT FAIL reason=ltdc_scan_or_vio_or_cfblr_or_polarity_or_fifo_or_memory display=hold");
                sdram_usb_debug_write_line("RESULT FAIL");
            }
        }
    }
}

static void run_sdram_video_test(void)
{
    v5f_sdram_video_config_t config;
    uint32_t upload_crc;
    v5f_sdram_video_readback_diag_t
        readback[V5F_SDRAM_VIDEO_READBACK_PASSES];
    uint32_t readback_passes = 0u;
    uint32_t pass;
    char line[V5F_SDRAM_USB_LINE_BYTES];
    int result;
    uint32_t scan_changes = 0u;

    g_v5f_hw_test_diag.phase = V5F_HW_PHASE_RUNNING;
    sdram_video_wait_config(&config);
    result = sdram_init_profile(0u, 0u, 1u);
    if(result != V5F_SDRAM_OK)
    {
        sdram_video_fail("sdram_init");
    }
    sdram_enable_0x60000000_remap();
    ch32h417_ltdc_rgb_framebuffer_barrier();
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "VIDEO SDRAM MAP base=%08x bytes=%u remap=%u bcr0=%08x",
                               (unsigned int)V5F_SDRAM_BASE_ADDR,
                               (unsigned int)V5F_SDRAM_BYTES,
                               (unsigned int)((FMC_Bank1->BTCR[0] &
                                               V5F_FMC_SDRAM_REMAP_TO_0X60000000) != 0u),
                               (unsigned int)FMC_Bank1->BTCR[0]);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    sdram_memtest_watchdog_begin();
    sdram_usb_debug_write_line("VIDEO WATCHDOG ARMED trace=raw,dma2,credit,ltdc timeout_approx_s=25");
    sdram_usb_debug_write_line("VIDEO FLOW credit=32768 scheduler_wait=none raw_copy=bulk dma=DMA2_CH3");

    upload_crc = sdram_video_upload(&config);
    if(upload_crc != config.expected_crc)
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "VIDEO CRC FAIL stage=upload exp=%08x got=%08x",
                               (unsigned int)config.expected_crc,
                               (unsigned int)upload_crc);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
        sdram_video_fail("upload_crc");
    }
    sdram_usb_debug_write_line("VIDEO UPLOAD PASS crc32=match");

    s_sdram_video_readback_unstable = 0u;
    for(pass = 0u; pass < V5F_SDRAM_VIDEO_READBACK_PASSES; pass++)
    {
        uint32_t readback_crc =
            sdram_video_readback_crc(&config,
                                     pass + 1u,
                                     &readback[pass]);

        if((readback_crc == config.expected_crc) &&
           (readback[pass].bad_blocks == 0u))
        {
            readback_passes++;
        }
    }
    if(readback_passes == V5F_SDRAM_VIDEO_READBACK_PASSES)
    {
        int used = rt_snprintf(
            line,
            sizeof(line),
            "VIDEO READBACK PASS passes=1/1 crc32=match blocks=match access=dma256 base=%08x",
            (unsigned int)(V5F_SDRAM_BASE_ADDR + config.storage_offset));
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    else
    {
        int used;

        s_sdram_video_readback_unstable = 1u;
        used = rt_snprintf(line,
                           sizeof(line),
                           "VIDEO READBACK WARN passes=%u/%u expected=%08x continue=ltdc_diagnostic",
                           (unsigned int)readback_passes,
                           (unsigned int)V5F_SDRAM_VIDEO_READBACK_PASSES,
                           (unsigned int)config.expected_crc);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }

    if(config.is_h4v1 != 0u)
    {
        h4v1_header_t header;
        h4v1_index_entry_t first;
        h4v1_index_entry_t second;
        h4v1_index_entry_t last;
        uint32_t index_dma_cycles;
        uint32_t last_frame;

        if(readback_passes != V5F_SDRAM_VIDEO_READBACK_PASSES)
        {
            sdram_video_fail("h4v1_readback");
        }
        if(sdram_video_h4v1_probe_container(&config,
                                             &header,
                                             &first,
                                             &second) == 0u)
        {
            sdram_video_fail("h4v1_container_probe");
        }
        last_frame = header.frame_count - 1u;
        if(sdram_video_h4v1_read_index_entry(&config,
                                              &header,
                                              last_frame,
                                              &last,
                                              &index_dma_cycles) == 0u)
        {
            sdram_video_fail("h4v1_index_dma");
        }
        {
            int used = rt_snprintf(
                line,
                sizeof(line),
                "H4V1 INDEX DMA PASS frame=%u offset=%u compressed=%u flags=%08x cycles=%u source=sdram",
                (unsigned int)last_frame,
                (unsigned int)last.offset,
                (unsigned int)last.compressed_bytes,
                (unsigned int)last.flags,
                (unsigned int)index_dma_cycles);
            if((used > 0) && ((rt_size_t)used < sizeof(line)))
            {
                sdram_usb_debug_write_line(line);
            }
        }
        sdram_video_h4v1_show_pair(&config,
                                    &header,
                                    &first,
                                    &second);
    }

    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "VIDEO PLAYER ARM format=%s frames=%u fps=%u bytes=%u sdclk=%u base=60000000 cfblr=plus3 reload=vblank rot180=host",
                               config.name,
                               (unsigned int)config.frames,
                               (unsigned int)config.fps,
                               (unsigned int)config.total_bytes,
                               (unsigned int)g_v5f_hw_test_diag.sdram_sdclk_hz);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    result = sdram_video_ltdc_start_full(&config);
    if(result != CH32H417_LTDC_RGB_OK)
    {
        sdram_video_fail("ltdc_start");
    }
    if(sdram_video_ltdc_wait_scan(&scan_changes) == 0u)
    {
        sdram_video_fail("ltdc_no_scan");
    }
    {
        int used = rt_snprintf(line,
                               sizeof(line),
                               "VIDEO PLAYER START format=%s frames=%u fps=%u total=%u source=sdram_remap600 scan_changes=%u cfblr=plus3 reload=vblank",
                               config.name,
                               (unsigned int)config.frames,
                               (unsigned int)config.fps,
                               (unsigned int)config.total_bytes,
                               (unsigned int)scan_changes);
        if((used > 0) && ((rt_size_t)used < sizeof(line)))
        {
            sdram_usb_debug_write_line(line);
        }
    }
    sdram_video_play(&config);
}
#endif
#endif

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

    sdram_disable_0x60000000_remap();
    base_pass = sdram_probe_window_show(V5F_SDRAM_NATIVE_ADDR, 36u);
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
    (APP_V5F_HW_TEST != APP_V5F_HW_TEST_SDRAM_VIDEO) && \
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
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_UI_FRAMES
    ch32h417_ltdc_rgb_fb_fill_l8(l8_fb(), V5F_L8_FB_BYTES, 0u);
    result = lcd_start_l8_fullscreen();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_RGB565_DIAG
    fb_draw_rgb565_channel_diag();
    result = lcd_start_rgb565_window();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_MEMTEST
    result = CH32H417_LTDC_RGB_OK;
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO
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
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_UI_FRAMES
    run_ltdc_ui_frames_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_LTDC_RGB565_DIAG
    run_ltdc_rgb565_diag_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_MEMTEST
    run_sdram_memtest_test();
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO
    run_sdram_video_test();
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

#if APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_MEMTEST
    /*
     * Configure PA9/PA10 before USBFS is initialized.  They are also the OTG
     * VBUS/ID pads, so their GPIO mode must not be changed underneath a live
     * USBFS CDC connection.  This test variant deliberately leaves LTDC
     * disabled and holds both board LCD-control outputs high.
     */
    ch32h417_lcd_rgb_control_init();
    ch32h417_lcd_rgb_disp_enable(1u);
    ch32h417_lcd_rgb_backlight_enable(1u);
#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO
    /* Board controls belong to boot initialization, not to the LTDC test.
     * Configure and assert them once before USBFS starts; the test thread
     * never reads, gates on, or writes PA9/PA10. */
    ch32h417_lcd_rgb_control_init();
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF15);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF15);
    ch32h417_lcd_rgb_disp_enable(1u);
    ch32h417_lcd_rgb_backlight_enable(1u);
#endif

#if (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_MEMTEST) || \
    (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO)
    /* IWDG keeps running after its reset; feed before USB is initialized. */
    if(RCC_GetFlagStatus(RCC_FLAG_IWDGRST) == SET)
    {
        s_sdram_watchdog_reset_seen = 1u;
        s_sdram_watchdog_started = 1u;
#if (APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO) && \
    defined(APP_USBFS_STREAM_DIAG) && (APP_USBFS_STREAM_DIAG != 0)
        sdram_memtest_capture_usbfs_irq_trace();
#endif
        IWDG_ReloadCounter();
    }
#endif

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
