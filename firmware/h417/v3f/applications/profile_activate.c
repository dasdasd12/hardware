#include "profile_activate.h"

#include <string.h>

#include "aik_profile_format.h"
#include "profile_runtime.h"
#include "profile_store.h"
#include "profile_sync.h"

/*
 * V3F has a 2KB linker stack. Keep the full runtime candidate out of the
 * call stack because activation also enters the Flash erase/write path.
 * Activation is foreground-only and therefore non-reentrant.
 */
static v3f_profile_runtime_t s_activation_candidate;

int v3f_profile_activate_slot(uint8_t slot_id)
{
    const v3f_profile_runtime_t *current;

    if(slot_id >= AIK_PROFILE_SLOT_COUNT_TOTAL)
    {
        return V3F_PROFILE_ACTIVATE_ERR_PARAM;
    }

    if(v3f_profile_runtime_prepare_slot(slot_id,
                                        &s_activation_candidate) !=
       V3F_PROFILE_OK)
    {
        return V3F_PROFILE_ACTIVATE_ERR_PACKAGE;
    }

    current = v3f_profile_runtime_get();
    if((current->valid != 0U) &&
       (v3f_profile_store_get_active_slot() == slot_id) &&
       (memcmp(current, &s_activation_candidate,
               sizeof(s_activation_candidate)) == 0))
    {
        return V3F_PROFILE_ACTIVATE_OK;
    }

    /*
     * Commit order is intentional: preparation above cannot alter the
     * live runtime, and the runtime commit below cannot fail. A metadata
     * write error therefore leaves both the visible bindings and rearm
     * state unchanged.
     */
    if(v3f_profile_store_set_active_slot(slot_id) != V3F_PROFILE_STORE_OK)
    {
        return V3F_PROFILE_ACTIVATE_ERR_META;
    }

    v3f_profile_runtime_commit_candidate(&s_activation_candidate);
    v3f_profile_sync_mark_all_dirty();
    return V3F_PROFILE_ACTIVATE_OK;
}
