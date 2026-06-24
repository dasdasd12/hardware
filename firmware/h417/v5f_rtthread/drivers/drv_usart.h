/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-05-13     AI Assistant First version for CH32H417
 */

#ifndef __DRV_USART_H__
#define __DRV_USART_H__

#include "ch32h417.h"
#include <rtdevice.h>

struct ch32_uart_hw_config
{
    uint32_t uart_periph_clock;
    uint32_t gpio_periph_clock;
    GPIO_TypeDef *tx_gpio_port;
    uint16_t tx_gpio_pin;
    GPIO_TypeDef *rx_gpio_port;
    uint16_t rx_gpio_pin;
    uint32_t remap;
    uint8_t tx_pin_source;
    uint8_t rx_pin_source;
    uint8_t gpio_af;
};

struct ch32_uart_config
{
    const char *name;
    USART_TypeDef *Instance;
    IRQn_Type irq_type;
};

struct ch32_uart
{
    struct ch32_uart_config *config;
    struct ch32_uart_hw_config *hw_config;
    struct rt_serial_device serial;
    USART_InitTypeDef Init;
};

int rt_hw_usart_init(void);

#endif /* __DRV_USART_H__ */
