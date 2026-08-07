#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "board_init.h"
#include "ch32h417_dbgmcu.h"
#include "ch32h417_iwdg.h"
#include "ch32h417_usbss_device.h"
#include "h417_usbfs_cdc_diag.h"
#include "h417_usbss_diag.h"
#include "system_ch32h417.h"

#define H417_USBSS_DIAG_TX_CHUNK 64U
#define H417_USBSS_DIAG_LINE_SIZE 640U
#define H417_USBSS_PROBE_ADDR 0x20178180U
#define H417_USBSS_PROBE_MAGIC 0x53533139U
#define H417_USBSS_PROBE_MAGIC_INV 0xACACCEC6U
#define H417_USBSS_PROBE_IWDG_RELOAD 4000U
#define H417_USBSS_PROBE_PLL_TIMEOUT_POLLS 2000000U

typedef enum
{
    H417_USBSS_PROBE_EMPTY = 0,
    H417_USBSS_PROBE_WATCHDOG_ARMING,
    H417_USBSS_PROBE_PLL_READ_ARMED,
    H417_USBSS_PROBE_PLL_VALUE_LOADED,
    H417_USBSS_PROBE_PLL_STORE_ARMED,
    H417_USBSS_PROBE_PLL_STORE_RETURNED,
    H417_USBSS_PROBE_PLL_READBACK_RETURNED,
    H417_USBSS_PROBE_PLL_READY,
    H417_USBSS_PROBE_PLL_TIMEOUT,
    H417_USBSS_PROBE_USB2_INIT_ARMED,
    H417_USBSS_PROBE_USB2_INIT_RETURNED,
    H417_USBSS_PROBE_USBSS_DEVICE_INIT_ARMED,
    H417_USBSS_PROBE_USBSS_DEVICE_INIT_RETURNED,
    H417_USBSS_PROBE_USBSS_ENUMERATED,
    H417_USBSS_PROBE_TRAP_CAPTURED,
    H417_USBSS_PROBE_RECOVERY_BOOT,
    H417_USBSS_PROBE_RECOVERY_USB2_RETURNED
} h417_usbss_probe_stage_t;

typedef struct
{
    uint32_t magic;
    uint32_t magic_inv;
    uint32_t stage;
    uint32_t reset_flags;
    uint32_t rcc_before;
    uint32_t rcc_loaded;
    uint32_t rcc_composed;
    uint32_t rcc_readback;
    uint32_t rcc_after_wait;
    uint32_t cycles;
    uint32_t pllcfgr2;
    uint32_t chip_id;
    uint32_t chip_revision;
    uint32_t trap_stage;
    uint32_t trap_mcause;
    uint32_t trap_mepc;
    uint32_t trap_mtval;
    uint32_t usbss_link_status;
    uint32_t usbss_link_int_flag;
    uint32_t usbss_usb_status;
    uint32_t usbss_usb_control;
    uint32_t usbss_enum_status;
    uint32_t usbss_device_enum_status;
    uint32_t usbss_link_int_ctrl;
    uint32_t usbss_link_cfg;
    uint32_t usbss_link_ctrl;
    uint32_t usbss_lmp_rx_data0;
    uint32_t usbss_lmp_tx_data0;
    uint32_t usbss_lmp_port_cap;
} h417_usbss_probe_record_t;

#define H417_USBSS_PROBE_RECORD \
    ((volatile h417_usbss_probe_record_t *)(uintptr_t)H417_USBSS_PROBE_ADDR)

static char s_tx_line[H417_USBSS_DIAG_LINE_SIZE];
static uint16_t s_tx_length;
static uint16_t s_tx_offset;
static uint8_t s_fs_was_ready;
static uint8_t s_probe_recovered;
static uint32_t s_probe_report_stage;
static uint32_t s_probe_reset_flags;
static uint32_t s_probe_boot_reset_flags;
static uint32_t s_probe_rcc_before;
static uint32_t s_probe_rcc_loaded;
static uint32_t s_probe_rcc_composed;
static uint32_t s_probe_rcc_readback;
static uint32_t s_probe_rcc_after_wait;
static uint32_t s_probe_cycles;
static uint32_t s_probe_pllcfgr2;
static uint32_t s_probe_chip_id;
static uint32_t s_probe_chip_revision;
static uint32_t s_probe_trap_stage;
static uint32_t s_probe_trap_mcause;
static uint32_t s_probe_trap_mepc;
static uint32_t s_probe_trap_mtval;
static uint32_t s_probe_usbss_link_status;
static uint32_t s_probe_usbss_link_int_flag;
static uint32_t s_probe_usbss_usb_status;
static uint32_t s_probe_usbss_usb_control;
static uint32_t s_probe_usbss_enum_status;
static uint32_t s_probe_usbss_device_enum_status;
static uint32_t s_probe_usbss_link_int_ctrl;
static uint32_t s_probe_usbss_link_cfg;
static uint32_t s_probe_usbss_link_ctrl;
static uint32_t s_probe_usbss_lmp_rx_data0;
static uint32_t s_probe_usbss_lmp_tx_data0;
static uint32_t s_probe_usbss_lmp_port_cap;
static uint32_t s_chip_id;

typedef enum
{
    H417_USBSS_STAGE_WAIT_START = 0,
    H417_USBSS_STAGE_PROBE_RESULT_DRAIN,
    H417_USBSS_STAGE_WAIT_BUFFERS_NEXT,
    H417_USBSS_STAGE_BUFFERS_DRAIN,
    H417_USBSS_STAGE_WAIT_USBHS_NEXT,
    H417_USBSS_STAGE_USBHS_DRAIN,
    H417_USBSS_STAGE_WAIT_PLATFORM_NEXT,
    H417_USBSS_STAGE_PLATFORM_DRAIN,
    H417_USBSS_STAGE_WAIT_PLL_ARM_NEXT,
    H417_USBSS_STAGE_PLL_ARM_DRAIN,
    H417_USBSS_STAGE_WAIT_PLL_WRITE_NEXT,
    H417_USBSS_STAGE_PLL_WRITTEN_DRAIN,
    H417_USBSS_STAGE_PLL_WAIT,
    H417_USBSS_STAGE_PLL_READY_DRAIN,
    H417_USBSS_STAGE_WAIT_INIT_ANNOUNCE_NEXT,
    H417_USBSS_STAGE_INIT_ANNOUNCE_DRAIN,
    H417_USBSS_STAGE_WAIT_DEVICE_INIT_NEXT,
    H417_USBSS_STAGE_DEVICE_INIT_RETURNED_DRAIN,
    H417_USBSS_STAGE_WAIT_ENUM,
    H417_USBSS_STAGE_ENUMERATED_DRAIN,
    H417_USBSS_STAGE_RUNNING,
    H417_USBSS_STAGE_FAILED
} h417_usbss_stage_t;

static const char *h417_usbss_event_name(uint32_t event_id)
{
    switch(event_id)
    {
        case H417_USBSS_DIAG_LINK_BEFORE:
            return "LINK_BEFORE";
        case H417_USBSS_DIAG_LINK_AFTER:
            return "LINK_AFTER";
        case H417_USBSS_DIAG_TIMER_FALLBACK_SUPPRESSED:
            return "TIMER_BLOCK";
        case H417_USBSS_DIAG_USBHS_ENABLE_SUPPRESSED:
            return "USBHS_ON_BLOCK";
        case H417_USBSS_DIAG_USBHS_DISABLE_SUPPRESSED:
            return "USBHS_OFF_BLOCK";
        default:
            return "SNAPSHOT";
    }
}

static const char *h417_usbss_link_state_name(uint32_t link_status)
{
    switch(link_status & LINK_STATE_MASK)
    {
        case LINK_STATE_U0:
            return "U0";
        case LINK_STATE_U1:
            return "U1";
        case LINK_STATE_U2:
            return "U2";
        case LINK_STATE_U3:
            return "U3";
        case LINK_STATE_DISABLE:
            return "DISABLED";
        case LINK_STATE_RXDET:
            return "RXDET";
        case LINK_STATE_INACTIVE:
            return "INACTIVE";
        case LINK_STATE_POLLING:
            return "POLLING";
        case LINK_STATE_RECOVERY:
            return "RECOVERY";
        case LINK_STATE_HOTRST:
            return "HOTRST";
        default:
            return "UNKNOWN";
    }
}

static uint32_t h417_usbss_diag_ucycle(void)
{
    uint32_t value;

    __asm__ volatile("csrr %0, cycle" : "=r"(value));
    return value;
}

static void h417_usbss_probe_fence(void)
{
    __asm__ volatile("fence rw, rw" ::: "memory");
}

void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void HardFault_Handler(void)
{
    volatile h417_usbss_probe_record_t *record = H417_USBSS_PROBE_RECORD;
    uint32_t trap_stage = record->stage;
    uint32_t mcause;
    uint32_t mepc;
    uint32_t mtval;

    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));
    __asm__ volatile("csrr %0, mepc" : "=r"(mepc));
    __asm__ volatile("csrr %0, mtval" : "=r"(mtval));
    record->trap_stage = trap_stage;
    record->trap_mcause = mcause;
    record->trap_mepc = mepc;
    record->trap_mtval = mtval;
    record->stage = H417_USBSS_PROBE_TRAP_CAPTURED;
    h417_usbss_probe_fence();

    while(1)
    {
        __asm__ volatile("nop");
    }
}

static uint8_t h417_usbss_probe_record_valid(void)
{
    return (uint8_t)(((H417_USBSS_PROBE_RECORD->magic ==
                       H417_USBSS_PROBE_MAGIC) &&
                      (H417_USBSS_PROBE_RECORD->magic_inv ==
                       H417_USBSS_PROBE_MAGIC_INV)) ? 1U : 0U);
}

static uint8_t h417_usbss_probe_stage_needs_recovery(uint32_t stage)
{
    return (uint8_t)(((stage >= H417_USBSS_PROBE_WATCHDOG_ARMING) &&
                      (stage <= H417_USBSS_PROBE_USB2_INIT_ARMED)) ||
                     ((stage >= H417_USBSS_PROBE_USBSS_DEVICE_INIT_ARMED) &&
                      (stage <= H417_USBSS_PROBE_USBSS_DEVICE_INIT_RETURNED)) ||
                     (stage == H417_USBSS_PROBE_TRAP_CAPTURED) ? 1U : 0U);
}

static void h417_usbss_probe_capture(uint32_t report_stage,
                                     uint8_t recovered)
{
    s_probe_recovered = recovered;
    s_probe_report_stage = report_stage;
    s_probe_reset_flags = H417_USBSS_PROBE_RECORD->reset_flags;
    s_probe_rcc_before = H417_USBSS_PROBE_RECORD->rcc_before;
    s_probe_rcc_loaded = H417_USBSS_PROBE_RECORD->rcc_loaded;
    s_probe_rcc_composed = H417_USBSS_PROBE_RECORD->rcc_composed;
    s_probe_rcc_readback = H417_USBSS_PROBE_RECORD->rcc_readback;
    s_probe_rcc_after_wait = H417_USBSS_PROBE_RECORD->rcc_after_wait;
    s_probe_cycles = H417_USBSS_PROBE_RECORD->cycles;
    s_probe_pllcfgr2 = H417_USBSS_PROBE_RECORD->pllcfgr2;
    s_probe_chip_id = H417_USBSS_PROBE_RECORD->chip_id;
    s_probe_chip_revision = H417_USBSS_PROBE_RECORD->chip_revision;
    s_probe_trap_stage = H417_USBSS_PROBE_RECORD->trap_stage;
    s_probe_trap_mcause = H417_USBSS_PROBE_RECORD->trap_mcause;
    s_probe_trap_mepc = H417_USBSS_PROBE_RECORD->trap_mepc;
    s_probe_trap_mtval = H417_USBSS_PROBE_RECORD->trap_mtval;
    s_probe_usbss_link_status = H417_USBSS_PROBE_RECORD->usbss_link_status;
    s_probe_usbss_link_int_flag =
        H417_USBSS_PROBE_RECORD->usbss_link_int_flag;
    s_probe_usbss_usb_status = H417_USBSS_PROBE_RECORD->usbss_usb_status;
    s_probe_usbss_usb_control = H417_USBSS_PROBE_RECORD->usbss_usb_control;
    s_probe_usbss_enum_status = H417_USBSS_PROBE_RECORD->usbss_enum_status;
    s_probe_usbss_device_enum_status =
        H417_USBSS_PROBE_RECORD->usbss_device_enum_status;
    s_probe_usbss_link_int_ctrl =
        H417_USBSS_PROBE_RECORD->usbss_link_int_ctrl;
    s_probe_usbss_link_cfg = H417_USBSS_PROBE_RECORD->usbss_link_cfg;
    s_probe_usbss_link_ctrl = H417_USBSS_PROBE_RECORD->usbss_link_ctrl;
    s_probe_usbss_lmp_rx_data0 = H417_USBSS_PROBE_RECORD->usbss_lmp_rx_data0;
    s_probe_usbss_lmp_tx_data0 = H417_USBSS_PROBE_RECORD->usbss_lmp_tx_data0;
    s_probe_usbss_lmp_port_cap = H417_USBSS_PROBE_RECORD->usbss_lmp_port_cap;
}

static void h417_usbss_probe_capture_usbss_state(void)
{
    volatile h417_usbss_probe_record_t *record = H417_USBSS_PROBE_RECORD;

    record->usbss_link_status = USBSSH->LINK_STATUS;
    record->usbss_link_int_flag = USBSSH->LINK_INT_FLAG;
    record->usbss_usb_status = USBSSD->USB_STATUS;
    record->usbss_usb_control = USBSSD->USB_CONTROL;
    record->usbss_enum_status = USB_Enum_Status;
    record->usbss_device_enum_status = USBSS_DevEnumStatus;
    record->usbss_link_int_ctrl = USBSSD->LINK_INT_CTRL;
    record->usbss_link_cfg = USBSSH->LINK_CFG;
    record->usbss_link_ctrl = USBSSH->LINK_CTRL;
    record->usbss_lmp_rx_data0 = USBSSH->LINK_LMP_RX_DATA0;
    record->usbss_lmp_tx_data0 = USBSSH->LINK_LMP_TX_DATA0;
    record->usbss_lmp_port_cap = USBSSH->LINK_LMP_PORT_CAP;
    h417_usbss_probe_fence();
}

static void h417_usbss_watchdog_pll_probe(void)
{
    volatile h417_usbss_probe_record_t *record = H417_USBSS_PROBE_RECORD;
    uint32_t prior_stage;
    uint32_t value;
    uint32_t wait_polls;

    s_probe_boot_reset_flags = RCC->RSTSCKR;

    if(h417_usbss_probe_record_valid() != 0U)
    {
        prior_stage = record->stage;
        if(h417_usbss_probe_stage_needs_recovery(prior_stage) != 0U)
        {
            h417_usbss_probe_capture(prior_stage, 1U);
            record->stage = H417_USBSS_PROBE_RECOVERY_BOOT;
            h417_usbss_probe_fence();
            return;
        }
    }

    record->magic = 0U;
    record->magic_inv = 0U;
    record->stage = H417_USBSS_PROBE_WATCHDOG_ARMING;
    record->reset_flags = RCC->RSTSCKR;
    record->rcc_before = RCC->CTLR;
    record->rcc_loaded = 0U;
    record->rcc_composed = 0U;
    record->rcc_readback = 0U;
    record->rcc_after_wait = 0U;
    record->cycles = 0U;
    record->pllcfgr2 = RCC->PLLCFGR2;
    record->chip_id = s_chip_id;
    record->chip_revision = Chip;
    record->trap_stage = H417_USBSS_PROBE_EMPTY;
    record->trap_mcause = 0U;
    record->trap_mepc = 0U;
    record->trap_mtval = 0U;
    record->usbss_link_status = 0U;
    record->usbss_link_int_flag = 0U;
    record->usbss_usb_status = 0U;
    record->usbss_usb_control = 0U;
    record->usbss_enum_status = 0U;
    record->usbss_device_enum_status = 0U;
    record->usbss_link_int_ctrl = 0U;
    record->usbss_link_cfg = 0U;
    record->usbss_link_ctrl = 0U;
    record->usbss_lmp_rx_data0 = 0U;
    record->usbss_lmp_tx_data0 = 0U;
    record->usbss_lmp_port_cap = 0U;
    record->magic_inv = H417_USBSS_PROBE_MAGIC_INV;
    h417_usbss_probe_fence();
    record->magic = H417_USBSS_PROBE_MAGIC;
    h417_usbss_probe_fence();

    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_32);
    IWDG_SetReload(H417_USBSS_PROBE_IWDG_RELOAD);
    IWDG_ReloadCounter();
    IWDG_Enable();

    record->stage = H417_USBSS_PROBE_PLL_READ_ARMED;
    h417_usbss_probe_fence();

    value = RCC->CTLR;
    record->rcc_loaded = value;
    record->stage = H417_USBSS_PROBE_PLL_VALUE_LOADED;
    h417_usbss_probe_fence();

    value |= (uint32_t)RCC_USBSS_PLLON;
    record->rcc_composed = value;
    record->stage = H417_USBSS_PROBE_PLL_STORE_ARMED;
    h417_usbss_probe_fence();

    RCC->CTLR = value;
    record->stage = H417_USBSS_PROBE_PLL_STORE_RETURNED;
    h417_usbss_probe_fence();

    record->rcc_readback = RCC->CTLR;
    record->stage = H417_USBSS_PROBE_PLL_READBACK_RETURNED;
    h417_usbss_probe_fence();

    wait_polls = 0U;
    while(((RCC->CTLR & RCC_USBSS_PLLRDY) == 0U) &&
          (wait_polls < H417_USBSS_PROBE_PLL_TIMEOUT_POLLS))
    {
        wait_polls++;
    }

    record->cycles = wait_polls;
    record->rcc_after_wait = RCC->CTLR;
    record->stage =
        ((record->rcc_after_wait & RCC_USBSS_PLLRDY) != 0U) ?
            H417_USBSS_PROBE_PLL_READY : H417_USBSS_PROBE_PLL_TIMEOUT;
    h417_usbss_probe_fence();
    h417_usbss_probe_capture(record->stage, 0U);
    IWDG_ReloadCounter();
}

static void h417_usbss_set_tx_length(int formatted_length)
{
    if(formatted_length <= 0)
    {
        s_tx_length = 0U;
    }
    else if((uint32_t)formatted_length >= sizeof(s_tx_line))
    {
        s_tx_length = (uint16_t)(sizeof(s_tx_line) - 1U);
    }
    else
    {
        s_tx_length = (uint16_t)formatted_length;
    }
    s_tx_offset = 0U;
}

static uint8_t h417_usbss_tx_idle(void)
{
    return s_tx_offset >= s_tx_length ? 1U : 0U;
}

static uint8_t h417_usbss_tx_drained(void)
{
    return (uint8_t)((h417_usbss_tx_idle() != 0U) &&
                     (h417_usbfs_cdc_diag_tx_idle() != 0U));
}

static void h417_usbss_tx_service(void)
{
    uint16_t chunk;

    if(h417_usbfs_cdc_diag_ready() == 0U)
    {
        if(s_fs_was_ready != 0U)
        {
            s_tx_offset = 0U;
        }
        s_fs_was_ready = 0U;
        return;
    }

    s_fs_was_ready = 1U;
    if(h417_usbss_tx_idle() != 0U)
    {
        return;
    }

    chunk = (uint16_t)(s_tx_length - s_tx_offset);
    if(chunk > H417_USBSS_DIAG_TX_CHUNK)
    {
        chunk = H417_USBSS_DIAG_TX_CHUNK;
    }

    if(h417_usbfs_cdc_diag_try_write(
           (const uint8_t *)&s_tx_line[s_tx_offset], chunk) != 0U)
    {
        s_tx_offset = (uint16_t)(s_tx_offset + chunk);
    }
}

static void h417_usbss_format_event(const h417_usbss_diag_event_t *event)
{
    h417_usbss_set_tx_length(
        snprintf(s_tx_line,
                 sizeof(s_tx_line),
                 "SS ev=%s seq=%lu cyc=%08lx state=%s enum=%lu dev=%lu "
                 "ls=%08lx li=%08lx cfg=%08lx ctl=%08lx us=%08lx uc=%08lx "
                 "rcc=%08lx pll2=%08lx drop=%lu\r\n",
                 h417_usbss_event_name(event->event_id),
                 (unsigned long)event->sequence,
                 (unsigned long)event->cycle,
                 h417_usbss_link_state_name(event->link_status),
                 (unsigned long)event->enum_status,
                 (unsigned long)event->device_enum_status,
                 (unsigned long)event->link_status,
                 (unsigned long)event->link_int_flag,
                 (unsigned long)event->link_cfg,
                 (unsigned long)event->link_ctrl,
                 (unsigned long)event->usb_status,
                 (unsigned long)event->usb_control,
                 (unsigned long)event->rcc_ctlr,
                 (unsigned long)event->rcc_pllcfgr2,
                 (unsigned long)h417_usbss_diag_dropped()));
}

static void h417_usbss_format_checkpoint(const char *step)
{
    h417_usbss_set_tx_length(
        snprintf(s_tx_line,
                 sizeof(s_tx_line),
                 "SS19 step=%s rcc=%08lx p2=%08lx\r\n",
                 step,
                 (unsigned long)RCC->CTLR,
                 (unsigned long)RCC->PLLCFGR2));
}

static void h417_usbss_format_usbss_checkpoint(const char *step)
{
    h417_usbss_set_tx_length(
        snprintf(s_tx_line,
                 sizeof(s_tx_line),
                 "SS19 step=%s rcc=%08lx p2=%08lx chip=%08lx/%lu "
                 "ls=%08lx li=%08lx lic=%08lx lcfg=%08lx lctl=%08lx "
                 "lrx=%08lx ltx=%08lx lcap=%08lx us=%08lx uc=%08lx "
                 "enum=%lu dev=%lu\r\n",
                 step,
                 (unsigned long)RCC->CTLR,
                 (unsigned long)RCC->PLLCFGR2,
                 (unsigned long)H417_USBSS_PROBE_RECORD->chip_id,
                 (unsigned long)H417_USBSS_PROBE_RECORD->chip_revision,
                 (unsigned long)H417_USBSS_PROBE_RECORD->usbss_link_status,
                 (unsigned long)H417_USBSS_PROBE_RECORD->usbss_link_int_flag,
                 (unsigned long)H417_USBSS_PROBE_RECORD->usbss_link_int_ctrl,
                 (unsigned long)H417_USBSS_PROBE_RECORD->usbss_link_cfg,
                 (unsigned long)H417_USBSS_PROBE_RECORD->usbss_link_ctrl,
                 (unsigned long)H417_USBSS_PROBE_RECORD->usbss_lmp_rx_data0,
                 (unsigned long)H417_USBSS_PROBE_RECORD->usbss_lmp_tx_data0,
                 (unsigned long)H417_USBSS_PROBE_RECORD->usbss_lmp_port_cap,
                 (unsigned long)H417_USBSS_PROBE_RECORD->usbss_usb_status,
                 (unsigned long)H417_USBSS_PROBE_RECORD->usbss_usb_control,
                 (unsigned long)H417_USBSS_PROBE_RECORD->usbss_enum_status,
                 (unsigned long)
                     H417_USBSS_PROBE_RECORD->usbss_device_enum_status));
}

static const char *h417_usbss_probe_stage_name(uint32_t stage)
{
    switch(stage)
    {
        case H417_USBSS_PROBE_WATCHDOG_ARMING:
            return "RECOVER_WDG_ARMING";
        case H417_USBSS_PROBE_PLL_READ_ARMED:
            return "RECOVER_READ_ARMED";
        case H417_USBSS_PROBE_PLL_VALUE_LOADED:
            return "RECOVER_VALUE_LOADED";
        case H417_USBSS_PROBE_PLL_STORE_ARMED:
            return "RECOVER_STORE_ARMED";
        case H417_USBSS_PROBE_PLL_STORE_RETURNED:
            return "RECOVER_STORE_RETURNED";
        case H417_USBSS_PROBE_PLL_READBACK_RETURNED:
            return "RECOVER_READBACK_RETURNED";
        case H417_USBSS_PROBE_PLL_READY:
            return (s_probe_recovered != 0U) ?
                "RECOVER_PLL_READY" : "PLL_READY";
        case H417_USBSS_PROBE_PLL_TIMEOUT:
            return (s_probe_recovered != 0U) ?
                "RECOVER_PLL_TIMEOUT" : "PLL_TIMEOUT";
        case H417_USBSS_PROBE_USB2_INIT_ARMED:
            return "RECOVER_USB2_INIT";
        case H417_USBSS_PROBE_USBSS_DEVICE_INIT_ARMED:
            return "RECOVER_DEVICE_INIT_ARMED";
        case H417_USBSS_PROBE_USBSS_DEVICE_INIT_RETURNED:
            return "RECOVER_DEVICE_INIT_RETURNED";
        case H417_USBSS_PROBE_USBSS_ENUMERATED:
            return "USBSS_ENUMERATED";
        case H417_USBSS_PROBE_TRAP_CAPTURED:
            return "RECOVER_TRAP";
        default:
            return "PROBE_INVALID";
    }
}

static void h417_usbss_format_probe_result(void)
{
    h417_usbss_set_tx_length(
        snprintf(s_tx_line,
                 sizeof(s_tx_line),
                 "SS19 boot=%s rec=%u stage=%lu rst0=%08lx rst=%08lx "
                 "before=%08lx loaded=%08lx value=%08lx read=%08lx "
                 "wait=%08lx now=%08lx polls=%08lx p2=%08lx chip=%08lx/%lu "
                 "trap_at=%lu mcause=%08lx mepc=%08lx mtval=%08lx "
                 "ls=%08lx li=%08lx lic=%08lx lcfg=%08lx lctl=%08lx "
                 "lrx=%08lx ltx=%08lx lcap=%08lx us=%08lx uc=%08lx "
                 "enum=%lu dev=%lu\r\n",
                 h417_usbss_probe_stage_name(s_probe_report_stage),
                 (unsigned int)s_probe_recovered,
                 (unsigned long)s_probe_report_stage,
                 (unsigned long)s_probe_reset_flags,
                 (unsigned long)s_probe_boot_reset_flags,
                 (unsigned long)s_probe_rcc_before,
                 (unsigned long)s_probe_rcc_loaded,
                 (unsigned long)s_probe_rcc_composed,
                 (unsigned long)s_probe_rcc_readback,
                 (unsigned long)s_probe_rcc_after_wait,
                 (unsigned long)RCC->CTLR,
                 (unsigned long)s_probe_cycles,
                 (unsigned long)s_probe_pllcfgr2,
                 (unsigned long)s_probe_chip_id,
                 (unsigned long)s_probe_chip_revision,
                 (unsigned long)s_probe_trap_stage,
                 (unsigned long)s_probe_trap_mcause,
                 (unsigned long)s_probe_trap_mepc,
                 (unsigned long)s_probe_trap_mtval,
                 (unsigned long)s_probe_usbss_link_status,
                 (unsigned long)s_probe_usbss_link_int_flag,
                 (unsigned long)s_probe_usbss_link_int_ctrl,
                 (unsigned long)s_probe_usbss_link_cfg,
                 (unsigned long)s_probe_usbss_link_ctrl,
                 (unsigned long)s_probe_usbss_lmp_rx_data0,
                 (unsigned long)s_probe_usbss_lmp_tx_data0,
                 (unsigned long)s_probe_usbss_lmp_port_cap,
                 (unsigned long)s_probe_usbss_usb_status,
                 (unsigned long)s_probe_usbss_usb_control,
                 (unsigned long)s_probe_usbss_enum_status,
                 (unsigned long)s_probe_usbss_device_enum_status));
}

static uint8_t h417_usbss_take_next(void)
{
    return h417_usbfs_cdc_diag_take_advance();
}

static void h417_usbss_prepare_buffers(void)
{
    memset(USBSS_EP0_Buf, 0, sizeof(USBSS_EP0_Buf));
    memset(USBSS_EP1_Rx_Buf, 0, sizeof(USBSS_EP1_Rx_Buf));
    memset(USBSS_EP2_Rx_Buf, 0, sizeof(USBSS_EP2_Rx_Buf));
    memset(USBSS_EP3_Rx_Buf, 0, sizeof(USBSS_EP3_Rx_Buf));
}

static void h417_usbss_prepare_platform(void)
{
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOB, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
    USB_Timer_Init();
}

static void h417_usbss_set_priorities(void)
{
    /* Keep the official USBSS timing; FS CDC remains a lower-priority probe. */
    NVIC_SetPriority(USBSS_LINK_IRQn, 0x00U);
    NVIC_SetPriority(USBSS_IRQn, 0x00U);
    NVIC_SetPriority(TIM12_IRQn, 0x00U);
    NVIC_SetPriority(USBFS_IRQn, 0x80U);
}

int main(void)
{
    h417_usbss_diag_event_t event;
    uint32_t last_heartbeat;
    uint32_t pll_wait_start = 0U;
    h417_usbss_stage_t stage = H417_USBSS_STAGE_WAIT_START;

    /* Match the official USBSS V3F prelude for the first PLL attempt. */
    SystemInit();
    SystemAndCoreClockUpdate();
    s_chip_id = DBGMCU_GetCHIPID();
    Chip = (s_chip_id >> 4U) & 0x0FU;
    h417_usbss_watchdog_pll_probe();
    IWDG_ReloadCounter();
    /* Restore the proven product setup before bringing up the debug USB2 path. */
    v3f_board_init();
    IWDG_ReloadCounter();
    H417_USBSS_PROBE_RECORD->stage = H417_USBSS_PROBE_USB2_INIT_ARMED;
    h417_usbss_probe_fence();
    h417_usbfs_cdc_diag_init();
    H417_USBSS_PROBE_RECORD->stage =
        (s_probe_recovered != 0U) ?
            H417_USBSS_PROBE_RECOVERY_USB2_RETURNED :
            H417_USBSS_PROBE_USB2_INIT_RETURNED;
    h417_usbss_probe_fence();
    IWDG_ReloadCounter();
    last_heartbeat = 0U;

    while(1)
    {
        if((stage != H417_USBSS_STAGE_DEVICE_INIT_RETURNED_DRAIN) &&
           (stage != H417_USBSS_STAGE_WAIT_ENUM))
        {
            IWDG_ReloadCounter();
        }
        h417_usbfs_cdc_diag_poll_control();
        h417_usbss_tx_service();

        if(stage == H417_USBSS_STAGE_WAIT_START)
        {
            if(h417_usbfs_cdc_diag_take_advance() != 0U)
            {
                h417_usbss_format_probe_result();
                stage = H417_USBSS_STAGE_PROBE_RESULT_DRAIN;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_PROBE_RESULT_DRAIN)
        {
            if(h417_usbss_tx_drained() != 0U)
            {
                stage = ((s_probe_recovered == 0U) &&
                         (s_probe_report_stage == H417_USBSS_PROBE_PLL_READY)) ?
                            H417_USBSS_STAGE_WAIT_BUFFERS_NEXT :
                            H417_USBSS_STAGE_FAILED;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_WAIT_BUFFERS_NEXT)
        {
            if(h417_usbss_take_next() != 0U)
            {
                h417_usbss_prepare_buffers();
                h417_usbss_format_checkpoint("BUFFERS_CLEARED");
                stage = H417_USBSS_STAGE_BUFFERS_DRAIN;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_BUFFERS_DRAIN)
        {
            if(h417_usbss_tx_drained() != 0U)
            {
                stage = H417_USBSS_STAGE_WAIT_USBHS_NEXT;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_WAIT_USBHS_NEXT)
        {
            if(h417_usbss_take_next() != 0U)
            {
                h417_usbfs_cdc_diag_quiesce_usbhs();
                h417_usbss_format_checkpoint("USBHS_QUIESCED");
                stage = H417_USBSS_STAGE_USBHS_DRAIN;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_USBHS_DRAIN)
        {
            if(h417_usbss_tx_drained() != 0U)
            {
                stage = H417_USBSS_STAGE_WAIT_PLATFORM_NEXT;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_WAIT_PLATFORM_NEXT)
        {
            if(h417_usbss_take_next() != 0U)
            {
                h417_usbss_prepare_platform();
                h417_usbss_format_checkpoint("PLATFORM_READY");
                stage = H417_USBSS_STAGE_PLATFORM_DRAIN;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_PLATFORM_DRAIN)
        {
            if(h417_usbss_tx_drained() != 0U)
            {
                stage = H417_USBSS_STAGE_WAIT_PLL_ARM_NEXT;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_WAIT_PLL_ARM_NEXT)
        {
            if(h417_usbss_take_next() != 0U)
            {
                h417_usbss_format_checkpoint("PLL_ARMED");
                stage = H417_USBSS_STAGE_PLL_ARM_DRAIN;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_PLL_ARM_DRAIN)
        {
            if(h417_usbss_tx_drained() != 0U)
            {
                stage = H417_USBSS_STAGE_WAIT_PLL_WRITE_NEXT;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_WAIT_PLL_WRITE_NEXT)
        {
            if(h417_usbss_take_next() != 0U)
            {
                RCC->CTLR |= (uint32_t)RCC_USBSS_PLLON;
                pll_wait_start = h417_usbss_diag_ucycle();
                h417_usbss_format_checkpoint("PLL_WRITE_OK");
                stage = H417_USBSS_STAGE_PLL_WRITTEN_DRAIN;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_PLL_WRITTEN_DRAIN)
        {
            if(h417_usbss_tx_drained() != 0U)
            {
                stage = H417_USBSS_STAGE_PLL_WAIT;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_PLL_WAIT)
        {
            if((RCC->CTLR & RCC_USBSS_PLLRDY) != 0U)
            {
                h417_usbss_format_checkpoint("PLL_READY");
                stage = H417_USBSS_STAGE_PLL_READY_DRAIN;
            }
            else if((uint32_t)(h417_usbss_diag_ucycle() - pll_wait_start) >=
                    (SystemCoreClock / 5U))
            {
                h417_usbss_format_checkpoint("PLL_TIMEOUT");
                stage = H417_USBSS_STAGE_FAILED;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_PLL_READY_DRAIN)
        {
            if(h417_usbss_tx_drained() != 0U)
            {
                stage = H417_USBSS_STAGE_WAIT_INIT_ANNOUNCE_NEXT;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_WAIT_INIT_ANNOUNCE_NEXT)
        {
            if(h417_usbss_take_next() != 0U)
            {
                h417_usbss_set_priorities();
                h417_usbss_format_checkpoint("CALL_DEVICE_INIT");
                stage = H417_USBSS_STAGE_INIT_ANNOUNCE_DRAIN;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_INIT_ANNOUNCE_DRAIN)
        {
            if(h417_usbss_tx_drained() != 0U)
            {
                stage = H417_USBSS_STAGE_WAIT_DEVICE_INIT_NEXT;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_WAIT_DEVICE_INIT_NEXT)
        {
            if(h417_usbss_take_next() != 0U)
            {
                H417_USBSS_PROBE_RECORD->stage =
                    H417_USBSS_PROBE_USBSS_DEVICE_INIT_ARMED;
                h417_usbss_probe_fence();
                USBSS_Device_Init(ENABLE);
                H417_USBSS_PROBE_RECORD->stage =
                    H417_USBSS_PROBE_USBSS_DEVICE_INIT_RETURNED;
                h417_usbss_probe_fence();
                h417_usbss_probe_capture_usbss_state();
                last_heartbeat = h417_usbss_diag_ucycle();
                h417_usbss_format_usbss_checkpoint("DEVICE_INIT_RETURNED");
                stage = H417_USBSS_STAGE_DEVICE_INIT_RETURNED_DRAIN;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_DEVICE_INIT_RETURNED_DRAIN)
        {
            h417_usbss_probe_capture_usbss_state();
            if(h417_usbss_tx_drained() != 0U)
            {
                stage = H417_USBSS_STAGE_WAIT_ENUM;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_WAIT_ENUM)
        {
            h417_usbss_probe_capture_usbss_state();
            if(((USBSSH->LINK_STATUS & LINK_STATE_MASK) == LINK_STATE_U0) &&
               (USBSS_DevEnumStatus != 0U))
            {
                H417_USBSS_PROBE_RECORD->stage =
                    H417_USBSS_PROBE_USBSS_ENUMERATED;
                h417_usbss_probe_fence();
                IWDG_ReloadCounter();
                h417_usbss_format_usbss_checkpoint("USBSS_ENUMERATED");
                stage = H417_USBSS_STAGE_ENUMERATED_DRAIN;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_ENUMERATED_DRAIN)
        {
            if(h417_usbss_tx_drained() != 0U)
            {
                stage = H417_USBSS_STAGE_RUNNING;
            }
            continue;
        }

        if(stage == H417_USBSS_STAGE_FAILED)
        {
            continue;
        }

        if((h417_usbfs_cdc_diag_ready() != 0U) &&
           (h417_usbss_tx_drained() != 0U))
        {
            if(h417_usbss_diag_pop(&event) != 0U)
            {
                h417_usbss_format_event(&event);
            }
            else if((uint32_t)(h417_usbss_diag_ucycle() - last_heartbeat) >=
                    SystemCoreClock)
            {
                h417_usbss_diag_snapshot(&event);
                h417_usbss_format_event(&event);
                last_heartbeat = h417_usbss_diag_ucycle();
            }
        }
    }
}
