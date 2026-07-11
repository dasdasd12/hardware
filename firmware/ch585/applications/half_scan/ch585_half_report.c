#include "ch585_half_report.h"

#include <string.h>

#include "aik_profile_format.h"

#define HID_USAGE_A             0x04U
#define HID_USAGE_B             0x05U
#define HID_USAGE_C             0x06U
#define HID_USAGE_D             0x07U
#define HID_USAGE_E             0x08U
#define HID_USAGE_F             0x09U
#define HID_USAGE_G             0x0AU
#define HID_USAGE_H             0x0BU
#define HID_USAGE_I             0x0CU
#define HID_USAGE_J             0x0DU
#define HID_USAGE_K             0x0EU
#define HID_USAGE_L             0x0FU
#define HID_USAGE_M             0x10U
#define HID_USAGE_N             0x11U
#define HID_USAGE_O             0x12U
#define HID_USAGE_P             0x13U
#define HID_USAGE_Q             0x14U
#define HID_USAGE_R             0x15U
#define HID_USAGE_S             0x16U
#define HID_USAGE_T             0x17U
#define HID_USAGE_U             0x18U
#define HID_USAGE_V             0x19U
#define HID_USAGE_W             0x1AU
#define HID_USAGE_X             0x1BU
#define HID_USAGE_Y             0x1CU
#define HID_USAGE_Z             0x1DU
#define HID_USAGE_1             0x1EU
#define HID_USAGE_2             0x1FU
#define HID_USAGE_3             0x20U
#define HID_USAGE_4             0x21U
#define HID_USAGE_5             0x22U
#define HID_USAGE_6             0x23U
#define HID_USAGE_7             0x24U
#define HID_USAGE_8             0x25U
#define HID_USAGE_9             0x26U
#define HID_USAGE_0             0x27U
#define HID_USAGE_ENTER         0x28U
#define HID_USAGE_ESCAPE        0x29U
#define HID_USAGE_BACKSPACE     0x2AU
#define HID_USAGE_TAB           0x2BU
#define HID_USAGE_SPACE         0x2CU
#define HID_USAGE_MINUS         0x2DU
#define HID_USAGE_EQUAL         0x2EU
#define HID_USAGE_LEFT_BRACKET  0x2FU
#define HID_USAGE_RIGHT_BRACKET 0x30U
#define HID_USAGE_BACKSLASH     0x31U
#define HID_USAGE_SEMICOLON     0x33U
#define HID_USAGE_QUOTE         0x34U
#define HID_USAGE_GRAVE         0x35U
#define HID_USAGE_COMMA         0x36U
#define HID_USAGE_PERIOD        0x37U
#define HID_USAGE_SLASH         0x38U
#define HID_USAGE_CAPS_LOCK     0x39U
#define HID_USAGE_F1            0x3AU
#define HID_USAGE_F2            0x3BU
#define HID_USAGE_F3            0x3CU
#define HID_USAGE_F4            0x3DU
#define HID_USAGE_F5            0x3EU
#define HID_USAGE_F6            0x3FU
#define HID_USAGE_F7            0x40U
#define HID_USAGE_F8            0x41U
#define HID_USAGE_F9            0x42U
#define HID_USAGE_F10           0x43U
#define HID_USAGE_F11           0x44U
#define HID_USAGE_F12           0x45U
#define HID_USAGE_RIGHT_ARROW   0x4FU
#define HID_USAGE_LEFT_ARROW    0x50U
#define HID_USAGE_DOWN_ARROW    0x51U
#define HID_USAGE_UP_ARROW      0x52U

#define HID_MOD_LEFT_CTRL   0x01U
#define HID_MOD_LEFT_SHIFT  0x02U
#define HID_MOD_LEFT_ALT    0x04U
#define HID_MOD_LEFT_GUI    0x08U
#define HID_MOD_RIGHT_CTRL  0x10U
#define HID_MOD_RIGHT_SHIFT 0x20U
#define HID_MOD_RIGHT_ALT   0x40U
#define HID_MOD_RIGHT_GUI   0x80U

#define CH585_FN_LAYER_KEY       38U
#define CH585_GLOBAL_DOWN_BYTES  ((AIK_KEY_COUNT_TOTAL + 7U) / 8U)

typedef struct
{
    uint8_t usage;
    uint8_t modifier_mask;
} ch585_key_output_t;

static const ch585_key_output_t s_factory_key_outputs[AIK_KEY_COUNT_TOTAL] =
{
    { HID_USAGE_F12, 0U },
    { HID_USAGE_F11, 0U },
    { HID_USAGE_F10, 0U },
    { HID_USAGE_F9, 0U },
    { HID_USAGE_F8, 0U },
    { HID_USAGE_F7, 0U },
    { HID_USAGE_F6, 0U },
    { HID_USAGE_BACKSPACE, 0U },
    { HID_USAGE_EQUAL, 0U },
    { HID_USAGE_MINUS, 0U },
    { HID_USAGE_0, 0U },
    { HID_USAGE_9, 0U },
    { HID_USAGE_8, 0U },
    { HID_USAGE_7, 0U },
    { HID_USAGE_BACKSLASH, 0U },
    { HID_USAGE_RIGHT_BRACKET, 0U },
    { HID_USAGE_LEFT_BRACKET, 0U },
    { HID_USAGE_P, 0U },
    { HID_USAGE_O, 0U },
    { HID_USAGE_I, 0U },
    { HID_USAGE_U, 0U },
    { HID_USAGE_Y, 0U },
    { HID_USAGE_ENTER, 0U },
    { HID_USAGE_QUOTE, 0U },
    { HID_USAGE_SEMICOLON, 0U },
    { HID_USAGE_L, 0U },
    { HID_USAGE_K, 0U },
    { HID_USAGE_J, 0U },
    { HID_USAGE_H, 0U },
    { 0U, HID_MOD_RIGHT_SHIFT },
    { HID_USAGE_SLASH, 0U },
    { HID_USAGE_PERIOD, 0U },
    { HID_USAGE_COMMA, 0U },
    { HID_USAGE_M, 0U },
    { HID_USAGE_N, 0U },
    { HID_USAGE_B, 0U },
    { 0U, HID_MOD_RIGHT_CTRL },
    { 0U, HID_MOD_RIGHT_GUI },
    { 0U, 0U },
    { 0U, HID_MOD_RIGHT_ALT },
    { HID_USAGE_SPACE, 0U },
    { HID_USAGE_F5, 0U },
    { HID_USAGE_F4, 0U },
    { HID_USAGE_F3, 0U },
    { HID_USAGE_F2, 0U },
    { HID_USAGE_F1, 0U },
    { HID_USAGE_ESCAPE, 0U },
    { HID_USAGE_6, 0U },
    { HID_USAGE_5, 0U },
    { HID_USAGE_4, 0U },
    { HID_USAGE_3, 0U },
    { HID_USAGE_2, 0U },
    { HID_USAGE_1, 0U },
    { HID_USAGE_GRAVE, 0U },
    { HID_USAGE_Y, 0U },
    { HID_USAGE_T, 0U },
    { HID_USAGE_R, 0U },
    { HID_USAGE_E, 0U },
    { HID_USAGE_W, 0U },
    { HID_USAGE_Q, 0U },
    { HID_USAGE_TAB, 0U },
    { HID_USAGE_G, 0U },
    { HID_USAGE_F, 0U },
    { HID_USAGE_D, 0U },
    { HID_USAGE_S, 0U },
    { HID_USAGE_A, 0U },
    { HID_USAGE_CAPS_LOCK, 0U },
    { HID_USAGE_B, 0U },
    { HID_USAGE_V, 0U },
    { HID_USAGE_C, 0U },
    { HID_USAGE_X, 0U },
    { HID_USAGE_Z, 0U },
    { 0U, HID_MOD_LEFT_SHIFT },
    { HID_USAGE_SPACE, 0U },
    { 0U, HID_MOD_LEFT_ALT },
    { 0U, HID_MOD_LEFT_GUI },
    { 0U, HID_MOD_LEFT_CTRL },
};

typedef struct
{
    uint8_t target_kind;   /* AIK_HP_TARGET_* */
    uint16_t value;
} ch585_local_binding_t;

#define CH585_LOCAL_SIGNAL_COUNT 11U

static const ch585_local_binding_t
s_factory_locals[CH585_LOCAL_SIGNAL_COUNT] =
{
    /* AIK_HP_SIGNAL_FIVEWAY_UP    */ { AIK_HP_TARGET_KEYBOARD, HID_USAGE_UP_ARROW },
    /* AIK_HP_SIGNAL_FIVEWAY_DOWN  */ { AIK_HP_TARGET_KEYBOARD, HID_USAGE_DOWN_ARROW },
    /* AIK_HP_SIGNAL_FIVEWAY_LEFT  */ { AIK_HP_TARGET_KEYBOARD, HID_USAGE_LEFT_ARROW },
    /* AIK_HP_SIGNAL_FIVEWAY_RIGHT */ { AIK_HP_TARGET_KEYBOARD, HID_USAGE_RIGHT_ARROW },
    /* AIK_HP_SIGNAL_FIVEWAY_PRESS */ { AIK_HP_TARGET_KEYBOARD, HID_USAGE_ENTER },
    /* AIK_HP_SIGNAL_WHEEL_UP      */ { AIK_HP_TARGET_MOUSE_WHEEL, 0x0001U },
    /* AIK_HP_SIGNAL_WHEEL_DOWN    */ { AIK_HP_TARGET_MOUSE_WHEEL, 0x00FFU },
    /* (reserved)                  */ { AIK_HP_TARGET_NONE, 0U },
    /* AIK_HP_SIGNAL_EC11_CW       */ { AIK_HP_TARGET_CONSUMER, AIK_CONSUMER_USAGE_VOLUME_UP },
    /* AIK_HP_SIGNAL_EC11_CCW      */ { AIK_HP_TARGET_CONSUMER, AIK_CONSUMER_USAGE_VOLUME_DOWN },
    /* AIK_HP_SIGNAL_EC11_PRESS    */ { AIK_HP_TARGET_CONSUMER, AIK_CONSUMER_USAGE_MUTE },
};

static ch585_key_output_t s_key_outputs[AIK_KEY_COUNT_TOTAL];
static ch585_local_binding_t s_locals[CH585_LOCAL_SIGNAL_COUNT];
static uint8_t s_tables_ready;

static void half_report_ensure_tables(void)
{
    if(s_tables_ready == 0U)
    {
        ch585_half_report_reset_factory();
    }
}

void ch585_half_report_reset_factory(void)
{
    memcpy(s_key_outputs, s_factory_key_outputs, sizeof(s_key_outputs));
    memcpy(s_locals, s_factory_locals, sizeof(s_locals));
    s_tables_ready = 1U;
}

void ch585_half_report_set_key_outputs(
    const uint8_t pairs[AIK_KEY_COUNT_TOTAL * 2U])
{
    uint8_t key_id;

    half_report_ensure_tables();
    for(key_id = 0U; key_id < AIK_KEY_COUNT_TOTAL; key_id++)
    {
        s_key_outputs[key_id].usage = pairs[(uint16_t)key_id * 2U];
        s_key_outputs[key_id].modifier_mask =
            pairs[((uint16_t)key_id * 2U) + 1U];
    }
}

void ch585_half_report_clear_locals(void)
{
    half_report_ensure_tables();
    memset(s_locals, 0, sizeof(s_locals));
}

void ch585_half_report_set_local(uint8_t signal_id, uint8_t target_kind,
                                 uint16_t value)
{
    half_report_ensure_tables();
    if(signal_id < CH585_LOCAL_SIGNAL_COUNT)
    {
        s_locals[signal_id].target_kind = target_kind;
        s_locals[signal_id].value = value;
    }
}

static uint8_t local_signal_down(uint8_t signal_id,
                                 const aik_spi_half_state_v1_t *left,
                                 const aik_spi_half_state_v1_t *right)
{
    switch(signal_id)
    {
        case AIK_HP_SIGNAL_FIVEWAY_UP:
            return (left != 0) ?
                   aik_spi_half_bit_down(left, AIK_LEFT_LOCAL_BIT_SCR_UP) : 0U;
        case AIK_HP_SIGNAL_FIVEWAY_DOWN:
            return (left != 0) ?
                   aik_spi_half_bit_down(left, AIK_LEFT_LOCAL_BIT_SCR_DOWN) : 0U;
        case AIK_HP_SIGNAL_FIVEWAY_LEFT:
            return (left != 0) ?
                   aik_spi_half_bit_down(left, AIK_LEFT_LOCAL_BIT_SCR_LEFT) : 0U;
        case AIK_HP_SIGNAL_FIVEWAY_RIGHT:
            return (left != 0) ?
                   aik_spi_half_bit_down(left, AIK_LEFT_LOCAL_BIT_SCR_RIGHT) : 0U;
        case AIK_HP_SIGNAL_FIVEWAY_PRESS:
            return (left != 0) ?
                   aik_spi_half_bit_down(left, AIK_LEFT_LOCAL_BIT_SCR_CENTER) : 0U;
        case AIK_HP_SIGNAL_WHEEL_UP:
            return (left != 0) ?
                   aik_spi_half_bit_down(left, AIK_LEFT_LOCAL_BIT_SCR_WHEEL_UP) : 0U;
        case AIK_HP_SIGNAL_WHEEL_DOWN:
            return (left != 0) ?
                   aik_spi_half_bit_down(left, AIK_LEFT_LOCAL_BIT_SCR_WHEEL_DOWN) : 0U;
        case AIK_HP_SIGNAL_EC11_CW:
            return (right != 0) ?
                   aik_spi_half_bit_down(right, AIK_RIGHT_LOCAL_BIT_EC11_CW) : 0U;
        case AIK_HP_SIGNAL_EC11_CCW:
            return (right != 0) ?
                   aik_spi_half_bit_down(right, AIK_RIGHT_LOCAL_BIT_EC11_CCW) : 0U;
        case AIK_HP_SIGNAL_EC11_PRESS:
            return (right != 0) ?
                   aik_spi_half_bit_down(right, AIK_RIGHT_LOCAL_BIT_EC11_MUTE) : 0U;
        default:
            return 0U;
    }
}

static uint8_t half_key_down(const aik_spi_half_state_v1_t *half,
                             uint8_t key_id)
{
    if(half == 0)
    {
        return 0U;
    }
    return (uint8_t)((half->down_bits[key_id >> 3] >> (key_id & 7U)) & 1U);
}

static void nkro16_set_usage(uint8_t nkro16[AIK_NKRO_REPORT_BYTES],
                             uint8_t usage)
{
    if((nkro16 != 0) && (usage >= HID_USAGE_A))
    {
        uint8_t bit_index = (uint8_t)(usage - HID_USAGE_A);
        uint8_t byte_index = (uint8_t)(2U + (bit_index >> 3));
        uint8_t bit_mask = (uint8_t)(1U << (bit_index & 7U));

        if(byte_index < AIK_NKRO_REPORT_BYTES)
        {
            nkro16[byte_index] |= bit_mask;
        }
    }
}

static uint8_t global_key_down(const aik_spi_half_state_v1_t *left,
                               const aik_spi_half_state_v1_t *right,
                               uint8_t key_id)
{
    if(key_id < AIK_KEY_COUNT_RIGHT)
    {
        return half_key_down(right, key_id);
    }

    return half_key_down(left, (uint8_t)(key_id - AIK_KEY_COUNT_RIGHT));
}

static uint8_t s_release_gate[CH585_GLOBAL_DOWN_BYTES];

void ch585_half_report_arm_release_gate(const aik_spi_half_state_v1_t *left,
                                        const aik_spi_half_state_v1_t *right)
{
    uint8_t key_id;

    memset(s_release_gate, 0, sizeof(s_release_gate));
    for(key_id = 0U; key_id < AIK_KEY_COUNT_TOTAL; key_id++)
    {
        if(global_key_down(left, right, key_id) != 0U)
        {
            s_release_gate[key_id >> 3] |= (uint8_t)(1U << (key_id & 7U));
        }
    }
}

static uint8_t fn_consumed_get(const uint8_t consumed[CH585_GLOBAL_DOWN_BYTES],
                               uint8_t key_id)
{
    return (uint8_t)((consumed[key_id >> 3] >> (key_id & 7U)) & 1U);
}

static void fn_consumed_set(uint8_t consumed[CH585_GLOBAL_DOWN_BYTES],
                            uint8_t key_id)
{
    consumed[key_id >> 3] |= (uint8_t)(1U << (key_id & 7U));
}

static void fn_consumed_clear(uint8_t consumed[CH585_GLOBAL_DOWN_BYTES],
                              uint8_t key_id)
{
    consumed[key_id >> 3] &= (uint8_t)~(uint8_t)(1U << (key_id & 7U));
}

static void update_fn_consumed(const aik_spi_half_state_v1_t *left,
                               const aik_spi_half_state_v1_t *right,
                               uint8_t consumed[CH585_GLOBAL_DOWN_BYTES])
{
    uint8_t key_id;
    uint8_t fn_down = global_key_down(left, right, CH585_FN_LAYER_KEY);

    for(key_id = 0U; key_id < AIK_KEY_COUNT_TOTAL; key_id++)
    {
        if(global_key_down(left, right, key_id) == 0U)
        {
            fn_consumed_clear(consumed, key_id);
        }
    }

    if(fn_down != 0U)
    {
        for(key_id = 0U; key_id < AIK_KEY_COUNT_TOTAL; key_id++)
        {
            if(global_key_down(left, right, key_id) != 0U)
            {
                fn_consumed_set(consumed, key_id);
            }
        }
    }
}

static void apply_local_keyboard_controls(const aik_spi_half_state_v1_t *left,
                                          const aik_spi_half_state_v1_t *right,
                                          uint8_t nkro16[AIK_NKRO_REPORT_BYTES])
{
    uint8_t signal_id;

    for(signal_id = 0U; signal_id < CH585_LOCAL_SIGNAL_COUNT; signal_id++)
    {
        const ch585_local_binding_t *binding = &s_locals[signal_id];

        if(binding->target_kind != AIK_HP_TARGET_KEYBOARD)
        {
            continue;
        }
        if(local_signal_down(signal_id, left, right) == 0U)
        {
            continue;
        }
        nkro16[0] |= (uint8_t)((binding->value >> 8) & 0xFFU);
        nkro16_set_usage(nkro16, (uint8_t)(binding->value & 0xFFU));
    }
}

void ch585_half_report_build_nkro16(const aik_spi_half_state_v1_t *left,
                                    const aik_spi_half_state_v1_t *right,
                                    uint8_t nkro16[AIK_NKRO_REPORT_BYTES])
{
    static uint8_t fn_consumed[CH585_GLOBAL_DOWN_BYTES];
    uint8_t key_id;

    if(nkro16 == 0)
    {
        return;
    }

    memset(nkro16, 0, AIK_NKRO_REPORT_BYTES);
    half_report_ensure_tables();
    update_fn_consumed(left, right, fn_consumed);
    for(key_id = 0U; key_id < AIK_KEY_COUNT_TOTAL; key_id++)
    {
        const ch585_key_output_t *output = &s_key_outputs[key_id];
        uint8_t gate_bit = (uint8_t)(1U << (key_id & 7U));

        if(global_key_down(left, right, key_id) == 0U)
        {
            s_release_gate[key_id >> 3] &= (uint8_t)~gate_bit;
            continue;
        }

        if((s_release_gate[key_id >> 3] & gate_bit) != 0U)
        {
            continue;
        }

        if(fn_consumed_get(fn_consumed, key_id) != 0U)
        {
            continue;
        }

        if(output->modifier_mask != 0U)
        {
            nkro16[0] |= output->modifier_mask;
        }

        nkro16_set_usage(nkro16, output->usage);
    }

    apply_local_keyboard_controls(left, right, nkro16);
}

uint16_t ch585_half_report_consumer_usage(const aik_spi_half_state_v1_t *left,
                                          const aik_spi_half_state_v1_t *right)
{
    static const uint8_t priority[3] = {
        AIK_HP_SIGNAL_EC11_PRESS,
        AIK_HP_SIGNAL_EC11_CW,
        AIK_HP_SIGNAL_EC11_CCW
    };
    uint8_t i;

    half_report_ensure_tables();
    for(i = 0U; i < 3U; i++)
    {
        const ch585_local_binding_t *binding = &s_locals[priority[i]];

        if((binding->target_kind == AIK_HP_TARGET_CONSUMER) &&
           (local_signal_down(priority[i], left, right) != 0U))
        {
            return binding->value;
        }
    }
    return AIK_CONSUMER_USAGE_NONE;
}

int8_t ch585_half_report_mouse_wheel(const aik_spi_half_state_v1_t *left,
                                     const aik_spi_half_state_v1_t *right)
{
    static const uint8_t priority[2] = {
        AIK_HP_SIGNAL_WHEEL_UP,
        AIK_HP_SIGNAL_WHEEL_DOWN
    };
    uint8_t i;

    half_report_ensure_tables();
    for(i = 0U; i < 2U; i++)
    {
        const ch585_local_binding_t *binding = &s_locals[priority[i]];

        if((binding->target_kind == AIK_HP_TARGET_MOUSE_WHEEL) &&
           (local_signal_down(priority[i], left, right) != 0U))
        {
            return (int8_t)(binding->value & 0xFFU);
        }
    }
    return 0;
}
