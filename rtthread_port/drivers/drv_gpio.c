/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-05-13     AI Assistant First version for CH32H417
 */

#include <board.h>
#include <rtdevice.h>

#ifdef RT_USING_PIN

#define PIN_NUM(port, no) (((((port) & 0xFu) << 4) | ((no) & 0xFu)))
#define PIN_PORT(pin)     ((rt_uint8_t)(((pin) >> 4) & 0xFu))
#define PIN_NO(pin)       ((rt_uint8_t)((pin) & 0xFu))

#define PIN_STPORT(pin)   ((GPIO_TypeDef *)(GPIOA_BASE + (0x400u * PIN_PORT(pin))))
#define PIN_STPIN(pin)    ((rt_uint16_t)(1u << PIN_NO(pin)))

#if defined(GPIOZ)
#define __CH32_PORT_MAX 12u
#elif defined(GPIOK)
#define __CH32_PORT_MAX 11u
#elif defined(GPIOJ)
#define __CH32_PORT_MAX 10u
#elif defined(GPIOI)
#define __CH32_PORT_MAX 9u
#elif defined(GPIOH)
#define __CH32_PORT_MAX 8u
#elif defined(GPIOG)
#define __CH32_PORT_MAX 7u
#elif defined(GPIOF)
#define __CH32_PORT_MAX 6u
#elif defined(GPIOE)
#define __CH32_PORT_MAX 5u
#elif defined(GPIOD)
#define __CH32_PORT_MAX 4u
#elif defined(GPIOC)
#define __CH32_PORT_MAX 3u
#elif defined(GPIOB)
#define __CH32_PORT_MAX 2u
#elif defined(GPIOA)
#define __CH32_PORT_MAX 1u
#else
#define __CH32_PORT_MAX 0u
#error Unsupported CH32 GPIO peripheral.
#endif

#define PIN_STPORT_MAX __CH32_PORT_MAX

static struct rt_pin_irq_hdr pin_irq_hdr_tab[] =
{
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
    {-1, 0, RT_NULL, RT_NULL},
};
static rt_uint32_t pin_irq_enable_mask = 0;

#define ITEM_NUM(items) (sizeof(items) / sizeof((items)[0]))

static rt_base_t ch32_pin_get(const char *name)
{
    rt_base_t pin = 0;
    int hw_port_num, hw_pin_num = 0;
    int i, name_len;

    name_len = rt_strlen(name);

    if ((name_len < 4) || (name_len >= 6))
    {
        return -RT_EINVAL;
    }
    if ((name[0] != 'P') || (name[2] != '.'))
    {
        return -RT_EINVAL;
    }

    if ((name[1] >= 'A') && (name[1] <= 'Z'))
    {
        hw_port_num = (int)(name[1] - 'A');
    }
    else
    {
        return -RT_EINVAL;
    }

    for (i = 3; i < name_len; i++)
    {
        hw_pin_num *= 10;
        hw_pin_num += name[i] - '0';
    }

    pin = PIN_NUM(hw_port_num, hw_pin_num);

    return pin;
}

static void ch32_pin_write(rt_device_t dev, rt_base_t pin, rt_uint8_t value)
{
    GPIO_TypeDef *gpio_port;
    rt_uint16_t gpio_pin;

    if (PIN_PORT(pin) < PIN_STPORT_MAX)
    {
        gpio_port = PIN_STPORT(pin);
        gpio_pin = PIN_STPIN(pin);
        GPIO_WriteBit(gpio_port, gpio_pin, (BitAction)value);
    }
}

static rt_ssize_t ch32_pin_read(rt_device_t dev, rt_base_t pin)
{
    GPIO_TypeDef *gpio_port;
    rt_uint16_t gpio_pin;
    rt_ssize_t value = PIN_LOW;

    if (PIN_PORT(pin) < PIN_STPORT_MAX)
    {
        gpio_port = PIN_STPORT(pin);
        gpio_pin = PIN_STPIN(pin);
        value = GPIO_ReadInputDataBit(gpio_port, gpio_pin);
    }
    else
    {
        return -RT_EINVAL;
    }

    return value;
}

static void ch32_pin_mode(rt_device_t dev, rt_base_t pin, rt_uint8_t mode)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (PIN_PORT(pin) >= PIN_STPORT_MAX)
    {
        return;
    }

    GPIO_InitStruct.GPIO_Pin   = PIN_STPIN(pin);
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_Very_High;

    switch (mode)
    {
    case PIN_MODE_OUTPUT:
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
        break;
    case PIN_MODE_INPUT:
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        break;
    case PIN_MODE_INPUT_PULLDOWN:
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPD;
        break;
    case PIN_MODE_INPUT_PULLUP:
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
        break;
    case PIN_MODE_OUTPUT_OD:
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_OD;
        break;
    default:
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        break;
    }

    GPIO_Init(PIN_STPORT(pin), &GPIO_InitStruct);
}

rt_inline rt_int32_t bit2bitno(rt_uint32_t bit)
{
    rt_int32_t i;
    for (i = 0; i < 32; i++)
    {
        if (((rt_uint32_t)0x01 << i) == bit)
        {
            return i;
        }
    }
    return -1;
}

static rt_err_t ch32_pin_attach_irq(struct rt_device *device, rt_base_t pin,
                                     rt_uint8_t mode, void (*hdr)(void *args), void *args)
{
    rt_base_t level;
    rt_int32_t irqindex = -1;

    if (PIN_PORT(pin) >= PIN_STPORT_MAX)
    {
        return -RT_ENOSYS;
    }

    irqindex = bit2bitno(PIN_STPIN(pin));
    if (irqindex < 0 || irqindex >= (rt_int32_t)ITEM_NUM(pin_irq_hdr_tab))
    {
        return -RT_ENOSYS;
    }

    level = rt_hw_interrupt_disable();
    if (pin_irq_hdr_tab[irqindex].pin == pin &&
        pin_irq_hdr_tab[irqindex].hdr == hdr &&
        pin_irq_hdr_tab[irqindex].mode == mode &&
        pin_irq_hdr_tab[irqindex].args == args)
    {
        rt_hw_interrupt_enable(level);
        return RT_EOK;
    }
    if (pin_irq_hdr_tab[irqindex].pin != -1)
    {
        rt_hw_interrupt_enable(level);
        return -RT_EBUSY;
    }
    pin_irq_hdr_tab[irqindex].pin = pin;
    pin_irq_hdr_tab[irqindex].hdr = hdr;
    pin_irq_hdr_tab[irqindex].mode = mode;
    pin_irq_hdr_tab[irqindex].args = args;
    rt_hw_interrupt_enable(level);

    return RT_EOK;
}

static rt_err_t ch32_pin_dettach_irq(struct rt_device *device, rt_base_t pin)
{
    rt_base_t level;
    rt_int32_t irqindex = -1;

    if (PIN_PORT(pin) >= PIN_STPORT_MAX)
    {
        return -RT_ENOSYS;
    }

    irqindex = bit2bitno(PIN_STPIN(pin));
    if (irqindex < 0 || irqindex >= (rt_int32_t)ITEM_NUM(pin_irq_hdr_tab))
    {
        return -RT_ENOSYS;
    }

    level = rt_hw_interrupt_disable();
    if (pin_irq_hdr_tab[irqindex].pin == -1)
    {
        rt_hw_interrupt_enable(level);
        return RT_EOK;
    }
    pin_irq_hdr_tab[irqindex].pin = -1;
    pin_irq_hdr_tab[irqindex].hdr = RT_NULL;
    pin_irq_hdr_tab[irqindex].mode = 0;
    pin_irq_hdr_tab[irqindex].args = RT_NULL;
    rt_hw_interrupt_enable(level);

    return RT_EOK;
}

static rt_err_t ch32_pin_irq_enable(struct rt_device *device, rt_base_t pin,
                                     rt_uint8_t enabled)
{
    rt_base_t level;
    rt_int32_t irqindex = -1;
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    EXTI_InitTypeDef EXTI_InitStructure = {0};
    IRQn_Type irqno;

    if (PIN_PORT(pin) >= PIN_STPORT_MAX)
    {
        return -RT_ENOSYS;
    }

    irqindex = bit2bitno(PIN_STPIN(pin));
    if (irqindex < 0 || irqindex >= (rt_int32_t)ITEM_NUM(pin_irq_hdr_tab))
    {
        return -RT_ENOSYS;
    }

    if (enabled == PIN_IRQ_ENABLE)
    {
        if (pin_irq_hdr_tab[irqindex].pin == -1)
        {
            return -RT_ENOSYS;
        }

        level = rt_hw_interrupt_disable();

        GPIO_InitStruct.GPIO_Pin   = PIN_STPIN(pin);
        GPIO_InitStruct.GPIO_Speed = GPIO_Speed_Very_High;

        EXTI_InitStructure.EXTI_Line    = PIN_STPIN(pin);
        EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
        EXTI_InitStructure.EXTI_LineCmd = ENABLE;

        switch (pin_irq_hdr_tab[irqindex].mode)
        {
        case PIN_IRQ_MODE_RISING:
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPD;
            EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
            break;
        case PIN_IRQ_MODE_FALLING:
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
            EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
            break;
        case PIN_IRQ_MODE_RISING_FALLING:
            GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
            EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
            break;
        }
        GPIO_Init(PIN_STPORT(pin), &GPIO_InitStruct);

        GPIO_EXTILineConfig(PIN_PORT(pin), (rt_uint8_t)irqindex);
        EXTI_Init(&EXTI_InitStructure);

        /* CH32H417 groups EXTI into EXTI7_0 and EXTI15_8 */
        if (irqindex <= 7)
            irqno = EXTI7_0_IRQn;
        else
            irqno = EXTI15_8_IRQn;

        NVIC_SetPriority(irqno, 5 << 4);
        NVIC_EnableIRQ(irqno);
        pin_irq_enable_mask |= PIN_STPIN(pin);

        rt_hw_interrupt_enable(level);
    }
    else if (enabled == PIN_IRQ_DISABLE)
    {
        level = rt_hw_interrupt_disable();

        pin_irq_enable_mask &= ~PIN_STPIN(pin);

        if (irqindex <= 7)
        {
            if (!(pin_irq_enable_mask & 0x00FF))
                NVIC_DisableIRQ(EXTI7_0_IRQn);
        }
        else
        {
            if (!(pin_irq_enable_mask & 0xFF00))
                NVIC_DisableIRQ(EXTI15_8_IRQn);
        }

        rt_hw_interrupt_enable(level);
    }
    else
    {
        return -RT_ENOSYS;
    }

    return RT_EOK;
}

static const struct rt_pin_ops _ch32_pin_ops =
{
    ch32_pin_mode,
    ch32_pin_write,
    ch32_pin_read,
    ch32_pin_attach_irq,
    ch32_pin_dettach_irq,
    ch32_pin_irq_enable,
    ch32_pin_get,
};

rt_inline void pin_irq_hdr(int irqno)
{
    if (pin_irq_hdr_tab[irqno].hdr)
    {
        pin_irq_hdr_tab[irqno].hdr(pin_irq_hdr_tab[irqno].args);
    }
}

static void ch32_exti_handler(rt_uint16_t line_mask, rt_uint8_t start, rt_uint8_t end)
{
    rt_uint8_t i;
    for (i = start; i <= end; i++)
    {
        rt_uint32_t line = (rt_uint32_t)(1u << i);
        if ((line_mask & line) && (EXTI_GetITStatus(line) != RESET))
        {
            pin_irq_hdr(i);
            EXTI_ClearITPendingBit(line);
        }
    }
}

void EXTI7_0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void EXTI7_0_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    ch32_exti_handler(0x00FF, 0, 7);
    rt_interrupt_leave();
    FREE_INT_SP();
}

void EXTI15_8_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void EXTI15_8_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    ch32_exti_handler(0xFF00, 8, 15);
    rt_interrupt_leave();
    FREE_INT_SP();
}

int rt_hw_pin_init(void)
{
#ifdef RCC_HB2Periph_GPIOA
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOA, ENABLE);
#endif
#ifdef RCC_HB2Periph_GPIOB
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOB, ENABLE);
#endif
#ifdef RCC_HB2Periph_GPIOC
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOC, ENABLE);
#endif
#ifdef RCC_HB2Periph_GPIOD
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOD, ENABLE);
#endif
#ifdef RCC_HB2Periph_GPIOE
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOE, ENABLE);
#endif
#ifdef RCC_HB2Periph_GPIOF
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOF, ENABLE);
#endif
#ifdef RCC_HB2Periph_GPIOG
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOG, ENABLE);
#endif
#ifdef RCC_HB2Periph_GPIOH
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOH, ENABLE);
#endif

    return rt_device_pin_register("pin", &_ch32_pin_ops, RT_NULL);
}

#endif /* RT_USING_PIN */
