#include "board_init.h"

#include "ch32h417.h"
#include "ch32h417_rcc.h"
#include "ch32h417_pwr.h"

#define V3F_TRACE_BASE ((volatile uint32_t *)0x20178000u)
#define V3F_TRACE_MAGIC 0x56334650u

#ifndef V3F_WAKE_V5F
#define V3F_WAKE_V5F 0
#endif

#define V5F_START_ADDR 0x00010000u

volatile uint32_t WFE_MASK = 0;
volatile uint32_t WFE_WkupSource = 0;

void v3f_board_init(void)
{
    SystemInit();
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE);

#if V3F_WAKE_V5F
    NVIC_WakeUp_V5F(V5F_START_ADDR);
#endif

    V3F_TRACE_BASE[0] = V3F_TRACE_MAGIC;
    V3F_TRACE_BASE[1] = RCC->CFGR2;
    V3F_TRACE_BASE[2] = RCC->CTLR;
    V3F_TRACE_BASE[3] = RCC->HBPCENR;
}

void v3f_board_delay_1ms(void)
{
    v3f_board_delay_us(1000U);
}

void v3f_board_delay_us(uint32_t us)
{
    uint32_t ticks;

    if(us == 0U)
    {
        return;
    }

    ticks = (uint32_t)((((uint64_t)SystemCoreClock * (uint64_t)us) +
                        999999ULL) / 1000000ULL);
    if(ticks == 0U)
    {
        ticks = 1U;
    }

    SysTick0->ISR &= ~(1U << 0);
    SysTick0->CNT = 0U;
    SysTick0->CMP = ticks;
    SysTick0->CTLR = (1U << 2);
    SysTick0->CTLR |= (1U << 0);
    while((SysTick0->ISR & (1U << 0)) == 0U)
    {
    }
    SysTick0->CTLR &= ~(1U << 0);
}

void v3f_trace_set(uint32_t index, uint32_t value)
{
    if(index < 64U)
    {
        V3F_TRACE_BASE[index] = value;
    }
}

void v3f_trace_inc(uint32_t index)
{
    if(index < 64U)
    {
        V3F_TRACE_BASE[index]++;
    }
}
