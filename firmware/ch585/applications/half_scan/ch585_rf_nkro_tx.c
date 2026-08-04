#include "ch585_rf_nkro_tx.h"

#include <string.h>

#include "HAL.h"
#include "rf_test.h"

#define RF_CHANNEL          16
#define RF_SYNC_WORD        0xA55A1234UL
#define RF_FRAME_MAGIC      0x55
#define RF_FRAME_LEN        25
#define RF_KBD_OFFSET       2
#define RF_CONSUMER_OFFSET  18
#define RF_RESERVED_OFFSET  20
#define RF_TARGET_OFFSET    RF_RESERVED_OFFSET
#define RF_SEQ_OFFSET       (RF_RESERVED_OFFSET + 1)
#define RF_WHEEL_OFFSET     (RF_RESERVED_OFFSET + 2)
#define RF_TX_TIMER_HZ      1000
#define RF_TX_TARGET_ID     0
#define RF_RELEASE_BURST_FRAMES 4U
#define RF_RELEASE_FRAME_GAP_MS 2U
#define RF_IDLE_WAIT_TIMEOUT_MS 20U

static tmosTaskID s_rf_task_id;
static rfRoleParam_t s_rf_param;
static rfipTx_t s_tx_param;
static uint8_t s_tx_buf[RF_FRAME_LEN] __attribute__((aligned(4)));
static uint8_t s_nkro16[AIK_NKRO_REPORT_BYTES] __attribute__((aligned(4)));
static volatile uint8_t s_started;
static volatile uint8_t s_enabled;
static volatile uint8_t s_rf_seq;
static volatile uint32_t s_tx_ticks;
static volatile uint32_t s_tx_done_count;
static volatile uint32_t s_report_count;
static volatile uint32_t s_wheel_report_count;
static volatile uint32_t s_wheel_tx_count;
static volatile uint16_t s_last_host_seq;
static volatile uint16_t s_consumer_pending_usage;
static volatile uint16_t s_consumer_last_input_usage;
static volatile int16_t s_consumer_delta_pending;
static volatile uint8_t s_consumer_release_pending;
static volatile int16_t s_mouse_wheel_pending;
static volatile int8_t s_last_mouse_wheel;
static volatile uint8_t s_last_flags;
static volatile uint8_t s_last_switch_status;
static volatile uint8_t s_release_async_active;
static volatile uint8_t s_release_frames_remaining;
static volatile uint8_t s_release_gap_ticks;
static volatile uint8_t s_release_tx_in_flight;
static volatile uint8_t s_release_done;

static uint8_t frame_xor(const uint8_t *buf, uint8_t len)
{
    uint8_t x = 0U;
    uint8_t i;

    for(i = 0U; i < len; i++)
    {
        x ^= buf[i];
    }
    return x;
}

static void fill_nkro_frame(uint8_t target_id)
{
    uint8_t nkro16[AIK_NKRO_REPORT_BYTES];
    uint16_t consumer_usage;
    int8_t mouse_wheel;

    memcpy(nkro16, s_nkro16, sizeof(nkro16));
    consumer_usage = AIK_CONSUMER_USAGE_NONE;
    if(s_consumer_release_pending != 0U)
    {
        consumer_usage = AIK_CONSUMER_USAGE_NONE;
        s_consumer_release_pending = 0U;
    }
    else if(s_consumer_pending_usage != AIK_CONSUMER_USAGE_NONE)
    {
        consumer_usage = s_consumer_pending_usage;
        s_consumer_pending_usage = AIK_CONSUMER_USAGE_NONE;
        s_consumer_release_pending = 1U;
    }
    else if(s_consumer_delta_pending > 0)
    {
        s_consumer_delta_pending--;
        consumer_usage = AIK_CONSUMER_USAGE_VOLUME_UP;
        s_consumer_release_pending = 1U;
    }
    else if(s_consumer_delta_pending < 0)
    {
        s_consumer_delta_pending++;
        consumer_usage = AIK_CONSUMER_USAGE_VOLUME_DOWN;
        s_consumer_release_pending = 1U;
    }

    if(s_mouse_wheel_pending > 0)
    {
        s_mouse_wheel_pending--;
        mouse_wheel = 1;
    }
    else if(s_mouse_wheel_pending < 0)
    {
        s_mouse_wheel_pending++;
        mouse_wheel = -1;
    }
    else
    {
        mouse_wheel = 0;
    }
    if(mouse_wheel != 0)
    {
        s_wheel_tx_count++;
    }
    memset(s_tx_buf, 0, sizeof(s_tx_buf));

    s_tx_buf[0] = RF_FRAME_MAGIC;
    s_tx_buf[1] = RF_FRAME_LEN - 2U;
    memcpy(&s_tx_buf[RF_KBD_OFFSET], nkro16, sizeof(nkro16));

    s_tx_buf[RF_CONSUMER_OFFSET] = (uint8_t)(consumer_usage & 0xFFU);
    s_tx_buf[RF_CONSUMER_OFFSET + 1U] = (uint8_t)(consumer_usage >> 8);
    s_tx_buf[RF_TARGET_OFFSET] = target_id;
    s_tx_buf[RF_SEQ_OFFSET] = s_rf_seq++;
    s_tx_buf[RF_WHEEL_OFFSET] = (uint8_t)mouse_wheel;
    s_tx_buf[RF_FRAME_LEN - 1U] = frame_xor(s_tx_buf, RF_FRAME_LEN - 1U);
}

static void rf_tx_start(uint8_t *buf)
{
    RFIP_SetTxStart();
    s_tx_param.frequency = RF_CHANNEL;
    s_tx_param.txDMA = (uint32_t)buf;
    RFIP_SetTxParm(&s_tx_param);
}

static void rf_clear_report_state(void)
{
    memset(s_nkro16, 0, sizeof(s_nkro16));
    s_consumer_pending_usage = AIK_CONSUMER_USAGE_NONE;
    s_consumer_last_input_usage = AIK_CONSUMER_USAGE_NONE;
    s_consumer_delta_pending = 0;
    s_consumer_release_pending = 0U;
    s_mouse_wheel_pending = 0;
    s_last_mouse_wheel = 0;
}

static void rf_send_release_burst(void)
{
    uint8_t i;

    rf_clear_report_state();
    for(i = 0U; i < RF_RELEASE_BURST_FRAMES; i++)
    {
        fill_nkro_frame(RF_TX_TARGET_ID);
        rf_tx_start(s_tx_buf);
        mDelaymS(RF_RELEASE_FRAME_GAP_MS);
        TMOS_SystemProcess();
    }
}

static uint8_t rf_wait_idle(uint16_t timeout_ms)
{
    while(RFRole_GetStatus(RF_TX_TARGET_ID) != 0U)
    {
        if(timeout_ms == 0U)
        {
            return 0U;
        }
        TMOS_SystemProcess();
        mDelaymS(1);
        timeout_ms--;
    }
    return 1U;
}

static void rf_process_callback(rfRole_States_t state, uint8_t id)
{
    (void)id;

    if((state & RF_STATE_TX_FINISH) != 0U)
    {
        s_tx_done_count++;
        if((s_release_async_active != 0U) &&
           (s_release_tx_in_flight != 0U))
        {
            s_release_tx_in_flight = 0U;
            if(s_release_frames_remaining == 0U)
            {
                s_release_done = 1U;
            }
            else
            {
                /*
                 * TMR0 runs at 1 kHz. Skipping one following tick keeps
                 * consecutive release-frame starts roughly 2 ms apart.
                 */
                s_release_gap_ticks =
                    (RF_RELEASE_FRAME_GAP_MS > 0U) ?
                    (RF_RELEASE_FRAME_GAP_MS - 1U) :
                    0U;
            }
        }
    }
}

static void rf_role_apply_config(void)
{
    rfRoleConfig_t conf = {0};

    conf.TxPower = LL_TX_POWEER_4_DBM;
    conf.rfProcessCB = rf_process_callback;
    conf.processMask = RF_STATE_TX_FINISH;
    RFRole_BasicInit(&conf);
    RFRole_SetParam(&s_rf_param);
}

static tmosEvents rf_process_event(tmosTaskID task_id, tmosEvents events)
{
    if((events & SYS_EVENT_MSG) != 0U)
    {
        uint8_t *msg = tmos_msg_receive(task_id);

        if(msg != 0)
        {
            tmos_msg_deallocate(msg);
        }
        return events ^ SYS_EVENT_MSG;
    }

    if((events & RF_TEST_TX_EVENT) != 0U)
    {
        PRINT("half_scan rf tx tick=%lu done=%lu reports=%lu wheel_set=%lu wheel_tx=%lu last_wheel=%d hseq=%u flags=%u\r\n",
              s_tx_ticks,
              s_tx_done_count,
              s_report_count,
              s_wheel_report_count,
              s_wheel_tx_count,
              (int)s_last_mouse_wheel,
              s_last_host_seq,
              s_last_flags);
        s_tx_ticks = 0U;
        s_tx_done_count = 0U;
        s_wheel_report_count = 0U;
        s_wheel_tx_count = 0U;
        return events ^ RF_TEST_TX_EVENT;
    }

    return 0U;
}

void ch585_rf_nkro_tx_init(void)
{
    memset(s_tx_buf, 0, sizeof(s_tx_buf));
    memset(s_nkro16, 0, sizeof(s_nkro16));
    s_consumer_pending_usage = AIK_CONSUMER_USAGE_NONE;
    s_consumer_last_input_usage = AIK_CONSUMER_USAGE_NONE;
    s_consumer_delta_pending = 0;
    s_consumer_release_pending = 0U;
    s_mouse_wheel_pending = 0;
    s_last_mouse_wheel = 0;
    s_last_switch_status = 0U;
    s_release_async_active = 0U;
    s_release_frames_remaining = 0U;
    s_release_gap_ticks = 0U;
    s_release_tx_in_flight = 0U;
    s_release_done = 0U;

    s_rf_task_id = TMOS_ProcessEventRegister(rf_process_event);

    s_rf_param.accessAddress = RF_SYNC_WORD;
    s_rf_param.crcInit = 0x555555;
    s_rf_param.properties = LLE_MODE_PHY_2M;
    s_rf_param.sendInterval = 1999U * 2U;
    s_rf_param.sendTime = 20U * 2U;
    rf_role_apply_config();

    s_tx_param.accessAddress = RF_SYNC_WORD;
    s_tx_param.crcInit = 0x555555;
    s_tx_param.properties = LLE_MODE_PHY_2M;
    s_tx_param.sendCount = 1U;
    s_tx_param.frequency = RF_CHANNEL;
    s_tx_param.txDMA = (uint32_t)s_tx_buf;

    PFIC_EnableIRQ(BLEB_IRQn);
    PFIC_EnableIRQ(BLEL_IRQn);

    tmos_start_reload_task(s_rf_task_id, RF_TEST_TX_EVENT, MS1_TO_SYSTEM_TIME(1000));
    TMR0_TimerInit(GetSysClock() / RF_TX_TIMER_HZ);
    TMR0_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
    PFIC_SetPriority(TMR0_IRQn, 0x80);
    PFIC_EnableIRQ(TMR0_IRQn);

    s_started = 1U;
    s_enabled = 1U;
    PRINT("half_scan RF 2.4G 1K legacy NKRO TX: %u-byte frame target=%u\r\n",
          RF_FRAME_LEN,
          RF_TX_TARGET_ID);
}

void ch585_rf_nkro_tx_poll(void)
{
    if(s_started != 0U)
    {
        TMOS_SystemProcess();
    }

    if(s_release_done != 0U)
    {
        TMR0_ITCfg(DISABLE, TMR0_3_IT_CYC_END);
        PFIC_DisableIRQ(TMR0_IRQn);
        s_release_done = 0U;
        s_release_async_active = 0U;
        /*
         * Keep RF idle here. Calling RFRole_Stop() would add another
         * synchronous radio operation to the USB SPI service path; both
         * later enable paths stop and reconfigure the role before use.
         */
        PRINT("half_scan rf async release done\r\n");
    }
}

void ch585_rf_nkro_tx_set_enabled(uint8_t enabled)
{
    if(s_started == 0U)
    {
        s_enabled = 0U;
        return;
    }

    if(enabled != 0U)
    {
        TMR0_ITCfg(DISABLE, TMR0_3_IT_CYC_END);
        PFIC_DisableIRQ(TMR0_IRQn);
        s_release_async_active = 0U;
        s_release_frames_remaining = 0U;
        s_release_gap_ticks = 0U;
        s_release_tx_in_flight = 0U;
        s_release_done = 0U;
        (void)RFRole_Stop();
        s_last_switch_status = RFRole_SwitchMode(1U);
        rf_role_apply_config();
        s_enabled = 1U;
        TMR0_ClearITFlag(TMR0_3_IT_CYC_END);
        TMR0_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
        PFIC_EnableIRQ(TMR0_IRQn);
        PRINT("half_scan rf enable switch=%u\r\n",
              (unsigned int)s_last_switch_status);
    }
    else
    {
        TMR0_ITCfg(DISABLE, TMR0_3_IT_CYC_END);
        PFIC_DisableIRQ(TMR0_IRQn);
        s_release_async_active = 0U;
        s_release_frames_remaining = 0U;
        s_release_gap_ticks = 0U;
        s_release_tx_in_flight = 0U;
        s_release_done = 0U;
        if(s_enabled != 0U)
        {
            rf_send_release_burst();
            /* Finish the release burst before BLE takes ownership of RFIP. */
            if(rf_wait_idle(RF_IDLE_WAIT_TIMEOUT_MS) == 0U)
            {
                PRINT("half_scan rf idle timeout status=%08lx\r\n",
                      (unsigned long)RFRole_GetStatus(RF_TX_TARGET_ID));
            }
        }
        s_enabled = 0U;
        (void)RFRole_Stop();
        rf_clear_report_state();
        PRINT("half_scan rf disable\r\n");
    }
}

void ch585_rf_nkro_tx_disable_async(void)
{
    if(s_started == 0U)
    {
        s_enabled = 0U;
        return;
    }

    TMR0_ITCfg(DISABLE, TMR0_3_IT_CYC_END);
    PFIC_DisableIRQ(TMR0_IRQn);
    if(s_release_async_active != 0U)
    {
        TMR0_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
        PFIC_EnableIRQ(TMR0_IRQn);
        return;
    }
    if(s_enabled == 0U)
    {
        return;
    }

    rf_clear_report_state();
    s_enabled = 0U;
    s_release_async_active = 1U;
    s_release_frames_remaining = RF_RELEASE_BURST_FRAMES;
    s_release_gap_ticks = 0U;
    s_release_tx_in_flight = 0U;
    s_release_done = 0U;
    TMR0_ClearITFlag(TMR0_3_IT_CYC_END);
    TMR0_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
    PFIC_EnableIRQ(TMR0_IRQn);
    PRINT("half_scan rf async release queued\r\n");
}

uint8_t ch585_rf_nkro_tx_is_enabled(void)
{
    return s_enabled;
}

void ch585_rf_nkro_tx_set_report(const uint8_t nkro16[AIK_NKRO_REPORT_BYTES],
                                 uint16_t consumer_usage,
                                 int8_t consumer_delta,
                                 int8_t mouse_wheel,
                                 uint16_t host_seq,
                                 uint8_t flags)
{
    if(nkro16 == 0)
    {
        return;
    }
    if(s_release_async_active != 0U)
    {
        return;
    }

    if((s_started != 0U) && (s_enabled != 0U))
    {
        PFIC_DisableIRQ(TMR0_IRQn);
    }

    memcpy(s_nkro16, nkro16, sizeof(s_nkro16));
    if((consumer_usage != AIK_CONSUMER_USAGE_NONE) &&
       (s_consumer_last_input_usage == AIK_CONSUMER_USAGE_NONE) &&
       (s_consumer_pending_usage == AIK_CONSUMER_USAGE_NONE))
    {
        s_consumer_pending_usage = consumer_usage;
    }
    s_consumer_last_input_usage = consumer_usage;
    if(consumer_delta > 0)
    {
        if(s_consumer_delta_pending <= (127 - consumer_delta))
        {
            s_consumer_delta_pending =
                (int16_t)(s_consumer_delta_pending + consumer_delta);
        }
        else
        {
            s_consumer_delta_pending = 127;
        }
    }
    else if(consumer_delta < 0)
    {
        if(s_consumer_delta_pending >= (-127 - consumer_delta))
        {
            s_consumer_delta_pending =
                (int16_t)(s_consumer_delta_pending + consumer_delta);
        }
        else
        {
            s_consumer_delta_pending = -127;
        }
    }
    if(mouse_wheel != 0)
    {
        if((mouse_wheel > 0) && (s_mouse_wheel_pending < 127))
        {
            s_mouse_wheel_pending++;
        }
        else if((mouse_wheel < 0) && (s_mouse_wheel_pending > -127))
        {
            s_mouse_wheel_pending--;
        }
        s_last_mouse_wheel = mouse_wheel;
        s_wheel_report_count++;
    }
    s_last_host_seq = host_seq;
    s_last_flags = flags;
    s_report_count++;

    if((s_started != 0U) && (s_enabled != 0U))
    {
        PFIC_EnableIRQ(TMR0_IRQn);
    }
}

uint32_t ch585_rf_nkro_tx_done_count(void)
{
    return s_tx_done_count;
}

uint32_t ch585_rf_nkro_tx_report_count(void)
{
    return s_report_count;
}

__INTERRUPT
__HIGH_CODE
void TMR0_IRQHandler(void)
{
    if(TMR0_GetITFlag(TMR0_3_IT_CYC_END))
    {
        TMR0_ClearITFlag(TMR0_3_IT_CYC_END);
        if((s_started == 0U) || (s_enabled == 0U))
        {
            if((s_started == 0U) ||
               (s_release_async_active == 0U) ||
               (s_release_tx_in_flight != 0U))
            {
                return;
            }
            if(s_release_gap_ticks != 0U)
            {
                s_release_gap_ticks--;
                return;
            }
            if(s_release_frames_remaining == 0U)
            {
                return;
            }

            fill_nkro_frame(RF_TX_TARGET_ID);
            s_release_frames_remaining--;
            s_release_tx_in_flight = 1U;
            rf_tx_start(s_tx_buf);
            return;
        }
        s_tx_ticks++;
        fill_nkro_frame(RF_TX_TARGET_ID);
        rf_tx_start(s_tx_buf);
    }
}
