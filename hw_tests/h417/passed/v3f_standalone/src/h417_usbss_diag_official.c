#include "h417_usbss_diag.h"

#include <string.h>

#include "ch32h417_usbss_device.h"
#include "ch32h417_usbhs_device.h"

#define H417_USBSS_DIAG_EVENT_COUNT 128U

static volatile h417_usbss_diag_event_t s_events[H417_USBSS_DIAG_EVENT_COUNT];
static volatile uint32_t s_event_head;
static volatile uint32_t s_event_tail;
static volatile uint32_t s_event_sequence;
static volatile uint32_t s_event_dropped;
static h417_usbss_diag_event_t s_last_event[6];
static uint32_t s_last_event_valid;

static uint32_t h417_usbss_diag_ucycle(void)
{
    uint32_t value;

    __asm__ volatile("csrr %0, cycle" : "=r"(value));
    return value;
}

static void h417_usbss_diag_fill(h417_usbss_diag_event_t *event,
                                 uint32_t event_id)
{
    event->sequence = 0U;
    event->cycle = h417_usbss_diag_ucycle();
    event->event_id = event_id;
    event->link_status = USBSSH->LINK_STATUS;
    event->link_int_flag = USBSSH->LINK_INT_FLAG;
    event->link_cfg = USBSSH->LINK_CFG;
    event->link_ctrl = USBSSH->LINK_CTRL;
    event->usb_status = USBSSD->USB_STATUS;
    event->usb_control = USBSSD->USB_CONTROL;
    event->rcc_ctlr = RCC->CTLR;
    event->rcc_pllcfgr2 = RCC->PLLCFGR2;
    event->enum_status = USB_Enum_Status;
    event->device_enum_status = USBSS_DevEnumStatus;
}

static uint8_t h417_usbss_diag_same_state(
    const h417_usbss_diag_event_t *left,
    const h417_usbss_diag_event_t *right)
{
    return (left->link_status == right->link_status) &&
           (left->link_int_flag == right->link_int_flag) &&
           (left->link_cfg == right->link_cfg) &&
           (left->link_ctrl == right->link_ctrl) &&
           (left->usb_status == right->usb_status) &&
           (left->usb_control == right->usb_control) &&
           (left->rcc_ctlr == right->rcc_ctlr) &&
           (left->rcc_pllcfgr2 == right->rcc_pllcfgr2) &&
           (left->enum_status == right->enum_status) &&
           (left->device_enum_status == right->device_enum_status);
}

void h417_usbss_diag_capture(h417_usbss_diag_event_id_t event_id)
{
    h417_usbss_diag_event_t event;
    uint32_t event_index = (uint32_t)event_id;
    uint32_t next;

    h417_usbss_diag_fill(&event, event_index);
    if((event_index < 6U) &&
       ((s_last_event_valid & (1UL << event_index)) != 0U) &&
       (h417_usbss_diag_same_state(&event, &s_last_event[event_index]) != 0U))
    {
        return;
    }

    next = (s_event_head + 1U) & (H417_USBSS_DIAG_EVENT_COUNT - 1U);

    if(next == s_event_tail)
    {
        s_event_dropped++;
        return;
    }

    if(event_index < 6U)
    {
        s_last_event[event_index] = event;
        s_last_event_valid |= 1UL << event_index;
    }

    event.sequence = ++s_event_sequence;
    memcpy((void *)&s_events[s_event_head], &event, sizeof(event));
    __asm__ volatile("fence rw, rw" ::: "memory");
    s_event_head = next;
}

void h417_usbss_diag_snapshot(h417_usbss_diag_event_t *event)
{
    if(event == 0)
    {
        return;
    }

    h417_usbss_diag_fill(event, 0U);
    event->sequence = s_event_sequence;
}

uint8_t h417_usbss_diag_pop(h417_usbss_diag_event_t *event)
{
    uint8_t available = 0U;

    if(event == 0)
    {
        return 0U;
    }

    if(s_event_tail != s_event_head)
    {
        memcpy(event,
               (const void *)&s_events[s_event_tail],
               sizeof(*event));
        __asm__ volatile("fence rw, rw" ::: "memory");
        s_event_tail =
            (s_event_tail + 1U) & (H417_USBSS_DIAG_EVENT_COUNT - 1U);
        available = 1U;
    }
    return available;
}

uint32_t h417_usbss_diag_dropped(void)
{
    return s_event_dropped;
}

void h417_usbhs_fallback_request(FunctionalState state);

/* Compile the WCH USBSS implementation unchanged except for test-policy hooks. */
#define USBHS_Device_Init h417_usbhs_fallback_request
#define TIM12_IRQHandler h417_wch_tim12_irq_unused
#include "ch32h417_usbss_device.c"
#undef TIM12_IRQHandler
#undef USBHS_Device_Init

void h417_usbhs_fallback_request(FunctionalState state)
{
    (void)state;
}

void TIM12_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM12_IRQHandler(void)
{
    if(TIM_GetITStatus(TIM12, TIM_IT_Update) == SET)
    {
        TIM_ClearITPendingBit(TIM12, TIM_IT_Update);
        USB_Timer_Start(DISABLE);
    }
}
