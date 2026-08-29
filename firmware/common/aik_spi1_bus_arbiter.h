#ifndef AIK_SPI1_BUS_ARBITER_H
#define AIK_SPI1_BUS_ARBITER_H

#include <stdint.h>

/*
 * Cross-core ownership contract for the physical SPI1 pins.
 *
 * V3F normally owns SPI1 for the two CH585 links.  V5F may borrow it for a
 * bounded SPI-NAND preload only after V3F has parked PB3/PB4/PB5 and published
 * QUIESCED.  HSEM31 is the actual exclusion primitive; this mailbox only
 * carries the quiesce/restart handshake.  The two 64-byte slots have one
 * writer each so neither core ever performs a read/modify/write on the other
 * core's cache line.
 *
 * Publication rule: a writer first retires its old commit word, writes all
 * metadata, executes aik_spi1_arb_fence(), and writes command/state last.
 * A reader samples the commit word on both sides of the metadata read.
 */
#define AIK_SPI1_ARB_MAILBOX_ADDR       0x20179000UL
#define AIK_SPI1_ARB_MAILBOX_BYTES      128U
#define AIK_SPI1_ARB_SLOT_BYTES         64U
#define AIK_SPI1_ARB_MAGIC              0x31425241UL /* "ARB1" */
#define AIK_SPI1_ARB_VERSION            1U

#define AIK_SPI1_ARB_HSEM_ID            31U
#define AIK_SPI1_ARB_HSEM_V3F_PID       0x31U
#define AIK_SPI1_ARB_HSEM_V5F_PID       0x51U
#define AIK_SPI1_ARB_HSEM_LOCK          0x80000000UL
#define AIK_SPI1_ARB_HSEM_V3F_CORE      0x00000000UL
#define AIK_SPI1_ARB_HSEM_V5F_CORE      0x00000100UL
#define AIK_SPI1_ARB_HSEM_V3F_OWNER \
    (AIK_SPI1_ARB_HSEM_LOCK | AIK_SPI1_ARB_HSEM_V3F_CORE | \
     AIK_SPI1_ARB_HSEM_V3F_PID)
#define AIK_SPI1_ARB_HSEM_V5F_OWNER \
    (AIK_SPI1_ARB_HSEM_LOCK | AIK_SPI1_ARB_HSEM_V5F_CORE | \
     AIK_SPI1_ARB_HSEM_V5F_PID)

/* Written only by V5F after V3F has published V3_ACTIVE. */
#define AIK_SPI1_ARB_CMD_IDLE           0U
#define AIK_SPI1_ARB_CMD_ACQUIRE        1U
#define AIK_SPI1_ARB_CMD_OWNED          2U
#define AIK_SPI1_ARB_CMD_RELEASE        3U
#define AIK_SPI1_ARB_CMD_CANCEL         4U

/* Written only by V3F.  V3_ACTIVE with the requested epoch is the resume ACK. */
#define AIK_SPI1_ARB_STATE_RESET        0U
#define AIK_SPI1_ARB_STATE_V3_ACTIVE    1U
#define AIK_SPI1_ARB_STATE_QUIESCED     2U
#define AIK_SPI1_ARB_STATE_REJECTED     3U
#define AIK_SPI1_ARB_STATE_FATAL        4U

#define AIK_SPI1_ARB_RESPONSE_RESTORED  (1UL << 0)

#define AIK_SPI1_ARB_ERROR_NONE             0U
#define AIK_SPI1_ARB_ERROR_HSEM_BOOT_BUSY   1U
#define AIK_SPI1_ARB_ERROR_SPI_BUSY_TIMEOUT 2U
#define AIK_SPI1_ARB_ERROR_HSEM_RETAKE      3U
#define AIK_SPI1_ARB_ERROR_BAD_REQUEST      4U

typedef struct __attribute__((aligned(64)))
{
    uint32_t magic;
    uint32_t version;
    uint32_t slot_bytes;
    uint32_t epoch;
    uint32_t command;
    uint32_t flags;
    uint32_t detail0;
    uint32_t detail1;
    uint32_t reserved[8];
} aik_spi1_arb_request_t;

typedef struct __attribute__((aligned(64)))
{
    uint32_t magic;
    uint32_t version;
    uint32_t slot_bytes;
    uint32_t epoch;
    uint32_t state;
    uint32_t error;
    uint32_t hsem_snapshot;
    uint32_t transition_count;
    uint32_t flags;
    uint32_t reserved[7];
} aik_spi1_arb_response_t;

typedef struct __attribute__((aligned(64)))
{
    aik_spi1_arb_request_t request;
    aik_spi1_arb_response_t response;
} aik_spi1_arb_mailbox_t;

typedef char aik_spi1_arb_request_size_must_be_64[
    (sizeof(aik_spi1_arb_request_t) == AIK_SPI1_ARB_SLOT_BYTES) ? 1 : -1];
typedef char aik_spi1_arb_response_size_must_be_64[
    (sizeof(aik_spi1_arb_response_t) == AIK_SPI1_ARB_SLOT_BYTES) ? 1 : -1];
typedef char aik_spi1_arb_mailbox_size_must_be_128[
    (sizeof(aik_spi1_arb_mailbox_t) == AIK_SPI1_ARB_MAILBOX_BYTES) ? 1 : -1];

#define AIK_SPI1_ARB_MAILBOX \
    ((volatile aik_spi1_arb_mailbox_t *)(uintptr_t)AIK_SPI1_ARB_MAILBOX_ADDR)

static inline void aik_spi1_arb_fence(void)
{
    __asm volatile("fence iorw, iorw" ::: "memory");
}

#endif
