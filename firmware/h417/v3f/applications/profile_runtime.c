#include "profile_runtime.h"

#include <string.h>

#include "factory_profile_image.h"
#include "profile_store.h"

static v3f_profile_runtime_t s_runtime;
static uint8_t s_rearm_pending;
static uint8_t s_rearm_mask[V3F_GLOBAL_DOWN_BYTES];

/* ------------------------------------------------------------------ */
/* AKPK package validation                                            */
/* ------------------------------------------------------------------ */

static uint32_t package_crc(const uint8_t *pkg, uint32_t total)
{
    static const uint8_t zeros[4] = {0U, 0U, 0U, 0U};
    uint32_t crc_field = offsetof(aik_pkg_header_t, package_crc32c);
    uint32_t crc;

    crc = aik_crc32c(0U, pkg, crc_field);
    crc = aik_crc32c(crc, zeros, 4U);
    crc = aik_crc32c(crc, pkg + crc_field + 4U,
                     total - crc_field - 4U);
    return crc;
}

int v3f_profile_package_validate(const uint8_t *pkg, uint32_t buf_limit,
                                 uint32_t *out_total_len)
{
    const aik_pkg_header_t *hdr = (const aik_pkg_header_t *)pkg;
    const aik_pkg_section_entry_t *dir;
    uint32_t dir_end;
    uint16_t i;

    if((pkg == 0) || (buf_limit < AIK_PKG_HEADER_SIZE))
    {
        return V3F_PROFILE_ERR_PARAM;
    }
    if((hdr->magic[0] != AIK_PKG_MAGIC0) ||
       (hdr->magic[1] != AIK_PKG_MAGIC1) ||
       (hdr->magic[2] != AIK_PKG_MAGIC2) ||
       (hdr->magic[3] != AIK_PKG_MAGIC3) ||
       (hdr->package_version != AIK_PKG_VERSION) ||
       (hdr->header_size != AIK_PKG_HEADER_SIZE))
    {
        return V3F_PROFILE_ERR_PACKAGE;
    }
    if((hdr->total_size < AIK_PKG_HEADER_SIZE) ||
       (hdr->total_size > buf_limit))
    {
        return V3F_PROFILE_ERR_PACKAGE;
    }

    dir_end = AIK_PKG_HEADER_SIZE +
              (uint32_t)hdr->section_count * sizeof(aik_pkg_section_entry_t);
    if(dir_end > hdr->total_size)
    {
        return V3F_PROFILE_ERR_PACKAGE;
    }

    if(package_crc(pkg, hdr->total_size) != hdr->package_crc32c)
    {
        return V3F_PROFILE_ERR_PACKAGE;
    }

    dir = (const aik_pkg_section_entry_t *)(pkg + AIK_PKG_HEADER_SIZE);
    for(i = 0U; i < hdr->section_count; i++)
    {
        if(dir[i].length == 0U)
        {
            continue;
        }
        if((dir[i].offset < dir_end) ||
           ((dir[i].offset + dir[i].length) > hdr->total_size) ||
           ((dir[i].offset & 3U) != 0U))
        {
            return V3F_PROFILE_ERR_PACKAGE;
        }
        if(aik_crc32c(0U, pkg + dir[i].offset, dir[i].length) !=
           dir[i].section_crc32c)
        {
            return V3F_PROFILE_ERR_PACKAGE;
        }
    }

    if(out_total_len != 0)
    {
        *out_total_len = hdr->total_size;
    }
    return V3F_PROFILE_OK;
}

static const uint8_t *package_find_section(const uint8_t *pkg,
                                           uint16_t kind,
                                           uint32_t *out_len)
{
    const aik_pkg_header_t *hdr = (const aik_pkg_header_t *)pkg;
    const aik_pkg_section_entry_t *dir =
        (const aik_pkg_section_entry_t *)(pkg + AIK_PKG_HEADER_SIZE);
    uint16_t i;

    for(i = 0U; i < hdr->section_count; i++)
    {
        if((dir[i].section_kind == kind) && (dir[i].length != 0U))
        {
            if(out_len != 0)
            {
                *out_len = dir[i].length;
            }
            return pkg + dir[i].offset;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* AKRT runtime table parsing                                         */
/* ------------------------------------------------------------------ */

typedef struct
{
    const uint8_t *payload;
    uint32_t entry_count;
    uint16_t entry_size;
} rt_section_t;

static uint32_t table_crc(const uint8_t *table, uint32_t total)
{
    static const uint8_t zeros[4] = {0U, 0U, 0U, 0U};
    uint32_t crc_field = offsetof(aik_rt_header_t, table_crc32c);
    uint32_t crc;

    crc = aik_crc32c(0U, table, crc_field);
    crc = aik_crc32c(crc, zeros, 4U);
    crc = aik_crc32c(crc, table + crc_field + 4U,
                     total - crc_field - 4U);
    return crc;
}

static int rt_locate_section(const uint8_t *table, uint32_t table_len,
                             uint16_t kind, uint16_t expected_entry_size,
                             rt_section_t *out)
{
    const aik_rt_header_t *hdr = (const aik_rt_header_t *)table;
    const aik_rt_section_entry_t *dir =
        (const aik_rt_section_entry_t *)(table + AIK_RT_HEADER_SIZE);
    uint16_t i;

    out->payload = 0;
    out->entry_count = 0U;
    out->entry_size = 0U;

    for(i = 0U; i < hdr->section_count; i++)
    {
        if(dir[i].section_kind != kind)
        {
            continue;
        }
        if(dir[i].entry_count == 0U)
        {
            return V3F_PROFILE_OK;
        }
        if((dir[i].length != (uint32_t)dir[i].entry_size *
                             dir[i].entry_count) ||
           ((expected_entry_size != 0U) &&
            (dir[i].entry_size != expected_entry_size)) ||
           ((dir[i].offset + dir[i].length) > table_len))
        {
            return V3F_PROFILE_ERR_TABLE;
        }
        out->payload = table + dir[i].offset;
        out->entry_count = dir[i].entry_count;
        out->entry_size = dir[i].entry_size;
        return V3F_PROFILE_OK;
    }
    return V3F_PROFILE_OK;
}

static const aik_rt_behavior_entry_t *rt_behavior(const rt_section_t *behaviors,
                                                  uint16_t index)
{
    if((behaviors->payload == 0) || (index >= behaviors->entry_count))
    {
        return 0;
    }
    return (const aik_rt_behavior_entry_t *)
           (behaviors->payload + (uint32_t)index * behaviors->entry_size);
}

static uint8_t local_signal_from_dispatch(uint16_t control_index,
                                          uint16_t event_code,
                                          uint8_t *out_signal)
{
    if(control_index == AIK_RT_CONTROL_INDEX_FIVEWAY)
    {
        switch(event_code)
        {
            case V3F_RT_EVENT_FW_UP: *out_signal = AIK_HP_SIGNAL_FIVEWAY_UP; return 1U;
            case V3F_RT_EVENT_FW_DOWN: *out_signal = AIK_HP_SIGNAL_FIVEWAY_DOWN; return 1U;
            case V3F_RT_EVENT_FW_LEFT: *out_signal = AIK_HP_SIGNAL_FIVEWAY_LEFT; return 1U;
            case V3F_RT_EVENT_FW_RIGHT: *out_signal = AIK_HP_SIGNAL_FIVEWAY_RIGHT; return 1U;
            case AIK_RT_EVENT_PRESS: *out_signal = AIK_HP_SIGNAL_FIVEWAY_PRESS; return 1U;
            case AIK_RT_EVENT_CW_STEP: *out_signal = AIK_HP_SIGNAL_WHEEL_UP; return 1U;
            case AIK_RT_EVENT_CCW_STEP: *out_signal = AIK_HP_SIGNAL_WHEEL_DOWN; return 1U;
            default: return 0U;
        }
    }
    if(control_index == AIK_RT_CONTROL_INDEX_ENC)
    {
        switch(event_code)
        {
            case AIK_RT_EVENT_PRESS: *out_signal = AIK_HP_SIGNAL_EC11_PRESS; return 1U;
            case AIK_RT_EVENT_CW_STEP: *out_signal = AIK_HP_SIGNAL_EC11_CW; return 1U;
            case AIK_RT_EVENT_CCW_STEP: *out_signal = AIK_HP_SIGNAL_EC11_CCW; return 1U;
            default: return 0U;
        }
    }
    return 0U;
}

static uint8_t local_target_from_behavior(const aik_rt_behavior_entry_t *bh,
                                          uint8_t *out_kind,
                                          uint16_t *out_value)
{
    if((bh == 0) || (bh->behavior_kind != AIK_RT_BEHAVIOR_HOST_INPUT))
    {
        return 0U;
    }
    switch(bh->data0)
    {
        case AIK_RT_HOST_USAGE_KEYBOARD:
            *out_kind = AIK_HP_TARGET_KEYBOARD;
            *out_value = (uint16_t)(bh->data1 & 0xFFFFU);
            return 1U;
        case AIK_RT_HOST_USAGE_CONSUMER:
            *out_kind = AIK_HP_TARGET_CONSUMER;
            *out_value = (uint16_t)(bh->data1 & 0xFFFFU);
            return 1U;
        case AIK_RT_HOST_USAGE_MOUSE_AXIS:
            if((bh->data1 & 0xFFU) == AIK_RT_MOUSE_AXIS_WHEEL)
            {
                *out_kind = AIK_HP_TARGET_MOUSE_WHEEL;
                *out_value = (uint16_t)((bh->data1 >> 8) & 0xFFU);
                return 1U;
            }
            return 0U;
        default:
            return 0U;
    }
}

static void runtime_add_local(v3f_profile_runtime_t *rt, uint8_t signal_id,
                              uint8_t target_kind, uint16_t value)
{
    if(rt->local_count >= V3F_PROFILE_LOCAL_MAX)
    {
        return;
    }
    rt->locals[rt->local_count].signal_id = signal_id;
    rt->locals[rt->local_count].target_kind = target_kind;
    rt->locals[rt->local_count].value = value;
    rt->local_count++;
}

int v3f_profile_runtime_install_package(const uint8_t *pkg,
                                        uint32_t buf_limit,
                                        uint8_t slot_id)
{
    const aik_pkg_header_t *pkg_hdr = (const aik_pkg_header_t *)pkg;
    const aik_rt_header_t *rt_hdr;
    const uint8_t *table;
    uint32_t table_len = 0U;
    rt_section_t scopes, dispatch, behaviors, triggers, params;
    v3f_profile_runtime_t next;
    uint16_t base_scope = AIK_RT_INVALID_INDEX;
    uint16_t overlay_scope = AIK_RT_INVALID_INDEX;
    uint32_t i;
    int status;

    status = v3f_profile_package_validate(pkg, buf_limit, 0);
    if(status != V3F_PROFILE_OK)
    {
        return status;
    }

    table = package_find_section(pkg, AIK_PKG_SECTION_RUNTIME_TABLE_CACHE,
                                 &table_len);
    if((table == 0) || (table_len < AIK_RT_HEADER_SIZE))
    {
        return V3F_PROFILE_ERR_TABLE;
    }

    rt_hdr = (const aik_rt_header_t *)table;
    if((rt_hdr->magic[0] != AIK_RT_MAGIC0) ||
       (rt_hdr->magic[1] != AIK_RT_MAGIC1) ||
       (rt_hdr->magic[2] != AIK_RT_MAGIC2) ||
       (rt_hdr->magic[3] != AIK_RT_MAGIC3) ||
       (rt_hdr->runtime_table_version != AIK_RT_TABLE_VERSION) ||
       (rt_hdr->runtime_abi_version != AIK_RT_ABI_VERSION) ||
       (rt_hdr->header_size != AIK_RT_HEADER_SIZE) ||
       (rt_hdr->section_count != AIK_RT_SECTION_COUNT) ||
       (rt_hdr->index_width != AIK_RT_INDEX_WIDTH) ||
       (rt_hdr->total_size < (AIK_RT_HEADER_SIZE +
                              AIK_RT_SECTION_COUNT *
                              sizeof(aik_rt_section_entry_t))) ||
       (rt_hdr->total_size > table_len))
    {
        return V3F_PROFILE_ERR_TABLE;
    }
    if(table_crc(table, rt_hdr->total_size) != rt_hdr->table_crc32c)
    {
        return V3F_PROFILE_ERR_TABLE;
    }

    if((rt_locate_section(table, rt_hdr->total_size,
                          AIK_RT_SECTION_SCOPE_TABLE,
                          (uint16_t)sizeof(aik_rt_scope_entry_t),
                          &scopes) != V3F_PROFILE_OK) ||
       (rt_locate_section(table, rt_hdr->total_size,
                          AIK_RT_SECTION_DISPATCH_TABLE,
                          (uint16_t)sizeof(aik_rt_dispatch_entry_t),
                          &dispatch) != V3F_PROFILE_OK) ||
       (rt_locate_section(table, rt_hdr->total_size,
                          AIK_RT_SECTION_BEHAVIOR_TABLE,
                          (uint16_t)sizeof(aik_rt_behavior_entry_t),
                          &behaviors) != V3F_PROFILE_OK) ||
       (rt_locate_section(table, rt_hdr->total_size,
                          AIK_RT_SECTION_TRIGGER_TABLE,
                          (uint16_t)sizeof(aik_rt_trigger_entry_t),
                          &triggers) != V3F_PROFILE_OK) ||
       (rt_locate_section(table, rt_hdr->total_size,
                          AIK_RT_SECTION_MUTABLE_PARAM_SLOTS,
                          (uint16_t)sizeof(aik_rt_mutable_param_slot_entry_t),
                          &params) != V3F_PROFILE_OK))
    {
        return V3F_PROFILE_ERR_TABLE;
    }

    if((scopes.payload == 0) || (dispatch.payload == 0))
    {
        return V3F_PROFILE_ERR_TABLE;
    }

    memset(&next, 0, sizeof(next));
    next.fn_hold_key = 0xFFU;
    next.active_slot = slot_id;
    next.revision = pkg_hdr->revision;
    next.generation16 = (uint16_t)(pkg_hdr->revision & 0xFFFFU);
    next.profile_id16 = (uint16_t)(pkg_hdr->profile_id_hash & 0xFFFFU);

    for(i = 0U; i < AIK_KEY_COUNT_TOTAL; i++)
    {
        next.triggers[i].press_pm = 450U;
        next.triggers[i].release_pm = 350U;
        next.triggers[i].rt_press_delta_pm = 300U;
        next.triggers[i].rt_release_delta_pm = 300U;
        next.triggers[i].mode = AIK_RT_MODE_RAPID_TRIGGER;
        next.triggers[i].filter_shift = 0U;
    }

    for(i = 0U; i < scopes.entry_count; i++)
    {
        const aik_rt_scope_entry_t *scope =
            (const aik_rt_scope_entry_t *)
            (scopes.payload + i * scopes.entry_size);

        if(scope->base_scope != 0U)
        {
            base_scope = scope->scope_index;
        }
        else if(overlay_scope == AIK_RT_INVALID_INDEX)
        {
            overlay_scope = scope->scope_index;
        }
    }
    if(base_scope == AIK_RT_INVALID_INDEX)
    {
        return V3F_PROFILE_ERR_TABLE;
    }

    for(i = 0U; i < triggers.entry_count; i++)
    {
        const aik_rt_trigger_entry_t *trig =
            (const aik_rt_trigger_entry_t *)
            (triggers.payload + i * triggers.entry_size);

        if(trig->control_index < AIK_KEY_COUNT_TOTAL)
        {
            next.triggers[trig->control_index].mode = trig->mode;
        }
    }

    for(i = 0U; i < params.entry_count; i++)
    {
        const aik_rt_mutable_param_slot_entry_t *param =
            (const aik_rt_mutable_param_slot_entry_t *)
            (params.payload + i * params.entry_size);
        const aik_rt_trigger_entry_t *trig;
        aik_hp_trigger_entry_t *dst;
        uint16_t value;

        if((param->owner_section_kind != AIK_RT_SECTION_TRIGGER_TABLE) ||
           (triggers.payload == 0) ||
           (param->owner_entry_index >= triggers.entry_count))
        {
            continue;
        }
        trig = (const aik_rt_trigger_entry_t *)
               (triggers.payload +
                (uint32_t)param->owner_entry_index * triggers.entry_size);
        if(trig->control_index >= AIK_KEY_COUNT_TOTAL)
        {
            continue;
        }
        dst = &next.triggers[trig->control_index];
        value = (param->initial_value < 0) ? 0U :
                ((param->initial_value > 0xFFFF) ? 0xFFFFU :
                 (uint16_t)param->initial_value);

        switch(param->param_id)
        {
            case AIK_RT_PARAM_PRESS_THRESHOLD_NORM:
                dst->press_pm = value;
                break;
            case AIK_RT_PARAM_RELEASE_THRESHOLD_NORM:
                if(dst->mode != AIK_RT_MODE_RAPID_TRIGGER)
                {
                    dst->release_pm = value;
                }
                break;
            case AIK_RT_PARAM_RESET_THRESHOLD_NORM:
                if(dst->mode == AIK_RT_MODE_RAPID_TRIGGER)
                {
                    dst->release_pm = value;
                }
                break;
            case AIK_RT_PARAM_PRESS_DELTA_NORM:
                dst->rt_press_delta_pm = value;
                break;
            case AIK_RT_PARAM_RELEASE_DELTA_NORM:
                dst->rt_release_delta_pm = value;
                break;
            case AIK_RT_PARAM_FILTER_SHIFT:
                dst->filter_shift = (uint8_t)(value & 0xFFU);
                break;
            default:
                break;
        }
    }

    for(i = 0U; i < dispatch.entry_count; i++)
    {
        const aik_rt_dispatch_entry_t *entry =
            (const aik_rt_dispatch_entry_t *)
            (dispatch.payload + i * dispatch.entry_size);
        const aik_rt_behavior_entry_t *bh;
        uint8_t signal_id;
        uint8_t target_kind;
        uint16_t target_value;

        if((entry->signal_source_kind != AIK_RT_SIGNAL_SOURCE_CONTROL) ||
           (entry->result_kind != AIK_RT_RESULT_BEHAVIOR))
        {
            continue;
        }

        bh = rt_behavior(&behaviors, entry->behavior_index);
        if(bh == 0)
        {
            continue;
        }

        if(entry->source_index < AIK_KEY_COUNT_TOTAL)
        {
            aik_hp_key_output_t *dst;

            if((entry->signal_event_code != AIK_RT_EVENT_CONTROL_LEVEL) &&
               (entry->signal_event_code != AIK_RT_EVENT_PRESS))
            {
                continue;
            }

            if(entry->scope_index == base_scope)
            {
                dst = &next.base_keys[entry->source_index];
            }
            else if((overlay_scope != AIK_RT_INVALID_INDEX) &&
                    (entry->scope_index == overlay_scope))
            {
                dst = &next.fn_keys[entry->source_index];
            }
            else
            {
                continue;
            }

            if((bh->behavior_kind == AIK_RT_BEHAVIOR_HOST_INPUT) &&
               (bh->data0 == AIK_RT_HOST_USAGE_KEYBOARD))
            {
                dst->usage = (uint8_t)(bh->data1 & 0xFFU);
                dst->modifier_mask = (uint8_t)((bh->data1 >> 8) & 0xFFU);
            }
            else if((bh->behavior_kind == AIK_RT_BEHAVIOR_OVERLAY_CONTROL) &&
                    (entry->scope_index == base_scope) &&
                    (overlay_scope != AIK_RT_INVALID_INDEX) &&
                    (bh->data0 == overlay_scope))
            {
                next.has_fn_overlay = 1U;
                next.fn_hold_key = (uint8_t)entry->source_index;
            }
        }
        else if(entry->scope_index == base_scope)
        {
            if(local_signal_from_dispatch(entry->source_index,
                                          entry->signal_event_code,
                                          &signal_id) == 0U)
            {
                continue;
            }
            if(local_target_from_behavior(bh, &target_kind,
                                          &target_value) == 0U)
            {
                continue;
            }
            runtime_add_local(&next, signal_id, target_kind, target_value);
        }
    }

    next.valid = 1U;
    s_runtime = next;
    s_rearm_pending = 1U;
    return V3F_PROFILE_OK;
}

uint8_t v3f_profile_runtime_rearm_take(void)
{
    uint8_t pending = s_rearm_pending;

    s_rearm_pending = 0U;
    return pending;
}

void v3f_profile_runtime_rearm_latch(const v3f_global_key_state_t *keys)
{
    if(keys != 0)
    {
        memcpy(s_rearm_mask, keys->down, sizeof(s_rearm_mask));
    }
}

/* ------------------------------------------------------------------ */
/* Boot-time load                                                     */
/* ------------------------------------------------------------------ */

uint8_t v3f_profile_runtime_init(void)
{
    uint8_t active = v3f_profile_store_get_active_slot();

    if((active >= AIK_PROFILE_USER_SLOT_FIRST) &&
       (active < AIK_PROFILE_SLOT_COUNT_TOTAL))
    {
        const uint8_t *slot = v3f_profile_store_slot_ptr(active);

        if((slot != 0) &&
           (v3f_profile_runtime_install_package(slot,
                                                V3F_PROFILE_SLOT_SIZE,
                                                active) == V3F_PROFILE_OK))
        {
            return active;
        }
    }

    if(v3f_profile_runtime_install_package(g_v3f_factory_profile_image,
                                           g_v3f_factory_profile_image_size,
                                           AIK_PROFILE_SLOT_FACTORY) ==
       V3F_PROFILE_OK)
    {
        return AIK_PROFILE_SLOT_FACTORY;
    }

    memset(&s_runtime, 0, sizeof(s_runtime));
    return 0xFFU;
}

const v3f_profile_runtime_t *v3f_profile_runtime_get(void)
{
    return &s_runtime;
}

uint8_t v3f_profile_runtime_valid(void)
{
    return s_runtime.valid;
}

/* ------------------------------------------------------------------ */
/* Report path                                                        */
/* ------------------------------------------------------------------ */

static void nkro16_set_usage(uint8_t nkro16[AIK_NKRO_REPORT_BYTES],
                             uint8_t usage)
{
    if(usage >= 0x04U)
    {
        uint8_t bit_index = (uint8_t)(usage - 0x04U);
        uint8_t byte_index = (uint8_t)(2U + (bit_index >> 3));
        uint8_t bit_mask = (uint8_t)(1U << (bit_index & 7U));

        if(byte_index < AIK_NKRO_REPORT_BYTES)
        {
            nkro16[byte_index] |= bit_mask;
        }
    }
}

void v3f_profile_runtime_build_nkro16(const v3f_global_key_state_t *keys,
                                      uint8_t nkro16[AIK_NKRO_REPORT_BYTES])
{
    uint8_t key_id;
    uint8_t fn_active = 0U;

    memset(nkro16, 0, AIK_NKRO_REPORT_BYTES);
    if((keys == 0) || (s_runtime.valid == 0U))
    {
        return;
    }

    if((s_runtime.has_fn_overlay != 0U) &&
       (s_runtime.fn_hold_key < AIK_KEY_COUNT_TOTAL) &&
       (v3f_global_key_is_down(keys, s_runtime.fn_hold_key) != 0U))
    {
        fn_active = 1U;
    }

    for(key_id = 0U; key_id < AIK_KEY_COUNT_TOTAL; key_id++)
    {
        const aik_hp_key_output_t *output = &s_runtime.base_keys[key_id];
        uint8_t rearm_bit = (uint8_t)(1U << (key_id & 7U));

        if(v3f_global_key_is_down(keys, key_id) == 0U)
        {
            s_rearm_mask[key_id >> 3] &= (uint8_t)~rearm_bit;
            continue;
        }
        if((s_rearm_mask[key_id >> 3] & rearm_bit) != 0U)
        {
            /* Held across a profile switch: stays neutral until the
             * key is physically released. */
            continue;
        }
        if((fn_active != 0U) && (key_id == s_runtime.fn_hold_key))
        {
            continue;
        }
        if(fn_active != 0U)
        {
            const aik_hp_key_output_t *fn_output =
                &s_runtime.fn_keys[key_id];

            if((fn_output->usage != 0U) || (fn_output->modifier_mask != 0U))
            {
                output = fn_output;
            }
        }

        if(output->modifier_mask != 0U)
        {
            nkro16[0] |= output->modifier_mask;
        }
        nkro16_set_usage(nkro16, output->usage);
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

void v3f_profile_runtime_apply_local_keyboard(
    const aik_spi_half_state_v1_t *left,
    uint8_t nkro16[AIK_NKRO_REPORT_BYTES])
{
    uint8_t i;

    if(s_runtime.valid == 0U)
    {
        return;
    }
    for(i = 0U; i < s_runtime.local_count; i++)
    {
        const aik_hp_local_entry_t *entry = &s_runtime.locals[i];

        if(entry->target_kind != AIK_HP_TARGET_KEYBOARD)
        {
            continue;
        }
        if(local_signal_down(entry->signal_id, left, 0) == 0U)
        {
            continue;
        }
        nkro16[0] |= (uint8_t)((entry->value >> 8) & 0xFFU);
        nkro16_set_usage(nkro16, (uint8_t)(entry->value & 0xFFU));
    }
}

static const aik_hp_local_entry_t *local_find(uint8_t signal_id,
                                              uint8_t target_kind)
{
    uint8_t i;

    for(i = 0U; i < s_runtime.local_count; i++)
    {
        if((s_runtime.locals[i].signal_id == signal_id) &&
           (s_runtime.locals[i].target_kind == target_kind))
        {
            return &s_runtime.locals[i];
        }
    }
    return 0;
}

uint16_t v3f_profile_runtime_consumer_usage(
    const aik_spi_half_state_v1_t *right)
{
    static const uint8_t priority[3] = {
        AIK_HP_SIGNAL_EC11_PRESS,
        AIK_HP_SIGNAL_EC11_CW,
        AIK_HP_SIGNAL_EC11_CCW
    };
    uint8_t i;

    if(s_runtime.valid == 0U)
    {
        return AIK_CONSUMER_USAGE_NONE;
    }
    for(i = 0U; i < 3U; i++)
    {
        const aik_hp_local_entry_t *entry =
            local_find(priority[i], AIK_HP_TARGET_CONSUMER);

        if((entry != 0) &&
           (local_signal_down(priority[i], 0, right) != 0U))
        {
            return entry->value;
        }
    }
    return AIK_CONSUMER_USAGE_NONE;
}

int8_t v3f_profile_runtime_mouse_wheel(const aik_spi_half_state_v1_t *left)
{
    static const uint8_t priority[2] = {
        AIK_HP_SIGNAL_WHEEL_UP,
        AIK_HP_SIGNAL_WHEEL_DOWN
    };
    uint8_t i;

    if(s_runtime.valid == 0U)
    {
        return 0;
    }
    for(i = 0U; i < 2U; i++)
    {
        const aik_hp_local_entry_t *entry =
            local_find(priority[i], AIK_HP_TARGET_MOUSE_WHEEL);

        if((entry != 0) &&
           (local_signal_down(priority[i], left, 0) != 0U))
        {
            return (int8_t)(entry->value & 0xFFU);
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* AKHR half patch derivation                                         */
/* ------------------------------------------------------------------ */

uint16_t v3f_profile_runtime_build_half_patch(uint8_t half_id,
                                              uint8_t *out,
                                              uint16_t out_max)
{
    aik_hp_header_t hdr;
    uint8_t key_count;
    uint8_t local_count = 0U;
    uint16_t offset;
    uint16_t i;

    if((out == 0) || (s_runtime.valid == 0U) ||
       ((half_id != AIK_HALF_ID_LEFT) && (half_id != AIK_HALF_ID_RIGHT)))
    {
        return 0U;
    }

    key_count = aik_spi_half_key_count(half_id);

    memset(&hdr, 0, sizeof(hdr));
    hdr.magic[0] = AIK_HP_MAGIC0;
    hdr.magic[1] = AIK_HP_MAGIC1;
    hdr.magic[2] = AIK_HP_MAGIC2;
    hdr.magic[3] = AIK_HP_MAGIC3;
    hdr.version = AIK_HP_VERSION;
    hdr.half_id = half_id;
    hdr.key_count = key_count;
    hdr.profile_id16 = s_runtime.profile_id16;
    hdr.generation16 = s_runtime.generation16;

    offset = (uint16_t)sizeof(aik_hp_header_t);
    hdr.trigger_offset = offset;
    offset = (uint16_t)(offset + (uint16_t)key_count *
                        (uint16_t)sizeof(aik_hp_trigger_entry_t));

    if(half_id == AIK_HALF_ID_LEFT)
    {
        hdr.flags |= AIK_HP_FLAG_HAS_DISPATCH77;
        hdr.dispatch_offset = offset;
        offset = (uint16_t)(offset + AIK_KEY_COUNT_TOTAL *
                            (uint16_t)sizeof(aik_hp_key_output_t));
    }

    /* The left half composes full wireless reports, so it receives every
     * local binding (including the right-half EC11). The right half only
     * publishes raw bits and needs none. */
    hdr.local_offset = offset;
    if(half_id == AIK_HALF_ID_LEFT)
    {
        local_count = s_runtime.local_count;
    }
    hdr.local_count = local_count;
    offset = (uint16_t)(offset + (uint16_t)local_count *
                        (uint16_t)sizeof(aik_hp_local_entry_t));
    hdr.total_len = offset;

    if((offset > out_max) || (offset > AIK_HP_MAX_SIZE))
    {
        return 0U;
    }

    memcpy(out, &hdr, sizeof(hdr));

    for(i = 0U; i < key_count; i++)
    {
        /* Global key id: right half maps to 0..40, left to 41..76. */
        uint8_t global_id = (half_id == AIK_HALF_ID_RIGHT) ?
                            (uint8_t)i :
                            (uint8_t)(AIK_KEY_COUNT_RIGHT + i);

        memcpy(out + hdr.trigger_offset +
               (uint16_t)i * sizeof(aik_hp_trigger_entry_t),
               &s_runtime.triggers[global_id],
               sizeof(aik_hp_trigger_entry_t));
    }

    if(half_id == AIK_HALF_ID_LEFT)
    {
        memcpy(out + hdr.dispatch_offset, s_runtime.base_keys,
               AIK_KEY_COUNT_TOTAL * sizeof(aik_hp_key_output_t));
    }

    if(local_count != 0U)
    {
        memcpy(out + hdr.local_offset, s_runtime.locals,
               (uint16_t)local_count * sizeof(aik_hp_local_entry_t));
    }

    {
        aik_hp_header_t *out_hdr = (aik_hp_header_t *)out;

        out_hdr->crc16 = aik_hp_crc(out, hdr.total_len);
    }

    return hdr.total_len;
}
