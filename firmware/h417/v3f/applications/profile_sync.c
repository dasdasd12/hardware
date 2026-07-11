#include "profile_sync.h"

#include <string.h>

#include "aik_profile_format.h"
#include "ch585_link.h"
#include "profile_runtime.h"
#include "profile_store.h"

#define SYNC_STATE_IDLE        0U
#define SYNC_STATE_DIRTY       1U
#define SYNC_STATE_BEGIN       2U
#define SYNC_STATE_CHUNKS      3U
#define SYNC_STATE_COMMIT      4U
#define SYNC_STATE_WAIT_COMMIT 5U
#define SYNC_STATE_SYNCED      6U
#define SYNC_STATE_BACKOFF     7U

#define SYNC_CMD_RETRIES       8U
#define SYNC_BACKOFF_TICKS     1024U
#define SYNC_COMMIT_WAIT_MAX   64U

typedef struct
{
    uint8_t state;
    uint8_t retries;
    uint16_t offset;
    uint16_t patch_len;
    uint16_t backoff;
    uint16_t commit_waits;
    uint8_t patch[AIK_HP_MAX_SIZE];
} half_sync_t;

static half_sync_t s_sync[2];
static uint16_t s_cmd_seq;

void v3f_profile_sync_init(void)
{
    memset(s_sync, 0, sizeof(s_sync));
}

void v3f_profile_sync_mark_all_dirty(void)
{
    if(v3f_profile_runtime_valid() == 0U)
    {
        return;
    }
    s_sync[AIK_HALF_ID_LEFT].state = SYNC_STATE_DIRTY;
    s_sync[AIK_HALF_ID_RIGHT].state = SYNC_STATE_DIRTY;
}

uint8_t v3f_profile_sync_half_synced(uint8_t half_id)
{
    if(half_id > AIK_HALF_ID_RIGHT)
    {
        return 0U;
    }
    return (uint8_t)(s_sync[half_id].state == SYNC_STATE_SYNCED);
}

static void sync_fail(half_sync_t *sync)
{
    sync->retries++;
    if(sync->retries >= SYNC_CMD_RETRIES)
    {
        sync->state = SYNC_STATE_BACKOFF;
        sync->backoff = SYNC_BACKOFF_TICKS;
        sync->retries = 0U;
    }
}

static uint8_t sync_patch_flags(void)
{
    uint8_t flags = AIK_SPI_PROFILE_FLAG_ACTIVATE;

    if(v3f_profile_runtime_get()->active_slot != AIK_PROFILE_SLOT_FACTORY)
    {
        flags |= AIK_SPI_PROFILE_FLAG_PERSIST;
    }
    return flags;
}

static uint8_t sync_send_begin(uint8_t half_id, half_sync_t *sync)
{
    const aik_hp_header_t *hdr = (const aik_hp_header_t *)sync->patch;
    aik_spi_host_cmd_v1_t cmd;
    aik_spi_profile_begin_v1_t begin;
    aik_spi_profile_xfer_v1_t xfer;

    memset(&cmd, 0, sizeof(cmd));
    memset(&begin, 0, sizeof(begin));
    begin.slot_id = v3f_profile_runtime_get()->active_slot;
    begin.patch_flags = sync_patch_flags();
    begin.total_len = sync->patch_len;
    begin.total_crc16 = hdr->crc16;
    begin.profile_id16 = hdr->profile_id16;
    begin.generation16 = hdr->generation16;

    cmd.cmd = AIK_SPI_CMD_PROFILE_BEGIN;
    cmd.host_seq = ++s_cmd_seq;
    aik_spi_host_cmd_set_payload(&cmd, &begin, sizeof(begin));
    aik_spi_host_cmd_finish(&cmd);

    if((v3f_ch585_link_profile_cmd(half_id, &cmd, &xfer) == 0U) ||
       (xfer.state != AIK_SPI_XFER_STATE_RECEIVING))
    {
        return 0U;
    }
    return 1U;
}

static uint8_t sync_send_chunk(uint8_t half_id, half_sync_t *sync)
{
    aik_spi_host_cmd_v1_t cmd;
    aik_spi_profile_chunk_v1_t chunk;
    aik_spi_profile_xfer_v1_t xfer;
    uint16_t remain = (uint16_t)(sync->patch_len - sync->offset);
    uint8_t take = (remain < AIK_SPI_PROFILE_CHUNK_DATA_MAX) ?
                   (uint8_t)remain : AIK_SPI_PROFILE_CHUNK_DATA_MAX;

    memset(&cmd, 0, sizeof(cmd));
    memset(&chunk, 0, sizeof(chunk));
    chunk.offset = sync->offset;
    chunk.len = take;
    memcpy(chunk.data, &sync->patch[sync->offset], take);

    cmd.cmd = AIK_SPI_CMD_PROFILE_CHUNK;
    cmd.host_seq = ++s_cmd_seq;
    aik_spi_host_cmd_set_payload(&cmd, &chunk, sizeof(chunk));
    aik_spi_host_cmd_finish(&cmd);

    if((v3f_ch585_link_profile_cmd(half_id, &cmd, &xfer) == 0U) ||
       (xfer.state != AIK_SPI_XFER_STATE_RECEIVING) ||
       (xfer.received_len != (uint16_t)(sync->offset + take)))
    {
        return 0U;
    }

    sync->offset = (uint16_t)(sync->offset + take);
    return 1U;
}

static uint8_t sync_send_commit(uint8_t half_id, half_sync_t *sync)
{
    const aik_hp_header_t *hdr = (const aik_hp_header_t *)sync->patch;
    aik_spi_host_cmd_v1_t cmd;
    aik_spi_profile_commit_v1_t commit;
    aik_spi_profile_xfer_v1_t xfer;

    memset(&cmd, 0, sizeof(cmd));
    memset(&commit, 0, sizeof(commit));
    commit.slot_id = v3f_profile_runtime_get()->active_slot;
    commit.patch_flags = sync_patch_flags();
    commit.total_len = sync->patch_len;
    commit.total_crc16 = hdr->crc16;

    cmd.cmd = AIK_SPI_CMD_PROFILE_COMMIT;
    cmd.host_seq = ++s_cmd_seq;
    aik_spi_host_cmd_set_payload(&cmd, &commit, sizeof(commit));
    aik_spi_host_cmd_finish(&cmd);

    /* The commit may block the half in a flash write; a missing ack
     * here is not a failure yet, WAIT_COMMIT polls for the outcome. */
    if(v3f_ch585_link_profile_cmd(half_id, &cmd, &xfer) != 0U)
    {
        if(xfer.state == AIK_SPI_XFER_STATE_DONE)
        {
            return 2U;
        }
        if((xfer.state & 0x80U) != 0U)
        {
            return 0U;
        }
    }
    return 1U;
}

static uint8_t sync_query_xfer(uint8_t half_id,
                               aik_spi_profile_xfer_v1_t *xfer)
{
    aik_spi_host_cmd_v1_t cmd;

    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd = AIK_SPI_CMD_PROFILE_GET_XFER;
    cmd.host_seq = ++s_cmd_seq;
    aik_spi_host_cmd_finish(&cmd);

    return v3f_ch585_link_profile_cmd(half_id, &cmd, xfer);
}

static uint8_t sync_step_half(uint8_t half_id, half_sync_t *sync)
{
    switch(sync->state)
    {
        case SYNC_STATE_DIRTY:
        {
            uint16_t len = v3f_profile_runtime_build_half_patch(
                half_id, sync->patch, sizeof(sync->patch));

            if(len == 0U)
            {
                sync->state = SYNC_STATE_IDLE;
                return 0U;
            }
            sync->patch_len = len;
            sync->offset = 0U;
            sync->retries = 0U;
            sync->commit_waits = 0U;
            sync->state = SYNC_STATE_BEGIN;
            return 0U;
        }

        case SYNC_STATE_BEGIN:
            if(sync_send_begin(half_id, sync) != 0U)
            {
                sync->retries = 0U;
                sync->state = SYNC_STATE_CHUNKS;
            }
            else
            {
                sync_fail(sync);
            }
            return 1U;

        case SYNC_STATE_CHUNKS:
            if(sync_send_chunk(half_id, sync) != 0U)
            {
                sync->retries = 0U;
                if(sync->offset >= sync->patch_len)
                {
                    sync->state = SYNC_STATE_COMMIT;
                }
            }
            else
            {
                sync_fail(sync);
                if(sync->state == SYNC_STATE_BACKOFF)
                {
                    return 1U;
                }
                /* Re-arm the whole transfer on repeated chunk trouble:
                 * the half may have dropped the session. */
                if(sync->retries == (SYNC_CMD_RETRIES / 2U))
                {
                    sync->state = SYNC_STATE_BEGIN;
                    sync->offset = 0U;
                }
            }
            return 1U;

        case SYNC_STATE_COMMIT:
        {
            uint8_t result = sync_send_commit(half_id, sync);

            if(result == 2U)
            {
                sync->state = SYNC_STATE_SYNCED;
            }
            else if(result == 1U)
            {
                sync->commit_waits = 0U;
                sync->state = SYNC_STATE_WAIT_COMMIT;
            }
            else
            {
                sync_fail(sync);
                if(sync->state != SYNC_STATE_BACKOFF)
                {
                    sync->state = SYNC_STATE_BEGIN;
                    sync->offset = 0U;
                }
            }
            return 1U;
        }

        case SYNC_STATE_WAIT_COMMIT:
        {
            aik_spi_profile_xfer_v1_t xfer;

            if(sync_query_xfer(half_id, &xfer) != 0U)
            {
                if(xfer.state == AIK_SPI_XFER_STATE_DONE)
                {
                    sync->state = SYNC_STATE_SYNCED;
                    return 1U;
                }
                if((xfer.state & 0x80U) != 0U)
                {
                    sync->state = SYNC_STATE_BEGIN;
                    sync->offset = 0U;
                    sync->retries = 0U;
                    return 1U;
                }
            }
            sync->commit_waits++;
            if(sync->commit_waits >= SYNC_COMMIT_WAIT_MAX)
            {
                sync->state = SYNC_STATE_BACKOFF;
                sync->backoff = SYNC_BACKOFF_TICKS;
            }
            return 1U;
        }

        case SYNC_STATE_BACKOFF:
            if(sync->backoff != 0U)
            {
                sync->backoff--;
            }
            if(sync->backoff == 0U)
            {
                sync->state = SYNC_STATE_DIRTY;
            }
            return 0U;

        default:
            return 0U;
    }
}

uint8_t v3f_profile_sync_poll(uint16_t host_seq)
{
    uint8_t mask = 0U;
    uint8_t half_id;

    if(v3f_profile_runtime_valid() == 0U)
    {
        return 0U;
    }

    /* Alternate sync transactions with normal polling ticks so the
     * half-state caches never go stale during a transfer. */
    if((host_seq & 1U) != 0U)
    {
        return 0U;
    }

    for(half_id = 0U; half_id <= AIK_HALF_ID_RIGHT; half_id++)
    {
        half_sync_t *sync = &s_sync[half_id];

        if((sync->state == SYNC_STATE_IDLE) ||
           (sync->state == SYNC_STATE_SYNCED))
        {
            continue;
        }
        if(sync_step_half(half_id, sync) != 0U)
        {
            mask |= (uint8_t)(1U << half_id);
            break; /* at most one sync transaction per tick */
        }
    }

    return mask;
}

void v3f_profile_sync_status_poll(uint16_t host_seq)
{
    static uint8_t next_half;
    aik_spi_profile_status_v1_t status;
    const v3f_profile_runtime_t *rt = v3f_profile_runtime_get();
    half_sync_t *sync;
    uint8_t half_id;

    if(v3f_profile_runtime_valid() == 0U)
    {
        return;
    }

    half_id = next_half;
    next_half = (uint8_t)((next_half + 1U) & 1U);
    sync = &s_sync[half_id];

    if((sync->state != SYNC_STATE_IDLE) &&
       (sync->state != SYNC_STATE_SYNCED))
    {
        return;
    }

    memset(&status, 0, sizeof(status));
    if(v3f_ch585_link_query_profile_status(half_id, host_seq,
                                           &status) == 0U)
    {
        return;
    }
    if(status.half_id != half_id)
    {
        return;
    }
    if(((status.flags & AIK_PROFILE_STATUS_FLAG_VALID) == 0U) ||
       (status.profile_id16 != rt->profile_id16) ||
       (status.generation16 != rt->generation16) ||
       (aik_spi_profile_status_active_slot(&status) != rt->active_slot))
    {
        sync->state = SYNC_STATE_DIRTY;
    }
}
