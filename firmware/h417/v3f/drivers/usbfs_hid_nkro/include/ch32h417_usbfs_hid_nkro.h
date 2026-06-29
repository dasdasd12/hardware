#ifndef CH32H417_USBFS_HID_NKRO_H
#define CH32H417_USBFS_HID_NKRO_H

#include <stdint.h>
#include "aik_spi_protocol.h"

typedef struct
{
    uint32_t irq_count;
    uint32_t transfer_count;
    uint32_t setup_count;
    uint32_t bus_reset_count;
    uint8_t last_intflag;
    uint8_t last_intst;
    uint8_t last_setup_type;
    uint8_t last_setup_request;
    uint16_t last_setup_value;
    uint16_t last_setup_index;
    uint16_t last_setup_length;
    uint32_t clock_ready;
    uint32_t clock_error;
    uint32_t rcc_ctlr;
    uint32_t rcc_cfgr2;
    uint32_t rcc_pllcfgr2;
    uint32_t usb_base_ctrl;
    uint32_t usb_udev_ctrl;
    uint32_t usb_int_en;
    uint32_t uep0_dma;
    uint32_t last_xfer_buf0;
    uint32_t last_xfer_buf1;
    uint32_t last_resp0;
    uint32_t last_tx_len;
    uint32_t last_rx_len;
} ch32h417_usbfs_hid_nkro_diag_t;

void ch32h417_usbfs_hid_nkro_init(void);
void ch32h417_usbfs_hid_nkro_send(const uint8_t nkro16[AIK_NKRO_REPORT_BYTES]);
uint8_t ch32h417_usbfs_hid_nkro_pending_empty(void);
uint8_t ch32h417_usbfs_hid_nkro_submit(const uint8_t nkro16[AIK_NKRO_REPORT_BYTES]);
uint32_t ch32h417_usbfs_hid_nkro_reports(void);
uint8_t ch32h417_usbfs_hid_nkro_debug_write(const char *line);
void ch32h417_usbfs_hid_nkro_diag_snapshot(ch32h417_usbfs_hid_nkro_diag_t *diag);

#endif
