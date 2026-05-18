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

void rt_trigger_software_interrupt(void)
{
    NVIC_SetPendingIRQ(Software_IRQn);
}

void rt_hw_do_after_save_above(void)
{
    __asm volatile("li t0, 0x20");
    __asm volatile("csrs 0x804, t0");
    NVIC_ClearPendingIRQ(Software_IRQn);
}
