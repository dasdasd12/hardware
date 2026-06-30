#ifndef AIK_PROFILE_RUNTIME_H
#define AIK_PROFILE_RUNTIME_H

#include <stdint.h>
#include <stddef.h>

#include "aik_spi_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AIK_PROFILE_RUNTIME_MAGIC 0x52504B41UL
#define AIK_PROFILE_RUNTIME_VERSION 1U
#define AIK_PROFILE_MAG_CONFIG_COUNT AIK_KEY_COUNT_TOTAL

#if defined(__GNUC__)
#define AIK_PROFILE_PACKED __attribute__((packed))
#else
#define AIK_PROFILE_PACKED
#endif

typedef struct AIK_PROFILE_PACKED
{
    uint8_t usage;
    uint8_t modifier_mask;
} aik_profile_key_output_v1_t;

typedef struct AIK_PROFILE_PACKED
{
    uint16_t released_adc;
    uint16_t pressed_adc;
    uint16_t press_pm;
    uint16_t release_pm;
    uint16_t rt_press_delta_pm;
    uint16_t rt_release_delta_pm;
    uint8_t filter_shift;
    uint8_t mode;
} aik_profile_mag_config_v1_t;

typedef struct AIK_PROFILE_PACKED
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t generation;
    uint16_t key_count;
    uint16_t flags;
    uint16_t mag_config_count;
    uint16_t reserved;
    aik_profile_key_output_v1_t key_output[AIK_KEY_COUNT_TOTAL];
    aik_profile_mag_config_v1_t mag_config[AIK_PROFILE_MAG_CONFIG_COUNT];
    uint16_t crc16;
} aik_profile_runtime_v1_t;

#define AIK_PROFILE_RUNTIME_SIZE ((uint16_t)sizeof(aik_profile_runtime_v1_t))

typedef char aik_profile_runtime_v1_size_fits_nand_page[
    (AIK_PROFILE_RUNTIME_SIZE <= 2048U) ? 1 : -1];

static inline uint16_t aik_profile_runtime_crc(
    const aik_profile_runtime_v1_t *runtime)
{
    return aik_spi_crc16_ccitt((const uint8_t *)runtime,
                               (uint16_t)offsetof(aik_profile_runtime_v1_t, crc16));
}

static inline uint8_t aik_profile_runtime_valid(
    const aik_profile_runtime_v1_t *runtime)
{
    return (runtime != 0) &&
           (runtime->magic == AIK_PROFILE_RUNTIME_MAGIC) &&
           (runtime->version == AIK_PROFILE_RUNTIME_VERSION) &&
           (runtime->size == AIK_PROFILE_RUNTIME_SIZE) &&
           (runtime->key_count == AIK_KEY_COUNT_TOTAL) &&
           (runtime->mag_config_count == AIK_PROFILE_MAG_CONFIG_COUNT) &&
           (runtime->crc16 == aik_profile_runtime_crc(runtime));
}

#ifdef __cplusplus
}
#endif

#endif
