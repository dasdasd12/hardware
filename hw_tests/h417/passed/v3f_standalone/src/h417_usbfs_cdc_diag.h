#ifndef H417_USBFS_CDC_DIAG_H
#define H417_USBFS_CDC_DIAG_H

#include <stdint.h>

typedef enum
{
    H417_USBFS_CDC_DIAG_COMMAND_NONE = 0,
    H417_USBFS_CDC_DIAG_COMMAND_START,
    H417_USBFS_CDC_DIAG_COMMAND_NEXT
} h417_usbfs_cdc_diag_command_t;

void h417_usbfs_cdc_diag_init(void);
void h417_usbfs_cdc_diag_poll_control(void);
uint8_t h417_usbfs_cdc_diag_take_advance(void);
uint8_t h417_usbfs_cdc_diag_ready(void);
h417_usbfs_cdc_diag_command_t h417_usbfs_cdc_diag_take_command(void);
uint8_t h417_usbfs_cdc_diag_try_write(const uint8_t *data, uint16_t length);
uint8_t h417_usbfs_cdc_diag_tx_idle(void);
void h417_usbfs_cdc_diag_quiesce_usbhs(void);

#endif
