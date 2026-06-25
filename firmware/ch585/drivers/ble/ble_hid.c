/********************************** (C) COPYRIGHT *******************************
 * File Name          : ble_hid.c
 * Description        : Minimal connectable BLE HID keyboard task for CH585M
 *******************************************************************************/

#include "CONFIG.h"
#include "HAL.h"
#include "ble_hid.h"
#include "devinfoservice.h"
#include "battservice.h"
#include "hidkbdservice.h"
#include "hiddev.h"
#include "usb_cdc_debug.h"

#define BLE_HID_LOG(...)            \
    do                              \
    {                               \
        USB_CDC_DebugLog(__VA_ARGS__); \
    } while(0)

#define BLE_HID_PARAM_UPDATE_EVT      0x0001
#define BLE_HID_PHY_UPDATE_EVT        0x0002
#define BLE_HID_KEY_TAP_DOWN_EVT      0x0004
#define BLE_HID_KEY_TAP_UP_EVT        0x0008
#define BLE_HID_ADV_INTERVAL          160
#define BLE_HID_MIN_CONN_INTERVAL     8
#define BLE_HID_MAX_CONN_INTERVAL     8
#define BLE_HID_SLAVE_LATENCY         0
#define BLE_HID_CONN_TIMEOUT          500
#define BLE_HID_DEVICE_NAME           "CH585_LX_TEST"
#define BLE_HID_IDLE_TIMEOUT_MS       60000
#define BLE_HID_KEY_TAP_HOLD_MS       120
#define BLE_HID_KEY_TAP_RETRY_MS      800
#define BLE_HID_KEY_TAP_MAX_RETRY     20
#define BLE_HID_KEY_TAP_QUEUE_SIZE    16
#define BLE_HID_CLEAR_BONDS_ON_BOOT   TRUE

typedef struct
{
    uint8_t modifier;
    uint8_t keycode;
} bleHidKeyTap_t;

static uint8_t g_ble_task_id = INVALID_TASK_ID;
static uint16_t g_conn_handle = GAP_CONNHANDLE_INIT;
static uint8_t g_key_tap_busy = FALSE;
static uint8_t g_key_tap_retry_count = 0;
static uint8_t g_key_tap_modifier = 0;
static uint8_t g_key_tap_keycode = HID_KEYBOARD_RESERVED;
static bleHidKeyTap_t g_key_tap_queue[BLE_HID_KEY_TAP_QUEUE_SIZE];
static uint8_t g_key_tap_queue_head = 0;
static uint8_t g_key_tap_queue_tail = 0;
static uint8_t g_key_tap_queue_count = 0;

static uint8_t scanRspData[] = {
    0x0E,
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'C', 'H', '5', '8', '5', '_', 'L', 'X', '_', 'T', 'E', 'S', 'T',

    0x05,
    GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
    LO_UINT16(BLE_HID_MIN_CONN_INTERVAL),
    HI_UINT16(BLE_HID_MIN_CONN_INTERVAL),
    LO_UINT16(BLE_HID_MAX_CONN_INTERVAL),
    HI_UINT16(BLE_HID_MAX_CONN_INTERVAL),

    0x05,
    GAP_ADTYPE_16BIT_MORE,
    LO_UINT16(HID_SERV_UUID),
    HI_UINT16(HID_SERV_UUID),
    LO_UINT16(BATT_SERV_UUID),
    HI_UINT16(BATT_SERV_UUID),

    0x02,
    GAP_ADTYPE_POWER_LEVEL,
    0
};

static uint8_t advertData[] = {
    0x02,
    GAP_ADTYPE_FLAGS,
    GAP_ADTYPE_FLAGS_GENERAL | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

    0x0E,
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'C', 'H', '5', '8', '5', '_', 'L', 'X', '_', 'T', 'E', 'S', 'T',

    0x03,
    GAP_ADTYPE_APPEARANCE,
    LO_UINT16(GAP_APPEARE_HID_KEYBOARD),
    HI_UINT16(GAP_APPEARE_HID_KEYBOARD)
};

static const uint8_t attDeviceName[GAP_DEVICE_NAME_LEN] = BLE_HID_DEVICE_NAME;

static hidDevCfg_t hidCfg = {
    BLE_HID_IDLE_TIMEOUT_MS,
    HID_FEATURE_FLAGS
};

static uint8_t hidRptCB(uint8_t id, uint8_t type, uint16_t uuid,
                        uint8_t oper, uint16_t *pLen, uint8_t *pData);
static void hidEvtCB(uint8_t evt);
static void hidStateCB(gapRole_States_t newState, gapRoleEvent_t *pEvent);
static void bleHidBuildKeyboardReport(uint8_t modifier, uint8_t keycode, uint8_t *report8);
static void bleHidStartKeyTap(uint8_t modifier, uint8_t keycode);
static void bleHidFinishCurrentKeyTap(void);
static void bleHidStartNextQueuedTap(void);
static uint8_t bleHidKeyTapQueuePush(uint8_t modifier, uint8_t keycode);
static uint8_t bleHidKeyTapQueuePop(bleHidKeyTap_t *tap);
static void bleHidClearKeyTapQueue(void);
static void bleHidResetKeyTapState(void);

static hidDevCB_t hidCBs = {
    hidRptCB,
    hidEvtCB,
    NULL,
    hidStateCB
};

void BLE_HID_Init(void)
{
    g_ble_task_id = TMOS_ProcessEventRegister(BLE_HID_ProcessEvent);

    {
        uint8_t adv_enable = TRUE;
        uint8_t adv_filter_policy = GAP_FILTER_POLICY_ALL;
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &adv_enable);
        GAPRole_SetParameter(GAPROLE_ADV_FILTER_POLICY, sizeof(uint8_t), &adv_filter_policy);
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
        GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scanRspData), scanRspData);
    }

    GGS_SetParameter(GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, (void *)attDeviceName);

    {
        uint32_t passkey = 0;
        uint8_t pair_mode = GAPBOND_PAIRING_MODE_WAIT_FOR_REQ;
        uint8_t mitm = FALSE;
        uint8_t io_cap = GAPBOND_IO_CAP_NO_INPUT_NO_OUTPUT;
        uint8_t bonding = TRUE;

        GAPBondMgr_SetParameter(GAPBOND_PERI_DEFAULT_PASSCODE, sizeof(uint32_t), &passkey);
        GAPBondMgr_SetParameter(GAPBOND_PERI_PAIRING_MODE, sizeof(uint8_t), &pair_mode);
        GAPBondMgr_SetParameter(GAPBOND_PERI_MITM_PROTECTION, sizeof(uint8_t), &mitm);
        GAPBondMgr_SetParameter(GAPBOND_PERI_IO_CAPABILITIES, sizeof(uint8_t), &io_cap);
        GAPBondMgr_SetParameter(GAPBOND_PERI_BONDING_ENABLED, sizeof(uint8_t), &bonding);
    }

    {
        uint8_t critical_level = 6;
        Batt_SetParameter(BATT_PARAM_CRITICAL_LEVEL, sizeof(uint8_t), &critical_level);
    }

    {
        uint16_t adv_interval = BLE_HID_ADV_INTERVAL;
        GAP_SetParamValue(TGAP_DISC_ADV_INT_MIN, adv_interval);
        GAP_SetParamValue(TGAP_DISC_ADV_INT_MAX, adv_interval);
    }

    {
        bStatus_t hid_status = Hid_AddService();
        BLE_HID_LOG("%s: Hid_AddService=%x\n", BLE_HID_DEVICE_NAME, hid_status);
    }

    HidDev_Register(&hidCfg, &hidCBs);

    if(BLE_HID_CLEAR_BONDS_ON_BOOT)
    {
        bStatus_t erase_status = HidDev_SetParameter(HIDDEV_ERASE_ALLBONDS, 0, NULL);
        BLE_HID_LOG("%s: erase bonds=%x\n", BLE_HID_DEVICE_NAME, erase_status);
    }
}

uint16_t BLE_HID_ProcessEvent(uint8_t task_id, uint16_t events)
{
    (void)task_id;

    if(events & SYS_EVENT_MSG)
    {
        uint8_t *pMsg = tmos_msg_receive(g_ble_task_id);
        if(pMsg != NULL)
        {
            tmos_msg_deallocate(pMsg);
        }
        return (events ^ SYS_EVENT_MSG);
    }

    if(events & BLE_HID_PARAM_UPDATE_EVT)
    {
        if(BLE_HID_IsConnected())
        {
            GAPRole_PeripheralConnParamUpdateReq(g_conn_handle,
                                                 BLE_HID_MIN_CONN_INTERVAL,
                                                 BLE_HID_MAX_CONN_INTERVAL,
                                                 BLE_HID_SLAVE_LATENCY,
                                                 BLE_HID_CONN_TIMEOUT,
                                                 g_ble_task_id);
        }
        return (events ^ BLE_HID_PARAM_UPDATE_EVT);
    }

    if(events & BLE_HID_PHY_UPDATE_EVT)
    {
        if(BLE_HID_IsConnected())
        {
            GAPRole_UpdatePHY(g_conn_handle, 0,
                              GAP_PHY_BIT_LE_2M, GAP_PHY_BIT_LE_2M, 0);
        }
        return (events ^ BLE_HID_PHY_UPDATE_EVT);
    }

    if(events & BLE_HID_KEY_TAP_DOWN_EVT)
    {
        uint8_t report[BLE_HID_KBD_REPORT_LEN];
        uint8_t status;

        if(!BLE_HID_IsConnected())
        {
            bleHidResetKeyTapState();
            return (events ^ BLE_HID_KEY_TAP_DOWN_EVT);
        }

        if(g_key_tap_keycode == HID_KEYBOARD_RESERVED)
        {
            bleHidResetKeyTapState();
            return (events ^ BLE_HID_KEY_TAP_DOWN_EVT);
        }

        bleHidBuildKeyboardReport(g_key_tap_modifier, g_key_tap_keycode, report);
        status = BLE_HID_SendKeyboard(report);
        if(status == SUCCESS)
        {
            BLE_HID_LOG("%s: key tap down sent mod=%02x key=%02x\n",
                        BLE_HID_DEVICE_NAME, g_key_tap_modifier, g_key_tap_keycode);
            g_key_tap_retry_count = 0;
            tmos_start_task(g_ble_task_id, BLE_HID_KEY_TAP_UP_EVT, BLE_HID_KEY_TAP_HOLD_MS);
        }
        else
        {
            g_key_tap_retry_count++;
            BLE_HID_LOG("%s: key tap down retry=%d status=%x\n",
                        BLE_HID_DEVICE_NAME, g_key_tap_retry_count, status);
            if(g_key_tap_retry_count < BLE_HID_KEY_TAP_MAX_RETRY)
            {
                tmos_start_task(g_ble_task_id, BLE_HID_KEY_TAP_DOWN_EVT, BLE_HID_KEY_TAP_RETRY_MS);
            }
            else
            {
                BLE_HID_LOG("%s: key tap down dropped mod=%02x key=%02x\n",
                            BLE_HID_DEVICE_NAME, g_key_tap_modifier, g_key_tap_keycode);
                bleHidFinishCurrentKeyTap();
            }
        }
        return (events ^ BLE_HID_KEY_TAP_DOWN_EVT);
    }

    if(events & BLE_HID_KEY_TAP_UP_EVT)
    {
        uint8_t report[BLE_HID_KBD_REPORT_LEN];
        uint8_t status;

        bleHidBuildKeyboardReport(0, HID_KEYBOARD_RESERVED, report);
        status = BLE_HID_SendKeyboard(report);
        if(status == SUCCESS)
        {
            BLE_HID_LOG("%s: key tap up status=%x\n", BLE_HID_DEVICE_NAME, status);
            bleHidFinishCurrentKeyTap();
        }
        else
        {
            g_key_tap_retry_count++;
            BLE_HID_LOG("%s: key tap up retry=%d status=%x\n",
                        BLE_HID_DEVICE_NAME, g_key_tap_retry_count, status);
            if(g_key_tap_retry_count < BLE_HID_KEY_TAP_MAX_RETRY)
            {
                tmos_start_task(g_ble_task_id, BLE_HID_KEY_TAP_UP_EVT, BLE_HID_KEY_TAP_RETRY_MS);
            }
            else
            {
                BLE_HID_LOG("%s: key tap up dropped mod=%02x key=%02x\n",
                            BLE_HID_DEVICE_NAME, g_key_tap_modifier, g_key_tap_keycode);
                bleHidFinishCurrentKeyTap();
            }
        }
        return (events ^ BLE_HID_KEY_TAP_UP_EVT);
    }

    return 0;
}

uint8_t BLE_HID_IsConnected(void)
{
    return (g_conn_handle != GAP_CONNHANDLE_INIT) ? TRUE : FALSE;
}

uint8_t BLE_HID_IsKeyTapBusy(void)
{
    return g_key_tap_busy;
}

uint8_t BLE_HID_GetQueuedTapCount(void)
{
    return g_key_tap_queue_count;
}

void BLE_HID_StartAdvert(void)
{
    uint8_t en = TRUE;
    GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &en);
}

void BLE_HID_StopAdvert(void)
{
    uint8_t en = FALSE;
    GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &en);
}

uint8_t BLE_HID_SendKeyboard(const uint8_t *report8)
{
    if(!BLE_HID_IsConnected())
    {
        return bleNotReady;
    }

    return HidDev_Report(HID_RPT_ID_KEY_IN,
                         HID_REPORT_TYPE_INPUT,
                         BLE_HID_KBD_REPORT_LEN,
                         (uint8_t *)report8);
}

uint8_t BLE_HID_TriggerKeyTap(uint8_t keycode)
{
    return BLE_HID_TriggerModifiedKeyTap(0, keycode);
}

uint8_t BLE_HID_TriggerModifiedKeyTap(uint8_t modifier, uint8_t keycode)
{
    if(keycode == HID_KEYBOARD_RESERVED)
    {
        return INVALIDPARAMETER;
    }

    if(!BLE_HID_IsConnected())
    {
        return bleNotReady;
    }

    if(g_key_tap_busy)
    {
        if(!bleHidKeyTapQueuePush(modifier, keycode))
        {
            BLE_HID_LOG("%s: key tap queue full mod=%02x key=%02x\n",
                        BLE_HID_DEVICE_NAME, modifier, keycode);
            return bleNoResources;
        }

        BLE_HID_LOG("%s: key tap queued mod=%02x key=%02x depth=%d\n",
                    BLE_HID_DEVICE_NAME, modifier, keycode, g_key_tap_queue_count);
        return SUCCESS;
    }

    bleHidStartKeyTap(modifier, keycode);
    return SUCCESS;
}

static void bleHidBuildKeyboardReport(uint8_t modifier, uint8_t keycode, uint8_t *report8)
{
    uint8_t i;

    for(i = 0; i < BLE_HID_KBD_REPORT_LEN; ++i)
    {
        report8[i] = 0;
    }

    report8[0] = modifier;
    report8[2] = keycode;
}

static void bleHidStartKeyTap(uint8_t modifier, uint8_t keycode)
{
    g_key_tap_busy = TRUE;
    g_key_tap_retry_count = 0;
    g_key_tap_modifier = modifier;
    g_key_tap_keycode = keycode;

    if(g_ble_task_id != INVALID_TASK_ID)
    {
        tmos_set_event(g_ble_task_id, BLE_HID_KEY_TAP_DOWN_EVT);
    }
}

static void bleHidFinishCurrentKeyTap(void)
{
    g_key_tap_busy = FALSE;
    g_key_tap_retry_count = 0;
    g_key_tap_modifier = 0;
    g_key_tap_keycode = HID_KEYBOARD_RESERVED;

    if(g_ble_task_id != INVALID_TASK_ID)
    {
        tmos_stop_task(g_ble_task_id, BLE_HID_KEY_TAP_DOWN_EVT);
        tmos_stop_task(g_ble_task_id, BLE_HID_KEY_TAP_UP_EVT);
    }

    bleHidStartNextQueuedTap();
}

static void bleHidStartNextQueuedTap(void)
{
    bleHidKeyTap_t tap;

    if(!BLE_HID_IsConnected())
    {
        bleHidClearKeyTapQueue();
        return;
    }

    if(bleHidKeyTapQueuePop(&tap))
    {
        BLE_HID_LOG("%s: key tap dequeue mod=%02x key=%02x left=%d\n",
                    BLE_HID_DEVICE_NAME, tap.modifier, tap.keycode, g_key_tap_queue_count);
        bleHidStartKeyTap(tap.modifier, tap.keycode);
    }
}

static uint8_t bleHidKeyTapQueuePush(uint8_t modifier, uint8_t keycode)
{
    if(g_key_tap_queue_count >= BLE_HID_KEY_TAP_QUEUE_SIZE)
    {
        return FALSE;
    }

    g_key_tap_queue[g_key_tap_queue_tail].modifier = modifier;
    g_key_tap_queue[g_key_tap_queue_tail].keycode = keycode;
    g_key_tap_queue_tail++;
    if(g_key_tap_queue_tail >= BLE_HID_KEY_TAP_QUEUE_SIZE)
    {
        g_key_tap_queue_tail = 0;
    }

    g_key_tap_queue_count++;
    return TRUE;
}

static uint8_t bleHidKeyTapQueuePop(bleHidKeyTap_t *tap)
{
    if(tap == NULL || g_key_tap_queue_count == 0)
    {
        return FALSE;
    }

    *tap = g_key_tap_queue[g_key_tap_queue_head];
    g_key_tap_queue_head++;
    if(g_key_tap_queue_head >= BLE_HID_KEY_TAP_QUEUE_SIZE)
    {
        g_key_tap_queue_head = 0;
    }

    g_key_tap_queue_count--;
    return TRUE;
}

static void bleHidClearKeyTapQueue(void)
{
    g_key_tap_queue_head = 0;
    g_key_tap_queue_tail = 0;
    g_key_tap_queue_count = 0;
}

static void bleHidResetKeyTapState(void)
{
    g_key_tap_busy = FALSE;
    g_key_tap_retry_count = 0;
    g_key_tap_modifier = 0;
    g_key_tap_keycode = HID_KEYBOARD_RESERVED;
    bleHidClearKeyTapQueue();

    if(g_ble_task_id != INVALID_TASK_ID)
    {
        tmos_stop_task(g_ble_task_id, BLE_HID_KEY_TAP_DOWN_EVT);
        tmos_stop_task(g_ble_task_id, BLE_HID_KEY_TAP_UP_EVT);
    }
}

static void hidStateCB(gapRole_States_t newState, gapRoleEvent_t *pEvent)
{
    switch(newState & GAPROLE_STATE_ADV_MASK)
    {
        case GAPROLE_STARTED:
        {
            BLE_HID_LOG("%s: init\n", BLE_HID_DEVICE_NAME);
        }
        break;

        case GAPROLE_ADVERTISING:
            if(pEvent->gap.opcode == GAP_MAKE_DISCOVERABLE_DONE_EVENT)
            {
                BLE_HID_LOG("%s: advertising\n", BLE_HID_DEVICE_NAME);
            }
            break;

        case GAPROLE_CONNECTED:
            if(pEvent->gap.opcode == GAP_LINK_ESTABLISHED_EVENT)
            {
                g_conn_handle = ((gapEstLinkReqEvent_t *)pEvent)->connectionHandle;
                bleHidResetKeyTapState();
                BLE_HID_LOG("%s: connected\n", BLE_HID_DEVICE_NAME);
                tmos_start_task(g_ble_task_id, BLE_HID_PARAM_UPDATE_EVT, 1600);
                tmos_start_task(g_ble_task_id, BLE_HID_PHY_UPDATE_EVT, 2400);
            }
            break;

        case GAPROLE_CONNECTED_ADV:
            if(pEvent->gap.opcode == GAP_MAKE_DISCOVERABLE_DONE_EVENT)
            {
                BLE_HID_LOG("%s: connected advertising\n", BLE_HID_DEVICE_NAME);
            }
            break;

        case GAPROLE_WAITING:
            if(pEvent->gap.opcode == GAP_LINK_TERMINATED_EVENT)
            {
                g_conn_handle = GAP_CONNHANDLE_INIT;
                bleHidResetKeyTapState();
                BLE_HID_LOG("%s: disconnected\n", BLE_HID_DEVICE_NAME);
                BLE_HID_StartAdvert();
            }
            else if(pEvent->gap.opcode == GAP_END_DISCOVERABLE_DONE_EVENT)
            {
                BLE_HID_LOG("%s: waiting\n", BLE_HID_DEVICE_NAME);
            }
            break;

        case GAPROLE_ERROR:
            BLE_HID_LOG("%s: error %x\n", BLE_HID_DEVICE_NAME, pEvent->gap.opcode);
            break;

        default:
            break;
    }
}

static uint8_t hidRptCB(uint8_t id, uint8_t type, uint16_t uuid,
                        uint8_t oper, uint16_t *pLen, uint8_t *pData)
{
    uint8_t status = SUCCESS;

    if(oper == HID_DEV_OPER_WRITE)
    {
        if(uuid == REPORT_UUID || uuid == BOOT_KEY_OUTPUT_UUID)
        {
            status = Hid_SetParameter(id, type, uuid, (uint8_t)(*pLen), pData);
        }
    }
    else if(oper == HID_DEV_OPER_READ)
    {
        status = Hid_GetParameter(id, type, uuid, pLen, pData);
    }
    else if(oper == HID_DEV_OPER_ENABLE)
    {
        BLE_HID_LOG("%s: notify enabled id=%d type=%d\n", BLE_HID_DEVICE_NAME, id, type);
    }

    return status;
}

static void hidEvtCB(uint8_t evt)
{
    (void)evt;
}
