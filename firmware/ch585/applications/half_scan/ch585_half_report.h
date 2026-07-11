#ifndef CH585_HALF_REPORT_H
#define CH585_HALF_REPORT_H

#include <stdint.h>

#include "aik_spi_protocol.h"

void ch585_half_report_build_nkro16(const aik_spi_half_state_v1_t *left,
                                    const aik_spi_half_state_v1_t *right,
                                    uint8_t nkro16[AIK_NKRO_REPORT_BYTES]);
uint16_t ch585_half_report_consumer_usage(const aik_spi_half_state_v1_t *left,
                                          const aik_spi_half_state_v1_t *right);
int8_t ch585_half_report_mouse_wheel(const aik_spi_half_state_v1_t *left,
                                     const aik_spi_half_state_v1_t *right);

/* Runtime table management. The factory table stays compiled in; the
 * active copies are replaced when an AKHR patch is applied. */
void ch585_half_report_reset_factory(void);
void ch585_half_report_set_key_outputs(
    const uint8_t pairs[AIK_KEY_COUNT_TOTAL * 2U]);
void ch585_half_report_clear_locals(void);
void ch585_half_report_set_local(uint8_t signal_id, uint8_t target_kind,
                                 uint16_t value);

/* Release-to-rearm after a table swap in wireless composition: keys
 * held right now stay suppressed until physically released. */
void ch585_half_report_arm_release_gate(const aik_spi_half_state_v1_t *left,
                                        const aik_spi_half_state_v1_t *right);

#endif
