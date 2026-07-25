#include "ch585_profile.h"

#include <string.h>

#include "CH58x_common.h"
#include "ISP585.h"
#include "ch585_half_report.h"

#ifndef CH585_HALF_ID
#define CH585_HALF_ID 0
#endif

#define META_MAGIC0 'A'
#define META_MAGIC1 'K'
#define META_MAGIC2 'A'
#define META_MAGIC3 'S'

typedef struct
{
    uint8_t magic[4];
    uint8_t version;
    uint8_t active_slot;
    uint16_t crc16;
} profile_meta_record_t;

typedef struct
{
    uint8_t state;
    uint8_t slot_id;
    uint8_t patch_flags;
    uint16_t expected_len;
    uint16_t expected_crc;
    uint16_t received_len;
    uint16_t last_ack_seq;
} profile_xfer_t;

static ch585_profile_state_t s_state;
static profile_xfer_t s_xfer;
static uint8_t s_xfer_buf[AIK_HP_MAX_SIZE];
static uint8_t s_slot_buf[AIK_HP_MAX_SIZE];

static uint32_t slot_eeprom_addr(uint8_t slot_id)
{
    return (uint32_t)(slot_id - AIK_PROFILE_USER_SLOT_FIRST) *
           CH585_PROFILE_EEPROM_SLOT_SIZE;
}

static uint8_t slot_id_is_user(uint8_t slot_id)
{
    return (uint8_t)((slot_id >= AIK_PROFILE_USER_SLOT_FIRST) &&
                     (slot_id < AIK_PROFILE_SLOT_COUNT_TOTAL));
}

/* ------------------------------------------------------------------ */
/* Factory defaults                                                   */
/* ------------------------------------------------------------------ */

static void profile_apply_factory(mag_key_engine_t *engine)
{
    mag_key_config_t cfg;

    ch585_half_report_reset_factory();

    if(engine != 0)
    {
        mag_key_default_config(&cfg);
        cfg.mode = MAG_KEY_MODE_RAPID_TRIGGER;
        (void)mag_key_engine_set_global_config(engine, &cfg);
    }

    s_state.active_slot = AIK_PROFILE_SLOT_FACTORY;
    s_state.patch_applied = 0U;
    s_state.profile_id16 = aik_profile_string_hash16("factory_default");
    s_state.generation16 = 1U;
}

/* ------------------------------------------------------------------ */
/* Patch application                                                  */
/* ------------------------------------------------------------------ */

static int profile_apply_patch(mag_key_engine_t *engine,
                               const uint8_t *patch, uint16_t len)
{
    const aik_hp_header_t *hdr = (const aik_hp_header_t *)patch;
    uint8_t expected_keys = aik_spi_half_key_count((uint8_t)CH585_HALF_ID);
    uint8_t i;

    if(aik_hp_valid(patch, len) == 0U)
    {
        return -1;
    }
    if((hdr->half_id != (uint8_t)CH585_HALF_ID) ||
       (hdr->key_count != expected_keys))
    {
        return -1;
    }
    if((hdr->trigger_offset + (uint16_t)hdr->key_count *
        sizeof(aik_hp_trigger_entry_t)) > hdr->total_len)
    {
        return -1;
    }
    if(((hdr->flags & AIK_HP_FLAG_HAS_DISPATCH77) != 0U) &&
       (((uint32_t)hdr->dispatch_offset +
         (AIK_KEY_COUNT_TOTAL * sizeof(aik_hp_key_output_t))) >
        hdr->total_len))
    {
        return -1;
    }
    if(((hdr->flags & AIK_HP_FLAG_HAS_FN_DISPATCH77) != 0U) &&
       (((hdr->flags & AIK_HP_FLAG_HAS_DISPATCH77) == 0U) ||
        (hdr->fn_hold_key >= AIK_KEY_COUNT_TOTAL) ||
        (((uint32_t)hdr->fn_dispatch_offset +
          (AIK_KEY_COUNT_TOTAL * sizeof(aik_hp_key_output_t))) >
         hdr->total_len)))
    {
        return -1;
    }
    if((hdr->local_count != 0U) &&
       (((uint32_t)hdr->local_offset +
         ((uint32_t)hdr->local_count * sizeof(aik_hp_local_entry_t))) >
        hdr->total_len))
    {
        return -1;
    }

    if(engine != 0)
    {
        for(i = 0U; i < hdr->key_count; i++)
        {
            const aik_hp_trigger_entry_t *trig =
                (const aik_hp_trigger_entry_t *)
                (patch + hdr->trigger_offset +
                 (uint16_t)i * sizeof(aik_hp_trigger_entry_t));
            mag_key_config_t cfg = engine->cfg[i];

            cfg.press_pm = trig->press_pm;
            cfg.release_pm = trig->release_pm;
            cfg.rt_press_delta_pm = trig->rt_press_delta_pm;
            cfg.rt_release_delta_pm = trig->rt_release_delta_pm;
            cfg.filter_shift = trig->filter_shift;
            if(trig->mode == AIK_RT_MODE_RAPID_TRIGGER)
            {
                cfg.mode = MAG_KEY_MODE_RAPID_TRIGGER;
            }
            else if(trig->mode == AIK_RT_MODE_DISABLED)
            {
                cfg.mode = MAG_KEY_MODE_DISABLED;
            }
            else
            {
                cfg.mode = MAG_KEY_MODE_STATIC;
            }
            (void)mag_key_engine_set_key_config(engine, i, &cfg);
        }
    }

    if((hdr->flags & AIK_HP_FLAG_HAS_DISPATCH77) != 0U)
    {
        ch585_half_report_set_key_outputs(patch + hdr->dispatch_offset);
    }
    ch585_half_report_clear_fn_overlay();
    if((hdr->flags & AIK_HP_FLAG_HAS_FN_DISPATCH77) != 0U)
    {
        ch585_half_report_set_fn_overlay(hdr->fn_hold_key,
                                        patch + hdr->fn_dispatch_offset);
    }

    ch585_half_report_clear_locals();
    for(i = 0U; i < hdr->local_count; i++)
    {
        const aik_hp_local_entry_t *entry =
            (const aik_hp_local_entry_t *)
            (patch + hdr->local_offset +
             (uint16_t)i * sizeof(aik_hp_local_entry_t));

        ch585_half_report_set_local(entry->signal_id, entry->target_kind,
                                    entry->value);
    }

    s_state.patch_applied = 1U;
    s_state.profile_id16 = hdr->profile_id16;
    s_state.generation16 = hdr->generation16;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Data-Flash slots                                                   */
/* ------------------------------------------------------------------ */

static int profile_slot_read(uint8_t slot_id, uint8_t *buf,
                             uint16_t *out_len)
{
    const aik_hp_header_t *hdr = (const aik_hp_header_t *)buf;

    if(slot_id_is_user(slot_id) == 0U)
    {
        return -1;
    }
    if(EEPROM_READ(slot_eeprom_addr(slot_id), buf,
                   sizeof(aik_hp_header_t)) != 0U)
    {
        return -1;
    }
    if((hdr->magic[0] != AIK_HP_MAGIC0) || (hdr->magic[1] != AIK_HP_MAGIC1) ||
       (hdr->magic[2] != AIK_HP_MAGIC2) || (hdr->magic[3] != AIK_HP_MAGIC3) ||
       (hdr->total_len < sizeof(aik_hp_header_t)) ||
       (hdr->total_len > AIK_HP_MAX_SIZE))
    {
        return -1;
    }
    if(EEPROM_READ(slot_eeprom_addr(slot_id), buf, hdr->total_len) != 0U)
    {
        return -1;
    }
    if(aik_hp_valid(buf, hdr->total_len) == 0U)
    {
        return -1;
    }
    if(out_len != 0)
    {
        *out_len = hdr->total_len;
    }
    return 0;
}

static int profile_slot_write(uint8_t slot_id, const uint8_t *patch,
                              uint16_t len)
{
    uint8_t verify[64];
    uint16_t offset;

    if((slot_id_is_user(slot_id) == 0U) || (len > AIK_HP_MAX_SIZE))
    {
        return -1;
    }
    if(EEPROM_ERASE(slot_eeprom_addr(slot_id),
                    CH585_PROFILE_EEPROM_SLOT_SIZE) != 0U)
    {
        return -1;
    }
    if(EEPROM_WRITE(slot_eeprom_addr(slot_id), (void *)patch, len) != 0U)
    {
        return -1;
    }
    for(offset = 0U; offset < len; offset += (uint16_t)sizeof(verify))
    {
        uint16_t remain = (uint16_t)(len - offset);
        uint16_t take = (remain < (uint16_t)sizeof(verify)) ?
                        remain : (uint16_t)sizeof(verify);

        if(EEPROM_READ(slot_eeprom_addr(slot_id) + offset, verify,
                       take) != 0U)
        {
            return -1;
        }
        if(memcmp(verify, patch + offset, take) != 0)
        {
            return -1;
        }
    }
    return 0;
}

static uint8_t profile_meta_read(void)
{
    profile_meta_record_t rec;

    if(EEPROM_READ(CH585_PROFILE_EEPROM_META_ADDR, &rec,
                   sizeof(rec)) != 0U)
    {
        return AIK_PROFILE_SLOT_FACTORY;
    }
    if((rec.magic[0] != META_MAGIC0) || (rec.magic[1] != META_MAGIC1) ||
       (rec.magic[2] != META_MAGIC2) || (rec.magic[3] != META_MAGIC3) ||
       (rec.version != 1U) ||
       (rec.crc16 != aik_spi_crc16_ccitt((const uint8_t *)&rec, 6U)) ||
       (rec.active_slot >= AIK_PROFILE_SLOT_COUNT_TOTAL))
    {
        return AIK_PROFILE_SLOT_FACTORY;
    }
    return rec.active_slot;
}

static int profile_meta_write(uint8_t slot_id)
{
    profile_meta_record_t rec;

    if(profile_meta_read() == slot_id)
    {
        return 0;
    }

    memset(&rec, 0, sizeof(rec));
    rec.magic[0] = META_MAGIC0;
    rec.magic[1] = META_MAGIC1;
    rec.magic[2] = META_MAGIC2;
    rec.magic[3] = META_MAGIC3;
    rec.version = 1U;
    rec.active_slot = slot_id;
    rec.crc16 = aik_spi_crc16_ccitt((const uint8_t *)&rec, 6U);

    if(EEPROM_ERASE(CH585_PROFILE_EEPROM_META_ADDR,
                    EEPROM_PAGE_SIZE) != 0U)
    {
        return -1;
    }
    if(EEPROM_WRITE(CH585_PROFILE_EEPROM_META_ADDR, &rec,
                    sizeof(rec)) != 0U)
    {
        return -1;
    }
    return (profile_meta_read() == slot_id) ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

void ch585_profile_boot_load(mag_key_engine_t *engine)
{
    uint8_t active = profile_meta_read();

    memset(&s_xfer, 0, sizeof(s_xfer));
    profile_apply_factory(engine);

    if(slot_id_is_user(active) != 0U)
    {
        uint16_t len = 0U;

        if((profile_slot_read(active, s_slot_buf, &len) == 0) &&
           (profile_apply_patch(engine, s_slot_buf, len) == 0))
        {
            s_state.active_slot = active;
        }
    }
}

const ch585_profile_state_t *ch585_profile_state(void)
{
    return &s_state;
}

void ch585_profile_fill_status(aik_spi_profile_status_v1_t *status)
{
    status->flags = AIK_PROFILE_STATUS_FLAG_VALID;
    if(s_state.patch_applied == 0U)
    {
        status->flags |= AIK_PROFILE_STATUS_FLAG_DEFAULT;
    }
    if((s_xfer.state == AIK_SPI_XFER_STATE_RECEIVING) ||
       (s_xfer.state == AIK_SPI_XFER_STATE_WRITING))
    {
        status->flags |= AIK_PROFILE_STATUS_FLAG_BUSY;
    }
    if((s_xfer.state & 0x80U) != 0U)
    {
        status->flags |= AIK_PROFILE_STATUS_FLAG_ERROR;
    }
    status->flags |= (uint8_t)((s_state.active_slot <<
                                AIK_PROFILE_STATUS_SLOT_SHIFT) &
                               AIK_PROFILE_STATUS_SLOT_MASK);
    status->profile_id16 = s_state.profile_id16;
    status->generation16 = s_state.generation16;
}

static void xfer_fail(uint8_t error_state)
{
    s_xfer.state = error_state;
    s_xfer.received_len = 0U;
    s_xfer.expected_len = 0U;
}

static void profile_handle_begin(const aik_spi_host_cmd_v1_t *cmd)
{
    aik_spi_profile_begin_v1_t begin;

    aik_spi_host_cmd_get_payload(cmd, &begin, sizeof(begin));

    if((begin.total_len < sizeof(aik_hp_header_t)) ||
       (begin.total_len > AIK_HP_MAX_SIZE))
    {
        xfer_fail(AIK_SPI_XFER_ERR_RANGE);
        return;
    }
    if((begin.slot_id >= AIK_PROFILE_SLOT_COUNT_TOTAL) ||
       ((begin.slot_id == AIK_PROFILE_SLOT_FACTORY) &&
        ((begin.patch_flags & AIK_SPI_PROFILE_FLAG_PERSIST) != 0U)))
    {
        xfer_fail(AIK_SPI_XFER_ERR_RANGE);
        return;
    }

    s_xfer.state = AIK_SPI_XFER_STATE_RECEIVING;
    s_xfer.slot_id = begin.slot_id;
    s_xfer.patch_flags = begin.patch_flags;
    s_xfer.expected_len = begin.total_len;
    s_xfer.expected_crc = begin.total_crc16;
    s_xfer.received_len = 0U;
}

static void profile_handle_chunk(const aik_spi_host_cmd_v1_t *cmd)
{
    aik_spi_profile_chunk_v1_t chunk;

    if(s_xfer.state != AIK_SPI_XFER_STATE_RECEIVING)
    {
        xfer_fail(AIK_SPI_XFER_ERR_STATE);
        return;
    }

    aik_spi_host_cmd_get_payload(cmd, &chunk, sizeof(chunk));

    if((chunk.len == 0U) ||
       (chunk.len > AIK_SPI_PROFILE_CHUNK_DATA_MAX) ||
       (((uint32_t)chunk.offset + chunk.len) > s_xfer.expected_len))
    {
        xfer_fail(AIK_SPI_XFER_ERR_RANGE);
        return;
    }
    if((s_xfer.received_len >= chunk.len) &&
       (chunk.offset == (uint16_t)(s_xfer.received_len - chunk.len)))
    {
        /* Duplicate of the chunk we already accepted (host retried the
         * command because the ack got lost): acknowledge idempotently. */
        return;
    }
    if(chunk.offset != s_xfer.received_len)
    {
        xfer_fail(AIK_SPI_XFER_ERR_RANGE);
        return;
    }

    memcpy(&s_xfer_buf[chunk.offset], chunk.data, chunk.len);
    s_xfer.received_len = (uint16_t)(s_xfer.received_len + chunk.len);
}

static void profile_handle_commit(const aik_spi_host_cmd_v1_t *cmd,
                                  mag_key_engine_t *engine)
{
    aik_spi_profile_commit_v1_t commit;

    aik_spi_host_cmd_get_payload(cmd, &commit, sizeof(commit));

    if((s_xfer.state != AIK_SPI_XFER_STATE_RECEIVING) ||
       (s_xfer.received_len != s_xfer.expected_len) ||
       (commit.total_len != s_xfer.expected_len) ||
       (commit.slot_id != s_xfer.slot_id) ||
       (commit.patch_flags != s_xfer.patch_flags))
    {
        xfer_fail(AIK_SPI_XFER_ERR_STATE);
        return;
    }
    if((commit.total_crc16 != s_xfer.expected_crc) ||
       (aik_hp_crc(s_xfer_buf, s_xfer.expected_len) != s_xfer.expected_crc) ||
       (aik_hp_valid(s_xfer_buf, s_xfer.expected_len) == 0U))
    {
        xfer_fail(AIK_SPI_XFER_ERR_CRC);
        return;
    }

    if((s_xfer.patch_flags & AIK_SPI_PROFILE_FLAG_PERSIST) != 0U)
    {
        s_xfer.state = AIK_SPI_XFER_STATE_WRITING;
        if(profile_slot_write(s_xfer.slot_id, s_xfer_buf,
                              s_xfer.expected_len) != 0)
        {
            xfer_fail(AIK_SPI_XFER_ERR_STORE);
            return;
        }
    }

    if((s_xfer.patch_flags & AIK_SPI_PROFILE_FLAG_ACTIVATE) != 0U)
    {
        if(profile_apply_patch(engine, s_xfer_buf,
                               s_xfer.expected_len) != 0)
        {
            xfer_fail(AIK_SPI_XFER_ERR_CRC);
            return;
        }
        s_state.active_slot = s_xfer.slot_id;
        if(((s_xfer.patch_flags & AIK_SPI_PROFILE_FLAG_PERSIST) != 0U) ||
           (s_xfer.slot_id == AIK_PROFILE_SLOT_FACTORY))
        {
            if(profile_meta_write(s_xfer.slot_id) != 0)
            {
                xfer_fail(AIK_SPI_XFER_ERR_STORE);
                return;
            }
        }
    }

    s_xfer.state = AIK_SPI_XFER_STATE_DONE;
}

static void profile_handle_set_active(const aik_spi_host_cmd_v1_t *cmd,
                                      mag_key_engine_t *engine)
{
    aik_spi_profile_set_active_v1_t set_active;

    aik_spi_host_cmd_get_payload(cmd, &set_active, sizeof(set_active));

    if(set_active.slot_id == AIK_PROFILE_SLOT_FACTORY)
    {
        profile_apply_factory(engine);
        if(profile_meta_write(AIK_PROFILE_SLOT_FACTORY) != 0)
        {
            xfer_fail(AIK_SPI_XFER_ERR_STORE);
            return;
        }
        s_xfer.state = AIK_SPI_XFER_STATE_DONE;
        return;
    }

    if(slot_id_is_user(set_active.slot_id) == 0U)
    {
        xfer_fail(AIK_SPI_XFER_ERR_RANGE);
        return;
    }

    {
        uint16_t len = 0U;

        if((profile_slot_read(set_active.slot_id, s_slot_buf, &len) != 0) ||
           (profile_apply_patch(engine, s_slot_buf, len) != 0))
        {
            xfer_fail(AIK_SPI_XFER_ERR_STORE);
            return;
        }
    }

    s_state.active_slot = set_active.slot_id;
    if(profile_meta_write(set_active.slot_id) != 0)
    {
        xfer_fail(AIK_SPI_XFER_ERR_STORE);
        return;
    }
    s_xfer.state = AIK_SPI_XFER_STATE_DONE;
}

void ch585_profile_handle_transfer_cmd(const aik_spi_host_cmd_v1_t *cmd,
                                       mag_key_engine_t *engine)
{
    s_xfer.last_ack_seq = cmd->host_seq;

    switch(cmd->cmd)
    {
        case AIK_SPI_CMD_PROFILE_BEGIN:
            profile_handle_begin(cmd);
            break;
        case AIK_SPI_CMD_PROFILE_CHUNK:
            profile_handle_chunk(cmd);
            break;
        case AIK_SPI_CMD_PROFILE_COMMIT:
            profile_handle_commit(cmd, engine);
            break;
        case AIK_SPI_CMD_PROFILE_ABORT:
            memset(&s_xfer, 0, sizeof(s_xfer));
            s_xfer.last_ack_seq = cmd->host_seq;
            break;
        case AIK_SPI_CMD_PROFILE_SET_ACTIVE:
            profile_handle_set_active(cmd, engine);
            break;
        case AIK_SPI_CMD_PROFILE_GET_XFER:
            /* Status readback only. */
            break;
        default:
            xfer_fail(AIK_SPI_XFER_ERR_UNSUPPORTED);
            break;
    }
}

void ch585_profile_fill_xfer(aik_spi_profile_xfer_v1_t *xfer)
{
    memset(xfer, 0, sizeof(*xfer));
    xfer->ack_seq = s_xfer.last_ack_seq;
    xfer->half_id = (uint8_t)CH585_HALF_ID;
    xfer->state = s_xfer.state;
    xfer->received_len = s_xfer.received_len;
    xfer->detail = s_xfer.expected_len;
    aik_spi_profile_xfer_finish(xfer);
}
