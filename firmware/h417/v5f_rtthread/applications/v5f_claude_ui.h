#ifndef V5F_CLAUDE_UI_H
#define V5F_CLAUDE_UI_H

#include <stdint.h>

/* claude_state is AIK_CLAUDE_STATE_RUNNING or AIK_CLAUDE_STATE_DONE. */
void v5f_claude_ui_draw(uint8_t claude_state);

#endif /* V5F_CLAUDE_UI_H */
