/********************************** (C) COPYRIGHT *******************************
 * File Name          : rf_keyboard_tx.c
 * Description        : Minimal CH585 2.4G keyboard-frame transmitter.
 *
 * This test transmitter sends NKRO keyboard frames over the CH585 2.4G RF IP.
 * The receivers are CH585M_RF_RX_USB and CH585M_RF_RX_USBHS. They filter frames
 * by target_id before converting them to USB HID keyboard reports.
 *******************************************************************************/

#include "rf_test.h"
#include "hal.h"

#define RF_CHANNEL          16
#define RF_SYNC_WORD        0xA55A1234UL
#define RF_FRAME_MAGIC      0x55
#define RF_FRAME_LEN        25
#define RF_KBD_OFFSET       2
#define RF_CONSUMER_OFFSET  18
#define RF_RESERVED_OFFSET  20
#define RF_TARGET_OFFSET    RF_RESERVED_OFFSET
#define RF_SEQ_OFFSET       (RF_RESERVED_OFFSET + 1)
#define RF_TX_TIMER_HZ      4
#define RF_TX_START_DELAY_SECONDS 3
#define RF_TX_START_DELAY_TICKS   (RF_TX_TIMER_HZ * RF_TX_START_DELAY_SECONDS)
#define RF_TX_INTER_KEY_TICKS     2

#define RF_TX_PHASE_WAIT_START   0
#define RF_TX_PHASE_SEND_RELEASE 1
#define RF_TX_PHASE_WAIT_NEXT    2
#define RF_TX_PHASE_DONE         3

#define HID_KEY_MIN    0x04
#define HID_KEY_A      0x04
#define HID_KEY_B      0x05
#define HID_KEY_C      0x06
#define HID_KEY_1      0x1E
#define HID_KEY_2      0x1F
#define HID_KEY_ENTER  0x28
#define HID_NKRO_BYTES 14

#define KVM_TARGET_1   1
#define KVM_TARGET_2   2

#define RF_KVM_ACTION_SWITCH_TARGET 0
#define RF_KVM_ACTION_TAP_KEY       1

typedef struct
{
    uint8_t action;
    uint8_t value;
} rfKvmDemoAction_t;

tmosTaskID rfTaskID;
rfRoleParam_t gParm;
rfipTx_t gTxParam;

__attribute__((__aligned__(4))) static uint8_t TxBuf[RF_FRAME_LEN];

static volatile uint32_t gTxCount;
static volatile uint32_t gTxTickCount;
static volatile uint8_t gTxPhase = RF_TX_PHASE_WAIT_START;
static volatile uint8_t gTxActionIndex;
static volatile uint8_t gTxCurrentTarget = KVM_TARGET_1;
static volatile uint8_t gTxCurrentKeycode;
static volatile uint8_t gTxSeq;

static const rfKvmDemoAction_t gDemoActions[] = {
    {RF_KVM_ACTION_SWITCH_TARGET, KVM_TARGET_1},
    {RF_KVM_ACTION_TAP_KEY,       HID_KEY_A},
    {RF_KVM_ACTION_TAP_KEY,       HID_KEY_1},
    {RF_KVM_ACTION_TAP_KEY,       HID_KEY_ENTER},

    {RF_KVM_ACTION_SWITCH_TARGET, KVM_TARGET_2},
    {RF_KVM_ACTION_TAP_KEY,       HID_KEY_B},
    {RF_KVM_ACTION_TAP_KEY,       HID_KEY_2},
    {RF_KVM_ACTION_TAP_KEY,       HID_KEY_ENTER},

    {RF_KVM_ACTION_SWITCH_TARGET, KVM_TARGET_1},
    {RF_KVM_ACTION_TAP_KEY,       HID_KEY_C},
    {RF_KVM_ACTION_TAP_KEY,       HID_KEY_1},
    {RF_KVM_ACTION_TAP_KEY,       HID_KEY_ENTER},
};
#define RF_DEMO_ACTION_COUNT (sizeof(gDemoActions) / sizeof(gDemoActions[0]))

static void KVM_SwitchTarget(uint8_t target_id)
{
    if((target_id == KVM_TARGET_1) || (target_id == KVM_TARGET_2))
    {
        gTxCurrentTarget = target_id;
    }
}

__HIGH_CODE
static void rf_tx_start(uint8_t *pBuf);

static uint8_t frame_xor(const uint8_t *buf, uint8_t len)
{
    uint8_t x = 0;

    for(uint8_t i = 0; i < len; i++)
    {
        x ^= buf[i];
    }
    return x;
}

static void fill_keyboard_frame(uint8_t target_id, uint8_t keycode, uint8_t key_down)
{
    for(uint8_t i = 0; i < RF_FRAME_LEN; i++)
    {
        TxBuf[i] = 0;
    }

    TxBuf[0] = RF_FRAME_MAGIC;
    TxBuf[1] = RF_FRAME_LEN - 2;

    /*
     * 16B NKRO report layout:
     * [modifier][reserved][bitmap14B].
     * HID usage 0x04 is key 'a'. The NKRO bitmap starts from usage 0x04.
     */
    if(key_down)
    {
        if(keycode >= HID_KEY_MIN)
        {
            uint8_t bit_index = keycode - HID_KEY_MIN;
            uint8_t byte_index = bit_index / 8;
            uint8_t bit_mask = (uint8_t)(1u << (bit_index % 8));

            if(byte_index < HID_NKRO_BYTES)
            {
                TxBuf[RF_KBD_OFFSET + 2 + byte_index] = bit_mask;
            }
        }
    }

    TxBuf[RF_CONSUMER_OFFSET] = 0x00;
    TxBuf[RF_CONSUMER_OFFSET + 1] = 0x00;

    for(uint8_t i = 0; i < 4; i++)
    {
        TxBuf[RF_RESERVED_OFFSET + i] = 0x00;
    }

    TxBuf[RF_TARGET_OFFSET] = target_id;
    TxBuf[RF_SEQ_OFFSET] = gTxSeq++;

    TxBuf[RF_FRAME_LEN - 1] = frame_xor(TxBuf, RF_FRAME_LEN - 1);
}

static void tx_start_next_demo_key(void)
{
    while(gTxActionIndex < RF_DEMO_ACTION_COUNT)
    {
        const rfKvmDemoAction_t *action = &gDemoActions[gTxActionIndex];

        if(action->action == RF_KVM_ACTION_SWITCH_TARGET)
        {
            KVM_SwitchTarget(action->value);
            gTxActionIndex++;
            continue;
        }

        if(action->action == RF_KVM_ACTION_TAP_KEY)
        {
            gTxCurrentKeycode = action->value;
            fill_keyboard_frame(gTxCurrentTarget, gTxCurrentKeycode, TRUE);
            rf_tx_start(TxBuf);
            gTxPhase = RF_TX_PHASE_SEND_RELEASE;
            return;
        }

        gTxActionIndex++;
    }

    gTxPhase = RF_TX_PHASE_DONE;
}

__HIGH_CODE
static void rf_tx_start(uint8_t *pBuf)
{
    RFIP_SetTxStart();
    gTxParam.frequency = RF_CHANNEL;
    gTxParam.txDMA = (uint32_t)pBuf;
    RFIP_SetTxParm(&gTxParam);
}

__HIGH_CODE
void RF_ProcessCallBack(rfRole_States_t sta, uint8_t id)
{
    (void)id;

    if(sta & RF_STATE_TX_FINISH)
    {
        gTxCount++;
    }
}

tmosEvents RFRole_ProcessEvent(tmosTaskID task_id, tmosEvents events)
{
    if(events & SYS_EVENT_MSG)
    {
        uint8_t *msgPtr = tmos_msg_receive(task_id);

        if(msgPtr)
        {
            tmos_msg_deallocate(msgPtr);
        }
        return events ^ SYS_EVENT_MSG;
    }

    if(events & RF_TEST_TX_EVENT)
    {
        PRINT("rf keyboard tx count=%lu\r\n", gTxCount);
        gTxCount = 0;
        return events ^ RF_TEST_TX_EVENT;
    }

    return 0;
}

__INTERRUPT
__HIGH_CODE
void TMR0_IRQHandler(void)
{
    if(TMR0_GetITFlag(TMR0_3_IT_CYC_END))
    {
        TMR0_ClearITFlag(TMR0_3_IT_CYC_END);

        gTxTickCount++;
        if(gTxPhase == RF_TX_PHASE_WAIT_START)
        {
            if(gTxTickCount >= RF_TX_START_DELAY_TICKS)
            {
                tx_start_next_demo_key();
            }
        }
        else if(gTxPhase == RF_TX_PHASE_SEND_RELEASE)
        {
            fill_keyboard_frame(gTxCurrentTarget, gTxCurrentKeycode, FALSE);
            rf_tx_start(TxBuf);
            gTxActionIndex++;
            gTxTickCount = 0;

            if(gTxActionIndex >= RF_DEMO_ACTION_COUNT)
            {
                gTxPhase = RF_TX_PHASE_DONE;
            }
            else
            {
                gTxPhase = RF_TX_PHASE_WAIT_NEXT;
            }
        }
        else if(gTxPhase == RF_TX_PHASE_WAIT_NEXT)
        {
            if(gTxTickCount >= RF_TX_INTER_KEY_TICKS)
            {
                tx_start_next_demo_key();
            }
        }
    }
}

void RFRole_Init(void)
{
    rfTaskID = TMOS_ProcessEventRegister(RFRole_ProcessEvent);

    {
        rfRoleConfig_t conf = {0};

        conf.TxPower = LL_TX_POWEER_4_DBM;
        conf.rfProcessCB = RF_ProcessCallBack;
        conf.processMask = RF_STATE_TX_FINISH;
        RFRole_BasicInit(&conf);
    }

    gParm.accessAddress = RF_SYNC_WORD;
    gParm.crcInit = 0x555555;
    gParm.properties = LLE_MODE_PHY_2M;
    gParm.sendInterval = 1999 * 2;
    gParm.sendTime = 20 * 2;
    RFRole_SetParam(&gParm);

    gTxParam.accessAddress = RF_SYNC_WORD;
    gTxParam.crcInit = 0x555555;
    gTxParam.properties = LLE_MODE_PHY_2M;
    gTxParam.sendCount = 1;
    gTxParam.frequency = RF_CHANNEL;
    gTxParam.txDMA = (uint32_t)TxBuf;

    PFIC_EnableIRQ(BLEB_IRQn);
    PFIC_EnableIRQ(BLEL_IRQn);

    PRINT("rf keyboard tx init.id=%d\r\n", rfTaskID);
    PRINT("KVM demo: target1=a1/enter, target2=b2/enter, target1=c1/enter after %d seconds\r\n",
          RF_TX_START_DELAY_SECONDS);

    tmos_start_reload_task(rfTaskID, RF_TEST_TX_EVENT, MS1_TO_SYSTEM_TIME(1000));
    TMR0_TimerInit(GetSysClock() / RF_TX_TIMER_HZ);
    TMR0_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
    PFIC_SetPriority(TMR0_IRQn, 0x80);
    PFIC_EnableIRQ(TMR0_IRQn);
}
