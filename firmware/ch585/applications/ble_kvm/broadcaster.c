/********************************** (C) COPYRIGHT *******************************
 * File Name          : broadcaster.c
 * Description        : Minimal non-connectable BLE advertising task
 *******************************************************************************/

#include "CONFIG.h"
#include "broadcaster.h"

#define MIN_BLE_ADV_INTERVAL    160
#define MIN_BLE_DEVICE_NAME     "CH585_LX_ADV1"

static uint8_t g_broadcaster_task_id = INVALID_TASK_ID;

/*
 * Keep the payload minimal so BLE assistants can identify the board
 * without relying on a scan response.
 */
static uint8_t advertData[] = {
    0x02,
    GAP_ADTYPE_FLAGS,
    GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

    0x0E,
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'C', 'H', '5', '8', '5', '_', 'L', 'X', '_', 'A', 'D', 'V', '1'
};

static void Broadcaster_ProcessTMOSMsg(tmos_event_hdr_t *pMsg);
static void Broadcaster_StateNotificationCB(gapRole_States_t newState);

static gapRolesBroadcasterCBs_t broadcasterCBs = {
    Broadcaster_StateNotificationCB,
    NULL
};

void Broadcaster_Init(void)
{
    g_broadcaster_task_id = TMOS_ProcessEventRegister(Broadcaster_ProcessEvent);

    {
        uint8_t initial_advertising_enable = TRUE;
        uint8_t initial_adv_event_type = GAP_ADTYPE_ADV_NONCONN_IND;

        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &initial_advertising_enable);
        GAPRole_SetParameter(GAPROLE_ADV_EVENT_TYPE, sizeof(uint8_t), &initial_adv_event_type);
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
    }

    {
        uint16_t advInt = MIN_BLE_ADV_INTERVAL;
        GAP_SetParamValue(TGAP_DISC_ADV_INT_MIN, advInt);
        GAP_SetParamValue(TGAP_DISC_ADV_INT_MAX, advInt);
    }

    tmos_set_event(g_broadcaster_task_id, SBP_START_DEVICE_EVT);
}

uint16_t Broadcaster_ProcessEvent(uint8_t task_id, uint16_t events)
{
    (void)task_id;

    if(events & SYS_EVENT_MSG)
    {
        uint8_t *pMsg;

        if((pMsg = tmos_msg_receive(g_broadcaster_task_id)) != NULL)
        {
            Broadcaster_ProcessTMOSMsg((tmos_event_hdr_t *)pMsg);
            tmos_msg_deallocate(pMsg);
        }

        return (events ^ SYS_EVENT_MSG);
    }

    if(events & SBP_START_DEVICE_EVT)
    {
        GAPRole_BroadcasterStartDevice(&broadcasterCBs);
        return (events ^ SBP_START_DEVICE_EVT);
    }

    return 0;
}

static void Broadcaster_ProcessTMOSMsg(tmos_event_hdr_t *pMsg)
{
    (void)pMsg;
}

static void Broadcaster_StateNotificationCB(gapRole_States_t newState)
{
    switch(newState)
    {
        case GAPROLE_STARTED:
            PRINT("%s: init\n", MIN_BLE_DEVICE_NAME);
            break;

        case GAPROLE_ADVERTISING:
            PRINT("%s: advertising\n", MIN_BLE_DEVICE_NAME);
            break;

        case GAPROLE_WAITING:
            PRINT("%s: waiting\n", MIN_BLE_DEVICE_NAME);
            break;

        case GAPROLE_ERROR:
            PRINT("%s: error\n", MIN_BLE_DEVICE_NAME);
            break;

        default:
            break;
    }
}
