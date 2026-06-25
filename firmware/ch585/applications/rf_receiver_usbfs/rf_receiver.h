/*******************************************************************************
 * rf_receiver.h - CH585 2.4G RF receiver protocol for USB HID output.
 *
 * Frame format:
 * [magic][len][16B kbd][2B consumer][target_id][seq][2B reserved][xor]
 ******************************************************************************/

#ifndef __RF_RECEIVER_H__
#define __RF_RECEIVER_H__

#include "CH58x_common.h"
#include "CONFIG.h"
#include "HAL.h"
#include "wchrf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RF_SYNC_WORD        0xA55A1234UL
#define RF_CHANNEL          16
#define RF_FRAME_MAGIC      0x55
#define RF_FRAME_LEN        25
#define RF_KBD_OFFSET       2
#define RF_CONSUMER_OFFSET  18
#define RF_RESERVED_OFFSET  20
#define RF_TARGET_OFFSET    RF_RESERVED_OFFSET
#define RF_SEQ_OFFSET       (RF_RESERVED_OFFSET + 1)
#define RF_TARGET_BROADCAST 0

#ifndef RF_LOCAL_TARGET_ID
#define RF_LOCAL_TARGET_ID  1
#endif

#define RF_RX_BUF_LEN       264

void    RF_Receiver_Init(void);
uint8_t RF_Receiver_GetKbdReport(uint8_t *out16);
uint8_t RF_Receiver_GetConsumer(uint16_t *out_usage);

tmosEvents RFRx_ProcessEvent(tmosTaskID task_id, tmosEvents events);
void RF_RX_ProcessCallBack(rfRole_States_t sta, uint8_t id);

#ifdef __cplusplus
}
#endif

#endif /* __RF_RECEIVER_H__ */
