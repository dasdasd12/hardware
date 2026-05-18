/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-05-13     AI Assistant RT-Thread config for CH32H417 V5F
 */

#ifndef __RTCONFIG_H__
#define __RTCONFIG_H__

/* RT-Thread Kernel */

#define RT_NAME_MAX         8
#define RT_ALIGN_SIZE       8
#define RT_THREAD_PRIORITY_256
#define RT_THREAD_PRIORITY_MAX  256
#define RT_TICK_PER_SECOND  1000
#define RT_USING_OVERFLOW_CHECK
#define RT_USING_HOOK
#define RT_HOOK_USING_FUNC_PTR
#define RT_USING_IDLE_HOOK
#define RT_IDLE_HOOK_LIST_SIZE  4
#define IDLE_THREAD_STACK_SIZE  1024

/* kservice optimization */
#define RT_KSERVICE_USING_STDLIB

/* Inter-Thread communication */
#define RT_USING_SEMAPHORE
#define RT_USING_MUTEX
#define RT_USING_EVENT
#define RT_USING_MAILBOX
#define RT_USING_MESSAGEQUEUE

/* Memory Management */
#define RT_USING_MEMPOOL
#define RT_USING_SMALL_MEM
#define RT_USING_SMALL_MEM_AS_HEAP
#define RT_USING_MEMHEAP
#define RT_USING_HEAP

/* Kernel Device Object */
#define RT_USING_DEVICE
#define RT_USING_CONSOLE
#define RT_CONSOLEBUF_SIZE  128
#define RT_CONSOLE_DEVICE_NAME  "uart8"
#define RT_USING_COMPONENTS_INIT

/* Thread */
#define RT_USING_USER_MAIN
#define RT_MAIN_THREAD_STACK_SIZE   2048
#define RT_MAIN_THREAD_PRIORITY     10

/* Compiler */
#define RT_USING_LIBC
#define RT_USING_NEWLIBC
/* #define RT_USING_POSIX_FS */
/* #define RT_USING_POSIX_STDIO */
/* #define RT_USING_POSIX_DELAY */
/* #define RT_USING_POSIX_CLOCK */
/* #define RT_USING_POSIX_PTHREAD */

/* C++ features */
/* #define RT_USING_CPLUSPLUS */

/* Command line */
#define RT_USING_FINSH
#define RT_USING_MSH
#define RT_USING_MSH_ONLY
#define FINSH_THREAD_NAME   "tshell"
#define FINSH_USING_HISTORY
#define FINSH_HISTORY_LINES 5
#define FINSH_USING_SYMTAB
#define FINSH_USING_DESCRIPTION
#define FINSH_THREAD_STACK_SIZE 2048
#define FINSH_THREAD_PRIORITY   20
#define FINSH_CMD_SIZE          80
#define FINSH_ARG_MAX           10

/* Device Drivers */
#define RT_USING_SERIAL
#define RT_SERIAL_USING_DMA
#define RT_USING_PIN

/* Board Configuration */
#define SOC_RISCV_FAMILY_CH32
#define SOC_SERIES_CH32H417

/* CH32H417 V5F specific */
#define ARCH_RISCV_FPU
#define ARCH_RISCV_FPU_S

/* RT_HW_ISR_NUM for trap_common.c */
#define RT_HW_ISR_NUM 256

/* SMP/AMP */
#define RT_CPUS_NR 1

/* Backtrace */
#define RT_BACKTRACE_LEVEL_MAX_NR 32

/* USB3.0 MAC address (unused for now) */
/* #define RT_CH32H417_MAC_ADDRESS 0x00,0x11,0x22,0x33,0x44,0x55 */

#endif /* __RTCONFIG_H__ */
