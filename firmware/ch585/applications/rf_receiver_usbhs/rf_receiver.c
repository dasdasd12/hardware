/*******************************************************************************
 * rf_receiver.c — 2.4G RF 接收器实现
 *
 * 运行在接收器 CH585 上（与键盘主机是同款硬件，但只运行接收器固件）。
 * 使用 CH585 RF IP（RFRole_BasicInit + RFIP_SetRx）持续监听。
 * 收到合法帧后，通过坏帧过滤（Magic + XOR）与 target_id 过滤，再交给 USBHS HID 输出。
 ******************************************************************************/

#include "rf_receiver.h"
#include "usb_hid.h"
#include <string.h>

/* ─── RX DMA 双缓冲区：先重新进 RX，再解析旧 buffer，减少 8K 下的接收死区。─── */
__attribute__((__aligned__(4))) static uint8_t g_rx_buf[2][RF_RX_BUF_LEN];
static volatile uint8_t g_rx_buf_index;

/* ─── 解码后的报告（供 USB 层读取）─── */
static uint8_t  g_kbd_report[16];
static uint16_t g_consumer_usage;
static volatile uint8_t g_report_ready = 0;

#define HID_KEY_MIN        0x04
#define HID_KEY_MAX_NKRO   0x6F
#define HID_MOD_MIN        0xE0
#define HID_MOD_MAX        0xE7
#define HID_NKRO_BYTES     14

#define HID_KEY_B          0x05
#define HID_KEY_H          0x0B
#define HID_KEY_I          0x0C
#define HID_KEY_J          0x0D
#define HID_KEY_K          0x0E
#define HID_KEY_L          0x0F
#define HID_KEY_M          0x10
#define HID_KEY_N          0x11
#define HID_KEY_O          0x12
#define HID_KEY_P          0x13
#define HID_KEY_U          0x18
#define HID_KEY_Y          0x1C
#define HID_KEY_7          0x24
#define HID_KEY_8          0x25
#define HID_KEY_9          0x26
#define HID_KEY_0          0x27
#define HID_KEY_ENTER      0x28
#define HID_KEY_BACKSPACE  0x2A
#define HID_KEY_SPACE      0x2C
#define HID_KEY_MINUS      0x2D
#define HID_KEY_EQUAL      0x2E
#define HID_KEY_LBRACKET   0x2F
#define HID_KEY_RBRACKET   0x30
#define HID_KEY_BACKSLASH  0x31
#define HID_KEY_SEMICOLON  0x33
#define HID_KEY_QUOTE      0x34
#define HID_KEY_COMMA      0x36
#define HID_KEY_DOT        0x37
#define HID_KEY_SLASH      0x38
#define HID_KEY_F6         0x3F
#define HID_KEY_F7         0x40
#define HID_KEY_F8         0x41
#define HID_KEY_F9         0x42
#define HID_KEY_F10        0x43
#define HID_KEY_F11        0x44
#define HID_KEY_F12        0x45
#define HID_MOD_RCTRL      0xE4
#define HID_MOD_RSHIFT     0xE5
#define HID_MOD_RALT       0xE6
#define HID_MOD_RGUI       0xE7

static const uint8_t g_right_key_to_hid[64] = {
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

/* ─── TMOS 任务 ID ─── */
static tmosTaskID g_rx_task_id = INVALID_TASK_ID;

/* ─── RFRole 参数 ─── */
static rfRoleParam_t g_rf_param;
static rfipRx_t      g_rx_param;

/* ─── TMOS 事件位 ─── */
#define RX_START_EVT    0x0001
#define RX_STATS_EVT    0x0002

static volatile uint32_t g_rx_irq_count;
static volatile uint32_t g_stress_ok_count;
static volatile uint32_t g_stress_lost_count;
static volatile uint32_t g_bad_xor_count;
static volatile uint32_t g_bad_len_count;
static volatile uint32_t g_legacy_ok_count;
static volatile uint32_t g_key_state_ok_count;
static volatile uint32_t g_crcerr_count;
static volatile uint32_t g_timeout_count;
static volatile uint16_t g_last_stress_seq;
static volatile uint16_t g_last_key_state_seq;
static volatile uint8_t  g_have_stress_seq;
static volatile int8_t   g_last_rssi;
static volatile uint32_t g_usb_hs_sent_count;
static volatile uint32_t g_usb_hs_drop_count;
static volatile uint8_t  g_usb_hs_report_pending;
static volatile uint32_t g_kbd_nonzero_count;
static uint8_t g_usb_hs_report[64];

/*******************************************************************************
 * static: XOR 校验
 ******************************************************************************/
static uint8_t frame_xor(const uint8_t *buf, uint8_t len)
{
    uint8_t x = 0;
    for (uint8_t i = 0; i < len; i++) x ^= buf[i];
    return x;
}

static void put_u16_le(uint8_t *buf, uint8_t off, uint16_t val)
{
    buf[off] = (uint8_t)val;
    buf[off + 1] = (uint8_t)(val >> 8);
}

static void put_u32_le(uint8_t *buf, uint8_t off, uint32_t val)
{
    buf[off] = (uint8_t)val;
    buf[off + 1] = (uint8_t)(val >> 8);
    buf[off + 2] = (uint8_t)(val >> 16);
    buf[off + 3] = (uint8_t)(val >> 24);
}

static uint8_t report_or16(const uint8_t *report)
{
    uint8_t value = 0U;
    uint8_t i;

    for(i = 0U; i < 16U; i++)
    {
        value |= report[i];
    }
    return value;
}

static void nkro_set_usage(uint8_t *report, uint8_t usage)
{
    if((usage >= HID_MOD_MIN) && (usage <= HID_MOD_MAX))
    {
        report[0] |= (uint8_t)(1u << (usage - HID_MOD_MIN));
        return;
    }

    if((usage >= HID_KEY_MIN) && (usage <= HID_KEY_MAX_NKRO))
    {
        uint8_t bit_index = (uint8_t)(usage - HID_KEY_MIN);
        uint8_t byte_index = (uint8_t)(bit_index >> 3);

        if(byte_index < HID_NKRO_BYTES)
        {
            report[2 + byte_index] |= (uint8_t)(1u << (bit_index & 7u));
        }
    }
}

static void build_right_half_report(const uint8_t down_bits[8], uint8_t report[16])
{
    for(uint8_t i = 0; i < 16; i++)
    {
        report[i] = 0;
    }

    for(uint8_t key_id = 0; key_id < 64; key_id++)
    {
        if(down_bits[key_id >> 3] & (uint8_t)(1u << (key_id & 7u)))
        {
            nkro_set_usage(report, g_right_key_to_hid[key_id]);
        }
    }
}

static void build_usb_hs_report(uint16_t seq)
{
    uint8_t xorv = 0;

    if(g_usb_hs_report_pending) {
        g_usb_hs_drop_count++;
        return;
    }

    memset(g_usb_hs_report, 0, sizeof(g_usb_hs_report));
    g_usb_hs_report[0] = 0xA8;
    g_usb_hs_report[1] = 0x01;
    put_u16_le(g_usb_hs_report, 2, seq);
    put_u32_le(g_usb_hs_report, 4, g_stress_ok_count);
    put_u32_le(g_usb_hs_report, 8, g_stress_lost_count);
    put_u32_le(g_usb_hs_report, 12, g_bad_xor_count);
    put_u32_le(g_usb_hs_report, 16, g_bad_len_count);
    put_u32_le(g_usb_hs_report, 20, g_crcerr_count);
    put_u32_le(g_usb_hs_report, 24, g_rx_irq_count);
    put_u32_le(g_usb_hs_report, 28, g_usb_hs_sent_count);
    put_u32_le(g_usb_hs_report, 32, g_usb_hs_drop_count);
    put_u32_le(g_usb_hs_report, 36, g_legacy_ok_count);
    put_u32_le(g_usb_hs_report, 40, g_timeout_count);
    g_usb_hs_report[44] = (uint8_t)g_last_rssi;
    put_u32_le(g_usb_hs_report, 45, g_key_state_ok_count);
    put_u16_le(g_usb_hs_report, 49, g_last_key_state_seq);
    put_u32_le(g_usb_hs_report, 51, g_kbd_nonzero_count);
    for(uint8_t i = 0; i < 8; i++)
    {
        g_usb_hs_report[55 + i] = g_kbd_report[i];
    }

    for(uint8_t i = 0; i < 63; i++) {
        xorv ^= g_usb_hs_report[i];
    }
    g_usb_hs_report[63] = xorv;
    g_usb_hs_report_pending = 1;
}

/*******************************************************************************
 * static: 开始接收
 ******************************************************************************/
__HIGH_CODE
static void rx_start(void)
{
    g_rx_param.frequency     = RF_CHANNEL;
    g_rx_param.timeOut       = 0;   /* 无超时，持续监听 */
    g_rx_param.rxDMA         = (uint32_t)g_rx_buf[g_rx_buf_index];
    RFIP_SetRx(&g_rx_param);
}

/*******************************************************************************
 * RF_RX_ProcessCallBack — RF IP 中断回调（中断上下文！）
 ******************************************************************************/
__HIGH_CODE
void RF_RX_ProcessCallBack(rfRole_States_t sta, uint8_t id)
{
    (void)id;

    if (sta & RF_STATE_RX) {
        /* 验证帧：Magic + 长度 + XOR */
        uint8_t *p = (uint8_t *)g_rx_param.rxDMA;
        g_rx_buf_index ^= 1;
        rx_start();
        g_rx_irq_count++;

        if (p[0] == RF_STRESS_MAGIC) {
            if (p[1] != RF_STRESS_PAYLOAD_LEN) {
                g_bad_len_count++;
            } else if (frame_xor(p, RF_STRESS_FRAME_LEN - 1) != p[RF_STRESS_FRAME_LEN - 1]) {
                g_bad_xor_count++;
            } else {
                uint16_t seq = (uint16_t)p[2] | ((uint16_t)p[3] << 8);

                if (g_have_stress_seq) {
                    uint16_t delta = (uint16_t)(seq - g_last_stress_seq);
                    if (delta != 1) {
                        g_stress_lost_count += (uint16_t)(delta - 1);
                    }
                } else {
                    g_have_stress_seq = 1;
                }

                g_last_stress_seq = seq;
                g_last_rssi = RFIP_ReadRssi();
                g_stress_ok_count++;
            }

            return;
        }

        if (p[0] == RF_KEYSTATE_MAGIC) {
            if (p[1] != RF_KEYSTATE_PAYLOAD_LEN) {
                g_bad_len_count++;
            } else if (frame_xor(p, RF_KEYSTATE_FRAME_LEN - 1) != p[RF_KEYSTATE_FRAME_LEN - 1]) {
                g_bad_xor_count++;
            } else {
                uint8_t next_report[16];
                uint16_t seq = (uint16_t)p[RF_KEYSTATE_SEQ_OFFSET] |
                               ((uint16_t)p[RF_KEYSTATE_SEQ_OFFSET + 1] << 8);

                g_last_key_state_seq = seq;
                g_last_rssi = RFIP_ReadRssi();
                g_key_state_ok_count++;

                build_right_half_report(&p[RF_KEYSTATE_DOWN_OFFSET], next_report);
                if(memcmp(g_kbd_report, next_report, sizeof(g_kbd_report)) != 0)
                {
                    memcpy(g_kbd_report, next_report, sizeof(g_kbd_report));
                    g_consumer_usage = 0;
                    g_report_ready = 1;
                }
            }

            return;
        }

        if (p[0] == RF_FRAME_MAGIC &&
            p[1] == (RF_FRAME_LEN - 2) &&
            frame_xor(p, RF_FRAME_LEN - 1) == p[RF_FRAME_LEN - 1])
        {
            uint8_t target_id = p[RF_TARGET_OFFSET];

            if ((target_id == RF_TARGET_BROADCAST) ||
                (target_id == RF_LOCAL_TARGET_ID))
            {
                /* 复制键盘报告（16B: modifier + reserved + bitmap14B）*/
                for (uint8_t i = 0; i < 16; i++)
                    g_kbd_report[i] = p[RF_KBD_OFFSET + i];
                if(report_or16(g_kbd_report) != 0U)
                {
                    g_kbd_nonzero_count++;
                }
                /* Consumer */
                g_consumer_usage = (uint16_t)p[RF_CONSUMER_OFFSET] | ((uint16_t)p[RF_CONSUMER_OFFSET + 1] << 8);
                g_report_ready = 1;
                g_legacy_ok_count++;
            }
        }
    }

    if (sta & RF_STATE_RX_CRCERR) {
        g_crcerr_count++;
        rx_start();
    }

    if (sta & RF_STATE_TIMEOUT) {
        g_timeout_count++;
        rx_start();
    }
}

/*******************************************************************************
 * RFRx_ProcessEvent — TMOS 任务
 ******************************************************************************/
tmosEvents RFRx_ProcessEvent(tmosTaskID task_id, tmosEvents events)
{
    if (events & SYS_EVENT_MSG) {
        uint8_t *p = tmos_msg_receive(task_id);
        if (p) tmos_msg_deallocate(p);
        return events ^ SYS_EVENT_MSG;
    }
    if (events & RX_START_EVT) {
        rx_start();
        return events ^ RX_START_EVT;
    }
    if (events & RX_STATS_EVT) {
        uint32_t key_state = g_key_state_ok_count;

#ifdef DEBUG
        static uint32_t last_irq;
        static uint32_t last_ok;
        static uint32_t last_lost;
        static uint32_t last_bad_xor;
        static uint32_t last_bad_len;
        static uint32_t last_crcerr;
        static uint32_t last_key_state;

        uint32_t irq = g_rx_irq_count;
        uint32_t ok = g_stress_ok_count;
        uint32_t lost = g_stress_lost_count;
        uint32_t bad_xor = g_bad_xor_count;
        uint32_t bad_len = g_bad_len_count;
        uint32_t crcerr = g_crcerr_count;

        PRINT("rf8k rx=%lu stress/s=%lu key/s=%lu lost/s=%lu total_stress=%lu total_key=%lu total_lost=%lu bad_xor/s=%lu bad_len/s=%lu crc/s=%lu rssi=%d seq=%u key_seq=%u\r\n",
              irq - last_irq,
              ok - last_ok,
              key_state - last_key_state,
              lost - last_lost,
              ok,
              key_state,
              lost,
              bad_xor - last_bad_xor,
              bad_len - last_bad_len,
              crcerr - last_crcerr,
              g_last_rssi,
              g_last_stress_seq,
              g_last_key_state_seq);

        last_irq = irq;
        last_ok = ok;
        last_lost = lost;
        last_bad_xor = bad_xor;
        last_bad_len = bad_len;
        last_crcerr = crcerr;
        last_key_state = key_state;
#endif

        build_usb_hs_report((key_state != 0) ? g_last_key_state_seq : g_last_stress_seq);
        return events ^ RX_STATS_EVT;
    }
    return 0;
}

/*******************************************************************************
 * RF_Receiver_Init
 ******************************************************************************/
void RF_Receiver_Init(void)
{
    g_rx_task_id = TMOS_ProcessEventRegister(RFRx_ProcessEvent);

    /* RFRole 全局配置 */
    {
        rfRoleConfig_t conf = {0};
        conf.TxPower     = LL_TX_POWEER_0_DBM;  /* 接收器不发射 */
        conf.rfProcessCB = RF_RX_ProcessCallBack;
        conf.processMask = RF_STATE_RX | RF_STATE_RX_CRCERR | RF_STATE_TIMEOUT;
        RFRole_BasicInit(&conf);
    }

    /* 链接层参数（与发射端完全一致）*/
    g_rf_param.accessAddress = RF_SYNC_WORD;
    g_rf_param.crcInit       = 0x555555;
    g_rf_param.properties    = LLE_MODE_PHY_2M;
    g_rf_param.sendInterval  = 0;
    g_rf_param.sendTime      = 0;
    RFRole_SetParam(&g_rf_param);

    /* RX 参数 */
    g_rx_param.accessAddress = RF_SYNC_WORD;
    g_rx_param.crcInit       = 0x555555;
    g_rx_param.properties    = LLE_MODE_PHY_2M;
    g_rx_buf_index           = 0;
    g_rx_param.rxDMA         = (uint32_t)g_rx_buf[g_rx_buf_index];
    g_rx_param.rxMaxLen      = RF_FRAME_LEN;
    PFIC_EnableIRQ(BLEB_IRQn);
    PFIC_EnableIRQ(BLEL_IRQn);

    /* 触发 TMOS 启动首次接收 */
    tmos_set_event(g_rx_task_id, RX_START_EVT);
    tmos_start_reload_task(g_rx_task_id, RX_STATS_EVT, MS1_TO_SYSTEM_TIME(1000));

    PRINT("rf rx usbhs init: legacy keyboard + 8K key-state/stress short frames, 2M PHY\r\n");
}

/*******************************************************************************
 * RF_Receiver_GetKbdReport — 轮询获取最新键盘报告
 *   返回 1=有新数据，0=无新数据
 ******************************************************************************/
uint8_t RF_Receiver_GetKbdReport(uint8_t *out16)
{
    if (!g_report_ready) return 0;
    for (uint8_t i = 0; i < 16; i++) out16[i] = g_kbd_report[i];
    g_report_ready = 0;
    return 1;
}

void RF_Receiver_ServiceUsbHsReport(void)
{
    uint8_t report[64];

    if(!g_usb_hs_report_pending) {
        return;
    }

    for(uint8_t i = 0; i < sizeof(report); i++) {
        report[i] = g_usb_hs_report[i];
    }

    if(USB_HID_SendCustom64(report)) {
        g_usb_hs_sent_count++;
        g_usb_hs_report_pending = 0;
    }
}

/*******************************************************************************
 * RF_Receiver_GetConsumer
 ******************************************************************************/
uint8_t RF_Receiver_GetConsumer(uint16_t *out_usage)
{
    *out_usage = g_consumer_usage;
    return 1;
}
