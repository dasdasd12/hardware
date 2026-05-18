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
#include <rtdevice.h>
#include "drv_usart.h"

#ifdef RT_USING_SERIAL

#ifdef BSP_USING_UART8
static struct ch32_uart_hw_config uart8_hw_config =
{
    RCC_HB1Periph_USART8,
    RCC_HB2Periph_GPIOB,
    GPIOB, GPIO_Pin_4,
    GPIOB, GPIO_Pin_5,
    GPIO_Remap_NONE,
    GPIO_PinSource4,
    GPIO_PinSource5,
    GPIO_AF11,
};
#endif

static struct ch32_uart_config uart_config[] =
{
#ifdef BSP_USING_UART8
    { "uart8", USART8, USART8_IRQn },
#endif
};

static struct ch32_uart uart_obj[sizeof(uart_config) / sizeof(uart_config[0])] = {0};

static rt_err_t ch32_configure(struct rt_serial_device *serial, struct serial_configure *cfg)
{
    struct ch32_uart *uart;
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RT_ASSERT(serial != RT_NULL);
    RT_ASSERT(cfg != RT_NULL);

    uart = (struct ch32_uart *)serial->parent.user_data;

    uart->Init.USART_BaudRate             = cfg->baud_rate;
    uart->Init.USART_HardwareFlowControl  = USART_HardwareFlowControl_None;
    uart->Init.USART_Mode                 = USART_Mode_Rx | USART_Mode_Tx;

    switch (cfg->data_bits)
    {
    case DATA_BITS_8:
        uart->Init.USART_WordLength = USART_WordLength_8b;
        break;
    case DATA_BITS_9:
        uart->Init.USART_WordLength = USART_WordLength_9b;
        break;
    default:
        uart->Init.USART_WordLength = USART_WordLength_8b;
        break;
    }

    switch (cfg->stop_bits)
    {
    case STOP_BITS_1:
        uart->Init.USART_StopBits = USART_StopBits_1;
        break;
    case STOP_BITS_2:
        uart->Init.USART_StopBits = USART_StopBits_2;
        break;
    default:
        uart->Init.USART_StopBits = USART_StopBits_1;
        break;
    }

    switch (cfg->parity)
    {
    case PARITY_NONE:
        uart->Init.USART_Parity = USART_Parity_No;
        break;
    case PARITY_ODD:
        uart->Init.USART_Parity = USART_Parity_Odd;
        break;
    case PARITY_EVEN:
        uart->Init.USART_Parity = USART_Parity_Even;
        break;
    default:
        uart->Init.USART_Parity = USART_Parity_No;
        break;
    }

    RCC_HB2PeriphClockCmd(uart->hw_config->gpio_periph_clock, ENABLE);
    RCC_HB1PeriphClockCmd(uart->hw_config->uart_periph_clock, ENABLE);

    GPIO_PinAFConfig(uart->hw_config->tx_gpio_port, uart->hw_config->tx_pin_source, uart->hw_config->gpio_af);
    GPIO_PinAFConfig(uart->hw_config->rx_gpio_port, uart->hw_config->rx_pin_source, uart->hw_config->gpio_af);

    GPIO_InitStructure.GPIO_Pin   = uart->hw_config->tx_gpio_pin;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(uart->hw_config->tx_gpio_port, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin   = uart->hw_config->rx_gpio_pin;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(uart->hw_config->rx_gpio_port, &GPIO_InitStructure);

    USART_Init(uart->config->Instance, &uart->Init);
    USART_Cmd(uart->config->Instance, ENABLE);

    return RT_EOK;
}

static rt_err_t ch32_control(struct rt_serial_device *serial, int cmd, void *arg)
{
    struct ch32_uart *uart;
    RT_ASSERT(serial != RT_NULL);
    uart = (struct ch32_uart *)serial->parent.user_data;

    switch (cmd)
    {
    case RT_DEVICE_CTRL_CLR_INT:
        NVIC_DisableIRQ(uart->config->irq_type);
        USART_ITConfig(uart->config->Instance, USART_IT_RXNE, DISABLE);
        break;
    case RT_DEVICE_CTRL_SET_INT:
        NVIC_EnableIRQ(uart->config->irq_type);
        USART_ITConfig(uart->config->Instance, USART_IT_RXNE, ENABLE);
        break;
    }
    return RT_EOK;
}

static int ch32_putc(struct rt_serial_device *serial, char c)
{
    struct ch32_uart *uart;
    RT_ASSERT(serial != RT_NULL);
    uart = (struct ch32_uart *)serial->parent.user_data;
    while (USART_GetFlagStatus(uart->config->Instance, USART_FLAG_TC) == RESET);
    uart->config->Instance->DATAR = c;
    return 1;
}

static int ch32_getc(struct rt_serial_device *serial)
{
    int ch;
    struct ch32_uart *uart;
    RT_ASSERT(serial != RT_NULL);
    uart = (struct ch32_uart *)serial->parent.user_data;
    ch = -1;
    if (USART_GetFlagStatus(uart->config->Instance, USART_FLAG_RXNE) != RESET)
    {
        ch = uart->config->Instance->DATAR & 0xff;
    }
    return ch;
}

static rt_ssize_t ch32dma_transmit(struct rt_serial_device *serial, rt_uint8_t *buf, rt_size_t size, int direction)
{
    return -RT_EIO;
}

static void uart_isr(struct rt_serial_device *serial)
{
    struct ch32_uart *uart = (struct ch32_uart *)serial->parent.user_data;
    RT_ASSERT(uart != RT_NULL);
    if (USART_GetITStatus(uart->config->Instance, USART_IT_RXNE) != RESET)
    {
        rt_hw_serial_isr(serial, RT_SERIAL_EVENT_RX_IND);
        USART_ClearITPendingBit(uart->config->Instance, USART_IT_RXNE);
    }
}

static const struct rt_uart_ops ch32_uart_ops =
{
    ch32_configure,
    ch32_control,
    ch32_putc,
    ch32_getc,
    ch32dma_transmit
};

#ifdef BSP_USING_UART8
void USART8_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART8_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    uart_isr(&(uart_obj[0].serial));
    rt_interrupt_leave();
    FREE_INT_SP();
}
#endif

int rt_hw_usart_init(void)
{
    rt_size_t obj_num = sizeof(uart_obj) / sizeof(struct ch32_uart);
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    rt_err_t result = RT_EOK;

    for (int i = 0; i < (int)obj_num; i++)
    {
        uart_obj[i].config        = &uart_config[i];
#ifdef BSP_USING_UART8
        if (uart_obj[i].config->Instance == USART8)
        {
            uart_obj[i].hw_config = &uart8_hw_config;
        }
#endif
        uart_obj[i].serial.ops    = &ch32_uart_ops;
        uart_obj[i].serial.config = config;

        result = rt_hw_serial_register(&uart_obj[i].serial, uart_obj[i].config->name,
                                       RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX,
                                       &uart_obj[i]);
        RT_ASSERT(result == RT_EOK);
    }

    return (int)result;
}

#endif /* RT_USING_SERIAL */
