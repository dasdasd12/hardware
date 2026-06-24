/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-05-13     AI Assistant First version for CH32H417 V5F
 */

#ifndef __BOARD_H__
#define __BOARD_H__

#include <rtthread.h>
#include "ch32h417.h"

/* CH32H417 V5F memory map */
/* FLASH:  0x00010000, 128K  */
/* RAM_CODE (ITCM): 0x200A0000, 128K */
/* RAM (DTCM): 0x200C0000+512+256, ~255K */

#define SRAM_SIZE  256
#define SRAM_END   (0x200C0000 + SRAM_SIZE * 1024)

#define CH32_FLASH_START_ADDRESS   ((uint32_t)0x00010000)
#define CH32_FLASH_SIZE            (128 * 1024)
#define CH32_FLASH_END_ADDRESS     ((uint32_t)(CH32_FLASH_START_ADDRESS + CH32_FLASH_SIZE))

extern int _ebss;
extern int _heap_end;

#define HEAP_BEGIN  ((void *)&_ebss)
#define HEAP_END    ((void *)&_heap_end)

#define RT_CONSOLE_DEVICE_NAME  "uart8"

#define GET_INT_SP()   __asm volatile("csrrw sp,mscratch,sp")
#define FREE_INT_SP()  __asm volatile("csrrw sp,mscratch,sp")

#ifndef GPIO_Remap_NONE
#define GPIO_Remap_NONE  0
#endif

void rt_hw_board_init(void);

#endif /* __BOARD_H__ */
