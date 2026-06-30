#include "CONFIG.h"
#include "HAL.h"

#include <string.h>

#include "aik_spi_protocol.h"
#include "ch585_ads7948_mux_acq.h"
#include "ch585_half_report.h"
#include "ch585_rf_nkro_tx.h"
#include "ch585_spi0_slave_link.h"
#include "magnetic_key_engine.h"

__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

#ifndef CH585_HALF_ID
#define CH585_HALF_ID AIK_HALF_ID_LEFT
#endif

#ifndef CH585_RF_TX_ENABLE
#define CH585_RF_TX_ENABLE 0
#endif

#ifndef CH585_SPI_ACCEPT_HOST_CMD
#define CH585_SPI_ACCEPT_HOST_CMD CH585_RF_TX_ENABLE
#endif

#ifndef CH585_HALF_SCAN_DEBUG_UART
#define CH585_HALF_SCAN_DEBUG_UART 0
#endif

#ifndef CH585_HALF_SCAN_DEBUG_UART_BAUD
#define CH585_HALF_SCAN_DEBUG_UART_BAUD 921600U
#endif

#ifndef CH585_HALF_SCAN_DEBUG_PERIOD_FRAMES
#define CH585_HALF_SCAN_DEBUG_PERIOD_FRAMES 100U
#endif

#if CH585_HALF_SCAN_DEBUG_UART && !defined(DEBUG)
#error CH585_HALF_SCAN_DEBUG_UART requires DEBUG=Debug_UART1 or another WCH DEBUG UART.
#endif

static ch585_ads7948_mux_acq_t s_acq;
static mag_key_engine_t s_engine;
static aik_spi_half_state_v1_t s_tx_frame __attribute__((aligned(4)));
static aik_spi_host_cmd_v1_t s_rx_cmd __attribute__((aligned(4)));
static aik_spi_half_state_v1_t s_right_frame __attribute__((aligned(4)));
static uint8_t s_rf_nkro16[AIK_NKRO_REPORT_BYTES];
static uint16_t s_compact_raw[AIK_KEY_COUNT_RIGHT];
static int s_last_spi_result;
static uint8_t s_right_frame_valid;

static ch585_ads7948_mux_side_t half_scan_side(void)
{
    return (CH585_HALF_ID == AIK_HALF_ID_RIGHT) ?
        CH585_ADS7948_MUX_SIDE_RIGHT :
        CH585_ADS7948_MUX_SIDE_LEFT;
}

static void half_scan_build_down_bits(void)
{
    uint8_t key;
    uint8_t key_count = aik_spi_half_key_count((uint8_t)CH585_HALF_ID);

    memset(s_tx_frame.down_bits, 0, sizeof(s_tx_frame.down_bits));
    for(key = 0U; key < key_count; key++)
    {
        const mag_key_state_t *state = mag_key_engine_state(&s_engine, key);

        if((state != 0) && (state->is_down != 0U))
        {
            s_tx_frame.down_bits[key >> 3] |= (uint8_t)(1U << (key & 7U));
        }
    }
}

static void half_scan_compact_raw(const ch585_ads7948_mux_acq_t *acq,
                                  uint16_t *compact,
                                  uint8_t key_count)
{
    const uint16_t *raw;
    const ch585_ads7948_mux_profile_t *profile;
    uint8_t key = 0U;
    uint8_t lane_index;

    if((acq == 0) || (compact == 0) || (acq->profile == 0))
    {
        return;
    }

    raw = ch585_ads7948_mux_acq_raw(acq);
    profile = acq->profile;
    for(lane_index = 0U;
        (lane_index < CH585_ADS7948_MUX_LANE_COUNT) && (key < key_count);
        lane_index++)
    {
        const ch585_ads7948_mux_lane_t *lane = &profile->lanes[lane_index];
        uint8_t mux;
        uint8_t mux_end = (uint8_t)(lane->mux_first + lane->mux_count);

        for(mux = lane->mux_first; (mux < mux_end) && (key < key_count); mux++)
        {
            uint16_t raw_index =
                ((uint16_t)lane_index * CH585_ADS7948_MUX_MUX_CHANNEL_COUNT) +
                (uint16_t)mux;

            compact[key++] = raw[raw_index];
        }
    }

    while(key < key_count)
    {
        compact[key++] = 0U;
    }
}

static void half_scan_build_frame(void)
{
    uint8_t key_count = aik_spi_half_key_count((uint8_t)CH585_HALF_ID);

    s_tx_frame.half_seq++;
    half_scan_build_down_bits();
    aik_spi_half_state_finish(&s_tx_frame, key_count);
}

static void half_scan_apply_host_cmd(void)
{
#if CH585_SPI_ACCEPT_HOST_CMD
    if(aik_spi_host_cmd_valid(&s_rx_cmd) == 0U)
    {
        return;
    }

#if CH585_RF_TX_ENABLE
    if(s_rx_cmd.cmd == AIK_SPI_CMD_POLL_WITH_RF)
    {
        memcpy(s_rf_nkro16, s_rx_cmd.nkro16, sizeof(s_rf_nkro16));
        ch585_rf_nkro_tx_set_report(s_rf_nkro16,
                                    s_rx_cmd.host_seq,
                                    s_rx_cmd.flags);
    }
    else if(s_rx_cmd.cmd == AIK_SPI_CMD_PUSH_RIGHT_STATE)
    {
        aik_spi_half_state_v1_t next_right;

        memcpy(&next_right, s_rx_cmd.nkro16, sizeof(next_right));
        if(aik_spi_half_state_valid(&next_right) == 0U)
        {
            return;
        }

        s_right_frame = next_right;
        s_right_frame_valid = 1U;
        ch585_half_report_build_nkro16(&s_tx_frame,
                                       s_right_frame_valid ? &s_right_frame : 0,
                                       s_rf_nkro16);
        ch585_rf_nkro_tx_set_report(s_rf_nkro16,
                                    s_rx_cmd.host_seq,
                                    s_rx_cmd.flags);
    }
#endif
#else
    (void)s_rx_cmd;
#endif
}

#if CH585_HALF_SCAN_DEBUG_UART
static void half_scan_debug_uart_init(void)
{
    GPIOPinRemap(DISABLE, RB_PIN_UART1);
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
    UART1_BaudRateCfg(CH585_HALF_SCAN_DEBUG_UART_BAUD);
}

static uint8_t half_scan_debug_first_down(uint8_t key_count)
{
    uint8_t key;

    for(key = 0U; key < key_count; key++)
    {
        if((s_tx_frame.down_bits[key >> 3] & (uint8_t)(1U << (key & 7U))) != 0U)
        {
            return key;
        }
    }
    return 0xFFU;
}

static void half_scan_debug_poll(uint8_t key_count)
{
    ch585_spi0_slave_link_stats_t spi_stats;
    uint16_t host_crc;
    uint16_t host_calc_crc;
    uint8_t key;
    uint8_t min_raw_key = 0U;
    uint8_t max_pos_key = 0U;
    uint16_t min_raw = 0xFFFFU;
    uint16_t max_pos = 0U;
#if CH585_SPI_ACCEPT_HOST_CMD
    uint8_t host_ok = aik_spi_host_cmd_valid(&s_rx_cmd);
#else
    uint8_t host_ok = 0U;
#endif

    if((s_tx_frame.half_seq % CH585_HALF_SCAN_DEBUG_PERIOD_FRAMES) != 0U)
    {
        return;
    }

    for(key = 0U; key < key_count; key++)
    {
        const mag_key_state_t *state = mag_key_engine_state(&s_engine, key);

        if(state == 0)
        {
            continue;
        }

        if(state->raw_adc < min_raw)
        {
            min_raw = state->raw_adc;
            min_raw_key = key;
        }
        if(state->position_pm > max_pos)
        {
            max_pos = state->position_pm;
            max_pos_key = key;
        }
    }

    ch585_spi0_slave_link_get_stats(&spi_stats);
    host_crc = s_rx_cmd.crc16;
    host_calc_crc = aik_spi_host_cmd_crc(&s_rx_cmd);
    PRINT("hs half=%u seq=%u scan=%lu raw_min=%u:%u pos_max=%u:%u down=%02x%02x%02x%02x%02x%02x first=%u spi=%lu abort=%lu last=%d host=%u cmd=%u hseq=%u rxcnt=%u rx=%02x%02x%02x%02x hcrc=%04x/%04x\r\n",
          (unsigned int)CH585_HALF_ID,
          (unsigned int)s_tx_frame.half_seq,
          (unsigned long)s_acq.frames,
          (unsigned int)min_raw_key,
          (unsigned int)min_raw,
          (unsigned int)max_pos_key,
          (unsigned int)max_pos,
          (unsigned int)s_tx_frame.down_bits[0],
          (unsigned int)s_tx_frame.down_bits[1],
          (unsigned int)s_tx_frame.down_bits[2],
          (unsigned int)s_tx_frame.down_bits[3],
          (unsigned int)s_tx_frame.down_bits[4],
          (unsigned int)s_tx_frame.down_bits[5],
          (unsigned int)half_scan_debug_first_down(key_count),
          (unsigned long)spi_stats.frames,
          (unsigned long)spi_stats.aborts,
          s_last_spi_result,
          (unsigned int)host_ok,
          (unsigned int)s_rx_cmd.cmd,
          (unsigned int)s_rx_cmd.host_seq,
          (unsigned int)spi_stats.last_rx_count,
          (unsigned int)spi_stats.last_rx_head[0],
          (unsigned int)spi_stats.last_rx_head[1],
          (unsigned int)spi_stats.last_rx_head[2],
          (unsigned int)spi_stats.last_rx_head[3],
          (unsigned int)host_crc,
          (unsigned int)host_calc_crc);
}
#else
static void half_scan_debug_uart_init(void)
{
}

static void half_scan_debug_poll(uint8_t key_count)
{
    (void)key_count;
}
#endif

static void half_scan_init(void)
{
    const ch585_ads7948_mux_profile_t *profile = ch585_ads7948_mux_profile(half_scan_side());
    mag_key_config_t cfg;

    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(SYSCLK_FREQ);
    half_scan_debug_uart_init();
    PRINT("half_scan start half=%u sys=%lu keys=%u rf=%u uart_dbg=%u\r\n",
          (unsigned int)CH585_HALF_ID,
          (unsigned long)GetSysClock(),
          (unsigned int)aik_spi_half_key_count((uint8_t)CH585_HALF_ID),
          (unsigned int)CH585_RF_TX_ENABLE,
          (unsigned int)CH585_HALF_SCAN_DEBUG_UART);
#if CH585_RF_TX_ENABLE
    CH58x_BLEInit();
#endif
    HAL_Init();

    memset(&s_acq, 0, sizeof(s_acq));
    memset(&s_engine, 0, sizeof(s_engine));
    memset(&s_tx_frame, 0, sizeof(s_tx_frame));
    memset(&s_rx_cmd, 0, sizeof(s_rx_cmd));
    memset(&s_right_frame, 0, sizeof(s_right_frame));
    memset(s_rf_nkro16, 0, sizeof(s_rf_nkro16));
    memset(s_compact_raw, 0, sizeof(s_compact_raw));
    s_last_spi_result = 0;
    s_right_frame_valid = 0U;

    ch585_ads7948_mux_gpio_init();
    (void)ch585_ads7948_mux_acq_init(&s_acq, profile);

    mag_key_default_config(&cfg);
    cfg.mode = MAG_KEY_MODE_RAPID_TRIGGER;
    (void)mag_key_engine_init(&s_engine,
                              aik_spi_half_key_count((uint8_t)CH585_HALF_ID),
                              &cfg);

    ch585_spi0_slave_link_init();
#if CH585_RF_TX_ENABLE
    ch585_rf_nkro_tx_init();
#endif

    ch585_ads7948_mux_acq_poll(&s_acq);
    half_scan_compact_raw(&s_acq,
                          s_compact_raw,
                          aik_spi_half_key_count((uint8_t)CH585_HALF_ID));
    (void)mag_key_engine_update(&s_engine, s_compact_raw);
    half_scan_build_frame();
}

int main(void)
{
    half_scan_init();

    while(1)
    {
        uint8_t key_count = aik_spi_half_key_count((uint8_t)CH585_HALF_ID);

        s_last_spi_result =
            ch585_spi0_slave_link_receive_frame((uint8_t *)&s_rx_cmd,
                                                AIK_SPI_HOST_CMD_SIZE);
        if(s_last_spi_result == CH585_SPI0_SLAVE_LINK_OK)
        {
            half_scan_apply_host_cmd();
        }
        s_last_spi_result =
            ch585_spi0_slave_link_serve_tx_frame((const uint8_t *)&s_tx_frame,
                                                 AIK_SPI_HALF_STATE_SIZE);
        ch585_ads7948_mux_acq_poll(&s_acq);
        half_scan_compact_raw(&s_acq, s_compact_raw, key_count);
        (void)mag_key_engine_update(&s_engine, s_compact_raw);
        half_scan_build_frame();
        half_scan_debug_poll(key_count);
#if CH585_RF_TX_ENABLE
        ch585_rf_nkro_tx_poll();
#endif
    }
}
