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
#define RF_TX_TIMER_HZ      1000
#define RF_TX_TARGET_ID     0

static tmosTaskID s_rf_task_id;
static rfRoleParam_t s_rf_param;
static rfipTx_t s_tx_param;
static uint8_t s_tx_buf[RF_FRAME_LEN] __attribute__((aligned(4)));
static uint8_t s_nkro16[AIK_NKRO_REPORT_BYTES] __attribute__((aligned(4)));
static volatile uint8_t s_started;
static volatile uint8_t s_rf_seq;
static volatile uint32_t s_tx_ticks;
static volatile uint32_t s_tx_done_count;
static volatile uint32_t s_report_count;
static volatile uint16_t s_last_host_seq;
static volatile uint8_t s_last_flags;

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

    memcpy(nkro16, s_nkro16, sizeof(nkro16));
    memset(s_tx_buf, 0, sizeof(s_tx_buf));

    s_tx_buf[0] = RF_FRAME_MAGIC;
    s_tx_buf[1] = RF_FRAME_LEN - 2U;
    memcpy(&s_tx_buf[RF_KBD_OFFSET], nkro16, sizeof(nkro16));

    s_tx_buf[RF_CONSUMER_OFFSET] = 0U;
    s_tx_buf[RF_CONSUMER_OFFSET + 1U] = 0U;
    s_tx_buf[RF_TARGET_OFFSET] = target_id;
    s_tx_buf[RF_SEQ_OFFSET] = s_rf_seq++;
    s_tx_buf[RF_FRAME_LEN - 1U] = frame_xor(s_tx_buf, RF_FRAME_LEN - 1U);
}

static void rf_tx_start(uint8_t *buf)
{
    RFIP_SetTxStart();
    s_tx_param.frequency = RF_CHANNEL;
    s_tx_param.txDMA = (uint32_t)buf;
    RFIP_SetTxParm(&s_tx_param);
}

static void rf_process_callback(rfRole_States_t state, uint8_t id)
{
    (void)id;

    if((state & RF_STATE_TX_FINISH) != 0U)
    {
        s_tx_done_count++;
    }
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
        PRINT("half_scan rf tx tick=%lu done=%lu reports=%lu hseq=%u flags=%u\r\n",
              s_tx_ticks,
              s_tx_done_count,
              s_report_count,
              s_last_host_seq,
              s_last_flags);
        s_tx_ticks = 0U;
        s_tx_done_count = 0U;
        return events ^ RF_TEST_TX_EVENT;
    }

    return 0U;
}

void ch585_rf_nkro_tx_init(void)
{
    rfRoleConfig_t conf = {0};

    memset(s_tx_buf, 0, sizeof(s_tx_buf));
    memset(s_nkro16, 0, sizeof(s_nkro16));

    s_rf_task_id = TMOS_ProcessEventRegister(rf_process_event);

    conf.TxPower = LL_TX_POWEER_4_DBM;
    conf.rfProcessCB = rf_process_callback;
    conf.processMask = RF_STATE_TX_FINISH;
    RFRole_BasicInit(&conf);

    s_rf_param.accessAddress = RF_SYNC_WORD;
    s_rf_param.crcInit = 0x555555;
    s_rf_param.properties = LLE_MODE_PHY_2M;
    s_rf_param.sendInterval = 1999U * 2U;
    s_rf_param.sendTime = 20U * 2U;
    RFRole_SetParam(&s_rf_param);

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
}

void ch585_rf_nkro_tx_set_report(const uint8_t nkro16[AIK_NKRO_REPORT_BYTES],
                                 uint16_t host_seq,
                                 uint8_t flags)
{
    if(nkro16 == 0)
    {
        return;
    }

    if(s_started != 0U)
    {
        PFIC_DisableIRQ(TMR0_IRQn);
    }

    memcpy(s_nkro16, nkro16, sizeof(s_nkro16));
    s_last_host_seq = host_seq;
    s_last_flags = flags;
    s_report_count++;

    if(s_started != 0U)
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
        s_tx_ticks++;
        fill_nkro_frame(RF_TX_TARGET_ID);
        rf_tx_start(s_tx_buf);
    }
}
