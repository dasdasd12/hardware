#ifndef H417_BOARD_CONFIG_H
#define H417_BOARD_CONFIG_H

/* H417 product-board model; V3F owns these shared board-level pins. */
#define H417_BOARD_MODEL_OLD 0U
#define H417_BOARD_MODEL_NEW 1U

#ifndef H417_BOARD_MODEL
#define H417_BOARD_MODEL H417_BOARD_MODEL_NEW
#endif

#if (H417_BOARD_MODEL != H417_BOARD_MODEL_OLD) && \
    (H417_BOARD_MODEL != H417_BOARD_MODEL_NEW)
#error Unsupported H417_BOARD_MODEL.
#endif

#if H417_BOARD_MODEL == H417_BOARD_MODEL_NEW
#define H417_BOARD_MODEL_NAME                       "NEW"
#define H417_BOARD_HAS_PHYSICAL_MODE_SWITCH         1U
/* NEW routes PE12 to LED_EN; OLD/reference SDRAM designs may use it as D9. */
#define H417_BOARD_HAS_RGB_POWER_ENABLE             1U
#define H417_BOARD_HAS_CAPS_LOCK_LED                1U
#define H417_BOARD_HAS_LEGACY_FN_OUTPUT_SWITCH      0U
#define H417_BOARD_HAS_LEGACY_FN_LIGHTING           0U
#else
#define H417_BOARD_MODEL_NAME                       "OLD"
#define H417_BOARD_HAS_PHYSICAL_MODE_SWITCH         0U
#define H417_BOARD_HAS_RGB_POWER_ENABLE             0U
#define H417_BOARD_HAS_CAPS_LOCK_LED                0U
#define H417_BOARD_HAS_LEGACY_FN_OUTPUT_SWITCH      1U
#define H417_BOARD_HAS_LEGACY_FN_LIGHTING           1U
#endif

#endif /* H417_BOARD_CONFIG_H */
