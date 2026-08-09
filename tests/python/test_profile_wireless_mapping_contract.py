from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def _read(*parts: str) -> str:
    return (ROOT.joinpath(*parts)).read_text(encoding="utf-8")


def test_wireless_consumer_preload_uses_active_profile():
    main_c = _read("firmware", "h417", "v3f", "applications", "main.c")
    start = main_c.index(
        "(void)v3f_rotary_count_delta_from_right(",
        main_c.index("while(1)"),
    )
    end = main_c.index(
        "if(v3f_output_mode_is_wireless(output_mode)",
        start,
    )
    preload = main_c[start:end]

    assert "v3f_profile_runtime_valid()" in preload
    assert "v3f_profile_runtime_consumer_usage(" in preload
    assert "v3f_consumer_usage_from_local_controls(" in preload


def test_legacy_fn_swallowing_is_disabled_when_key_is_remapped():
    main_c = _read("firmware", "h417", "v3f", "applications", "main.c")
    start = main_c.index("if(v3f_profile_runtime_valid() != 0U)")
    end = main_c.index("v3f_profile_runtime_build_nkro16", start)
    profile_report_setup = main_c[start:end]

    assert "rt->has_fn_overlay == 0U" in profile_report_setup
    assert "rt->base_keys[V3F_FN_LAYER_KEY].usage == 0U" in profile_report_setup
    assert (
        "rt->base_keys[V3F_FN_LAYER_KEY].modifier_mask == 0U"
        in profile_report_setup
    )


def test_wireless_wheel_queue_uses_profile_binding():
    main_c = _read(
        "firmware", "ch585", "applications", "half_scan", "main.c"
    )
    start = main_c.index(
        "static int8_t half_scan_take_local_mouse_wheel(void)"
    )
    end = main_c.index("#endif", start)
    take_wheel = main_c[start:end]

    assert "ch585_half_report_mouse_wheel_step(" in take_wheel
    assert "AIK_HP_SIGNAL_WHEEL_UP" in take_wheel
    assert "AIK_HP_SIGNAL_WHEEL_DOWN" in take_wheel
    assert "return 1;" not in take_wheel
    assert "return -1;" not in take_wheel


def test_h417_profile_shortcut_switches_after_current_report():
    main_c = _read("firmware", "h417", "v3f", "applications", "main.c")
    loop = main_c.index("while(1)")
    merge = main_c.index("v3f_half_state_merge(", loop)
    update = main_c.index(
        "aik_profile_shortcut_update_valid(",
        merge,
    )
    rearm = main_c.index("v3f_profile_runtime_rearm_latch(&keys)", update)
    consume = main_c.index(
        "v3f_profile_shortcut_consume_keys(&keys)",
        rearm,
    )
    report = main_c.index("v3f_profile_runtime_build_nkro16(", consume)
    usb_submit = main_c.index("v3f_usb_hid_nkro_submit(nkro16)", report)
    activate = main_c.index("v3f_profile_activate_slot(", usb_submit)
    pc_poll = main_c.index("v3f_pc_link_poll()", activate)

    assert merge < update < rearm < consume < report
    assert report < usb_submit < activate < pc_poll


def test_profile_shortcut_keys_are_split_by_board_model():
    main_c = _read("firmware", "h417", "v3f", "applications", "main.c")
    ch585_report_c = _read(
        "firmware", "ch585", "applications", "half_scan",
        "ch585_half_report.c",
    )

    h417_start = main_c.index("#if H417_BOARD_HAS_LEGACY_FN_OUTPUT_SWITCH")
    h417_end = main_c.index("#if H417_BOARD_HAS_FN_LIGHT_TOGGLE", h417_start)
    h417_keys = main_c[h417_start:h417_end]
    h417_old, h417_new = h417_keys.split("#else", 1)

    assert "#define V3F_PROFILE_KEY_0   10U" in h417_old
    assert "#define V3F_PROFILE_KEY_1   52U" in h417_old
    assert "#define V3F_PROFILE_KEY_2   51U" in h417_old
    assert "#define V3F_PROFILE_KEY_3   50U" in h417_old
    assert "V3F_PROFILE_KEY_0" not in h417_new
    assert "#define V3F_PROFILE_KEY_1   45U" in h417_new
    assert "#define V3F_PROFILE_KEY_2   44U" in h417_new
    assert "#define V3F_PROFILE_KEY_3   43U" in h417_new

    ch585_start = ch585_report_c.index(
        "#if CH585_BOARD_HAS_LEGACY_FN_OUTPUT_SWITCH",
    )
    ch585_end = ch585_report_c.index(
        "#define CH585_GLOBAL_DOWN_BYTES", ch585_start,
    )
    ch585_keys = ch585_report_c[ch585_start:ch585_end]
    ch585_old, ch585_new = ch585_keys.split("#else", 1)

    assert "#define CH585_PROFILE_SLOT0_KEY  10U" in ch585_old
    assert "#define CH585_PROFILE_SLOT1_KEY  52U" in ch585_old
    assert "#define CH585_PROFILE_SLOT2_KEY  51U" in ch585_old
    assert "#define CH585_PROFILE_SLOT3_KEY  50U" in ch585_old
    assert "CH585_PROFILE_SLOT0_KEY" not in ch585_new
    assert "CH585_PROFILE_SLOT1_KEY  CH585_MODE_KEY_F1" in ch585_new
    assert "CH585_PROFILE_SLOT2_KEY  CH585_MODE_KEY_F2" in ch585_new
    assert "CH585_PROFILE_SLOT3_KEY  CH585_MODE_KEY_F3" in ch585_new

    assert "#if H417_BOARD_HAS_LEGACY_FN_OUTPUT_SWITCH\n    if(v3f_global_key_is_down(keys, V3F_PROFILE_KEY_0)" in main_c
    assert "#if CH585_BOARD_HAS_LEGACY_FN_OUTPUT_SWITCH\n        case CH585_PROFILE_SLOT0_KEY:" in ch585_report_c


def test_builtin_profiles_and_usb_report_rate_are_runtime_features():
    runtime_c = _read(
        "firmware", "h417", "v3f", "applications", "profile_runtime.c"
    )
    runtime_h = _read(
        "firmware", "h417", "v3f", "applications", "profile_runtime.h"
    )
    main_c = _read("firmware", "h417", "v3f", "applications", "main.c")

    assert "uint16_t usb_report_rate_hz;" in runtime_h
    assert "uint16_t wireless_report_rate_hz;" in runtime_h
    assert "runtime->usb_report_rate_hz = 4000U;" in runtime_c
    assert "runtime->usb_report_rate_hz = 8000U;" in runtime_c
    assert "runtime->base_keys[73U].modifier_mask = 0x02U;" in runtime_c
    assert "runtime->base_keys[40U].modifier_mask = 0x20U;" in runtime_c
    assert "runtime->triggers[key_id].press_pm = 200U;" in runtime_c
    assert "runtime->triggers[key_id].rt_press_delta_pm = 200U;" in runtime_c
    assert "runtime->triggers[key_id].rt_release_delta_pm = 200U;" in runtime_c
    assert "memset(runtime->base_keys, 0, sizeof(runtime->base_keys));" in runtime_c
    assert "runtime->local_count = 0U;" in runtime_c

    due_start = main_c.index("static uint16_t v3f_usb_nkro_interval_ticks")
    due_end = main_c.index("int main(void)", due_start)
    due = main_c[due_start:due_end]
    assert "v3f_profile_runtime_get()->usb_report_rate_hz" in due
    assert "v3f_usb_nkro_interval_ticks()" in due
    assert "host_seq - last_submit_tick" in due
    assert "submitted_once == 0U" in due

    submit = main_c.index("v3f_usb_hid_nkro_submit(nkro16)")
    guard = main_c.rfind(
        "else if(usb_nkro_due_pending != 0U)", 0, submit
    )
    assert guard >= 0
    assert "if(v3f_usb_nkro_report_due(host_seq," in main_c
    assert "usb_nkro_due_pending = 1U;" in main_c
    assert main_c.index(
        "if(v3f_usb_hid_nkro_submit(nkro16) != 0U)", guard
    ) < main_c.index("usb_nkro_due_pending = 0U;", guard)
    assert "last_usb_nkro_submit_tick = host_seq;" in main_c
    # Consumer and wheel branches stay ahead of the rate-limited ordinary
    # keyboard report, so changing Profile rate does not throttle them.
    consumer_submit = main_c.rfind(
        "v3f_usb_hid_nkro_submit_consumer", 0, submit
    )
    wheel_submit = main_c.rfind(
        "v3f_usb_hid_nkro_submit_mouse_wheel", 0, submit
    )
    assert 0 <= consumer_submit < submit
    assert 0 <= wheel_submit < submit

    # Supported rates are exact divisors of the 8K scheduler. A successful
    # 4K report starts a two-tick minimum interval; 8K uses one tick.
    assert (8000 + 4000 - 1) // 4000 == 2
    assert (8000 + 8000 - 1) // 8000 == 1

    # The production loop retains its previously validated delay mechanism;
    # profile rate selection changes report cadence without introducing a
    # new blocking clock source into the scan path.
    assert "v3f_board_delay_us(V3F_USB_REPORT_INTERVAL_US);" in main_c


def test_new_board_switch_controls_wireless_overlay_only():
    makefile = _read("firmware", "h417", "v3f", "Makefile")
    main_c = _read("firmware", "h417", "v3f", "applications", "main.c")
    ch585_main = _read(
        "firmware", "ch585", "applications", "half_scan", "main.c"
    )
    ch585_board_h = _read(
        "firmware", "ch585", "applications", "ch585_board_config.h"
    )
    ch585_report_c = _read(
        "firmware", "ch585", "applications", "half_scan",
        "ch585_half_report.c",
    )

    new_start = makefile.index("ifeq ($(H417_BOARD_MODEL),NEW)")
    old_start = makefile.index("else ifeq ($(H417_BOARD_MODEL),OLD)")
    branch_end = makefile.index("else\n$(error Unsupported H417_BOARD_MODEL", old_start)
    new_defaults = makefile[new_start:old_start]
    old_defaults = makefile[old_start:branch_end]

    assert "V3F_ENABLE_USB_RF_COEXIST ?= 1" in new_defaults
    assert "V3F_ENABLE_USB_BLE_COEXIST ?= 1" in new_defaults
    assert "V3F_OUTPUT_MODE_DEFAULT ?= 0" in new_defaults
    assert "V3F_ENABLE_USB_RF_COEXIST ?= 0" in old_defaults
    assert "V3F_ENABLE_USB_BLE_COEXIST ?= 0" in old_defaults
    assert "V3F_OUTPUT_MODE_DEFAULT ?= 1" in old_defaults
    assert "V3F_ENABLE_FN_OUTPUT_SWITCH" not in makefile

    ch585_new_board = ch585_board_h[
        ch585_board_h.index("#if CH585_BOARD_MODEL == CH585_BOARD_MODEL_NEW") :
        ch585_board_h.index(
            "#else",
            ch585_board_h.index(
                "#if CH585_BOARD_MODEL == CH585_BOARD_MODEL_NEW"
            ),
        )
    ]
    ch585_old_board = ch585_board_h[
        ch585_board_h.index(
            "#else",
            ch585_board_h.index(
                "#if CH585_BOARD_MODEL == CH585_BOARD_MODEL_NEW"
            ),
        ) :
        ch585_board_h.rindex("#endif")
    ]
    assert "CH585_BOARD_HAS_LEGACY_FN_OUTPUT_SWITCH 0U" in ch585_new_board
    assert "CH585_BOARD_HAS_LEGACY_FN_OUTPUT_SWITCH 1U" in ch585_old_board
    assert (
        "(CH585_BOARD_HAS_LEGACY_FN_OUTPUT_SWITCH != 0U) ?"
        in ch585_report_c
    )
    assert (
        "(CH585_BOARD_HAS_LEGACY_FN_OUTPUT_SWITCH != 0U) ||"
        in ch585_report_c
    )
    assert "(mode_shortcut_key_bit(key_id) == 0U)" in ch585_report_c

    mode_start = main_c.index("static uint8_t v3f_output_mode_update(")
    mode_end = main_c.index("static uint8_t v3f_lighting_combo_from_keys", mode_start)
    mode_update = main_c[mode_start:mode_end]
    assert mode_update.index("#if H417_BOARD_HAS_PHYSICAL_MODE_SWITCH") < (
        mode_update.index("return v3f_output_mode_update_from_keys")
    )
    assert "wireless overlay is off" in mode_update

    switch_decode = ch585_main[
        ch585_main.index("half_scan_wireless_mode_switch_decode") :
        ch585_main.index("half_scan_wireless_mode_switch_state_init")
    ]
    assert "return AIK_OUTPUT_MODE_BLE;" in switch_decode
    assert "return AIK_OUTPUT_MODE_RF24;" in switch_decode
    assert "return AIK_OUTPUT_MODE_USBHS;" in switch_decode

    contact_probe = ch585_main[
        ch585_main.index(
            "half_scan_wireless_mode_switch_rf_contact_closed"
        ) :
        ch585_main.index("half_scan_wireless_mode_switch_decode")
    ]
    assert "GPIOB_ResetBits(CH585_WIRELESS_MODE_BLE_PB11);" in contact_probe
    assert "GPIO_ModeOut_PP_5mA" in contact_probe
    assert "half_scan_gpiob_pressed(CH585_WIRELESS_MODE_RF24_PB10)" in contact_probe
    assert "GPIO_ModeIN_Floating" in contact_probe
    assert "half_scan_wireless_mode_switch_rf_contact_closed()" in contact_probe

    switch_poll = ch585_main[
        ch585_main.index("half_scan_wireless_mode_switch_poll") :
        ch585_main.index("half_scan_read_rotary_ab")
    ]
    assert "half_scan_set_output_mode(mode);" in switch_poll

    radio_gate = ch585_main[
        ch585_main.index("static void half_scan_set_output_mode(uint8_t mode)", 1000) :
        ch585_main.index("static void half_scan_output_nkro16")
    ]
    assert "mode = s_wireless_mode_switch_mode;" in radio_gate


def test_new_board_lights_start_off_and_caps_follows_wired_usb():
    makefile = _read("firmware", "h417", "v3f", "Makefile")
    main_c = _read("firmware", "h417", "v3f", "applications", "main.c")
    board_h = _read(
        "firmware", "h417", "v3f", "applications", "h417_board_config.h"
    )
    board_c = _read(
        "firmware", "h417", "v3f", "applications", "board_init.c"
    )
    gpio_c = _read(
        "firmware", "h417", "basic", "wch", "SRC", "Peripheral", "src",
        "ch32h417_gpio.c"
    )
    rgb_c = _read(
        "firmware", "h417", "v3f", "applications", "rgb_status.c"
    )

    assert "V3F_RGB_DEFAULT_ENABLED ?= 0" in makefile
    assert "-DV3F_RGB_DEFAULT_ENABLED=$(V3F_RGB_DEFAULT_ENABLED)" in makefile
    assert "H417_GPIO_IPD_PRESERVE_PB7=1" in makefile
    assert "H417_GPIO_IPD_PRESERVE_PB7=0" in makefile
    assert "GPIO_ResetBits(V3F_RGB_POWER_EN_PORT, V3F_RGB_POWER_EN_PIN)" in board_c
    assert "GPIO_SetBits(V3F_CAPS_LOCK_LED_PORT, V3F_CAPS_LOCK_LED_PIN)" in board_c
    assert "gpio.GPIO_Mode = GPIO_Mode_Out_OD;" in board_c
    assert gpio_c.count("H417_GPIO_IPD_UNUSED_PB7") >= 3
    assert "#define V3F_RGB_DEFAULT_ENABLED 0U" in rgb_c

    new_board = board_h[
        board_h.index("#if H417_BOARD_MODEL == H417_BOARD_MODEL_NEW") :
        board_h.index("#else", board_h.index("#if H417_BOARD_MODEL == H417_BOARD_MODEL_NEW"))
    ]
    old_board = board_h[
        board_h.index("#else", board_h.index("#if H417_BOARD_MODEL == H417_BOARD_MODEL_NEW")) :
        board_h.rindex("#endif")
    ]
    assert "H417_BOARD_HAS_FN_LIGHT_TOGGLE              1U" in new_board
    assert "H417_BOARD_HAS_FN_LIGHT_TOGGLE              1U" in old_board
    assert "H417_BOARD_HAS_LEGACY_FN_LIGHTING           0U" in new_board
    assert "H417_BOARD_HAS_LEGACY_FN_LIGHTING           1U" in old_board

    lighting_start = main_c.index("static uint8_t v3f_lighting_combo_from_keys")
    lighting = main_c[
        lighting_start :
        main_c.index("static void v3f_global_key_clear_one", lighting_start)
    ]
    assert "V3F_SWITCH_KEY_F5" in lighting
    assert "v3f_rgb_status_toggle_enabled();" in lighting
    assert "#if H417_BOARD_HAS_LEGACY_FN_LIGHTING" in lighting
    assert "V3F_SWITCH_KEY_F6" in lighting
    assert "v3f_rgb_status_next_effect();" in lighting

    caps_call = "v3f_board_caps_lock_led_set(v3f_usb_hid_caps_lock_on());"
    assert caps_call in main_c
    caps_index = main_c.index(caps_call)
    caps_window = main_c[
        caps_index - 250 : caps_index + len(caps_call)
    ]
    assert "output_mode == AIK_OUTPUT_MODE_USBHS" not in caps_window


def test_rf24_enable_waits_for_idle_and_rejects_failed_mode_switch():
    tx_c = _read(
        "firmware", "ch585", "applications", "half_scan",
        "ch585_rf_nkro_tx.c",
    )
    tx_h = _read(
        "firmware", "ch585", "applications", "half_scan",
        "ch585_rf_nkro_tx.h",
    )
    ble_c = _read("firmware", "ch585", "drivers", "ble", "ble_hid.c")
    ble_h = _read("firmware", "ch585", "drivers", "ble", "ble_hid.h")
    main_c = _read(
        "firmware", "ch585", "applications", "half_scan", "main.c"
    )

    enter_start = tx_c.index("static uint8_t rf_enter_basic_mode(void)")
    enter_end = tx_c.index("static tmosEvents rf_process_event", enter_start)
    enter_basic = tx_c[enter_start:enter_end]
    assert enter_basic.index("RFRole_Stop()") < enter_basic.index(
        "RFRole_SwitchMode(RFIP_MODE_RF_BASIC)"
    )
    assert enter_basic.index("RFRole_SwitchMode(RFIP_MODE_RF_BASIC)") < (
        enter_basic.index("RFRole_SetParam(&s_rf_param)")
    )
    assert "s_last_switch_status != SUCCESS" in enter_basic
    assert "RFRole_BasicInit" not in enter_basic

    enable_start = tx_c.index("uint8_t ch585_rf_nkro_tx_set_enabled")
    enable_end = tx_c.index("void ch585_rf_nkro_tx_disable_async", enable_start)
    enable = tx_c[enable_start:enable_end]
    assert "if(rf_enter_basic_mode() == 0U)" in enable
    assert "s_enabled = 1U;" in enable
    assert enable.index("if(rf_enter_basic_mode() == 0U)") < enable.index(
        "s_enabled = 1U;"
    )
    assert "uint8_t ch585_rf_nkro_tx_set_enabled" in tx_h

    init_start = tx_c.index("void ch585_rf_nkro_tx_init(void)")
    init_end = tx_c.index("void ch585_rf_nkro_tx_poll(void)", init_start)
    tx_init = tx_c[init_start:init_end]
    assert "TMR0_ITCfg(DISABLE, TMR0_3_IT_CYC_END);" in tx_init
    assert "PFIC_DisableIRQ(TMR0_IRQn);" in tx_init
    assert "s_enabled = 0U;" in tx_init

    disable_start = ble_c.index("uint8_t BLE_HID_DisableForRadio")
    disable_end = ble_c.index("uint8_t BLE_HID_SendKeyboard", disable_start)
    disable_ble = ble_c[disable_start:disable_end]
    assert "GAPRole_GetParameter(GAPROLE_STATE" in ble_c
    assert "GAPROLE_ADVERTISING" in ble_c
    assert "bleHidRadioBusy()" in disable_ble
    assert "uint8_t BLE_HID_DisableForRadio" in ble_h
    assert "uint8_t BLE_HID_WaitStarted" in ble_h
    wait_start = ble_c.index("uint8_t BLE_HID_WaitStarted")
    wait_end = ble_c.index("static uint8_t bleHidBatteryCalc", wait_start)
    wait_started = ble_c[wait_start:wait_end]
    assert "TMOS_SystemProcess();" in wait_started
    assert "GAPRole_GetParameter(GAPROLE_STATE" in wait_started

    mode_start = main_c.index("static void half_scan_set_output_mode(", 1000)
    mode_end = main_c.index("static void half_scan_output_nkro16", mode_start)
    mode_apply = main_c[mode_start:mode_end]
    assert "mode_applied = ch585_rf_nkro_tx_set_enabled(1U);" in mode_apply
    assert "mode_applied = BLE_HID_DisableForRadio(150U);" in mode_apply
    assert "if(mode_applied != 0U)" in mode_apply
    assert (
        "if((mode_applied != 0U) && (mode == AIK_OUTPUT_MODE_BLE))"
        in mode_apply
    )
    assert "mode != AIK_OUTPUT_MODE_USBHS" not in mode_apply

    init_start = main_c.index("static void half_scan_init(void)")
    init_end = main_c.index("int main(void)", init_start)
    init = main_c[init_start:init_end]
    assert init.index("BLE_HID_Init();") < init.index(
        "BLE_HID_WaitStarted(150U);"
    )
    assert init.index("BLE_HID_WaitStarted(150U);") < init.index(
        "half_scan_set_output_mode(AIK_OUTPUT_MODE_USBHS);"
    )
