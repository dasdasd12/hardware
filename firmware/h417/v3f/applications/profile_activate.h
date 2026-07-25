#ifndef V3F_PROFILE_ACTIVATE_H
#define V3F_PROFILE_ACTIVATE_H

/*
 * Shared Profile activation transaction used by the PC command path
 * and local firmware shortcuts.
 *
 * The target package is fully parsed before persistent metadata or the
 * live runtime changes. Metadata is then written first, followed by the
 * no-failure runtime commit and asynchronous CH585 synchronisation.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define V3F_PROFILE_ACTIVATE_OK           0
#define V3F_PROFILE_ACTIVATE_ERR_PARAM   -1
#define V3F_PROFILE_ACTIVATE_ERR_PACKAGE -2
#define V3F_PROFILE_ACTIVATE_ERR_META    -3

int v3f_profile_activate_slot(uint8_t slot_id);

#ifdef __cplusplus
}
#endif

#endif /* V3F_PROFILE_ACTIVATE_H */
