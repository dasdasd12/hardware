#ifndef V5F_FLASH_ANIMATION_H
#define V5F_FLASH_ANIMATION_H

#include <stdint.h>

/* PE14/CTRL_OUT is active low: low selects animation, high selects product UI. */
#define V5F_FLASH_ANIMATION_OK                  0
#define V5F_FLASH_ANIMATION_ERR_DISPLAY        (-1)

void v5f_flash_animation_init(void);
void v5f_flash_animation_poll(void);
uint8_t v5f_flash_animation_requested(void);
uint8_t v5f_flash_animation_failure_latched(void);
const char *v5f_flash_animation_last_failure(void);

/*
 * Blocking service used by the product display owner.  The H4V1 player must
 * call v5f_flash_animation_should_continue() at frame boundaries so a stable
 * UI selection can terminate playback without another LTDC owner appearing.
 */
int v5f_flash_animation_run_blocking(void);
uint8_t v5f_flash_animation_should_continue(void);

#endif /* V5F_FLASH_ANIMATION_H */
