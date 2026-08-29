#include "v5f_spi1_lease.h"

#include <rtthread.h>

#include "aik_spi1_bus_arbiter.h"
#include "ch32h417_hsem.h"

typedef enum
{
    V5F_SPI1_LEASE_IDLE = 0,
    V5F_SPI1_LEASE_REQUESTED,
    V5F_SPI1_LEASE_OWNED,
    V5F_SPI1_LEASE_RELEASING,
} v5f_spi1_lease_state_t;

typedef struct
{
    v5f_spi1_lease_state_t state;
    uint32_t epoch;
    uint32_t completed_bytes;
    uint32_t progress_count;
    uint8_t hsem_owned;
} v5f_spi1_lease_context_t;

static v5f_spi1_lease_context_t s_lease;

static rt_tick_t timeout_ticks(uint32_t timeout_ms)
{
    uint64_t scaled = (uint64_t)timeout_ms * RT_TICK_PER_SECOND;
    rt_tick_t ticks = (rt_tick_t)((scaled + 999u) / 1000u);

    return (ticks != 0u) ? ticks : 1u;
}

static uint8_t wait_expired(rt_tick_t start, rt_tick_t limit)
{
    return (uint8_t)((rt_tick_t)(rt_tick_get() - start) >= limit);
}

static uint8_t request_header_valid(
    const volatile aik_spi1_arb_request_t *request)
{
    return (uint8_t)((request->magic == AIK_SPI1_ARB_MAGIC) &&
                     (request->version == AIK_SPI1_ARB_VERSION) &&
                     (request->slot_bytes == AIK_SPI1_ARB_SLOT_BYTES));
}

static uint8_t response_header_valid(
    const volatile aik_spi1_arb_response_t *response)
{
    return (uint8_t)((response->magic == AIK_SPI1_ARB_MAGIC) &&
                     (response->version == AIK_SPI1_ARB_VERSION) &&
                     (response->slot_bytes == AIK_SPI1_ARB_SLOT_BYTES));
}

static uint32_t hsem_owner(void)
{
    aik_spi1_arb_fence();
    return HSEM->RX[AIK_SPI1_ARB_HSEM_ID];
}

/* V5F is the sole runtime writer of the request cache line. */
static void publish_request(uint32_t command,
                            uint32_t flags,
                            uint32_t detail0,
                            uint32_t detail1)
{
    volatile aik_spi1_arb_request_t *request =
        &AIK_SPI1_ARB_MAILBOX->request;

    /* Retire the previous committed command before changing its metadata. */
    request->command = AIK_SPI1_ARB_CMD_IDLE;
    aik_spi1_arb_fence();
    request->magic = AIK_SPI1_ARB_MAGIC;
    request->version = AIK_SPI1_ARB_VERSION;
    request->slot_bytes = AIK_SPI1_ARB_SLOT_BYTES;
    request->epoch = s_lease.epoch;
    request->flags = flags;
    request->detail0 = detail0;
    request->detail1 = detail1;
    aik_spi1_arb_fence();
    /* Command is the request commit word and must be written last. */
    request->command = command;
    aik_spi1_arb_fence();
}

static uint8_t response_is(uint32_t epoch, uint32_t state)
{
    const volatile aik_spi1_arb_response_t *response =
        &AIK_SPI1_ARB_MAILBOX->response;
    uint32_t committed_state;

    aik_spi1_arb_fence();
    committed_state = response->state;
    aik_spi1_arb_fence();
    if((response_header_valid(response) == 0u) ||
       (response->epoch != epoch) ||
       (response->error != AIK_SPI1_ARB_ERROR_NONE))
    {
        return 0u;
    }
    aik_spi1_arb_fence();
    return (uint8_t)((response->state == committed_state) &&
                     (committed_state == state));
}

static uint8_t response_failed(uint32_t epoch)
{
    const volatile aik_spi1_arb_response_t *response =
        &AIK_SPI1_ARB_MAILBOX->response;
    uint32_t state;

    aik_spi1_arb_fence();
    state = response->state;
    aik_spi1_arb_fence();
    return (uint8_t)((response_header_valid(response) != 0u) &&
                     (response->epoch == epoch) &&
                     ((state == AIK_SPI1_ARB_STATE_REJECTED) ||
                      (state == AIK_SPI1_ARB_STATE_FATAL) ||
                      (response->error != AIK_SPI1_ARB_ERROR_NONE)));
}

static int wait_for_v3_active(uint32_t timeout_ms,
                              uint8_t require_restored)
{
    const volatile aik_spi1_arb_response_t *response =
        &AIK_SPI1_ARB_MAILBOX->response;
    rt_tick_t start = rt_tick_get();
    rt_tick_t limit = timeout_ticks(timeout_ms);

    while(wait_expired(start, limit) == 0u)
    {
        if((response_is(s_lease.epoch, AIK_SPI1_ARB_STATE_V3_ACTIVE) != 0u) &&
           ((require_restored == 0u) ||
            ((response->flags & AIK_SPI1_ARB_RESPONSE_RESTORED) != 0u)) &&
           (response->hsem_snapshot == AIK_SPI1_ARB_HSEM_V3F_OWNER) &&
           (hsem_owner() == AIK_SPI1_ARB_HSEM_V3F_OWNER))
        {
            return V5F_SPI1_LEASE_OK;
        }
        rt_thread_mdelay(1);
    }
    return V5F_SPI1_LEASE_ERR_RELEASE;
}

static int cancel_request(uint32_t timeout_ms, int requested_result)
{
    int active_result;

    if(s_lease.hsem_owned != 0u)
    {
        HSEM_ReleaseOneSem(HSEM_ID31, AIK_SPI1_ARB_HSEM_V5F_PID);
        aik_spi1_arb_fence();
        s_lease.hsem_owned = 0u;
    }
    publish_request(AIK_SPI1_ARB_CMD_CANCEL,
                    V5F_SPI1_LEASE_RELEASE_CLEAN,
                    s_lease.completed_bytes,
                    s_lease.progress_count);
    s_lease.state = V5F_SPI1_LEASE_RELEASING;
    active_result = wait_for_v3_active(timeout_ms, 0u);
    if(active_result == V5F_SPI1_LEASE_OK)
    {
        s_lease.state = V5F_SPI1_LEASE_IDLE;
        s_lease.completed_bytes = 0u;
        s_lease.progress_count = 0u;
        return requested_result;
    }
    return active_result;
}

int v5f_spi1_lease_acquire(uint32_t timeout_ms,
                           v5f_spi1_lease_continue_fn_t continue_fn)
{
    const volatile aik_spi1_arb_request_t *request =
        &AIK_SPI1_ARB_MAILBOX->request;
    const volatile aik_spi1_arb_response_t *response =
        &AIK_SPI1_ARB_MAILBOX->response;
    rt_tick_t start;
    rt_tick_t limit;

    if(s_lease.state != V5F_SPI1_LEASE_IDLE)
    {
        return V5F_SPI1_LEASE_ERR_PROTOCOL;
    }
    if((continue_fn != RT_NULL) && (continue_fn() == 0u))
    {
        return V5F_SPI1_LEASE_ERR_CANCELLED;
    }

    start = rt_tick_get();
    limit = timeout_ticks(timeout_ms);
    while(wait_expired(start, limit) == 0u)
    {
        aik_spi1_arb_fence();
        if((response_header_valid(response) != 0u) &&
           (response->state == AIK_SPI1_ARB_STATE_V3_ACTIVE) &&
           (response->error == AIK_SPI1_ARB_ERROR_NONE) &&
           (hsem_owner() == AIK_SPI1_ARB_HSEM_V3F_OWNER))
        {
            break;
        }
        if((continue_fn != RT_NULL) && (continue_fn() == 0u))
        {
            return V5F_SPI1_LEASE_ERR_CANCELLED;
        }
        rt_thread_mdelay(1);
    }
    if(wait_expired(start, limit) != 0u)
    {
        return V5F_SPI1_LEASE_ERR_TIMEOUT;
    }

    aik_spi1_arb_fence();
    if(request_header_valid(request) == 0u)
    {
        return V5F_SPI1_LEASE_ERR_PROTOCOL;
    }
    s_lease.epoch = request->epoch + 1u;
    if(s_lease.epoch == 0u)
    {
        s_lease.epoch = 1u;
    }
    s_lease.completed_bytes = 0u;
    s_lease.progress_count = 0u;
    s_lease.hsem_owned = 0u;
    s_lease.state = V5F_SPI1_LEASE_REQUESTED;
    publish_request(AIK_SPI1_ARB_CMD_ACQUIRE, 0u, 0u, 0u);

    start = rt_tick_get();
    while(wait_expired(start, limit) == 0u)
    {
        if((continue_fn != RT_NULL) && (continue_fn() == 0u))
        {
            return cancel_request(timeout_ms,
                                  V5F_SPI1_LEASE_ERR_CANCELLED);
        }
        if(response_failed(s_lease.epoch) != 0u)
        {
            return cancel_request(timeout_ms,
                                  V5F_SPI1_LEASE_ERR_PROTOCOL);
        }
        if(response_is(s_lease.epoch,
                       AIK_SPI1_ARB_STATE_QUIESCED) != 0u)
        {
            if(HSEM_Take(HSEM_ID31,
                         AIK_SPI1_ARB_HSEM_V5F_PID) == READY)
            {
                aik_spi1_arb_fence();
                if(hsem_owner() == AIK_SPI1_ARB_HSEM_V5F_OWNER)
                {
                    s_lease.hsem_owned = 1u;
                    s_lease.state = V5F_SPI1_LEASE_OWNED;
                    publish_request(AIK_SPI1_ARB_CMD_OWNED, 0u, 0u, 0u);
                    return V5F_SPI1_LEASE_OK;
                }
            }
        }
        rt_thread_mdelay(1);
    }
    return cancel_request(timeout_ms, V5F_SPI1_LEASE_ERR_TIMEOUT);
}

int v5f_spi1_lease_progress(uint32_t completed_bytes)
{
    if((s_lease.state != V5F_SPI1_LEASE_OWNED) ||
       (s_lease.hsem_owned == 0u) ||
       (hsem_owner() != AIK_SPI1_ARB_HSEM_V5F_OWNER))
    {
        return V5F_SPI1_LEASE_ERR_HSEM;
    }
    s_lease.completed_bytes = completed_bytes;
    s_lease.progress_count++;
    publish_request(AIK_SPI1_ARB_CMD_OWNED,
                    0u,
                    s_lease.completed_bytes,
                    s_lease.progress_count);
    return V5F_SPI1_LEASE_OK;
}

int v5f_spi1_lease_release(uint32_t release_flags, uint32_t timeout_ms)
{
    uint8_t hsem_lost = 0u;
    int active_result;

    if(s_lease.state == V5F_SPI1_LEASE_IDLE)
    {
        return V5F_SPI1_LEASE_OK;
    }
    if(s_lease.hsem_owned != 0u)
    {
        if(hsem_owner() != AIK_SPI1_ARB_HSEM_V5F_OWNER)
        {
            hsem_lost = 1u;
            release_flags = V5F_SPI1_LEASE_RELEASE_DIRTY;
        }
        else
        {
            HSEM_ReleaseOneSem(HSEM_ID31,
                               AIK_SPI1_ARB_HSEM_V5F_PID);
            aik_spi1_arb_fence();
        }
        s_lease.hsem_owned = 0u;
    }
    publish_request(AIK_SPI1_ARB_CMD_RELEASE,
                    release_flags,
                    s_lease.completed_bytes,
                    s_lease.progress_count);
    s_lease.state = V5F_SPI1_LEASE_RELEASING;
    active_result = wait_for_v3_active(timeout_ms, 1u);
    if(active_result != V5F_SPI1_LEASE_OK)
    {
        return active_result;
    }

    s_lease.state = V5F_SPI1_LEASE_IDLE;
    s_lease.completed_bytes = 0u;
    s_lease.progress_count = 0u;
    return (hsem_lost == 0u) ? V5F_SPI1_LEASE_OK :
                               V5F_SPI1_LEASE_ERR_HSEM;
}

uint8_t v5f_spi1_lease_owned(void)
{
    return (uint8_t)((s_lease.state == V5F_SPI1_LEASE_OWNED) &&
                     (s_lease.hsem_owned != 0u) &&
                     (hsem_owner() == AIK_SPI1_ARB_HSEM_V5F_OWNER));
}
