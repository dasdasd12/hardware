/********************************** (C) COPYRIGHT *******************************
 * File Name          : broadcaster.h
 * Description        : Minimal BLE broadcaster task for CH585M
 *******************************************************************************/

#ifndef CH585M_BROADCASTER_H
#define CH585M_BROADCASTER_H

#ifdef __cplusplus
extern "C" {
#endif

#define SBP_START_DEVICE_EVT    0x0001

void Broadcaster_Init(void);
uint16_t Broadcaster_ProcessEvent(uint8_t task_id, uint16_t events);

#ifdef __cplusplus
}
#endif

#endif
