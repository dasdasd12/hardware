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
#include <stddef.h>
#include <string.h>

#define RF_CHANNEL          16
#define RF_SYNC_WORD        0xA55A1234UL
#define RF_FRAME_MAGIC      0x55
#define RF_FRAME_LEN        25
#define RF_STRESS_MAGIC     0xA8
#define RF_STRESS_FRAME_LEN 8
#define RF_STRESS_PAYLOAD_LEN (RF_STRESS_FRAME_LEN - 2)
#define RF_KEYSTATE_MAGIC   0xB1
#define RF_KEYSTATE_FRAME_LEN 13
#define RF_KEYSTATE_PAYLOAD_LEN (RF_KEYSTATE_FRAME_LEN - 2)
#define RF_KEYSTATE_DOWN_OFFSET 4
#define RF_KBD_OFFSET       2
#define RF_CONSUMER_OFFSET  18
#define RF_RESERVED_OFFSET  20
#define RF_TARGET_OFFSET    RF_RESERVED_OFFSET
#define RF_SEQ_OFFSET       (RF_RESERVED_OFFSET + 1)

#define RF_TX_MODE_STRESS_8K      1
#define RF_TX_MODE_LEGACY_DEMO    2
#define RF_TX_MODE_KEYSTATE_8K    3

#ifndef RF_TX_MODE
#define RF_TX_MODE RF_TX_MODE_KEYSTATE_8K
#endif

#ifndef RF_TX_KEYSTATE_DEMO_ENABLE
#define RF_TX_KEYSTATE_DEMO_ENABLE 0
#endif

#if RF_TX_MODE == RF_TX_MODE_LEGACY_DEMO
#define RF_TX_TIMER_HZ      4
#else
#define RF_TX_TIMER_HZ      8000
#endif

#define RF_TX_START_DELAY_SECONDS 3
#define RF_TX_START_DELAY_TICKS   (RF_TX_TIMER_HZ * RF_TX_START_DELAY_SECONDS)
#define RF_TX_INTER_KEY_TICKS     2
#define RF_TX_KEYSTATE_DEMO_PERIOD_TICKS RF_TX_TIMER_HZ
#define RF_TX_KEYSTATE_DEMO_HOLD_TICKS   (RF_TX_TIMER_HZ / 20)
#define RF_TX_KEYSTATE_DEMO_KEY_ID       58

#define RF_SPI_BRIDGE_FRAME_MAGIC        0xB8
#define RF_SPI_BRIDGE_FRAME_TYPE_REPORT  0x31
#define RF_SPI_BRIDGE_FRAME_VERSION      1
#define RF_SPI_BRIDGE_REPORT_LEN         8

#define RF_TX_PHASE_WAIT_START   0
#define RF_TX_PHASE_SEND_RELEASE 1
#define RF_TX_PHASE_WAIT_NEXT    2
#define RF_TX_PHASE_DONE         3

#define HID_KEY_MIN    0x04
#define HID_KEY_A      0x04
#define HID_KEY_B      0x05
#define HID_KEY_C      0x06
#define HID_KEY_H      0x0B
#define HID_KEY_I      0x0C
#define HID_KEY_J      0x0D
#define HID_KEY_K      0x0E
#define HID_KEY_L      0x0F
#define HID_KEY_M      0x10
#define HID_KEY_N      0x11
#define HID_KEY_O      0x12
#define HID_KEY_P      0x13
#define HID_KEY_U      0x18
#define HID_KEY_Y      0x1C
#define HID_KEY_1      0x1E
#define HID_KEY_2      0x1F
#define HID_KEY_7      0x24
#define HID_KEY_8      0x25
#define HID_KEY_9      0x26
#define HID_KEY_0      0x27
#define HID_KEY_ENTER  0x28
#define HID_KEY_BACKSPACE 0x2A
#define HID_KEY_SPACE  0x2C
#define HID_KEY_MINUS  0x2D
#define HID_KEY_EQUAL  0x2E
#define HID_KEY_LBRACKET 0x2F
#define HID_KEY_RBRACKET 0x30
#define HID_KEY_BACKSLASH 0x31
#define HID_KEY_SEMICOLON 0x33
#define HID_KEY_QUOTE  0x34
#define HID_KEY_COMMA  0x36
#define HID_KEY_DOT    0x37
#define HID_KEY_SLASH  0x38
#define HID_KEY_F6     0x3F
#define HID_KEY_F7     0x40
#define HID_KEY_F8     0x41
#define HID_KEY_F9     0x42
#define HID_KEY_F10    0x43
#define HID_KEY_F11    0x44
#define HID_KEY_F12    0x45
#define HID_NKRO_BYTES 14
#define HID_MOD_MIN    0xE0
#define HID_MOD_MAX    0xE7
#define HID_MOD_RCTRL  0xE4
#define HID_MOD_RSHIFT 0xE5
#define HID_MOD_RALT   0xE6
#define HID_MOD_RGUI   0xE7

#define KVM_TARGET_1   1
#define KVM_TARGET_2   2

#define RF_KVM_ACTION_SWITCH_TARGET 0
#define RF_KVM_ACTION_TAP_KEY       1

typedef struct
{
    uint8_t action;
    uint8_t value;
} rfKvmDemoAction_t;

typedef struct __attribute__((packed))
{
    uint8_t magic;
    uint8_t type;
    uint8_t version;
    uint8_t seq;
    uint8_t flags;
    uint8_t report[RF_SPI_BRIDGE_REPORT_LEN];
    uint8_t first_key;
    uint16_t crc16;
} rfSpiBridgeFrame_t;

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
static volatile uint16_t gStressSeq;
static volatile uint16_t gKeyStateSeq;

static uint8_t gSpiBridgeRx[sizeof(rfSpiBridgeFrame_t)] __attribute__((aligned(4)));
static volatile uint8_t gSpiBridgeArmed;
static volatile uint8_t gSpiBridgeLastSeq;
static volatile uint8_t gSpiBridgeLastFlags;
static volatile uint8_t gSpiBridgeLastFirstKey = 0xFF;
static volatile uint32_t gSpiBridgeValidFrames;
static volatile uint32_t gSpiBridgeCrcErrors;
static volatile uint32_t gSpiBridgeMagicErrors;
static uint8_t gSpiBridgeLastReport[RF_SPI_BRIDGE_REPORT_LEN];

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

static const uint8_t gRightKeyToHid[64] = {
    HID_KEY_F12,       HID_KEY_F11,      HID_KEY_F10,      HID_KEY_F9,
    HID_KEY_F8,        HID_KEY_F7,       HID_KEY_F6,       HID_KEY_BACKSPACE,
    HID_KEY_EQUAL,     HID_KEY_MINUS,    0,                0,
    0,                 0,                0,                0,
    HID_KEY_0,         HID_KEY_9,        HID_KEY_8,        HID_KEY_7,
    HID_KEY_BACKSLASH, HID_KEY_RBRACKET, HID_KEY_LBRACKET, HID_KEY_P,
    HID_KEY_O,         HID_KEY_I,        0,                0,
    0,                 0,                0,                0,
    HID_KEY_U,         HID_KEY_Y,        HID_KEY_ENTER,    HID_KEY_QUOTE,
    HID_KEY_SEMICOLON, HID_KEY_L,        HID_KEY_K,        HID_KEY_J,
    HID_KEY_H,         HID_MOD_RSHIFT,   0,                0,
    0,                 0,                0,                0,
    HID_KEY_SLASH,     HID_KEY_DOT,      HID_KEY_COMMA,    HID_KEY_M,
    HID_KEY_N,         HID_KEY_B,        HID_MOD_RCTRL,    HID_MOD_RGUI,
    0,                 HID_MOD_RALT,     HID_KEY_SPACE,    0,
    0,                 0,                0,                0,
};

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

static uint16_t crc16_ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    for(uint16_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for(uint8_t bit = 0; bit < 8; bit++)
        {
            if(crc & 0x8000)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static void spi_bridge_arm_rx(void)
{
    R8_SPI0_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;
    R8_SPI0_CTRL_MOD |= RB_SPI_FIFO_DIR;
    R32_SPI0_DMA_BEG = (uint32_t)gSpiBridgeRx;
    R32_SPI0_DMA_END = (uint32_t)(gSpiBridgeRx + sizeof(gSpiBridgeRx));
    R16_SPI0_TOTAL_CNT = sizeof(gSpiBridgeRx);
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END | RB_SPI_IF_DMA_END |
                       RB_SPI_IF_FIFO_OV | RB_SPI_IF_BYTE_END;
    R8_SPI0_CTRL_CFG |= RB_SPI_DMA_ENABLE;
    gSpiBridgeArmed = TRUE;
}

static uint8_t spi_bridge_frame_valid(const rfSpiBridgeFrame_t *frame)
{
    uint16_t expected;

    if((frame->magic != RF_SPI_BRIDGE_FRAME_MAGIC) ||
       (frame->type != RF_SPI_BRIDGE_FRAME_TYPE_REPORT) ||
       (frame->version != RF_SPI_BRIDGE_FRAME_VERSION))
    {
        gSpiBridgeMagicErrors++;
        return FALSE;
    }

    expected = crc16_ccitt((const uint8_t *)frame,
                           (uint16_t)offsetof(rfSpiBridgeFrame_t, crc16));
    if(frame->crc16 != expected)
    {
        gSpiBridgeCrcErrors++;
        return FALSE;
    }

    return TRUE;
}

static void spi_bridge_handle_frame(const rfSpiBridgeFrame_t *frame)
{
    if(!spi_bridge_frame_valid(frame))
    {
        return;
    }

    memcpy(gSpiBridgeLastReport, frame->report, RF_SPI_BRIDGE_REPORT_LEN);
    gSpiBridgeLastSeq = frame->seq;
    gSpiBridgeLastFlags = frame->flags;
    gSpiBridgeLastFirstKey = frame->first_key;
    gSpiBridgeValidFrames++;
}

static void spi_bridge_init(void)
{
    memset(gSpiBridgeRx, 0, sizeof(gSpiBridgeRx));
    memset(gSpiBridgeLastReport, 0, sizeof(gSpiBridgeLastReport));
    gSpiBridgeLastFirstKey = 0xFF;

    GPIOPinRemap(DISABLE, RB_PIN_SPI0);
    SPI0_SlaveInit();
    SPI0_DataMode(Mode0_HighBitINFront);
    SetFirstData(0xA5);
    spi_bridge_arm_rx();

    PRINT("RF TX SPI bridge ready: SPI0 PA12/PA13/PA14/PA15 frame=%u\r\n",
          (unsigned int)sizeof(rfSpiBridgeFrame_t));
}

static void set_down_bit_for_usage(uint8_t down_bits[8], uint8_t usage)
{
    for(uint8_t key_id = 0; key_id < 64; key_id++)
    {
        if(gRightKeyToHid[key_id] == usage)
        {
            down_bits[key_id >> 3] |= (uint8_t)(1u << (key_id & 7u));
            return;
        }
    }
}

static void spi_bridge_report_to_down_bits(uint8_t down_bits[8])
{
    uint8_t report[RF_SPI_BRIDGE_REPORT_LEN];

    memcpy(report, gSpiBridgeLastReport, sizeof(report));

    for(uint8_t mod = 0; mod < 8; mod++)
    {
        if(report[0] & (uint8_t)(1u << mod))
        {
            set_down_bit_for_usage(down_bits, (uint8_t)(HID_MOD_MIN + mod));
        }
    }

    for(uint8_t i = 2; i < RF_SPI_BRIDGE_REPORT_LEN; i++)
    {
        if(report[i] != 0)
        {
            set_down_bit_for_usage(down_bits, report[i]);
        }
    }
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

static void fill_stress_frame(void)
{
    uint16_t seq = gStressSeq++;

    for(uint8_t i = 0; i < RF_STRESS_FRAME_LEN; i++)
    {
        TxBuf[i] = 0;
    }

    TxBuf[0] = RF_STRESS_MAGIC;
    TxBuf[1] = RF_STRESS_PAYLOAD_LEN;
    TxBuf[2] = (uint8_t)seq;
    TxBuf[3] = (uint8_t)(seq >> 8);
    TxBuf[4] = 0x5A;
    TxBuf[5] = 0xA5;
    TxBuf[6] = (uint8_t)(gTxTickCount & 0xFF);
    TxBuf[7] = frame_xor(TxBuf, RF_STRESS_FRAME_LEN - 1);
}

__attribute__((weak))
uint8_t RF_TX_GetDownBits(uint8_t down_bits[8])
{
    for(uint8_t i = 0; i < 8; i++)
    {
        down_bits[i] = 0;
    }

    if(gSpiBridgeValidFrames != 0)
    {
        spi_bridge_report_to_down_bits(down_bits);
    }

#if RF_TX_KEYSTATE_DEMO_ENABLE
    {
        uint32_t phase = gTxTickCount % RF_TX_KEYSTATE_DEMO_PERIOD_TICKS;

        if(phase < RF_TX_KEYSTATE_DEMO_HOLD_TICKS)
        {
            uint8_t key_id = RF_TX_KEYSTATE_DEMO_KEY_ID;
            down_bits[key_id >> 3] |= (uint8_t)(1u << (key_id & 7u));
        }
    }
#endif

    return 1;
}

void RFRole_Poll(void)
{
    if(!gSpiBridgeArmed)
    {
        spi_bridge_arm_rx();
        return;
    }

    if((R8_SPI0_INT_FLAG & RB_SPI_IF_CNT_END) == 0)
    {
        return;
    }

    R8_SPI0_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;
    gSpiBridgeArmed = FALSE;
    spi_bridge_handle_frame((const rfSpiBridgeFrame_t *)gSpiBridgeRx);
    spi_bridge_arm_rx();
}

static void fill_keystate_frame(void)
{
    uint8_t down_bits[8];
    uint16_t seq = gKeyStateSeq++;

    RF_TX_GetDownBits(down_bits);

    for(uint8_t i = 0; i < RF_KEYSTATE_FRAME_LEN; i++)
    {
        TxBuf[i] = 0;
    }

    TxBuf[0] = RF_KEYSTATE_MAGIC;
    TxBuf[1] = RF_KEYSTATE_PAYLOAD_LEN;
    TxBuf[2] = (uint8_t)seq;
    TxBuf[3] = (uint8_t)(seq >> 8);
    for(uint8_t i = 0; i < 8; i++)
    {
        TxBuf[RF_KEYSTATE_DOWN_OFFSET + i] = down_bits[i];
    }
    TxBuf[RF_KEYSTATE_FRAME_LEN - 1] = frame_xor(TxBuf, RF_KEYSTATE_FRAME_LEN - 1);
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
        uint16_t seq =
#if RF_TX_MODE == RF_TX_MODE_KEYSTATE_8K
            gKeyStateSeq;
#else
            gStressSeq;
#endif

        PRINT("rf tx gen=%lu done=%lu seq=%u hz=%u br=%lu br_crc=%lu br_magic=%lu hseq=%u first=%u\r\n",
              gTxTickCount,
              gTxCount,
              seq,
              RF_TX_TIMER_HZ,
              gSpiBridgeValidFrames,
              gSpiBridgeCrcErrors,
              gSpiBridgeMagicErrors,
              gSpiBridgeLastSeq,
              gSpiBridgeLastFirstKey);
        gTxCount = 0;
        gTxTickCount = 0;
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
#if RF_TX_MODE == RF_TX_MODE_STRESS_8K
        fill_stress_frame();
        rf_tx_start(TxBuf);
#elif RF_TX_MODE == RF_TX_MODE_KEYSTATE_8K
        fill_keystate_frame();
        rf_tx_start(TxBuf);
#else
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
#endif
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

    spi_bridge_init();

    PFIC_EnableIRQ(BLEB_IRQn);
    PFIC_EnableIRQ(BLEL_IRQn);

    PRINT("rf keyboard tx init.id=%d\r\n", rfTaskID);
#if RF_TX_MODE == RF_TX_MODE_STRESS_8K
    PRINT("RF 2.4G 8K stress TX: %u-byte frame, no ACK, 2M PHY\r\n", RF_STRESS_FRAME_LEN);
#elif RF_TX_MODE == RF_TX_MODE_KEYSTATE_8K
    PRINT("RF 2.4G 8K key-state TX: %u-byte frame, down_bits[8], demo=%u\r\n",
          RF_KEYSTATE_FRAME_LEN,
          RF_TX_KEYSTATE_DEMO_ENABLE);
#else
    PRINT("KVM demo: target1=a1/enter, target2=b2/enter, target1=c1/enter after %d seconds\r\n",
          RF_TX_START_DELAY_SECONDS);
#endif

    tmos_start_reload_task(rfTaskID, RF_TEST_TX_EVENT, MS1_TO_SYSTEM_TIME(1000));
    TMR0_TimerInit(GetSysClock() / RF_TX_TIMER_HZ);
    TMR0_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
    PFIC_SetPriority(TMR0_IRQn, 0x80);
    PFIC_EnableIRQ(TMR0_IRQn);
}
