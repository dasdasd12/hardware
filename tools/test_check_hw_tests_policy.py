import io
import os
import re


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
CHECK_SCRIPT = os.path.join(ROOT, "tools", "check_hw_tests.py")
H417_MAKEFILE = os.path.join(ROOT, "hw_tests", "h417", "Makefile")
H417_USB_CDC_SOURCE = os.path.join(
    ROOT, "firmware", "h417", "v5f_rtthread", "applications", "usb_cdc_dual.c"
)
V5F_HW_TEST_SOURCE = os.path.join(
    ROOT, "hw_tests", "h417", "passed", "v5f_rtthread", "src", "v5f_hw_test.c"
)


def read_check_script():
    with io.open(CHECK_SCRIPT, "r", encoding="utf-8") as handle:
        return handle.read()


def read_h417_makefile():
    with io.open(H417_MAKEFILE, "r", encoding="utf-8") as handle:
        return handle.read()


def read_h417_usb_cdc_source():
    with io.open(H417_USB_CDC_SOURCE, "r", encoding="utf-8") as handle:
        return handle.read()


def read_v5f_hw_test_source():
    with io.open(V5F_HW_TEST_SOURCE, "r", encoding="utf-8") as handle:
        return handle.read()


def test_ch585_adc_and_spi_are_not_forbidden_keywords():
    text = read_check_script()

    assert "CH585 ADC use" not in text
    assert "CH585 SPI use" not in text
    assert not re.search(r"forbidden_ch585\s*=\s*{", text)


def test_ch585_firmware_test_residue_is_checked_explicitly():
    text = read_check_script()

    assert "assert_ch585_firmware_has_no_test_residue" in text
    assert "CH585 firmware test residue" in text


def test_h417_sdram_tests_stay_in_hw_tests_until_driver_cleanup():
    text = read_check_script()

    assert "h417_v5f_sdram_memtest" in text
    assert "h417_v5f_sdram_ltdc_rgb565" in text
    assert "h417_v5f_sdram_remap_probe" in text
    assert "h417_v5f_sdram_dq_probe" in text
    assert "firmware/h417/drivers/sdram" in text
    assert "SDRAM bring-up must stay in hw_tests" in text


def test_h417_sdram_tests_do_not_depend_on_uart_console():
    text = read_v5f_hw_test_source()

    assert 'rt_kprintf("SDRAM' not in text
    assert "sdram_status_lcd_start" in text
    assert "sdram_status_show" in text


def test_h417_sdram_failure_screen_encodes_stage_and_error():
    text = read_v5f_hw_test_source()

    assert "sdram_status_fail_show" in text
    assert "sdram_status_error_count" in text
    assert "V5F_SDRAM_STATUS_FAIL" not in text


def test_h417_sdram_failure_screen_shows_expected_and_actual_bits():
    text = read_v5f_hw_test_source()

    assert "sdram_status_word_bits_show" in text
    assert "sdram_probe_data_bus_show" in text
    assert "sdram_status_word_bits_show(g_v5f_hw_test_diag.sdram_expected" in text
    assert "sdram_status_word_bits_show(g_v5f_hw_test_diag.sdram_actual" in text


def test_h417_sdram_clock_period_uses_official_fmc_values():
    text = read_v5f_hw_test_source()
    start = text.index("static uint32_t sdram_select_clock_period")
    end = text.index("static uint16_t sdram_refresh_count")
    clock_period_function = text[start:end]

    assert "FMC_SDClockPeriod_2HCLK" in clock_period_function
    assert "FMC_SDClockPeriod_3HCLK" in clock_period_function
    assert "return 1u;" not in clock_period_function


def test_h417_sdram_init_does_not_write_raw_misc_bits():
    text = read_v5f_hw_test_source()
    start = text.index("static int sdram_init")
    end = text.index("static uint32_t sdram_pattern")
    sdram_init_function = text[start:end]

    assert "FMC_Bank5_6->MISC |=" not in sdram_init_function
    assert "FMC_ENHANCE_READ_MODE_Disable" in sdram_init_function


def test_h417_sdram_init_enables_fmc_global_gate():
    text = read_v5f_hw_test_source()
    start = text.index("static int sdram_init")
    end = text.index("static uint32_t sdram_pattern")
    sdram_init_function = text[start:end]

    assert "FMC_BCR1_FMCEN" in sdram_init_function


def test_h417_sdram_gpio_enables_high_speed_io_domains():
    text = read_v5f_hw_test_source()
    start = text.index("static void sdram_gpio_init")
    end = text.index("static int sdram_init")
    sdram_gpio_init_function = text[start:end]

    assert "GPIO_Remap_VIO1V8_IO_HSLV" in sdram_gpio_init_function
    assert "GPIO_Remap_VIO3V3_IO_HSLV" in sdram_gpio_init_function
    assert "GPIO_Remap_VDD3V3_IO_HSLV" in sdram_gpio_init_function


def test_h417_sdram_gpio_maps_xi_xo_pins_to_pd0_pd1_for_dq10_dq11():
    text = read_v5f_hw_test_source()
    start = text.index("static void sdram_gpio_init")
    end = text.index("static int sdram_init")
    sdram_gpio_init_function = text[start:end]

    remap_index = sdram_gpio_init_function.index("GPIO_Remap_PD0PD1")
    pd0_index = sdram_gpio_init_function.index(
        "sdram_gpio_af(GPIOD, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF1)"
    )
    pd1_index = sdram_gpio_init_function.index(
        "sdram_gpio_af(GPIOD, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF1)"
    )

    assert remap_index < pd0_index
    assert remap_index < pd1_index


def test_h417_sdram_policy_checks_high_speed_io_domains():
    text = read_check_script()

    assert "GPIO_Remap_VIO1V8_IO_HSLV" in text
    assert "GPIO_Remap_VIO3V3_IO_HSLV" in text
    assert "GPIO_Remap_VDD3V3_IO_HSLV" in text
    assert "GPIO_Remap_PD0PD1" in text
    assert "FMC_BCR1_FMCEN" in text


def test_h417_sdram_remap_probe_is_explicit_test_only_diagnostic():
    text = read_v5f_hw_test_source()

    assert "APP_V5F_HW_TEST_SDRAM_REMAP_PROBE" in text
    assert "V5F_SDRAM_REMAP_ADDR" in text
    assert "V5F_FMC_SDRAM_REMAP_TO_0X60000000" in text
    assert "sdram_probe_window_show" in text
    assert "run_sdram_remap_probe_test" in text


def test_h417_sdram_remap_probe_keeps_all_rows_visible_without_heartbeat():
    text = read_v5f_hw_test_source()
    start = text.index("static void V5F_MAYBE_UNUSED run_sdram_remap_probe_test")
    end = text.index("#endif", start)
    remap_probe_function = text[start:end]

    assert "base_pass = sdram_probe_window_show(V5F_SDRAM_BASE_ADDR, 36u)" in remap_probe_function
    assert "remap_pass = sdram_probe_window_show(V5F_SDRAM_REMAP_ADDR, 96u)" in remap_probe_function
    assert "fb_fill_user_rect_rgb565((uint16_t)(V5F_RGB_FB_WIDTH - 34u)" not in remap_probe_function
    assert "blink" not in remap_probe_function


def test_h417_sdram_dq_probe_maps_16_bit_one_hot_readback():
    text = read_v5f_hw_test_source()

    assert "APP_V5F_HW_TEST_SDRAM_DQ_PROBE" in text
    assert "run_sdram_dq_probe_test" in text
    assert "sdram_dq_probe_matrix_show" in text
    assert "volatile uint16_t *probe" in text
    assert "1u << row" in text


def test_h417_sdram_dq_probe_includes_byte_lane_dqm_readback():
    text = read_v5f_hw_test_source()

    assert "sdram_dqm_byte_probe_show" in text
    assert "volatile uint8_t *probe_bytes" in text
    assert "probe_bytes[0]" in text
    assert "probe_bytes[1]" in text
    assert "0x00FFu" in text
    assert "0xFF00u" in text


def test_h417_sdram_dq_probe_places_dqm_rows_before_matrix_with_revision_marker():
    text = read_v5f_hw_test_source()
    full_start = text.index("static void sdram_dq_probe_full_show")
    full_end = text.index("#if APP_ENABLE_USB_TEST", full_start)
    full_function = text[full_start:full_end]
    lower_start = text.index("static void sdram_dq_probe_lower_show")
    lower_end = text.index("static void sdram_dq_probe_full_show", lower_start)
    lower_function = text[lower_start:lower_end]

    marker_index = full_function.index("sdram_dq_probe_revision_marker_show()")
    phase_index = full_function.index("sdram_phase_probe_show(probe, 22u)")
    lower_index = full_function.index("sdram_dq_probe_lower_show(probe)")
    dqm_index = lower_function.index("sdram_dqm_byte_probe_show(probe, 82u)")
    matrix_index = lower_function.index("sdram_dq_probe_matrix_show(probe, 20u, 166u)")

    assert marker_index < phase_index < lower_index
    assert dqm_index < matrix_index


def test_h417_sdram_dq_probe_scans_fmc_read_phase_before_byte_lane_probe():
    text = read_v5f_hw_test_source()
    start = text.index("static void sdram_dq_probe_full_show")
    end = text.index("#if APP_ENABLE_USB_TEST", start)
    dq_probe_function = text[start:end]

    assert "sdram_phase_probe_show(probe, 22u)" in dq_probe_function
    assert "sdram_phase_probe_apply" in text
    assert "for(phase = 0u; phase < 16u; phase++)" in text
    assert "FMC_MISC_Phase_Sel" in text


def test_h417_sdram_dq_probe_selects_phase_by_bit_match_score():
    text = read_v5f_hw_test_source()

    assert "sdram_phase_probe_bit_score" in text
    assert "best_bit_score" in text
    assert "matching_bits += sdram_phase_probe_bit_score(expected, actual)" in text


def test_h417_sdram_dq_probe_scans_read_pipe_and_phase_together():
    text = read_v5f_hw_test_source()

    assert "sdram_read_pipe_probe_apply" in text
    assert "best_pipe" in text
    assert "for(pipe = 0u; pipe < 3u; pipe++)" in text
    assert "FMC_SDCR1_RPIPE" in text
    assert "FMC_ReadPipeDelay_2HCLK" in text


def test_h417_sdram_dq_probe_build_enables_usbfs_cdc_debug_channel():
    text = read_h417_makefile()

    assert "APP_V5F_HW_TEST_USB_CDC" in text
    assert "h417_v5f_sdram_dq_probe" in text
    assert "APP_ENABLE_USB_TEST=1" in text
    assert "APP_ENABLE_USB2_FS_CDC=1" in text
    assert "APP_ENABLE_USB2_HS_CDC=0" in text
    assert "APP_ENABLE_USBSS_CDC=0" in text


def test_h417_usb_cdc_exposes_rx_line_api_for_hw_tests():
    text = read_h417_usb_cdc_source()

    assert "ch32h417_usb_cdc_read_line" in text
    assert "ch32h417_usbfs_cdc_read_line" in text
    assert "#if APP_ENABLE_USBSS_CDC" in text
    assert "cdc_rx_line" in text
    assert "cdc_queue_rx_byte" in text
    assert "cdc_read_line" in text


def test_h417_sdram_dq_probe_uses_usb_cdc_debug_commands():
    text = read_v5f_hw_test_source()

    assert "sdram_usb_debug_init" in text
    assert "ch32h417_dual_cdc_init" in text
    assert "ch32h417_usb_cdc_write" in text
    assert "ch32h417_usb_cdc_read_line" in text
    assert "sdram_usb_debug_handle_command" in text
    assert '"scan"' in text
    assert '"regs"' in text
    assert '"rcc"' in text
    assert '"pad"' in text
    assert '"bias"' in text
    assert '"wlow"' in text
    assert '"hslv"' in text
    assert '"uport"' in text
    assert '"dq"' in text
    assert '"addr"' in text
    assert '"dump"' in text
    assert "p <0-15>" in text
    assert "r <0-2>" in text


def test_h417_sdram_dq_probe_usb_regs_command_reports_pinmux_registers():
    text = read_v5f_hw_test_source()

    assert "sdram_usb_debug_regs" in text
    assert "AFIO->PCFR1" in text
    assert "AFIO->GPIOD_AFLR" in text
    assert "GPIOD->CFGLR" in text
    assert "GPIOD->INDR" in text
    assert "FMC_Bank5_6->MISC" in text


def test_h417_sdram_dq_probe_usb_bias_command_reads_sdram_with_pd0_pd1_pull_bias():
    text = read_v5f_hw_test_source()

    assert "sdram_usb_debug_bias" in text
    assert "GPIO_Mode_IPD" in text
    assert "GPIO_Mode_IPU" in text
    assert "sdram_usb_debug_restore_pd0_pd1" in text
    assert "sdram_probe_write_read16" in text
    assert "SDRAM bias" in text
    assert "GPIOD->INDR" in text


def test_h417_sdram_dq_probe_usb_wlow_command_writes_sdram_with_pd0_pd1_forced_low():
    text = read_v5f_hw_test_source()

    assert "sdram_usb_debug_wlow" in text
    assert "GPIO_Mode_Out_PP" in text
    assert "sdram_usb_debug_pd0_pd1_drive_read(0x0u)" in text
    assert "sdram_usb_debug_restore_pd0_pd1" in text
    assert "SDRAM wlow" in text


def test_h417_sdram_dq_probe_usb_hslv_command_applies_raw_io_domain_bits():
    text = read_v5f_hw_test_source()

    assert "sdram_usb_debug_hslv" in text
    assert "AFIO_PCFR1_VIO18_IO_HSLV" in text
    assert "AFIO_PCFR1_VIO33_IO_HSLV" in text
    assert "AFIO_PCFR1_VDD33_IO_HSLV" in text
    assert "SDRAM hslv" in text


def test_h417_sdram_dq_probe_usb_uport_command_scans_uhsif_port_remap_bits():
    text = read_v5f_hw_test_source()

    assert "sdram_usb_debug_uport" in text
    assert "AFIO_PCFR1_UHSIF_PORT_REMAP" in text
    assert "for(rm = 0u; rm < 4u; rm++)" in text
    assert "SDRAM uport" in text
    assert "AFIO->PCFR1 = saved_pcfr1" in text


def test_h417_sdram_dq_probe_usb_dq_command_reports_text_one_hot_map():
    text = read_v5f_hw_test_source()

    assert "sdram_usb_debug_dq" in text
    assert "for(bit = 0u; bit < 16u; bit++)" in text
    assert "1u << bit" in text
    assert "SDRAM dq" in text


def test_h417_sdram_dq_probe_usb_addr_command_reports_multiple_address_windows():
    text = read_v5f_hw_test_source()

    assert "sdram_usb_debug_addr" in text
    assert "addr_offsets" in text
    assert "V5F_SDRAM_BASE_ADDR + offset" in text
    assert "SDRAM addr" in text


def test_h417_sdram_dq_probe_usb_rcc_command_reports_clock_and_vio_state():
    text = read_v5f_hw_test_source()

    assert "sdram_usb_debug_rcc" in text
    assert "RCC->CTLR" in text
    assert "RCC->CFGR0" in text
    assert "RCC->PLLCFGR" in text
    assert "RCC->PLLCFGR2" in text
    assert "PWR->CTLR" in text
    assert "PWR->CSR" in text
    assert "PWR_GetVIO18InitialStatus" in text


def test_h417_sdram_dq_probe_usb_pad_command_temporarily_drives_pd0_pd1():
    text = read_v5f_hw_test_source()

    assert "sdram_usb_debug_pad" in text
    assert "sdram_usb_debug_restore_pd0_pd1" in text
    assert "GPIO_Pin_0 | GPIO_Pin_1" in text
    assert "GPIO_Mode_Out_PP" in text
    assert "GPIO_Mode_AF_PP" in text
    assert "GPIOD->OUTDR" in text
    assert "GPIOD->INDR" in text
    assert "AFIO->GPIOD_AFLR" in text
