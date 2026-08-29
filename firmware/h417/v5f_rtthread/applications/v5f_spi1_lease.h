#ifndef V5F_SPI1_LEASE_H
#define V5F_SPI1_LEASE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t (*v5f_spi1_lease_continue_fn_t)(void);

enum
{
    V5F_SPI1_LEASE_OK = 0,
    V5F_SPI1_LEASE_ERR_TIMEOUT = -1,
    V5F_SPI1_LEASE_ERR_CANCELLED = -2,
    V5F_SPI1_LEASE_ERR_PROTOCOL = -3,
    V5F_SPI1_LEASE_ERR_HSEM = -4,
    V5F_SPI1_LEASE_ERR_RELEASE = -5
};

enum
{
    V5F_SPI1_LEASE_RELEASE_CLEAN = 0u,
    V5F_SPI1_LEASE_RELEASE_DIRTY = 1u
};

/*
 * Acquire the cross-core SPI1 lease.  This call must complete before the
 * caller reads or changes SPI1, DMA1 CH2/CH3, their DMAMUX fields, or the
 * NAND GPIO pins.  continue_fn may be null; a zero return cancels the wait.
 */
int v5f_spi1_lease_acquire(uint32_t timeout_ms,
                           v5f_spi1_lease_continue_fn_t continue_fn);

/* Publish forward progress while a long NAND preload owns the lease. */
int v5f_spi1_lease_progress(uint32_t completed_bytes);

/*
 * Release is idempotent.  DIRTY tells V3F to rebuild its SPI1/CH585 state
 * even when the V5F peripheral restore path timed out.
 */
int v5f_spi1_lease_release(uint32_t release_flags, uint32_t timeout_ms);

uint8_t v5f_spi1_lease_owned(void);

#ifdef __cplusplus
}
#endif

#endif /* V5F_SPI1_LEASE_H */
