/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-05-13     AI Assistant First version for CH32H417 V5F
 */

#include "board.h"
#include <rthw.h>
#include <rtthread.h>

#ifdef RT_USING_SERIAL
#include "drv_usart.h"
#endif

extern uint32_t SystemCoreClock;
extern uint32_t HCLKClock;
extern int rt_hw_pin_init(void);

static uint32_t _SysTick_Config(rt_uint32_t ticks)
{
    /* Set lowest priority for SysTick1 and Software interrupts */
    NVIC_SetPriority(SysTick1_IRQn, 0xF0);
    NVIC_SetPriority(Software_IRQn, 0xF0);
    NVIC_EnableIRQ(SysTick1_IRQn);
    NVIC_EnableIRQ(Software_IRQn);

    SysTick1->CTLR = 0;
    SysTick1->ISR  = 0;
    SysTick1->CNT  = 0;
    SysTick1->CMP  = ticks - 1;
    SysTick1->CTLR = 0x0F;

    return 0;
}

/**
 * This function will initial your board.
 */
void rt_hw_board_init(void)
{
    /* System Clock Update */
    SystemAndCoreClockUpdate();

    /*
     * CH32H417 SysTick1 is clocked from HCLK on V5F. SystemCoreClock can be
     * higher than HCLK when the FPRE divider is active, which makes RT-Thread
     * delays run slow if it is used as the tick source frequency.
     */
    _SysTick_Config(HCLKClock / RT_TICK_PER_SECOND);

#if defined(RT_USING_USER_MAIN) && defined(RT_USING_HEAP)
    rt_system_heap_init((void *)HEAP_BEGIN, (void *)HEAP_END);
#endif

    /* USART driver initialization is open by default */
#ifdef RT_USING_SERIAL
    rt_hw_usart_init();
#endif

#ifdef RT_USING_CONSOLE
    rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
#endif

#ifdef RT_USING_PIN
    rt_hw_pin_init();
#endif

    /* Call components board initial (use INIT_BOARD_EXPORT()) */
#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif
}

void SysTick1_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void SysTick1_Handler(void)
{
    GET_INT_SP();
    /* enter interrupt */
    rt_interrupt_enter();

    SysTick1->ISR = 0;
    rt_tick_increase();

    /* leave interrupt */
    rt_interrupt_leave();
    FREE_INT_SP();
}
