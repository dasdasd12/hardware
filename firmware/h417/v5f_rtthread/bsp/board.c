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

/* Early bring-up UART: mirrors firmware/h417/basic/wch/SRC/Debug/debug.c
   USART_Printf_Init(DEBUG_UART8). Does not depend on RT-Thread device
   framework so it is visible the instant V5F runs. */
static void bsp_early_uart_init(uint32_t baud)
{
    GPIO_InitTypeDef  GPIO_InitStructure  = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOB, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART8, ENABLE);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource4, GPIO_AF11);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = baud;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx;
    USART_Init(USART8, &USART_InitStructure);
    USART_Cmd(USART8, ENABLE);
}

static void bsp_early_putc(char c)
{
    while (USART_GetFlagStatus(USART8, USART_FLAG_TC) == RESET);
    USART8->DATAR = (uint8_t)c;
}

static void bsp_early_puts(const char *s)
{
    while (*s) {
        bsp_early_putc(*s++);
    }
}

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

    /* Early UART before anything else can hang; visible if clock tree is alive */
    bsp_early_uart_init(115200);
    bsp_early_puts("\r\n[V5F] early-uart up\r\n");

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
