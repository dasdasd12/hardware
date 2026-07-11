#ifndef CH585_PROFILE_H
#define CH585_PROFILE_H

/*
 * CH585-side profile slot storage and runtime application.
 *
 * Each half persists up to 3 user AKHR patches in internal Data-Flash
 * (EEPROM API) plus an active-slot record; slot 0 is the factory
 * default compiled into this firmware (never written):
 *
 *   0x0000  user slot 1 (4KB)
 *   0x1000  user slot 2
 *   0x2000  user slot 3
 *   0x3000  active-slot record (256B page)
 *
 * The BLE SNV region starts at 0x7000 and is never touched here.
 *
 * The H417 pushes patches through the AIK_SPI_CMD_PROFILE_* command
 * group; ch585_profile_handle_transfer_cmd() implements the receive
 * state machine and ch585_profile_fill_xfer() builds the response
 * frame acknowledging each command.
 */

#include <stdint.h>

#include "aik_profile_format.h"
#include "magnetic_key_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CH585_PROFILE_EEPROM_SLOT_SIZE 0x1000UL
#define CH585_PROFILE_EEPROM_META_ADDR 0x3000UL

typedef struct
{
    uint8_t active_slot;      /* 0 = factory built-in */
    uint8_t patch_applied;    /* a user AKHR is live (not factory) */
    uint16_t profile_id16;
    uint16_t generation16;
} ch585_profile_state_t;

/* Boot path: restore active slot from Data-Flash, fall back to the
 * factory defaults on any validation failure. */
void ch585_profile_boot_load(mag_key_engine_t *engine);

const ch585_profile_state_t *ch585_profile_state(void);

/* Fill the (already zeroed) status frame fields other than ack_seq. */
void ch585_profile_fill_status(aik_spi_profile_status_v1_t *status);

/* SPI receive path for AIK_SPI_CMD_PROFILE_*. */
void ch585_profile_handle_transfer_cmd(const aik_spi_host_cmd_v1_t *cmd,
                                       mag_key_engine_t *engine);
void ch585_profile_fill_xfer(aik_spi_profile_xfer_v1_t *xfer);

#ifdef __cplusplus
}
#endif

#endif /* CH585_PROFILE_H */
