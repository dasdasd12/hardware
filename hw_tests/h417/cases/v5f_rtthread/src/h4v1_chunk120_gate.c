#include <stdint.h>

#include "h4v1_video.h"

extern void h4v1_chunk120_fail_alias(const char *reason)
    __attribute__((noreturn));

/*
 * This function is deliberately isolated from v5f_hw_test_h4v1.c.  There is
 * no LTO in the qualified build, so the caller cannot turn a successful
 * return into proof that the container is chunked and delete the legacy H4V1
 * code which anchors the known-good USB/SDRAM/LTDC link layout.
 */
void __attribute__((noipa))
h4v1_chunk120_require(const h4v1_header_t *header)
{
    uint32_t flags = ((volatile const h4v1_header_t *)header)->flags;

    __asm__ volatile("" : "+r"(flags) : : "memory");
    if((flags & H4V1_CONTAINER_CHUNKED_ABSOLUTE) == 0u)
    {
        h4v1_chunk120_fail_alias("h4v1_chunked_only");
    }
}
