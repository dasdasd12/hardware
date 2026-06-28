#include "ch585_sideband.h"

#include <stddef.h>
#include <string.h>

static void put_u16_le(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8U);
}

static uint16_t get_u16_le(const uint8_t *src)
{
    return (uint16_t)(((uint16_t)src[1] << 8U) | src[0]);
}

uint8_t ch585_sideband_crc8(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint8_t crc = 0U;

    if ((data == NULL) && (len != 0U))
    {
        return 0U;
    }

    for (i = 0U; i < len; i++)
    {
        uint8_t bit;
        crc ^= data[i];
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1U) ^ 0x07U)
                                : (uint8_t)(crc << 1U);
        }
    }

    return crc;
}

int ch585_sideband_encode(const ch585_sideband_frame_t *frame,
                          uint8_t *wire,
                          uint16_t wire_cap,
                          uint16_t *wire_len)
{
    uint8_t event_count;
    uint16_t len;
    uint8_t i;

    if ((frame == NULL) || (wire == NULL) || (wire_len == NULL))
    {
        return CH585_SIDEBAND_STATUS_PARAM;
    }

    event_count = frame->event_count;
    if (event_count > CH585_SIDEBAND_MAX_EVENTS)
    {
        event_count = CH585_SIDEBAND_MAX_EVENTS;
    }
    len = (uint16_t)(CH585_SIDEBAND_FIXED_SIZE +
                     (event_count * CH585_SIDEBAND_EVENT_SIZE) + 1U);
    if (wire_cap < len)
    {
        return CH585_SIDEBAND_STATUS_SHORT;
    }

    memset(wire, 0, len);
    wire[0] = CH585_SIDEBAND_MAGIC;
    wire[1] = CH585_SIDEBAND_VERSION;
    wire[2] = CH585_SIDEBAND_TYPE_STATUS;
    wire[3] = frame->source_id;
    wire[4] = frame->seq;
    wire[5] = frame->flags;
    wire[6] = event_count;
    wire[7] = frame->alert_flags;
    put_u16_le(&wire[8], frame->vbat_mv);
    put_u16_le(&wire[10], frame->soc_q8_percent);
    put_u16_le(&wire[12], frame->input_mask);
    wire[14] = frame->charge_state;
    wire[15] = 0U;

    for (i = 0U; i < event_count; i++)
    {
        uint16_t off = (uint16_t)(CH585_SIDEBAND_FIXED_SIZE +
                                  (i * CH585_SIDEBAND_EVENT_SIZE));
        wire[off + 0U] = frame->events[i].type;
        wire[off + 1U] = frame->events[i].id;
        wire[off + 2U] = (uint8_t)frame->events[i].value;
        wire[off + 3U] = frame->events[i].flags;
    }

    wire[len - 1U] = ch585_sideband_crc8(wire, (uint16_t)(len - 1U));
    *wire_len = len;
    return CH585_SIDEBAND_STATUS_OK;
}

int ch585_sideband_decode(const uint8_t *wire,
                          uint16_t wire_len,
                          ch585_sideband_frame_t *frame)
{
    uint8_t event_count;
    uint16_t expected_len;
    uint8_t i;

    if ((wire == NULL) || (frame == NULL))
    {
        return CH585_SIDEBAND_STATUS_PARAM;
    }
    if (wire_len < (CH585_SIDEBAND_FIXED_SIZE + 1U))
    {
        return CH585_SIDEBAND_STATUS_SHORT;
    }
    if ((wire[0] != CH585_SIDEBAND_MAGIC) ||
        (wire[1] != CH585_SIDEBAND_VERSION) ||
        (wire[2] != CH585_SIDEBAND_TYPE_STATUS))
    {
        return CH585_SIDEBAND_STATUS_MAGIC;
    }

    event_count = wire[6];
    if (event_count > CH585_SIDEBAND_MAX_EVENTS)
    {
        return CH585_SIDEBAND_STATUS_PARAM;
    }

    expected_len = (uint16_t)(CH585_SIDEBAND_FIXED_SIZE +
                              (event_count * CH585_SIDEBAND_EVENT_SIZE) + 1U);
    if (wire_len < expected_len)
    {
        return CH585_SIDEBAND_STATUS_SHORT;
    }
    if (ch585_sideband_crc8(wire, (uint16_t)(expected_len - 1U)) !=
        wire[expected_len - 1U])
    {
        return CH585_SIDEBAND_STATUS_CRC;
    }

    memset(frame, 0, sizeof(*frame));
    frame->source_id = wire[3];
    frame->seq = wire[4];
    frame->flags = wire[5];
    frame->event_count = event_count;
    frame->alert_flags = wire[7];
    frame->vbat_mv = get_u16_le(&wire[8]);
    frame->soc_q8_percent = get_u16_le(&wire[10]);
    frame->input_mask = get_u16_le(&wire[12]);
    frame->charge_state = wire[14];

    for (i = 0U; i < event_count; i++)
    {
        uint16_t off = (uint16_t)(CH585_SIDEBAND_FIXED_SIZE +
                                  (i * CH585_SIDEBAND_EVENT_SIZE));
        frame->events[i].type = wire[off + 0U];
        frame->events[i].id = wire[off + 1U];
        frame->events[i].value = (int8_t)wire[off + 2U];
        frame->events[i].flags = wire[off + 3U];
    }

    return CH585_SIDEBAND_STATUS_OK;
}
