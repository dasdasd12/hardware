#ifndef V3F_APPROVAL_MAILBOX_H
#define V3F_APPROVAL_MAILBOX_H

#include "aik_approval_mailbox.h"

#define V3F_APPROVAL_MAILBOX_OK          0
#define V3F_APPROVAL_MAILBOX_ERR_PARAM  (-1)
#define V3F_APPROVAL_MAILBOX_ERR_STATE  (-2)
#define V3F_APPROVAL_MAILBOX_ERR_TAG    (-3)

void v3f_approval_mailbox_init(void);
uint8_t v3f_approval_mailbox_active(void);
uint8_t v3f_approval_mailbox_selected_yes(void);
void v3f_approval_mailbox_set_selected_yes(uint8_t selected_yes);

int v3f_approval_mailbox_show(uint32_t request_tag,
                              uint8_t risk,
                              const char *tool,
                              uint16_t tool_len,
                              const char *summary,
                              uint16_t summary_len);
int v3f_approval_mailbox_clear(uint32_t request_tag);

#endif /* V3F_APPROVAL_MAILBOX_H */
