/*
 * Host-side protocol test for the fixed V3F -> V5F approval mailbox.
 *
 * Build and run from hardware:
 *   make -C tests/host run
 */

#include <stdio.h>
#include <string.h>

#include "aik_approval_mailbox.h"

static int s_failures;

#define CHECK(condition) \
    do { \
        if(!(condition)) { \
            s_failures++; \
            printf("FAIL %s:%d: %s\n", \
                   __FILE__, __LINE__, #condition); \
        } \
    } while(0)

int main(void)
{
    volatile aik_approval_mailbox_t mailbox;
    aik_approval_payload_t payload;
    aik_approval_payload_t snapshot;
    uint32_t sequence = 0u;
    uint32_t read_sequence = 0u;

    memset((void *)&mailbox, 0xa5, sizeof(mailbox));
    aik_approval_payload_clear(&payload);
    payload.request_tag = 0x1234abcdu;
    payload.active = 1u;
    payload.selected_yes = 0u;
    payload.claude_state = AIK_CLAUDE_STATE_RUNNING;
    payload.risk = 2u;
    payload.tool_len = 4u;
    payload.summary_len = 11u;
    memcpy(payload.tool, "Bash", 4u);
    memcpy(payload.summary, "Run make -B", 11u);

    CHECK(AIK_APPROVAL_MAILBOX_ADDRESS == 0x20178800u);
    CHECK((AIK_APPROVAL_MAILBOX_ADDRESS %
           AIK_APPROVAL_MAILBOX_ALIGNMENT) == 0u);
    CHECK(sizeof(mailbox) <= AIK_APPROVAL_MAILBOX_REGION_BYTES);
    CHECK(sizeof(payload) == 148u);

    sequence = aik_approval_mailbox_publish(&mailbox, &payload, sequence);
    CHECK(sequence == 2u);
    CHECK(aik_approval_mailbox_read(&mailbox,
                                    &snapshot,
                                    &read_sequence) == 1u);
    CHECK(read_sequence == sequence);
    CHECK(memcmp(&snapshot, &payload, sizeof(payload)) == 0);
    CHECK(snapshot.claude_state == AIK_CLAUDE_STATE_RUNNING);

    /* A V3F-only restart inherits the retained value and publishes a
     * different even sequence, so a still-running V5F cannot miss idle. */
    {
        aik_approval_payload_t idle;
        uint32_t retained =
            aik_approval_mailbox_retained_sequence(&mailbox);

        aik_approval_payload_clear(&idle);
        sequence = aik_approval_mailbox_publish(&mailbox, &idle, retained);
        CHECK(retained == 2u);
        CHECK(sequence == 4u);
        CHECK(aik_approval_mailbox_read(&mailbox,
                                        &snapshot,
                                        &read_sequence) == 1u);
        CHECK(snapshot.active == 0u);
        CHECK(snapshot.claude_state == AIK_CLAUDE_STATE_OFF);
    }

    /* A partial writer snapshot is never accepted. */
    mailbox.sequence = sequence + 1u;
    CHECK(aik_approval_mailbox_retained_sequence(&mailbox) ==
          (sequence + 1u));
    CHECK(aik_approval_mailbox_read(&mailbox,
                                    &snapshot,
                                    &read_sequence) == 0u);

    sequence = aik_approval_mailbox_publish(
        &mailbox,
        &payload,
        aik_approval_mailbox_retained_sequence(&mailbox));
    CHECK(sequence == 6u);

    /* Payload corruption with a stable sequence is rejected by CRC. */
    mailbox.payload.summary[0] ^= 1;
    CHECK(aik_approval_mailbox_read(&mailbox,
                                    &snapshot,
                                    &read_sequence) == 0u);

    sequence = aik_approval_mailbox_publish(&mailbox, &payload, sequence);
    CHECK(aik_approval_mailbox_read(&mailbox,
                                    &snapshot,
                                    &read_sequence) == 1u);

    /* Bounds and printable-ASCII checks protect the V5F renderer. */
    payload.claude_state = AIK_CLAUDE_STATE_DONE + 1u;
    sequence = aik_approval_mailbox_publish(&mailbox, &payload, sequence);
    CHECK(aik_approval_mailbox_read(&mailbox,
                                    &snapshot,
                                    &read_sequence) == 0u);
    payload.claude_state = AIK_CLAUDE_STATE_DONE;

    payload.tool_len = AIK_APPROVAL_TOOL_MAX + 1u;
    sequence = aik_approval_mailbox_publish(&mailbox, &payload, sequence);
    CHECK(aik_approval_mailbox_read(&mailbox,
                                    &snapshot,
                                    &read_sequence) == 0u);

    payload.tool_len = 1u;
    payload.tool[0] = '\n';
    sequence = aik_approval_mailbox_publish(&mailbox, &payload, sequence);
    CHECK(aik_approval_mailbox_read(&mailbox,
                                    &snapshot,
                                    &read_sequence) == 0u);

    if(s_failures == 0)
    {
        printf("host approval mailbox: all checks passed\n");
        return 0;
    }
    printf("host approval mailbox: %d checks failed\n", s_failures);
    return 1;
}
