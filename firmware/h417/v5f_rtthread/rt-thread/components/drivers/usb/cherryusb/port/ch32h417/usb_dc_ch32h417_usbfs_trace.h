/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USB_DC_CH32H417_USBFS_TRACE_H
#define USB_DC_CH32H417_USBFS_TRACE_H

#include <stdint.h>

/*
 * The last 512 bytes of shared SRAM are reserved by the H417 SDRAM
 * diagnostics. USBFS writes a compact last-stage record at 0x2017fe00 so an
 * IWDG reboot can report where the fast interrupt stopped. It is not cleared
 * by C startup and is copied by the test before USB is initialized.
 */
#define CH32H417_USBFS_RETAIN_TRACE_ADDR    0x2017FE00U
#define CH32H417_USBFS_RETAIN_TRACE_MAGIC   0x55534254U
#define CH32H417_USBFS_RETAIN_TRACE_VERSION 2U

enum ch32h417_usbfs_trace_stage {
    CH32H417_USBFS_TRACE_IDLE = 0,
    CH32H417_USBFS_TRACE_IRQ_ENTER = 1,
    CH32H417_USBFS_TRACE_RT_ENTERED = 2,
    CH32H417_USBFS_TRACE_HANDLER_ENTER = 3,
    CH32H417_USBFS_TRACE_TRANSFER = 4,
    CH32H417_USBFS_TRACE_OUT_ENTER = 5,
    CH32H417_USBFS_TRACE_OUT_COPY_BEGIN = 6,
    CH32H417_USBFS_TRACE_OUT_COPY_END = 7,
    CH32H417_USBFS_TRACE_OUT_CALLBACK_BEGIN = 8,
    CH32H417_USBFS_TRACE_OUT_CALLBACK_END = 9,
    CH32H417_USBFS_TRACE_FLAG_CLEAR_BEGIN = 10,
    CH32H417_USBFS_TRACE_FLAG_CLEAR_END = 11,
    CH32H417_USBFS_TRACE_HANDLER_END = 12,
    CH32H417_USBFS_TRACE_RT_LEAVE_BEGIN = 13,
    CH32H417_USBFS_TRACE_RT_LEAVE_IRQ_OFF = 14,
    CH32H417_USBFS_TRACE_RT_LEAVE_NEST_DEC = 15,
    CH32H417_USBFS_TRACE_RT_LEAVE_END = 16,
};

struct ch32h417_usbfs_retain_trace {
    uint32_t magic;
    uint32_t version;
    uint32_t sequence;
    uint32_t stage;
    uint32_t irq_state;
    uint32_t ep_state;
    uint32_t progress;
    uint32_t sp;
    uint32_t mscratch;
    uint32_t mstatus;
    uint32_t transfer_count;
    uint32_t nak_count;
    uint32_t interrupt_nest;
    uint32_t leave_hook;
    uint32_t gp;
};

#endif /* USB_DC_CH32H417_USBFS_TRACE_H */
