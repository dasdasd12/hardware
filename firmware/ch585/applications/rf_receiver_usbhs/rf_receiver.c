/*******************************************************************************
 * rf_receiver.c — 2.4G RF 接收器实现
 *
 * 运行在接收器 CH585 上（与键盘主机是同款硬件，但只运行接收器固件）。
 * 使用 CH585 RF IP（RFRole_BasicInit + RFIP_SetRx）持续监听。
 * 收到合法帧后，通过坏帧过滤（Magic + XOR）与 target_id 过滤，再交给 USBHS HID 输出。
 ******************************************************************************/

#include "rf_receiver.h"

/* ─── RX DMA 缓冲区 ─── */
__attribute__((__aligned__(4))) static uint8_t g_rx_buf[RF_RX_BUF_LEN];

/* ─── 解码后的报告（供 USB 层读取）─── */
static uint8_t  g_kbd_report[16];
static uint16_t g_consumer_usage;
static volatile uint8_t g_report_ready = 0;

/* ─── TMOS 任务 ID ─── */
static tmosTaskID g_rx_task_id = INVALID_TASK_ID;

/* ─── RFRole 参数 ─── */
static rfRoleParam_t g_rf_param;
static rfipRx_t      g_rx_param;

/* ─── TMOS 事件位 ─── */
#define RX_START_EVT    0x0001

/*******************************************************************************
 * static: XOR 校验
 ******************************************************************************/
static uint8_t frame_xor(const uint8_t *buf, uint8_t len)
{
    uint8_t x = 0;
    for (uint8_t i = 0; i < len; i++) x ^= buf[i];
    return x;
}

/*******************************************************************************
 * static: 开始接收
 ******************************************************************************/
__HIGH_CODE
static void rx_start(void)
{
    g_rx_param.frequency     = RF_CHANNEL;
    g_rx_param.timeOut       = 0;   /* 无超时，持续监听 */
    RFIP_SetRx(&g_rx_param);
}

/*******************************************************************************
 * RF_RX_ProcessCallBack — RF IP 中断回调（中断上下文！）
 ******************************************************************************/
__HIGH_CODE
void RF_RX_ProcessCallBack(rfRole_States_t sta, uint8_t id)
{
    if (sta & RF_STATE_RX) {
        /* 验证帧：Magic + 长度 + XOR */
        uint8_t *p = (uint8_t *)g_rx_param.rxDMA;
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
                /* Consumer */
                g_consumer_usage = (uint16_t)p[RF_CONSUMER_OFFSET] | ((uint16_t)p[RF_CONSUMER_OFFSET + 1] << 8);
                g_report_ready = 1;
            }
        }
        /* 继续监听 */
        rx_start();
    }

    if (sta & RF_STATE_RX_CRCERR) {
        rx_start();
    }

    if (sta & RF_STATE_TIMEOUT) {
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
    g_rx_param.rxDMA         = (uint32_t)g_rx_buf;
    g_rx_param.rxMaxLen      = RF_FRAME_LEN;
    PFIC_EnableIRQ(BLEB_IRQn);
    PFIC_EnableIRQ(BLEL_IRQn);

    /* 触发 TMOS 启动首次接收 */
    tmos_set_event(g_rx_task_id, RX_START_EVT);
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

/*******************************************************************************
 * RF_Receiver_GetConsumer
 ******************************************************************************/
uint8_t RF_Receiver_GetConsumer(uint16_t *out_usage)
{
    *out_usage = g_consumer_usage;
    return 1;
}
