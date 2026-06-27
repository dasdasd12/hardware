/*******************************************************************************
 * rf_receiver.h - CH585 2.4G RF receiver protocol for USBHS HID output.
 *
 * Legacy frame format:
 * [0x55][len][16B kbd][2B consumer][target_id][seq][2B reserved][xor]
 *
 * Runtime key-state short frame:
 * [0xB1][len=11][seq_lo][seq_hi][down_bits[8]][xor]
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
#define RF_STRESS_MAGIC     0xA8
#define RF_STRESS_FRAME_LEN 8
#define RF_STRESS_PAYLOAD_LEN (RF_STRESS_FRAME_LEN - 2)
#define RF_KEYSTATE_MAGIC   0xB1
#define RF_KEYSTATE_FRAME_LEN 13
#define RF_KEYSTATE_PAYLOAD_LEN (RF_KEYSTATE_FRAME_LEN - 2)
#define RF_KEYSTATE_SEQ_OFFSET 2
#define RF_KEYSTATE_DOWN_OFFSET 4
#define RF_KBD_OFFSET       2
#define RF_CONSUMER_OFFSET  18
#define RF_RESERVED_OFFSET  20
#define RF_TARGET_OFFSET    RF_RESERVED_OFFSET
#define RF_SEQ_OFFSET       (RF_RESERVED_OFFSET + 1)
#define RF_TARGET_BROADCAST 0

#ifndef RF_LOCAL_TARGET_ID
#define RF_LOCAL_TARGET_ID  2
#endif

#define RF_RX_BUF_LEN       264

#ifndef RF_8K_STATS_ENABLE
#define RF_8K_STATS_ENABLE  1
#endif

void    RF_Receiver_Init(void);
uint8_t RF_Receiver_GetKbdReport(uint8_t *out16);
uint8_t RF_Receiver_GetConsumer(uint16_t *out_usage);
void    RF_Receiver_ServiceUsbHsReport(void);

tmosEvents RFRx_ProcessEvent(tmosTaskID task_id, tmosEvents events);
void RF_RX_ProcessCallBack(rfRole_States_t sta, uint8_t id);

#ifdef __cplusplus
}
#endif

#endif /* __RF_RECEIVER_H__ */
