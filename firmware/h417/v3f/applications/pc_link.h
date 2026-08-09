#ifndef V3F_PC_LINK_H
#define V3F_PC_LINK_H

/*
 * PC configuration channel over the USBFS CDC interface.
 *
 * Line-oriented ASCII protocol (each request/response is one line):
 *
 *   AK PING
 *     -> OK PONG 1
 *   AK INFO
 *     -> OK INFO active=<slot> id16=<hex4> gen=<n> slots=<v1><v2><v3>
 *                stored=<s1><s2><s3>
 *        (slot bit 1 means usable: a valid stored package or the
 *         empty-slot factory fallback; stored bit 1 means a committed
 *         user package is present)
 *   AK SYNC <half 0..1>
 *     -> OK SYNC <half> s=<state> r=<retries> o=<offset> n=<length>
 *                w=<commit_waits> b=<backoff>
 *   AK LINK <half 0..1>
 *     -> OK LINK <half> e=<link_errors> i=<invalid_frames>
 *                c=<command_frames> p=<profile_ok> q=<profile_invalid>
 *   AK BEGIN <slot 1..3> <total_len_hex> <crc32c_hex8>
 *     -> OK BEGIN            (staging erased, upload may start)
 *   AK DATA <offset_hex> <hex_bytes>
 *     -> OK DATA <next_offset_hex>
 *   AK COMMIT
 *     -> OK COMMIT <slot>    (staging validated + copied into the slot)
 *   AK ABORT
 *     -> OK ABORT
 *   AK ACTIVATE <slot 0..3>  (0 = factory default)
 *     -> OK ACTIVATE <slot> id16=<hex4> gen=<n>
 *   AK DELETE <slot 1..3>
 *     -> OK DELETE <slot>    (active deletion falls back to factory)
 *   AK APPROVAL SHOW <tag8hex> <risk1hex> <toolhex-or-> <summaryhex-or->
 *     -> OK APPROVAL SHOW <tag8hex> risk=<hex> tool=<n> summary=<n>
 *   AK APPROVAL CLEAR <tag8hex>
 *     -> OK APPROVAL CLEAR <tag8hex>
 *   errors -> ERR <code> <detail>
 *
 * Flash erase/program happens inline (explicit foreground operation);
 * HID reporting and CH585 polling pause for tens of milliseconds
 * during BEGIN and COMMIT.
 */

#include <stdint.h>

void v3f_pc_link_poll(void);

/* Transport-independent core (exposed for host tests): execute one
 * request line; responses go through the registered writer. */
void v3f_pc_link_handle_line(const char *line);
void v3f_pc_link_set_writer(void (*writer)(const char *line));

#endif /* V3F_PC_LINK_H */
