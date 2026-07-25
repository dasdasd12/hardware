#ifndef AIK_APPROVAL_MAILBOX_H
#define AIK_APPROVAL_MAILBOX_H

#include <stdint.h>

/*
 * V3F -> V5F single-writer/single-reader approval and Claude state snapshot.
 *
 * 0x20178000..0x201780ff is occupied by the V3F trace block and the
 * experimental V5F USB paths use memory through 0x2017863f.  Keep this
 * mailbox at the next 2 KiB-aligned application-owned window.
 */
#define AIK_APPROVAL_MAILBOX_ADDRESS       0x20178800u
#define AIK_APPROVAL_MAILBOX_REGION_BYTES  2048u
#define AIK_APPROVAL_MAILBOX_ALIGNMENT     64u

#define AIK_APPROVAL_MAILBOX_MAGIC         0x56525041u /* "APRV" */
#define AIK_APPROVAL_MAILBOX_VERSION       1u

#define AIK_APPROVAL_TOOL_MAX              16u
#define AIK_APPROVAL_SUMMARY_MAX           120u
#define AIK_APPROVAL_RISK_MAX              0x0fu

#define AIK_CLAUDE_STATE_OFF               0u
#define AIK_CLAUDE_STATE_RUNNING           1u
#define AIK_CLAUDE_STATE_DONE              2u

typedef struct
{
    uint32_t request_tag;
    uint8_t active;
    uint8_t selected_yes;
    uint8_t risk;
    uint8_t tool_len;
    uint8_t summary_len;
    /* Reuses one formerly reserved byte; payload size and version stay fixed. */
    uint8_t claude_state;
    uint8_t reserved[2];
    char tool[AIK_APPROVAL_TOOL_MAX];
    char summary[AIK_APPROVAL_SUMMARY_MAX];
} aik_approval_payload_t;

typedef struct __attribute__((aligned(AIK_APPROVAL_MAILBOX_ALIGNMENT)))
{
    uint32_t magic;
    uint16_t version;
    uint16_t struct_size;
    volatile uint32_t sequence;
    uint16_t payload_crc16;
    uint16_t reserved;
    aik_approval_payload_t payload;
} aik_approval_mailbox_t;

typedef char aik_approval_mailbox_size_check[
    (sizeof(aik_approval_mailbox_t) <= AIK_APPROVAL_MAILBOX_REGION_BYTES) ?
        1 : -1];
typedef char aik_approval_mailbox_address_alignment_check[
    ((AIK_APPROVAL_MAILBOX_ADDRESS %
      AIK_APPROVAL_MAILBOX_ALIGNMENT) == 0u) ? 1 : -1];

#define AIK_APPROVAL_MAILBOX \
    ((volatile aik_approval_mailbox_t *)(uintptr_t) \
        AIK_APPROVAL_MAILBOX_ADDRESS)

static inline void aik_approval_mailbox_barrier(void)
{
#if defined(__riscv)
    __asm volatile("fence iorw, iorw" ::: "memory");
#else
    __sync_synchronize();
#endif
}

static inline void aik_approval_payload_clear(aik_approval_payload_t *payload)
{
    uint8_t *bytes = (uint8_t *)payload;
    uint32_t index;

    for(index = 0u; index < (uint32_t)sizeof(*payload); index++)
    {
        bytes[index] = 0u;
    }
}

static inline uint8_t aik_approval_text_is_ascii(const char *text,
                                                 uint8_t length)
{
    uint8_t index;

    for(index = 0u; index < length; index++)
    {
        uint8_t value = (uint8_t)text[index];

        if((value < 0x20u) || (value > 0x7eu))
        {
            return 0u;
        }
    }
    return 1u;
}

/* CRC-16/CCITT-FALSE over the fixed-size, zero-padded payload. */
static inline uint16_t aik_approval_payload_crc16(
    const aik_approval_payload_t *payload)
{
    const uint8_t *bytes = (const uint8_t *)payload;
    uint16_t crc = 0xffffu;
    uint32_t index;

    for(index = 0u; index < (uint32_t)sizeof(*payload); index++)
    {
        uint8_t bit;

        crc ^= (uint16_t)((uint16_t)bytes[index] << 8);
        for(bit = 0u; bit < 8u; bit++)
        {
            crc = (uint16_t)((crc & 0x8000u) != 0u ?
                (uint16_t)((crc << 1) ^ 0x1021u) :
                (uint16_t)(crc << 1));
        }
    }
    return crc;
}

/*
 * Publish one complete snapshot.  Odd sequence values mean a writer owns the
 * payload; the final even value makes the snapshot visible to V5F.
 */
static inline uint32_t aik_approval_mailbox_publish(
    volatile aik_approval_mailbox_t *mailbox,
    const aik_approval_payload_t *payload,
    uint32_t current_sequence)
{
    const uint8_t *source = (const uint8_t *)payload;
    volatile uint8_t *destination =
        (volatile uint8_t *)&mailbox->payload;
    uint32_t next_sequence =
        (uint32_t)((current_sequence & ~1u) + 2u);
    uint32_t index;

    if(next_sequence == 0u)
    {
        next_sequence = 2u;
    }

    mailbox->sequence = next_sequence - 1u;
    aik_approval_mailbox_barrier();

    mailbox->magic = AIK_APPROVAL_MAILBOX_MAGIC;
    mailbox->version = AIK_APPROVAL_MAILBOX_VERSION;
    mailbox->struct_size = (uint16_t)sizeof(*mailbox);
    mailbox->reserved = 0u;
    for(index = 0u; index < (uint32_t)sizeof(*payload); index++)
    {
        destination[index] = source[index];
    }
    mailbox->payload_crc16 = aik_approval_payload_crc16(payload);

    aik_approval_mailbox_barrier();
    mailbox->sequence = next_sequence;
    aik_approval_mailbox_barrier();
    return next_sequence;
}

/*
 * Preserve sequence monotonicity across a V3F-only reset. Header fields are
 * enough for this seed: an odd retained value means reset interrupted a
 * publish, and the next publish still advances past the last visible even
 * value.
 */
static inline uint32_t aik_approval_mailbox_retained_sequence(
    const volatile aik_approval_mailbox_t *mailbox)
{
    uint32_t sequence = mailbox->sequence;

    aik_approval_mailbox_barrier();
    if((sequence == 0u) ||
       (mailbox->magic != AIK_APPROVAL_MAILBOX_MAGIC) ||
       (mailbox->version != AIK_APPROVAL_MAILBOX_VERSION) ||
       (mailbox->struct_size != (uint16_t)sizeof(*mailbox)))
    {
        return 0u;
    }
    return sequence;
}

/*
 * Copy a coherent snapshot.  The caller keeps its previous display when this
 * returns zero (writer active, invalid retained SRAM, or CRC mismatch).
 */
static inline uint8_t aik_approval_mailbox_read(
    const volatile aik_approval_mailbox_t *mailbox,
    aik_approval_payload_t *payload,
    uint32_t *sequence_out)
{
    const volatile uint8_t *source =
        (const volatile uint8_t *)&mailbox->payload;
    uint8_t *destination = (uint8_t *)payload;
    uint32_t sequence_before;
    uint32_t sequence_after;
    uint32_t magic;
    uint16_t version;
    uint16_t struct_size;
    uint16_t payload_crc16;
    uint32_t index;

    sequence_before = mailbox->sequence;
    if((sequence_before == 0u) || ((sequence_before & 1u) != 0u))
    {
        return 0u;
    }

    aik_approval_mailbox_barrier();
    magic = mailbox->magic;
    version = mailbox->version;
    struct_size = mailbox->struct_size;
    payload_crc16 = mailbox->payload_crc16;
    for(index = 0u; index < (uint32_t)sizeof(*payload); index++)
    {
        destination[index] = source[index];
    }
    aik_approval_mailbox_barrier();

    sequence_after = mailbox->sequence;
    if((sequence_before != sequence_after) ||
       ((sequence_after & 1u) != 0u) ||
       (magic != AIK_APPROVAL_MAILBOX_MAGIC) ||
       (version != AIK_APPROVAL_MAILBOX_VERSION) ||
       (struct_size != (uint16_t)sizeof(*mailbox)) ||
       (payload_crc16 != aik_approval_payload_crc16(payload)) ||
       (payload->active > 1u) ||
       (payload->selected_yes > 1u) ||
       (payload->risk > AIK_APPROVAL_RISK_MAX) ||
       (payload->claude_state > AIK_CLAUDE_STATE_DONE) ||
       (payload->tool_len > AIK_APPROVAL_TOOL_MAX) ||
       (payload->summary_len > AIK_APPROVAL_SUMMARY_MAX) ||
       (aik_approval_text_is_ascii(payload->tool,
                                   payload->tool_len) == 0u) ||
       (aik_approval_text_is_ascii(payload->summary,
                                   payload->summary_len) == 0u))
    {
        return 0u;
    }

    if(sequence_out != 0)
    {
        *sequence_out = sequence_after;
    }
    return 1u;
}

#endif /* AIK_APPROVAL_MAILBOX_H */
