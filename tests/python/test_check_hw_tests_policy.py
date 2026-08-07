import io
import os
import re


ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)
)
CHECK_SCRIPT = os.path.join(ROOT, "tools", "check_hw_tests.py")
H417_MAKEFILE = os.path.join(ROOT, "hw_tests", "h417", "Makefile")
H417_USB_CDC_SOURCE = os.path.join(
    ROOT, "firmware", "h417", "v5f_rtthread", "applications", "usb_cdc_dual.c"
)
V5F_HW_TEST_SOURCE = os.path.join(
    ROOT, "hw_tests", "h417", "cases", "v5f_rtthread", "src", "v5f_hw_test.c"
)
V5F_HW_TEST_HEADER = os.path.join(
    ROOT, "hw_tests", "h417", "cases", "v5f_rtthread", "include", "v5f_hw_test.h"
)
V5F_RTTHREAD_MAKEFILE = os.path.join(
    ROOT, "firmware", "h417", "v5f_rtthread", "Makefile"
)
H417_OFFICIAL_SDRAM16_SOURCE = os.path.join(
    ROOT,
    "hw_tests",
    "h417",
    "cases",
    "v5f_rtthread",
    "src",
    "official",
    "wch_sdram_16bit",
    "hardware.c",
)
H417_OFFICIAL_SDRAM16_BOARD_DRIVER = os.path.join(
    ROOT,
    "hw_tests",
    "h417",
    "cases",
    "v5f_rtthread",
    "src",
    "official",
    "wch_sdram_16bit",
    "h417_v5f_sdram_official_16bit_board.c",
)
H417_V3F_WAKE_STUB_SOURCE = os.path.join(
    ROOT, "hw_tests", "h417", "cases", "v3f_standalone", "src", "h417_v5f_wake_stub.c"
)
H417_SDRAM_RESULT_READER = os.path.join(
    ROOT, "hw_tests", "h417", "tools", "read_sdram_result.ps1"
)


def read_check_script():
    with io.open(CHECK_SCRIPT, "r", encoding="utf-8") as handle:
        return handle.read()


def read_h417_makefile():
    with io.open(H417_MAKEFILE, "r", encoding="utf-8") as handle:
        return handle.read()


def read_h417_sdram_result_reader():
    with io.open(H417_SDRAM_RESULT_READER, "r", encoding="utf-8") as handle:
        return handle.read()


def read_h417_usb_cdc_source():
    with io.open(H417_USB_CDC_SOURCE, "r", encoding="utf-8") as handle:
        return handle.read()


def read_v5f_hw_test_source():
    with io.open(V5F_HW_TEST_SOURCE, "r", encoding="utf-8") as handle:
        return handle.read()


def read_v5f_hw_test_header():
    with io.open(V5F_HW_TEST_HEADER, "r", encoding="utf-8") as handle:
        return handle.read()


def read_v5f_rtthread_makefile():
    with io.open(V5F_RTTHREAD_MAKEFILE, "r", encoding="utf-8") as handle:
        return handle.read()


def read_h417_official_sdram16_source():
    with io.open(H417_OFFICIAL_SDRAM16_SOURCE, "r", encoding="utf-8") as handle:
        return handle.read()


def read_h417_official_sdram16_board_driver():
    with io.open(H417_OFFICIAL_SDRAM16_BOARD_DRIVER, "r", encoding="utf-8") as handle:
        return handle.read()


def read_h417_v3f_wake_stub_source():
    with io.open(H417_V3F_WAKE_STUB_SOURCE, "r", encoding="utf-8") as handle:
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
    assert "h417_v5f_sdram_official_16bit" in text
    assert "firmware/h417/v5f_rtthread/drivers/sdram" in text
    assert "SDRAM bring-up must stay in hw_tests" in text


def test_h417_sdram_official_16bit_example_is_imported_under_hw_tests():
    makefile = read_h417_makefile()
    v5f_makefile = read_v5f_rtthread_makefile()
    header = read_v5f_hw_test_header()
    source = read_v5f_hw_test_source()
    official = read_h417_official_sdram16_source()
    board_driver = read_h417_official_sdram16_board_driver()

    assert "h417_v5f_sdram_official_16bit" in makefile
    assert "APP_V5F_HW_TEST_SDRAM_OFFICIAL_16BIT 17" in header
    assert "sdram_official_16bit" in v5f_makefile
    assert "wch_sdram_16bit" in v5f_makefile
    assert "run_sdram_official_16bit_test" in source
    assert "extern void h417_v5f_sdram_official_16bit_init(void)" in source
    assert "h417_v5f_sdram_official_16bit_board.c" in v5f_makefile
    assert "SDRAM_Initialization_Sequence" in official
    assert "FMC_Bank1->BTCR[0] |= (1 << 24)" in official
    assert "Bank5_SDRAM_ADDR                         ((u32)(0X60000000))" in official
    assert "SDRAM_Initialization_Sequence()" in board_driver
    assert "FMC_Bank1->BTCR[0] |= (1 << 24)" in board_driver
    assert "h417_v5f_sdram_official_16bit_gpio_config" in board_driver
    assert "h417_v5f_sdram_official_16bit_gpio_af(GPIOD, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF1)" in board_driver
    assert "h417_v5f_sdram_official_16bit_gpio_af(GPIOD, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF1)" in board_driver
    assert "void Hardware(void)" not in board_driver


def test_h417_sdram_official_16bit_enables_usb_fs_cdc_debug():
    makefile = read_h417_makefile()
    match = re.search(
        r"else ifeq \(\$\(HW_TEST\),h417_v5f_sdram_official_16bit\)\n"
        r"(?P<body>.*?)(?=\nelse ifeq|\nendif)",
        makefile,
        re.S,
    )

    assert match is not None
    assert "APP_V5F_HW_TEST_USB_CDC := 1" in match.group("body")
    assert (
        "APP_ENABLE_USB_TEST=1 APP_ENABLE_USB2_FS_CDC=1 "
        "APP_ENABLE_USB2_HS_CDC=0 APP_ENABLE_USBSS_CDC=0"
    ) in makefile


def test_h417_sdram_official_16bit_uses_cdc_status_loop():
    source = read_v5f_hw_test_source()
    start = source.index("static void V5F_MAYBE_UNUSED run_sdram_official_16bit_test")
    end = source.index("static void V5F_MAYBE_UNUSED run_sdram_dq_probe_test")
    official_test = source[start:end]

    assert "V5F_SDRAM_USB_DEBUG_ENABLED" in source
    assert "APP_V5F_HW_TEST_SDRAM_OFFICIAL_16BIT" in source
    assert "g_v5f_hw_test_diag.sdram_sdclk_hz = HCLKClock;" in official_test
    assert "g_v5f_hw_test_diag.sdram_sdclk_hz = HCLKClock / 2u;" not in official_test
    assert "sdram_usb_debug_init(probe)" in official_test
    assert "sdram_usb_debug_poll(probe)" in official_test
    assert 'sdram_usb_debug_report(probe, "tick")' in official_test


def test_h417_v5f_sdram_tests_use_hw_tests_owned_v3f_wake_stub():
    makefile = read_h417_makefile()
    stub = read_h417_v3f_wake_stub_source()

    assert "H417_V3F_WAKE_STUB_TEST := h417_v5f_wake_stub" in makefile
    assert "H417_V3F_WAKE_BUILD_ROOT := $(BUILD_ROOT)/$(H417_HW_TEST_BUILD_NAME)/V3F" in makefile
    assert "H417_LOCAL_V3F_PROJECT=h417_V3F" in makefile
    assert "HW_TEST=$(H417_V3F_WAKE_STUB_TEST)" in makefile
    assert '"$(H417_FIRMWARE_ROOT)" v3f' not in makefile

    assert "H417_V3F_WAKE_TRACE_BASE" in stub
    assert "H417_V3F_WAKE_TRACE_MAGIC" in stub
    assert "V5F_START_ADDR      0x00010000u" in stub
    assert "SystemInit()" in stub
    assert "RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE)" in stub
    assert "NVIC_WakeUp_V5F(V5F_START_ADDR)" in stub
    assert "USB" not in stub
    assert "CH585" not in stub
    assert "HID" not in stub


def test_h417_sdram_tests_do_not_depend_on_uart_console():
    text = read_v5f_hw_test_source()

    assert 'rt_kprintf("SDRAM' not in text
    assert "sdram_status_lcd_start" in text
    assert "sdram_status_show" in text


def test_h417_sdram_memtest_uses_main_cdc_without_lcd_result_path():
    text = read_v5f_hw_test_source()
    makefile = read_h417_makefile()
    start = text.index("static void run_sdram_memtest_test")
    end = text.index("#endif", start)
    run_function = text[start:end]

    assert re.search(
        r"h417_v5f_sdram_memtest[\s\S]*?APP_V5F_HW_TEST_USB_CDC\s*:=\s*1",
        makefile,
    )
    assert "V5F_SDRAM_MEMTEST_CDC_ONLY" in text
    assert "sdram_memtest_cdc_wait_for_start()" in run_function
    assert "sdram_memtest_cdc_begin()" in run_function
    assert "sdram_memtest_bank_settle_scan()" in run_function
    assert "sdram_memtest_bank_sync_scan()" in run_function
    assert "sdram_memtest_access_profile_scan()" in run_function
    assert "sdram_memtest_dma_path_scan()" in run_function
    assert "sdram_memtest_cas_scan()" in run_function
    assert "sdram_memtest_nrfs_scan()" in run_function
    assert "sdram_memtest_prefetch_validate()" in run_function
    assert "sdram_memtest_prefetch_coherence()" in run_function
    assert "sdram_memtest_map16_scan()" in run_function
    assert "sdram_memtest_recovery_scan()" in run_function
    assert "sdram_memtest_refresh_profile_scan()" in run_function
    assert "sdram_memtest_transition_scan()" in run_function
    assert "sdram_memtest_x8_bank_probe(&bank_failure)" in run_function
    assert "sdram_memtest_x8_full(" in run_function
    assert "FULL_TEST START mode=prefetch" in run_function
    assert "sdram_status_lcd_start" not in run_function
    assert "sdram_status_show" not in run_function
    assert "fb_fill" not in run_function
    assert "H417 SDRAM CDC TEST v61 DMA2 ISOLATION" in text
    assert "WATCHDOG RECOVERY cause=IWDG" in text
    assert "WATCHDOG DMA controller=%u channel=3 cfgr=%08x cntr=%u paddr=%08x maddr=%08x" in text
    assert "WATCHDOG FMC sdsr=%08x sdcr=%08x sdtr=%08x sdrtr=%08x misc=%08x" in text
    assert "WATCHDOG ARMED timeout_approx_s=25 retain=2017ff00 checkpoint=pre-enable" in text
    assert "memcpy(framed, line, length)" in text
    assert "V5F_SDRAM_DMA_RESTART_GAP_US   100u" in text
    assert "gap_us=100 log=silent" in text
    assert "RANGE C0000000-C1FFFFFF bytes=33554432 access=short-dma1,stress-dma2-word32" in text
    assert "BANK_SPAN bytes=00800000" in text
    assert "GEOMETRY fmc_width=16 device_width=16" in text
    assert "LANES assumed_good=f3ff ignored=0c00 source=v36-v43" in text
    assert "X16_DQ START patterns=0000,ffff,aaaa,5555,walking1,walking0" in text
    assert "X16_BIT D%u FAIL bad=%u/%u first=%08x exp=%04x got=%04x" in text
    assert "X16_SEQ START words=4096/bank access=alternating-bank" in text
    assert "DMA_ONLY START mode=dma32-isolation timer=systick1 hz=%u block=16384" in text
    assert "DMA_ROUTE short=DMA1_CH3 stress=DMA2_CH3 gap_us=100" in text
    assert "DMA CALL START cpu_sdram_access=none" in text
    assert "DMA CALL DONE" in text
    assert "DMA_ONLY TIMER PROBE PASS delta=%u" in text
    assert "DMA_ONLY CPU ACCESS SKIP read=1 write=1" in text
    assert "BW %s bytes=%u cycles=%u raw=%u.%02uMB/s useful=%u.%02uMB/s" in text
    assert "sdram_memtest_dma_timed" in text
    assert 'sdram_memtest_bandwidth_report("DMA32_READ"' in text
    assert 'sdram_memtest_bandwidth_report("DMA32_WRITE"' in text
    assert "DMA_ONLY DMA32_WRITE START" in text
    assert "DMA_ONLY DMA32_READ START" in text
    assert "DMA256 SKIP reason=dma32-isolation no_256bit_transaction=1" in text
    assert "DMA_ONLY END verify=PASS dma=PASS cpu_sdram=none" in text
    assert "DMA_FULL START mode=%s controller=DMA2 channel=3 bytes=33554432" in text
    assert "gap_us=100 log=silent" in text
    assert "sdram_memtest_dma_stream_prepare" in text
    assert "sdram_memtest_dma_stream_transfer" in text
    assert "DMA_Init(DMA2_Channel3, &dma)" in text
    assert "DMA_Cmd(DMA2_Channel3, ENABLE)" in text
    assert "DMA_SLICE RECOVER %s block=%u addr=%08x START" in text
    assert "DMA_SLICE RECOVER %s block=%u %s code=%d stage=%u" in text
    assert "DMA_SLICED STRESS START mode=%s passes=4 bytes_per_pass=33554432" in text
    assert "DMA_SLICED %s pass=%u/4 %s" in text
    assert "DMA_SLICED STRESS END PASS mode=%s passes=4 transferred=268435456" in text
    assert "DMA_FULL %s %s progress=%u/32MiB" in text
    assert "DMA_TRACE %s %s block=%u addr=%08x %s" in text
    assert "DMA_BANK_SWITCH %s %s block=%u addr=%08x PALL_START" in text
    assert "DMA_BANK_SWITCH %s %s block=%u addr=%08x PALL_%s" in text
    assert "DMA_LONG START mode=wide256 count=65535 span=2097120" in text
    assert "DMA_LONG bank=%u segment=%u addr=%08x WRITE_START" in text
    assert "DMA_LONG bank=%u segment=%u addr=%08x READ_START" in text
    assert "DMA_LONG END %s bytes=%u bad=%u" in text
    assert "DMA_LENGTH START addr=c0200000 mode=wide256 meminc=0" in text
    assert "DMA_LENGTH PREP WRITE_START count=65535 span=2097120" in text
    assert "DMA_LENGTH READ count=%u span=%u START" in text
    assert "DMA_LENGTH END PASS max_count=65535 max_span=2097120" in text
    assert "DMA_REINIT START loops=128 mode=wide256 count=65535" in text
    assert "dma_reset=each_transfer" in text
    assert "DMA_REINIT loop=%u target=%u addr=%08x SAFE_REINIT_START" in text
    assert "DMA_REINIT loop=%u target=%u SAFE_REINIT_%s code=%d stage=%u" in text
    assert "DMA_REINIT loop=%u target=%u addr=%08x READ_START" in text
    assert "DMA_REINIT END %s loops=%u bad=%u" in text
    assert "DMA_LENGTH TEST SKIP result=v49_pass" in text
    assert "DMA_FULL RETENTION delay_ms=1000" in text
    assert "DMA_FULL END mode=%s %s write_ok=%u read_ok=%u bad=%u" in text
    assert '"DMA32_FULL_WRITE"' in text
    assert '"DMA32_FULL_READ"' in text
    assert '"DMA256_FULL_WRITE"' in text
    assert '"DMA256_FULL_READ"' in text
    assert "MASKED14 DMA RESULT FAIL good_mask=%04x required=f3ff dma_rw=%s" in text
    assert "MASKED14 DMA RESULT PASS mask=f3ff assumed=1 dma_rw=pass" in text
    assert "FMC_MemoryDataWidth_16" in text
    assert "sdram_memtest_x16_low8_read" in text
    assert "& 0x00FFu" in text
    assert "BANK_READ_SWITCH grouped_bad=%u/256 alt_bad=%u/256" in text
    assert "BANK_PAIR_PALL bank=%u cmd=%u" in text
    assert "BANK_MODE START modes=normal,enhance,rburst phases=16 pipes=3" in text
    assert "BANK_MODE SCAN mode=%s enh=%u rb=%u" in text
    assert "BANK_MODE FORCE mode=normal enh=0 rb=0" in text
    assert "BANK_SETTLE START access=cpu16 compare=low8 discard=0,1,2,4,8,16,32" in text
    assert "BANK_SETTLE END max=256 each_score_order=discard0,1,2,4,8,16,32" in text
    assert "BANK_SYNC START access=cpu16 compare=low8 strategies=direct,d1,fence,us1,us10,pall,d32,us10_r8191" in text
    assert "BANK_SYNC END max=256 normal_refresh=240 slow_refresh=8191" in text
    assert "MAP16 START access=cpu16 compare=low8 native=c0000000 remap=60000000" in text
    assert 'sdram_memtest_map16_report("remap_read_native"' in text
    assert 'sdram_memtest_map16_report("native_read_remap"' in text
    assert "MAP16 END restored=native remap=0" in text
    assert "ACCESS_PROFILE START write=cpu32 repeated-byte compare=low8" in text
    assert 'sdram_memtest_access_profile_report("normal_native"' in text
    assert 'sdram_memtest_access_profile_report("wch_official"' in text
    assert "ACCESS_PROFILE END restored=native/normal" in text
    assert "DMA_PATH START source=sdram destination=sram channel=DMA1_CH3 modes=word,256" in text
    assert "DMA_SAMPLE word_ok=%u b0=%08x/%08x wide_ok=%u b1=%08x/%08x mask=00ff00ff" in text
    assert "DMA_BANK word=%u/256 bad=%u wide256=%u/256 bad=%u compare=00ff00ff" in text
    assert "DMA_REC word direct=%u d1=%u d2=%u pall=%u pall_ar1=%u us10=%u bad=%u" in text
    assert "DMA_REC wide256 direct=%u d1=%u d2=%u pall=%u pall_ar1=%u us10=%u bad=%u" in text
    assert "CAS_SCAN START modes=cl3,cl2 phases=16 pipes=3 score=cpu16/256+dma" in text
    assert "CAS_CPU cl=%u scan=%u/256 verify=%u/256 phase=%u pipe=%u cmd=%u" in text
    assert "CAS_DMA cl=%u word=%u/256 bad=%u wide256=%u/256 bad=%u" in text
    assert "CAS_SCAN END restored=cl3 phase=10 pipe=0 mode=0230" in text
    assert "COLD_SCAN START order=normal_n0,prefetch_n0,normal_n15 phases=16 pipes=3" in text
    assert "COLD_PROFILE name=%s prefetch=%u nrfs=%u init=%d scan=%u verify=%u d32=%u dma256=%u" in text
    assert "COLD_SCAN END manual_bit15=1_prefetch header_constants_invalid=1" in text
    assert "NRFS_SCAN START mode=normal values=0..15 refreshes_per_event=value+1" in text
    assert 'sdram_memtest_score_list("NRFS_CPU direct="' in text
    assert 'sdram_memtest_score_list("NRFS_DMA wide256="' in text
    assert "NRFS_SCAN END restored=normal/nrfs0" in text
    assert "PREFETCH_CPU direct=%u/4096 bad=%u" in text
    assert "PREFETCH_BOUNDARY bank=%u logical=%08x score=%u/256" in text
    assert "PREFETCH_DMA_RAW bank=%u ok=%u exp=%02x raw=%08x/%08x" in text
    assert "PREFETCH_DMA word=%u,%u,%u,%u,%u,%u bad=%u" in text
    assert "PREFETCH_DMA wide=%u,%u,%u,%u,%u,%u bad=%u" in text
    assert "COHERENCE_SINGLE first=%02x after_write=%02x normal=%02x reenable=%02x" in text
    assert "COHERENCE_BANK fresh=%u/256 rewrite=%u/256 rewrite_d32=%u/256" in text
    assert "COHERENCE_SEQ prefetch_write=%u/2048 normal_write_then_prefetch=%u/2048" in text
    assert "COHERENCE_DMA normal_write_then_prefetch word=%u/256 wide=%u/256" in text
    assert "COHERENCE END cpu=%s dma=%s setup=%u" in text
    assert "V5F_SDRAM_CLOCK_PERIOD_1HCLK   1u" in text
    assert "V5F_SDRAM_OFFICIAL_REFRESH     677u" in text
    assert "V5F_SDRAM_REFRESH_PERIOD_US    32000u" in text
    assert "NORMAL100_BANK score=%u/4096 bad=%u" in text
    assert "NORMAL100_SEQ score=%u/2048 bad=%u" in text
    assert "NORMAL100_DMA word=%u/256 wide=%u/256 bad=%u/%u" in text
    assert "FULL_TEST START mode=normal100 access=cpu16 compare=low8" in text
    assert "FULL_TEST START mode=prefetch access=cpu16 compare=low8" in text
    assert "CPU STORAGE RESULT PASS mode=prefetch logical_bytes=16777216 compare=low8" in text
    assert "FUNCTION RESULT FAIL reason=prefetch_dma_direct cpu_storage=pass" in text
    assert "DMA_BOUNDARY word=%u/3 bad=%u wide256=%u/3 bad=%u span=64 compare=00ff00ff" in text
    assert "buffer[index] & 0x00FF00FFu" in text
    assert "RECOVERY START refresh=8191 strategies=direct,us20,d32,pall1,pall2,pall_ar1,toggle" in text
    assert "RECOVERY SCORE direct=%u us20=%u d32=%u pall1=%u pall2=%u pall_ar1=%u toggle=%u" in text
    assert "REFRESH_PROFILE START counts=41,80,110,160,240,480,8191" in text
    assert "CPU STORAGE RESULT PASS mode=prefetch logical_bytes=16777216 compare=low8" in text
    assert "FUNCTION RESULT FAIL reason=prefetch_dma_direct cpu_storage=pass" in text
    assert "TRANS START access=cpu16 compare=low8 pair=35/ca direct,fence,pall max=128" in text
    assert 'sdram_memtest_transition_report("column", 0u, 1u)' in text
    assert 'sdram_memtest_transition_report("row", 0u, 1u << 9)' in text
    assert 'sdram_memtest_transition_report("ba0", 0u, 1u << 22)' in text
    assert 'sdram_memtest_transition_report("ba1", 0u, 1u << 23)' in text
    assert 'sdram_memtest_transition_report("ba0_nb2", 0u, 1u << 22)' in text
    assert "V5F_SDRAM_ENHANCE_READ_BIT     (1u << 15)" in text
    assert "full_test=all_16MB" in text
    assert 'sdram_memtest_cdc_stage("MARCH", "PASS passes=6 rw_turnaround=separated")' in text
    assert "RETENTION PASS delay_ms=1000 checksum=%08x" in text
    assert "V5F_SDRAM_MAX_SDCLK_HZ         100000000u" in text
    assert "MAP base=%08x remap=%u nor_en=%u bcr0=%08x misc=%08x" in text
    assert "IO pwrctl=%08x pcfr1=%08x pd0rm=%u hslv=%u" in text
    assert "#define V5F_SDRAM_BASE_ADDR            V5F_SDRAM_NATIVE_ADDR" in text
    assert "RESULT PASS" in text
    assert "RESULT FAIL" in text

    with io.open(H417_SDRAM_RESULT_READER, "r", encoding="utf-8") as handle:
        reader = handle.read()
    assert '$serial.Write("start`r`n")' in reader
    assert 'Contains("H417 SDRAM CDC TEST")' in reader
    assert 'Contains("CONTINUOUS START")' in reader
    assert 'Contains("RESULT PASS")' in reader
    assert 'Contains("RESULT FAIL")' in reader


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


def test_h417_sdram_init_selects_normal_read_mode_without_raw_misc_writes():
    text = read_v5f_hw_test_source()
    start = text.index("static int sdram_init")
    end = text.index("static uint32_t sdram_pattern")
    sdram_init_function = text[start:end]

    assert "FMC_Bank5_6->MISC |=" not in sdram_init_function
    assert "V5F_SDRAM_NORMAL_READ_MODE" in sdram_init_function


def test_h417_sdram_refresh_count_is_encoded_in_sdrtr_bits_13_to_1():
    text = read_v5f_hw_test_source()
    start = text.index("static void sdram_set_refresh_count")
    end = text.index("static void sdram_gpio_af", start)
    refresh_function = text[start:end]

    assert "((uint32_t)refresh_count << 1) & FMC_SDRTR_COUNT" in refresh_function
    assert "sdram_set_refresh_count(refresh_count)" in text


def test_h417_sdram_init_keeps_nor_psram_gate_clear():
    text = read_v5f_hw_test_source()
    start = text.index("static int sdram_init")
    end = text.index("static uint32_t sdram_pattern")
    sdram_init_function = text[start:end]

    assert "FMC_Bank1->BTCR[0] &= ~FMC_BCR1_FMCEN" in sdram_init_function
    assert "FMC_Bank1->BTCR[0] |= FMC_BCR1_FMCEN" not in sdram_init_function


def test_h417_sdram_gpio_starts_with_hslv_off_for_baseline_comparison():
    text = read_v5f_hw_test_source()
    start = text.index("static void sdram_gpio_init")
    end = text.index("static int sdram_init")
    sdram_gpio_init_function = text[start:end]

    assert "AFIO->PCFR1 &= ~(AFIO_PCFR1_VIO18_IO_HSLV" in sdram_gpio_init_function
    assert "AFIO_PCFR1_VIO33_IO_HSLV" in sdram_gpio_init_function
    assert "AFIO_PCFR1_VDD33_IO_HSLV" in sdram_gpio_init_function


def test_h417_sdram_memtest_keeps_x16_fmc_and_tests_even_low_bytes():
    text = read_v5f_hw_test_source()
    start = text.index("static void run_sdram_memtest_test")
    end = text.index("static void V5F_MAYBE_UNUSED run_sdram_ltdc_rgb565_test")
    run_function = text[start:end]

    assert "LOW8 CONFIG fmc_width=16 cpu_width=16 stride=2 compare_mask=00ff" in run_function
    assert "DQM_CFG dqml_pc2=%x/af%x dqmh_pe3=%x/af%x" in run_function
    assert "BANK_CFG sdcr=%08x mwid=%u nb=%u" in run_function
    assert "sdram_memtest_x8_bank_probe(&bank_failure)" in run_function
    assert "sdram_memtest_bank_tune()" in run_function
    assert "dma_ok = sdram_memtest_prefetch_validate()" in run_function
    assert "result = sdram_init_profile(0u, 0u, 1u)" in run_function
    assert "result = sdram_init();" not in run_function
    assert "sdram_memtest_x8_full(" in run_function
    assert "test_bytes = V5F_SDRAM_BYTES" in run_function
    assert "FULL_TEST START mode=prefetch access=cpu16 compare=low8" in run_function
    assert "CPU STORAGE RESULT PASS mode=prefetch" in run_function
    assert "FUNCTION RESULT FAIL reason=prefetch_dma_direct" in run_function
    assert "BANK0 FULL_TEST PASS" not in run_function
    assert "sdram_memtest_cdc_continuous_rw()" not in run_function
    assert "sdram_memtest_cdc_summary(1u)" in run_function

    low8_start = text.index("static void sdram_memtest_x16_low8_write")
    low8_end = text.index("static uint8_t sdram_memtest_x8_pattern", low8_start)
    low8_access = text[low8_start:low8_end]
    assert "((volatile uint16_t *)base)[logical_offset]" in low8_access
    assert "& 0x00FFu" in low8_access

    init_start = text.index("static int sdram_init")
    init_end = text.index("static uint32_t sdram_pattern", init_start)
    init_function = text[init_start:init_end]
    assert "init.FMC_MemoryDataWidth = FMC_MemoryDataWidth_16" in init_function
    assert "sdram_x8_force_dqmh_high" not in text

    gpio_start = text.index("static void sdram_gpio_init")
    gpio_end = text.index("static int sdram_init", gpio_start)
    gpio_function = text[gpio_start:gpio_end]
    assert "sdram_gpio_af(GPIOC, GPIO_Pin_2, GPIO_PinSource2, GPIO_AF15)" in gpio_function
    assert "sdram_gpio_af(GPIOE, GPIO_Pin_3, GPIO_PinSource3, GPIO_AF1)" in gpio_function


def test_h417_sdram_direction_split_separates_fmc_writes_from_device_reads():
    text = read_v5f_hw_test_source()
    start = text.index("sdram_memtest_cdc_continuous_rw(void)")
    end = text.index("#endif", start)
    direction_test = text[start:end]
    write_start = direction_test.index("SCOPE WRITE START")
    write_end = direction_test.index("SCOPE WRITE END")
    read_start = direction_test.index("SCOPE READ START")
    write_phase = direction_test[write_start:write_end]
    read_phase = direction_test[read_start:]

    assert "{0x0000u, 0x0400u, 0x0800u, 0x0C00u}" in direction_test
    assert "trigger=WE#_LOW" in direction_test
    assert "trigger=CAS#_LOW+WE#_HIGH" in direction_test
    assert "probe[index] = patterns[index]" in write_phase
    assert "actual = probe[index]" not in write_phase
    assert "actual = probe[index]" in read_phase
    assert "probe[index] = patterns[index]" not in read_phase
    assert "no_writes=1" in read_phase
    assert "READ_SCAN START phases=16 pipes=3" in direction_test
    assert "best_raw=%u/48" in direction_test
    assert "fused = (uint16_t)((actual & ~0x0C00u)" in direction_test
    assert "MODE_SCAN START afr=1 reads_only=1 modes=afpp,afod,float,ipu,ipd" in direction_test
    assert "MODE_SCAN %s cfg=%02x raw=%u/192 fuse=%u/256 map=" in direction_test
    assert "GPIO_Mode_AF_OD" in direction_test
    assert "GPIO_Mode_IN_FLOATING" in direction_test
    assert "GPIO_Mode_IPU" in direction_test
    assert "GPIO_Mode_IPD" in direction_test
    assert "ROUTE_SCAN START reads_only=1 rm=0/1 hslv=0/1 af=1 mode=afpp" in direction_test
    assert "ROUTE_SCAN rm=%u hslv=%u pcfr1=%08x raw=%u/192 fuse=%u/256 map=" in direction_test
    assert "AFIO_PCFR1_PD0_1_REMAP" in direction_test
    assert "AFIO_PCFR1_VIO18_IO_HSLV" in direction_test
    assert "AFIO_PCFR1_VIO33_IO_HSLV" in direction_test
    assert "AFIO_PCFR1_VDD33_IO_HSLV" in direction_test
    assert "SCOPE PIN report=%u map=" in read_phase


def test_h417_sdram_gpio_keeps_qeu6_dedicated_pd0_pd1_for_dq10_dq11():
    text = read_v5f_hw_test_source()
    start = text.index("static void sdram_gpio_init")
    end = text.index("static int sdram_init")
    sdram_gpio_init_function = text[start:end]

    remap_index = sdram_gpio_init_function.index(
        "AFIO->PCFR1 &= ~AFIO_PCFR1_PD0_1_REMAP"
    )
    pa9_isolate_index = sdram_gpio_init_function.index(
        "GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF15)"
    )
    pa10_isolate_index = sdram_gpio_init_function.index(
        "GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF15)"
    )
    pd0_index = sdram_gpio_init_function.index(
        "sdram_gpio_af(GPIOD, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF1)"
    )
    pd1_index = sdram_gpio_init_function.index(
        "sdram_gpio_af(GPIOD, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF1)"
    )

    assert remap_index < pa9_isolate_index
    assert remap_index < pa10_isolate_index
    assert remap_index < pd0_index
    assert remap_index < pd1_index
    assert "sdram_gpio_af(GPIOD, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF0)" not in sdram_gpio_init_function
    assert "sdram_gpio_af(GPIOD, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF0)" not in sdram_gpio_init_function


def test_h417_sdram_acceptance_compares_all_16_data_bits():
    text = read_v5f_hw_test_source()
    start = text.index("static void V5F_MAYBE_UNUSED run_sdram_memtest_test")
    end = text.index("static void run_sdram_video_test", start)
    run_function = text[start:end]

    assert "H417 SDRAM CDC TEST v71 FULL16" in text
    assert "LANES compare=ffff ignored=0000 source=v70_dq_raw_pass" in text
    assert "x16_compare_mask = 0xFFFFu" in run_function
    assert "sdram_memtest_bandwidth(x16_compare_mask)" in run_function
    assert "FULL16 DMA RESULT PASS mask=ffff dma_rw=pass" in run_function
    assert "sdram_expected = 0xFFFFu" in run_function
    assert "x16_good_mask = 0xF3FFu" not in run_function


def test_h417_sdram_video_target_isolates_ltdc_pclk_with_internal_static_frame():
    text = read_v5f_hw_test_source()
    run_start = text.index("static void run_sdram_video_test(void)")
    run_end = text.index("#endif", run_start)
    run_test = text[run_start:run_end]

    probe_start = text.index(
        "static void __attribute__((noreturn))\n"
        "sdram_video_run_pclk_ipc_probe(void)"
    )
    probe_end = text.index("static void run_sdram_video_test(void)", probe_start)
    probe = text[probe_start:probe_end]

    assert "H417 SDRAM LTDC TEST v29 ARGB8888 IPC STATIC INTERNAL" in probe
    assert "source=internal_shared_sram no_sdram=1 no_upload=1" in probe
    assert "memcpy(s_lcd_fb," in probe
    assert "V5F_LTDC_ARGB8888_PROBE_CRC32" in probe
    assert "panel.pixel_clock_polarity = LTDC_PCPolarity_IPC" in probe
    assert "layer.pixel_format = LTDC_Pixelformat_ARGB8888" in probe
    assert "layer.framebuffer = (uint32_t)(uintptr_t)s_lcd_fb" in probe
    assert "ch32h417_ltdc_rgb_start_layer1(&panel, &layer, &black)" in probe
    assert "sdram_video_ltdc_wait_scan(&scan_changes)" in probe
    assert "PCLK RESULT PASS format=argb8888 internal_crc=pass scan=pass ipc=1" in probe
    assert "sdram_init_profile(" not in probe
    assert "sdram_enable_0x60000000_remap()" not in probe
    assert "sdram_video_upload(" not in probe
    assert "sdram_memtest_dma_stream_transfer(" not in probe
    assert "LTDC_LayerAddress(" not in probe
    assert "ch32h417_lcd_rgb_control_init()" not in probe
    assert "ch32h417_lcd_rgb_disp_enable(" not in probe
    assert "ch32h417_lcd_rgb_backlight_enable(" not in probe

    assert run_test.index("sdram_video_run_pclk_ipc_probe()") < run_test.index(
        "sdram_video_wait_config(&config)"
    )

    boot_start = text.index("int v5f_hw_test_start(void)")
    boot = text[boot_start:]
    video_start = boot.index("#elif APP_V5F_HW_TEST == APP_V5F_HW_TEST_SDRAM_VIDEO")
    video_end = boot.index("#endif", video_start)
    video_boot = boot[video_start:video_end]
    assert "ch32h417_lcd_rgb_control_init()" in video_boot
    assert "GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF15)" in video_boot
    assert "GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF15)" in video_boot
    assert "ch32h417_lcd_rgb_disp_enable(1u)" in video_boot
    assert "ch32h417_lcd_rgb_backlight_enable(1u)" in video_boot

    reader = read_h417_sdram_result_reader()
    assert '$allText.Contains("H417 SDRAM LTDC TEST")' in reader


def test_h417_sdram_policy_checks_io_voltage_and_pd0_pd1_controls():
    text = read_check_script()

    assert "AFIO_PCFR1_VIO18_IO_HSLV" in text
    assert "AFIO_PCFR1_VIO33_IO_HSLV" in text
    assert "AFIO_PCFR1_VDD33_IO_HSLV" in text
    assert "AFIO_PCFR1_PD0_1_REMAP" in text
    assert r"GPIO_PinSource9,\s*GPIO_AF15" in text
    assert r"GPIO_PinSource10,\s*GPIO_AF15" in text
    assert "H417 SDRAM CDC TEST v71 FULL16" in text
    assert "x16_compare_mask\\s*=\\s*0xFFFFu" in text
    assert "BTCR\\[0\\]\\s*\\|=\\s*FMC_BCR1_FMCEN" in text


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

    assert "sdram_disable_0x60000000_remap()" in remap_probe_function
    assert "base_pass = sdram_probe_window_show(V5F_SDRAM_NATIVE_ADDR, 36u)" in remap_probe_function
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
    full_end = text.index("#if V5F_SDRAM_USB_DEBUG_ENABLED", full_start)
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
    end = text.index("#if V5F_SDRAM_USB_DEBUG_ENABLED", start)
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
    assert '"scope"' in text
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


def test_h417_sdram_dq_probe_usb_scope_command_repeats_fixed_pattern_cycles():
    text = read_v5f_hw_test_source()

    assert "sdram_usb_debug_scope" in text
    assert "sdram_usb_debug_parse_u16" in text
    assert "uint8_t base = 16u" in text
    assert "sdram_scope_cycle_count" in text
    assert "V5F_SDRAM_SCOPE_CYCLES" in text
    assert "SDRAM scope" in text


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
