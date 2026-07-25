from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


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
