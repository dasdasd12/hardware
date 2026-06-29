#include "ch32h417.h"
#include "ch32h417_rcc.h"
#include "system_ch32h417.h"

#define H417_V3F_WAKE_TRACE_BASE  ((volatile uint32_t *)0x20178000u)
#define H417_V3F_WAKE_TRACE_MAGIC 0x5633574Bu
#define V5F_START_ADDR      0x00010000u

enum
{
    H417_V3F_WAKE_STAGE_ENTER = 1,
    H417_V3F_WAKE_STAGE_SYSTEM_INIT = 2,
    H417_V3F_WAKE_STAGE_PWR_CLOCK = 3,
    H417_V3F_WAKE_STAGE_WAKE_V5F = 4,
    H417_V3F_WAKE_STAGE_LOOP = 5,
};

static void h417_v3f_wake_trace(uint32_t stage)
{
    H417_V3F_WAKE_TRACE_BASE[0] = H417_V3F_WAKE_TRACE_MAGIC;
    H417_V3F_WAKE_TRACE_BASE[1] = stage;
    H417_V3F_WAKE_TRACE_BASE[2] = RCC->CTLR;
    H417_V3F_WAKE_TRACE_BASE[3] = RCC->CFGR0;
}

int main(void)
{
    h417_v3f_wake_trace(H417_V3F_WAKE_STAGE_ENTER);

    SystemInit();
    SystemAndCoreClockUpdate();
    h417_v3f_wake_trace(H417_V3F_WAKE_STAGE_SYSTEM_INIT);

    RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE);
    h417_v3f_wake_trace(H417_V3F_WAKE_STAGE_PWR_CLOCK);

    NVIC_WakeUp_V5F(V5F_START_ADDR);
    h417_v3f_wake_trace(H417_V3F_WAKE_STAGE_WAKE_V5F);

    while(1)
    {
        H417_V3F_WAKE_TRACE_BASE[1] = H417_V3F_WAKE_STAGE_LOOP;
        H417_V3F_WAKE_TRACE_BASE[4]++;
        __asm__ volatile("nop");
    }
}
