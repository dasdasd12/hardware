#ifndef H417_USBSS_DIAG_H
#define H417_USBSS_DIAG_H

#include <stdint.h>

typedef enum
{
    H417_USBSS_DIAG_LINK_BEFORE = 1,
    H417_USBSS_DIAG_LINK_AFTER = 2,
    H417_USBSS_DIAG_TIMER_FALLBACK_SUPPRESSED = 3,
    H417_USBSS_DIAG_USBHS_ENABLE_SUPPRESSED = 4,
    H417_USBSS_DIAG_USBHS_DISABLE_SUPPRESSED = 5
} h417_usbss_diag_event_id_t;

typedef struct
{
    uint32_t sequence;
    uint32_t cycle;
    uint32_t event_id;
    uint32_t link_status;
    uint32_t link_int_flag;
    uint32_t link_cfg;
    uint32_t link_ctrl;
    uint32_t usb_status;
    uint32_t usb_control;
    uint32_t rcc_ctlr;
    uint32_t rcc_pllcfgr2;
    uint32_t enum_status;
    uint32_t device_enum_status;
} h417_usbss_diag_event_t;

void h417_usbss_diag_capture(h417_usbss_diag_event_id_t event_id);
void h417_usbss_diag_snapshot(h417_usbss_diag_event_t *event);
uint8_t h417_usbss_diag_pop(h417_usbss_diag_event_t *event);
uint32_t h417_usbss_diag_dropped(void);

#endif
