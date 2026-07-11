#ifndef AIK_PROFILE_FORMAT_H
#define AIK_PROFILE_FORMAT_H

/*
 * Shared profile container formats for the AI keyboard.
 *
 * Wire/storage layouts follow docs/architecture/keyboard_config_site:
 *   - profile_package.html  -> ProfilePackageBinary v1 ("AKPK")
 *   - runtime_contract.html -> RuntimeTableBinary v1   ("AKRT")
 * The half patch ("AKHR") is this firmware's compact derivation pushed from
 * the H417 to each CH585 half over SPI and persisted in CH585 Data-Flash.
 *
 * All multi-byte fields are little-endian. AKPK/AKRT integrity uses
 * CRC-32C (Castagnoli); AKHR and SPI frames use CRC16-CCITT from
 * aik_spi_protocol.h.
 */

#include <stdint.h>
#include <stddef.h>

#include "aik_spi_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__)
#define AIK_PROFILE_PACKED __attribute__((packed))
#else
#define AIK_PROFILE_PACKED
#endif

/* ------------------------------------------------------------------ */
/* Slot conventions (shared by H417 and both CH585 halves)            */
/* ------------------------------------------------------------------ */

#define AIK_PROFILE_SLOT_FACTORY      0U
#define AIK_PROFILE_USER_SLOT_FIRST   1U
#define AIK_PROFILE_USER_SLOT_COUNT   3U
#define AIK_PROFILE_SLOT_COUNT_TOTAL  (AIK_PROFILE_USER_SLOT_FIRST + \
                                       AIK_PROFILE_USER_SLOT_COUNT)

#define AIK_PROFILE_SCHEMA_VERSION    1U

/* ------------------------------------------------------------------ */
/* CRC-32C / Castagnoli (poly 0x1EDC6F41, reflected 0x82F63B78,       */
/* init 0xFFFFFFFF, xorout 0xFFFFFFFF, refin/refout true).            */
/* Check: CRC32C("123456789") == 0xE3069283                           */
/* ------------------------------------------------------------------ */

static inline uint32_t aik_crc32c(uint32_t seed, const uint8_t *data,
                                  uint32_t len)
{
    uint32_t crc = seed ^ 0xFFFFFFFFUL;
    uint32_t i;

    for(i = 0U; i < len; i++)
    {
        uint8_t bit;

        crc ^= data[i];
        for(bit = 0U; bit < 8U; bit++)
        {
            if((crc & 1UL) != 0UL)
            {
                crc = (crc >> 1) ^ 0x82F63B78UL;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}

/* String hashes used in AKPK headers: CRC-32C over the raw ASCII id. */
static inline uint32_t aik_profile_string_hash32(const char *s)
{
    uint32_t len = 0U;

    while((s != 0) && (s[len] != '\0'))
    {
        len++;
    }
    return aik_crc32c(0U, (const uint8_t *)s, len);
}

/* 16-bit id used in SPI status frames: low 16 bits of the CRC-32C
 * string hash, so it can also be recovered from the AKPK header's
 * profile_id_hash without the source string. */
static inline uint16_t aik_profile_string_hash16(const char *s)
{
    return (uint16_t)(aik_profile_string_hash32(s) & 0xFFFFU);
}

/* ------------------------------------------------------------------ */
/* ProfilePackageBinary v1 ("AKPK")                                   */
/* ------------------------------------------------------------------ */

#define AIK_PKG_MAGIC0 'A'
#define AIK_PKG_MAGIC1 'K'
#define AIK_PKG_MAGIC2 'P'
#define AIK_PKG_MAGIC3 'K'

#define AIK_PKG_VERSION      1U
#define AIK_PKG_HEADER_SIZE  68U

#define AIK_PKG_SECTION_SOURCE_PROFILE_JSON            0x0001U
#define AIK_PKG_SECTION_RUNTIME_TABLE_CACHE            0x0002U
#define AIK_PKG_SECTION_RESOURCE_ESTIMATE_JSON         0x0003U
#define AIK_PKG_SECTION_FEATURE_FLAGS_JSON             0x0004U
#define AIK_PKG_SECTION_RUNTIME_TABLE_CACHE_META_JSON  0x0005U

#define AIK_PKG_ENCODING_CANONICAL_JSON       0x01U
#define AIK_PKG_ENCODING_RUNTIME_TABLE_BINARY 0x02U
#define AIK_PKG_ENCODING_RAW_BINARY           0x03U

typedef struct AIK_PROFILE_PACKED
{
    uint8_t magic[4];               /* "AKPK" */
    uint16_t package_version;
    uint16_t header_size;           /* fixed 68 */
    uint16_t section_count;
    uint16_t profile_schema_version;
    uint32_t flags;
    uint32_t keyboard_model_id_hash; /* aik_profile_string_hash32 */
    uint32_t profile_id_hash;        /* aik_profile_string_hash32 */
    uint32_t revision;
    uint8_t source_hash[32];         /* SHA-256 of canonical source JSON */
    uint32_t total_size;
    uint32_t package_crc32c;         /* CRC-32C bytes 0..total_size-1,
                                        this field zeroed during calc */
} aik_pkg_header_t;

typedef struct AIK_PROFILE_PACKED
{
    uint16_t section_kind;
    uint8_t encoding;
    uint8_t flags;
    uint32_t offset;                 /* from start of package, 4-aligned */
    uint32_t length;
    uint32_t section_crc32c;         /* CRC-32C over payload only */
} aik_pkg_section_entry_t;

typedef char aik_pkg_header_size_check[
    (sizeof(aik_pkg_header_t) == AIK_PKG_HEADER_SIZE) ? 1 : -1];
typedef char aik_pkg_section_entry_size_check[
    (sizeof(aik_pkg_section_entry_t) == 16U) ? 1 : -1];

/* ------------------------------------------------------------------ */
/* RuntimeTableBinary v1 ("AKRT")                                     */
/*                                                                    */
/* v1 firmware populates: control_index_map, trigger_table,           */
/* dispatch_table, behavior_table, mutable_param_slots,               */
/* resource_limits, scope_table. The remaining sections are present   */
/* in the directory with entry_count = 0 / length = 0.                */
/* ------------------------------------------------------------------ */

#define AIK_RT_MAGIC0 'A'
#define AIK_RT_MAGIC1 'K'
#define AIK_RT_MAGIC2 'R'
#define AIK_RT_MAGIC3 'T'

#define AIK_RT_TABLE_VERSION   1U
#define AIK_RT_ABI_VERSION     1U
#define AIK_RT_COMPILER_IR_VERSION 1U
#define AIK_RT_HEADER_SIZE     108U
#define AIK_RT_SECTION_COUNT   13U
#define AIK_RT_INDEX_WIDTH     2U    /* uint16 */
#define AIK_RT_ALIGNMENT       4U
#define AIK_RT_INVALID_INDEX   0xFFFFU

#define AIK_RT_SECTION_CONTROL_INDEX_MAP        0x0001U
#define AIK_RT_SECTION_TRIGGER_TABLE            0x0002U
#define AIK_RT_SECTION_INTERACTION_TABLE        0x0003U
#define AIK_RT_SECTION_DISPATCH_TABLE           0x0004U
#define AIK_RT_SECTION_BEHAVIOR_TABLE           0x0005U
#define AIK_RT_SECTION_MACRO_BYTECODE           0x0006U
#define AIK_RT_SECTION_MUTABLE_PARAM_SLOTS      0x0007U
#define AIK_RT_SECTION_RESOURCE_LIMITS          0x0008U
#define AIK_RT_SECTION_SCOPE_TABLE              0x0009U
#define AIK_RT_SECTION_INTERACTION_MEMBER_TABLE 0x000AU
#define AIK_RT_SECTION_VIRTUAL_SIGNAL_TABLE     0x000BU
#define AIK_RT_SECTION_DKS_STAGE_TABLE          0x000CU
#define AIK_RT_SECTION_PARAM_CONSTRAINT_TABLE   0x000DU

typedef struct AIK_PROFILE_PACKED
{
    uint8_t magic[4];               /* "AKRT" */
    uint16_t runtime_table_version;
    uint16_t runtime_abi_version;
    uint16_t compiler_ir_version;
    uint16_t profile_schema_version;
    uint16_t header_size;           /* fixed 108 */
    uint16_t section_count;         /* fixed 13 */
    uint8_t index_width;            /* 2 */
    uint8_t alignment;              /* 4 */
    uint16_t reserved2;
    uint32_t flags;
    uint32_t required_feature_flags;
    uint16_t resource_limit_profile_id;
    uint16_t reserved;
    uint8_t source_hash[32];
    uint8_t control_map_hash[32];
    uint32_t firmware_compat;
    uint32_t total_size;
    uint32_t table_crc32c;          /* CRC-32C bytes 0..total_size-1,
                                       this field zeroed during calc;
                                       section CRCs keep real values */
} aik_rt_header_t;

typedef struct AIK_PROFILE_PACKED
{
    uint16_t section_kind;
    uint16_t entry_size;
    uint32_t entry_count;
    uint32_t offset;                /* from start of table, 4-aligned */
    uint32_t length;                /* == entry_size * entry_count */
    uint32_t section_crc32c;        /* payload only */
} aik_rt_section_entry_t;

typedef char aik_rt_header_size_check[
    (sizeof(aik_rt_header_t) == AIK_RT_HEADER_SIZE) ? 1 : -1];
typedef char aik_rt_section_entry_size_check[
    (sizeof(aik_rt_section_entry_t) == 20U) ? 1 : -1];

/* binary enum v1 (runtime_contract.html) */

#define AIK_RT_CONTROL_TYPE_AKEY     0x01U
#define AIK_RT_CONTROL_TYPE_DKEY     0x02U
#define AIK_RT_CONTROL_TYPE_BUTTON   0x03U
#define AIK_RT_CONTROL_TYPE_AXIS     0x04U
#define AIK_RT_CONTROL_TYPE_ENCODER  0x05U

#define AIK_RT_SOURCE_KIND_H417_KEY         0x01U /* main hall keys */
#define AIK_RT_SOURCE_KIND_CH585_PERIPHERAL 0x02U /* fiveway / EC11 */
#define AIK_RT_SOURCE_KIND_CH585_EXTENSION  0x03U

#define AIK_RT_MODE_NORMAL        0x01U
#define AIK_RT_MODE_RAPID_TRIGGER 0x02U
#define AIK_RT_MODE_ANALOG        0x03U
#define AIK_RT_MODE_DIGITAL       0x04U
#define AIK_RT_MODE_DETENT_DELTA  0x05U
#define AIK_RT_MODE_RAW_DELTA     0x06U
#define AIK_RT_MODE_DISABLED      0x7FU

#define AIK_RT_SIGNAL_SOURCE_CONTROL        0x01U
#define AIK_RT_SIGNAL_SOURCE_VIRTUAL_SIGNAL 0x02U

#define AIK_RT_RESULT_BEHAVIOR    0x01U
#define AIK_RT_RESULT_TRANSPARENT 0x02U
#define AIK_RT_RESULT_NO_OP       0x03U

#define AIK_RT_BEHAVIOR_HOST_INPUT      0x01U
#define AIK_RT_BEHAVIOR_MACRO_CALL      0x02U
#define AIK_RT_BEHAVIOR_PROFILE_SWITCH  0x03U
#define AIK_RT_BEHAVIOR_OVERLAY_CONTROL 0x04U
#define AIK_RT_BEHAVIOR_DEVICE_COMMAND  0x05U
#define AIK_RT_BEHAVIOR_TAP_HOLD        0x06U
#define AIK_RT_BEHAVIOR_DKS             0x07U

#define AIK_RT_HOST_USAGE_KEYBOARD     0x0001U
#define AIK_RT_HOST_USAGE_CONSUMER     0x0002U
#define AIK_RT_HOST_USAGE_MOUSE_BUTTON 0x0003U
#define AIK_RT_HOST_USAGE_MOUSE_AXIS   0x0004U

/* host_input data packing:
 *   data0 = host_usage_kind
 *   data1 = usage_id
 *     keyboard: bits0-7 usage, bits8-15 modifier_mask (0 usage = modifier only)
 *     consumer: 16-bit consumer usage
 *     mouse_axis: bits0-7 axis id (0=wheel), bits8-15 signed step
 */
#define AIK_RT_MOUSE_AXIS_WHEEL 0x00U

#define AIK_RT_EVENT_PRESS            0x0001U
#define AIK_RT_EVENT_RELEASE          0x0002U
#define AIK_RT_EVENT_VALUE_CHANGED    0x0003U
#define AIK_RT_EVENT_NEGATIVE_PRESS   0x0004U
#define AIK_RT_EVENT_NEGATIVE_RELEASE 0x0005U
#define AIK_RT_EVENT_POSITIVE_PRESS   0x0006U
#define AIK_RT_EVENT_POSITIVE_RELEASE 0x0007U
#define AIK_RT_EVENT_CW_STEP          0x0008U
#define AIK_RT_EVENT_CCW_STEP         0x0009U
#define AIK_RT_EVENT_VIRTUAL_PRESS    0x000AU
#define AIK_RT_EVENT_VIRTUAL_RELEASE  0x000BU
#define AIK_RT_EVENT_CONTROL_LEVEL    0xFFFFU /* DispatchEntry: control-level */

#define AIK_RT_VALUE_KIND_I32     0x01U
#define AIK_RT_VALUE_KIND_U32     0x02U
#define AIK_RT_VALUE_KIND_ENUM16  0x03U
#define AIK_RT_VALUE_KIND_BOOL8   0x04U

#define AIK_RT_UNIT_NONE     0x00U
#define AIK_RT_UNIT_UM       0x01U
#define AIK_RT_UNIT_MS       0x02U
#define AIK_RT_UNIT_NORM_I16 0x03U
#define AIK_RT_UNIT_ENUM     0x04U
#define AIK_RT_UNIT_COUNT    0x05U

/* param_id enum v1. 0x0015..0x0018 are firmware extensions pending
 * upstream doc assignment (runtime_contract.html leaves these params
 * without ids); values are chosen from the unassigned base gap.
 * Trigger values are carried in permille of full travel (0..1000),
 * matching the CH585 magnetic key engine native unit. */
#define AIK_RT_PARAM_PRESS_THRESHOLD_NORM      0x0003U
#define AIK_RT_PARAM_PRESS_DELTA_NORM          0x0004U
#define AIK_RT_PARAM_RELEASE_DELTA_NORM        0x0014U
#define AIK_RT_PARAM_RELEASE_THRESHOLD_NORM    0x0015U
#define AIK_RT_PARAM_RESET_THRESHOLD_NORM      0x0016U
#define AIK_RT_PARAM_DEADZONE_NORM             0x0017U
#define AIK_RT_PARAM_FILTER_SHIFT              0x0018U

typedef struct AIK_PROFILE_PACKED
{
    uint16_t control_index;
    uint8_t control_type;
    uint8_t source_kind;
    uint16_t source_index;
    uint16_t source_control_index;
    uint32_t control_id_hash;
    uint16_t first_trigger_index;
    uint16_t trigger_count;
} aik_rt_control_index_map_entry_t;

typedef struct AIK_PROFILE_PACKED
{
    uint16_t trigger_index;
    uint16_t control_index;
    uint8_t control_type;
    uint8_t mode;
    uint16_t first_param_slot_index;
    uint16_t param_slot_count;
    uint16_t first_signal_event_code;
    uint16_t signal_count;
    uint16_t runtime_state_bytes;
    uint16_t flags;
    uint16_t reserved;
} aik_rt_trigger_entry_t;

typedef struct AIK_PROFILE_PACKED
{
    uint16_t scope_index;
    int16_t priority;
    uint8_t default_active;
    uint8_t base_scope;
    uint8_t unbound_result_kind;
    uint8_t reserved;
    uint16_t unbound_behavior_index;
    uint16_t reserved2;
} aik_rt_scope_entry_t;

typedef struct AIK_PROFILE_PACKED
{
    uint16_t dispatch_index;
    uint16_t scope_index;
    uint8_t signal_source_kind;
    uint8_t flags;
    uint16_t source_index;          /* control_index or virtual_signal_index */
    uint16_t signal_event_code;     /* 0xFFFF = control-level binding */
    uint8_t result_kind;
    uint8_t reserved;
    uint16_t behavior_index;
    uint16_t reserved2;
} aik_rt_dispatch_entry_t;

typedef struct AIK_PROFILE_PACKED
{
    uint16_t behavior_index;
    uint8_t behavior_kind;
    uint8_t flags;                  /* must be 0 in v1 */
    uint32_t data0;
    uint32_t data1;
    uint32_t data2;
    uint32_t data3;
} aik_rt_behavior_entry_t;

typedef struct AIK_PROFILE_PACKED
{
    uint16_t slot_index;
    uint16_t owner_section_kind;
    uint16_t owner_entry_index;
    uint16_t constraint_group_id;
    uint16_t param_id;
    uint16_t param_order;
    uint8_t value_kind;
    uint8_t value_width;
    uint8_t unit_kind;
    uint8_t writable;
    int32_t min_value;
    int32_t max_value;
    int32_t initial_value;
} aik_rt_mutable_param_slot_entry_t;

typedef struct AIK_PROFILE_PACKED
{
    uint16_t max_control_count;
    uint16_t max_trigger_count;
    uint16_t max_scope_count;
    uint16_t max_dispatch_count;
    uint16_t max_interaction_count;
    uint16_t max_behavior_count;
    uint16_t max_virtual_signal_count;
    uint16_t max_dks_stage_count;
    uint32_t max_macro_bytes;
    uint16_t max_param_slot_count;
    uint16_t max_param_constraint_count;
    uint16_t max_runtime_state_bytes;
    uint16_t reserved;
    uint32_t required_feature_flags;
} aik_rt_resource_limits_entry_t;

typedef char aik_rt_cim_entry_size_check[
    (sizeof(aik_rt_control_index_map_entry_t) == 16U) ? 1 : -1];
typedef char aik_rt_trigger_entry_size_check[
    (sizeof(aik_rt_trigger_entry_t) == 20U) ? 1 : -1];
typedef char aik_rt_scope_entry_size_check[
    (sizeof(aik_rt_scope_entry_t) == 12U) ? 1 : -1];
typedef char aik_rt_dispatch_entry_size_check[
    (sizeof(aik_rt_dispatch_entry_t) == 16U) ? 1 : -1];
typedef char aik_rt_behavior_entry_size_check[
    (sizeof(aik_rt_behavior_entry_t) == 20U) ? 1 : -1];
typedef char aik_rt_param_slot_entry_size_check[
    (sizeof(aik_rt_mutable_param_slot_entry_t) == 28U) ? 1 : -1];
typedef char aik_rt_resource_limits_entry_size_check[
    (sizeof(aik_rt_resource_limits_entry_t) == 32U) ? 1 : -1];

/* Device control index allocation for ak_h417_ch585_v1:
 * key_000..key_076 -> control_index 0..76 (== global key id),
 * fiveway_000 -> 77, enc_000 -> 78. */
#define AIK_RT_CONTROL_INDEX_FIVEWAY 77U
#define AIK_RT_CONTROL_INDEX_ENC     78U
#define AIK_RT_CONTROL_COUNT_V1      79U

/* ------------------------------------------------------------------ */
/* Half patch "AKHR": derived per-half config pushed H417 -> CH585    */
/* and persisted verbatim in CH585 Data-Flash user slots.             */
/* CRC16-CCITT over bytes 0..total_len-1 with crc16 field zeroed.     */
/* ------------------------------------------------------------------ */

#define AIK_HP_MAGIC0 'A'
#define AIK_HP_MAGIC1 'K'
#define AIK_HP_MAGIC2 'H'
#define AIK_HP_MAGIC3 'R'

#define AIK_HP_VERSION 1U

#define AIK_HP_FLAG_HAS_DISPATCH77 0x0001U

typedef struct AIK_PROFILE_PACKED
{
    uint8_t magic[4];               /* "AKHR" */
    uint8_t version;
    uint8_t half_id;                /* AIK_HALF_ID_LEFT / _RIGHT */
    uint8_t key_count;              /* keys in this half */
    uint8_t local_count;            /* local binding entries */
    uint16_t profile_id16;          /* aik_profile_string_hash16 */
    uint16_t generation16;          /* identity.revision & 0xFFFF */
    uint16_t flags;
    uint16_t trigger_offset;        /* from patch start */
    uint16_t dispatch_offset;       /* 0 when absent */
    uint16_t local_offset;          /* 0 when absent */
    uint16_t total_len;
    uint16_t reserved;
    uint16_t reserved2;
    uint16_t crc16;
} aik_hp_header_t;

/* Per-key trigger parameters, permille of full travel. release_pm is
 * the static release / rapid-trigger reset threshold. */
typedef struct AIK_PROFILE_PACKED
{
    uint16_t press_pm;
    uint16_t release_pm;
    uint16_t rt_press_delta_pm;
    uint16_t rt_release_delta_pm;
    uint8_t mode;                   /* AIK_RT_MODE_NORMAL / _RAPID_TRIGGER /
                                       _DISABLED */
    uint8_t filter_shift;
} aik_hp_trigger_entry_t;

/* Local control binding entry. */
#define AIK_HP_SIGNAL_FIVEWAY_UP      0U
#define AIK_HP_SIGNAL_FIVEWAY_DOWN    1U
#define AIK_HP_SIGNAL_FIVEWAY_LEFT    2U
#define AIK_HP_SIGNAL_FIVEWAY_RIGHT   3U
#define AIK_HP_SIGNAL_FIVEWAY_PRESS   4U
#define AIK_HP_SIGNAL_WHEEL_UP        5U
#define AIK_HP_SIGNAL_WHEEL_DOWN      6U
#define AIK_HP_SIGNAL_EC11_CW         8U
#define AIK_HP_SIGNAL_EC11_CCW        9U
#define AIK_HP_SIGNAL_EC11_PRESS      10U

#define AIK_HP_TARGET_NONE        0U
#define AIK_HP_TARGET_KEYBOARD    1U  /* value: bits0-7 usage, bits8-15 mod */
#define AIK_HP_TARGET_CONSUMER    2U  /* value: consumer usage */
#define AIK_HP_TARGET_MOUSE_WHEEL 3U  /* value: signed 8-bit step in bits0-7 */

typedef struct AIK_PROFILE_PACKED
{
    uint8_t signal_id;
    uint8_t target_kind;
    uint16_t value;
} aik_hp_local_entry_t;

/* Dispatch table pushed to the left half for wireless NKRO composition:
 * AIK_KEY_COUNT_TOTAL entries of {usage, modifier_mask}. */
typedef struct AIK_PROFILE_PACKED
{
    uint8_t usage;
    uint8_t modifier_mask;
} aik_hp_key_output_t;

typedef char aik_hp_header_size_check[
    (sizeof(aik_hp_header_t) == 28U) ? 1 : -1];
typedef char aik_hp_trigger_entry_size_check[
    (sizeof(aik_hp_trigger_entry_t) == 10U) ? 1 : -1];
typedef char aik_hp_local_entry_size_check[
    (sizeof(aik_hp_local_entry_t) == 4U) ? 1 : -1];

/* Upper bound for one half patch: header + 41 triggers + 77-key dispatch
 * + local entries, with headroom for future growth. */
#define AIK_HP_MAX_SIZE 1024U

static inline uint16_t aik_hp_crc(const uint8_t *patch, uint16_t total_len)
{
    const aik_hp_header_t *hdr = (const aik_hp_header_t *)patch;
    uint16_t crc_off = (uint16_t)offsetof(aik_hp_header_t, crc16);
    uint16_t crc = 0xFFFFU;
    uint16_t i;

    (void)hdr;
    for(i = 0U; i < total_len; i++)
    {
        uint8_t byte = ((i == crc_off) || (i == (uint16_t)(crc_off + 1U))) ?
                       0U : patch[i];
        uint8_t bit;

        crc ^= (uint16_t)byte << 8;
        for(bit = 0U; bit < 8U; bit++)
        {
            if((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static inline uint8_t aik_hp_valid(const uint8_t *patch, uint16_t buf_len)
{
    const aik_hp_header_t *hdr = (const aik_hp_header_t *)patch;

    if((patch == 0) || (buf_len < sizeof(aik_hp_header_t)))
    {
        return 0U;
    }
    if((hdr->magic[0] != AIK_HP_MAGIC0) || (hdr->magic[1] != AIK_HP_MAGIC1) ||
       (hdr->magic[2] != AIK_HP_MAGIC2) || (hdr->magic[3] != AIK_HP_MAGIC3))
    {
        return 0U;
    }
    if((hdr->version != AIK_HP_VERSION) ||
       (hdr->total_len < sizeof(aik_hp_header_t)) ||
       (hdr->total_len > buf_len) ||
       (hdr->total_len > AIK_HP_MAX_SIZE))
    {
        return 0U;
    }
    return (hdr->crc16 == aik_hp_crc(patch, hdr->total_len)) ? 1U : 0U;
}

#ifdef __cplusplus
}
#endif

#endif /* AIK_PROFILE_FORMAT_H */
