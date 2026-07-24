#include "approval_mailbox.h"

#include "aik_approval_mailbox.h"

static aik_approval_payload_t s_payload;
static uint32_t s_sequence;

static void publish_payload(void)
{
    s_sequence = aik_approval_mailbox_publish(AIK_APPROVAL_MAILBOX,
                                               &s_payload,
                                               s_sequence);
}

void v3f_approval_mailbox_init(void)
{
    s_sequence =
        aik_approval_mailbox_retained_sequence(AIK_APPROVAL_MAILBOX);
    aik_approval_payload_clear(&s_payload);
    publish_payload();
}

uint8_t v3f_approval_mailbox_active(void)
{
    return s_payload.active;
}

uint8_t v3f_approval_mailbox_selected_yes(void)
{
    return s_payload.selected_yes;
}

void v3f_approval_mailbox_set_selected_yes(uint8_t selected_yes)
{
    uint8_t normalized = (uint8_t)(selected_yes != 0u);

    if((s_payload.active == 0u) ||
       (s_payload.selected_yes == normalized))
    {
        return;
    }

    s_payload.selected_yes = normalized;
    publish_payload();
}

int v3f_approval_mailbox_show(uint32_t request_tag,
                              uint8_t risk,
                              const char *tool,
                              uint16_t tool_len,
                              const char *summary,
                              uint16_t summary_len)
{
    uint8_t claude_state = s_payload.claude_state;
    uint16_t index;

    if((risk > AIK_APPROVAL_RISK_MAX) ||
       (tool_len > AIK_APPROVAL_TOOL_MAX) ||
       (summary_len > AIK_APPROVAL_SUMMARY_MAX) ||
       ((tool_len != 0u) && (tool == 0)) ||
       ((summary_len != 0u) && (summary == 0)) ||
       (aik_approval_text_is_ascii(tool, (uint8_t)tool_len) == 0u) ||
       (aik_approval_text_is_ascii(summary, (uint8_t)summary_len) == 0u))
    {
        return V3F_APPROVAL_MAILBOX_ERR_PARAM;
    }

    aik_approval_payload_clear(&s_payload);
    s_payload.claude_state = claude_state;
    s_payload.request_tag = request_tag;
    s_payload.active = 1u;
    s_payload.selected_yes = 0u; /* Every new request defaults to No. */
    s_payload.risk = risk;
    s_payload.tool_len = (uint8_t)tool_len;
    s_payload.summary_len = (uint8_t)summary_len;
    for(index = 0u; index < tool_len; index++)
    {
        s_payload.tool[index] = tool[index];
    }
    for(index = 0u; index < summary_len; index++)
    {
        s_payload.summary[index] = summary[index];
    }
    publish_payload();
    return V3F_APPROVAL_MAILBOX_OK;
}

int v3f_approval_mailbox_clear(uint32_t request_tag)
{
    uint8_t claude_state;

    if(s_payload.active == 0u)
    {
        return V3F_APPROVAL_MAILBOX_ERR_STATE;
    }
    if(s_payload.request_tag != request_tag)
    {
        return V3F_APPROVAL_MAILBOX_ERR_TAG;
    }

    claude_state = s_payload.claude_state;
    aik_approval_payload_clear(&s_payload);
    s_payload.claude_state = claude_state;
    publish_payload();
    return V3F_APPROVAL_MAILBOX_OK;
}

int v3f_approval_mailbox_set_claude_state(uint8_t claude_state)
{
    if(claude_state > AIK_CLAUDE_STATE_DONE)
    {
        return V3F_APPROVAL_MAILBOX_ERR_PARAM;
    }

    if(claude_state == AIK_CLAUDE_STATE_OFF)
    {
        if((s_payload.claude_state == AIK_CLAUDE_STATE_OFF) &&
           (s_payload.active == 0u))
        {
            return V3F_APPROVAL_MAILBOX_OK;
        }

        /* Process exit invalidates any approval that belonged to it. */
        aik_approval_payload_clear(&s_payload);
        publish_payload();
        return V3F_APPROVAL_MAILBOX_OK;
    }

    if(s_payload.claude_state == claude_state)
    {
        return V3F_APPROVAL_MAILBOX_OK;
    }

    s_payload.claude_state = claude_state;
    publish_payload();
    return V3F_APPROVAL_MAILBOX_OK;
}
