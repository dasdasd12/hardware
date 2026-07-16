import io
import os
import unittest


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))


def read_text(*parts):
    with io.open(os.path.join(ROOT, *parts), "r", encoding="utf-8") as handle:
        return handle.read()


class Profile32kFeasibilityHwTests(unittest.TestCase):
    def test_usbss_target_uses_official_ch372_example_on_v3f(self):
        hw_makefile = read_text("hw_tests", "h417", "Makefile")

        self.assertIn("h417_v3f_usbss_ch372", hw_makefile)
        self.assertIn("CH372Device", hw_makefile)
        self.assertIn("ch32h417_usbss_device.c", hw_makefile)
        self.assertIn("ch32h417_usbss_it.c", hw_makefile)
        self.assertIn("-DRun_Core=Run_Core_V3F", hw_makefile)

        diag_source = read_text(
            "hw_tests", "h417", "passed", "v3f_standalone", "src",
            "h417_usbss_diag_official.c"
        )
        diag_main = read_text(
            "hw_tests", "h417", "passed", "v3f_standalone", "src",
            "h417_usbss_fs_diag.c"
        )
        fs_adapter = read_text(
            "hw_tests", "h417", "passed", "v3f_standalone", "src",
            "h417_usbfs_cdc_diag.c"
        )
        product_usbhs = read_text(
            "hw_tests", "h417", "passed", "v3f_standalone", "src",
            "h417_product_usbhs_baseline.c"
        )
        diag_linker = read_text(
            "hw_tests", "h417", "passed", "v3f_standalone",
            "Link_h417_v3f_usbss_diag.ld"
        )
        self.assertIn("h417_v3f_usbss_fs_diag", hw_makefile)
        self.assertIn("WCH_USBFS_COMPAT_HID_SRC", hw_makefile)
        self.assertIn(
            "SYSTEM_SRC := $(H417_FIRMWARE_ROOT)/v3f/bsp/system_ch32h417.c",
            hw_makefile,
        )
        self.assertIn(
            "STARTUP := $(H417_FIRMWARE_ROOT)/v3f/bsp/startup_ch32h417_v3f.S",
            hw_makefile,
        )
        self.assertIn(
            "LDSCRIPT := $(V3F_STANDALONE_ROOT)/Link_h417_v3f_usbss_diag.ld",
            hw_makefile,
        )
        self.assertIn(
            "TEST_SRC += $(USBHS_HID_NKRO_SRC)/ch32h417_usbhs_hid_nkro.c",
            hw_makefile,
        )
        self.assertIn("h417_product_usbhs_baseline.c", hw_makefile)
        self.assertLess(
            hw_makefile.index(
                "TEST_SRC := $(WCH_USBFS_COMPAT_HID_SRC)/ch32h417_usbfs_device.c"
            ),
            hw_makefile.index(
                "TEST_SRC += $(V3F_STANDALONE_SRC_ROOT)/h417_usbss_diag_official.c"
            ),
        )
        self.assertIn('#include "ch32h417_usbss_device.c"', diag_source)
        self.assertIn('csrr %0, cycle', diag_source)
        self.assertNotIn("mcycle", diag_source)
        self.assertIn("USBHS_Device_Init h417_usbhs_fallback_request", diag_source)
        self.assertNotIn("#define USBSS_LINK_Handle", diag_source)
        self.assertNotIn("H417_USBSS_DIAG_LINK_BEFORE", diag_source)
        self.assertNotIn("NVIC_EnableIRQ(USBSS_LINK_IRQn)", diag_source)
        self.assertIn("SS19 boot=%s", diag_main)
        self.assertIn("SS19 step=%s", diag_main)
        self.assertIn("RECOVER_READ_ARMED", diag_main)
        self.assertIn("RECOVER_VALUE_LOADED", diag_main)
        self.assertIn("RECOVER_STORE_ARMED", diag_main)
        self.assertIn("RECOVER_STORE_RETURNED", diag_main)
        self.assertIn("RECOVER_READBACK_RETURNED", diag_main)
        self.assertIn("RECOVER_TRAP", diag_main)
        self.assertIn("RECOVER_DEVICE_INIT_ARMED", diag_main)
        self.assertIn("RECOVER_DEVICE_INIT_RETURNED", diag_main)
        self.assertIn("USBSS_ENUMERATED", diag_main)
        self.assertIn("trap_mcause", diag_main)
        self.assertIn('csrr %0, cycle', diag_main)
        self.assertNotIn("mcycle", diag_main)
        self.assertIn("H417_USBSS_PROBE_ADDR 0x20178180U", diag_main)
        self.assertIn("IWDG_Enable();", diag_main)
        self.assertIn("SystemAndCoreClockUpdate();", diag_main)
        self.assertIn("s_chip_id = DBGMCU_GetCHIPID();", diag_main)
        self.assertIn("Chip = (s_chip_id >> 4U) & 0x0FU;", diag_main)
        self.assertLess(
            diag_main.index("Chip = (s_chip_id >> 4U) & 0x0FU;"),
            diag_main.index("USBSS_Device_Init(ENABLE);"),
        )
        self.assertLess(
            diag_main.index("h417_usbss_watchdog_pll_probe();"),
            diag_main.index("h417_usbfs_cdc_diag_init();"),
        )
        self.assertIn("v3f_board_init();", diag_main)
        self.assertIn("H417_USBSS_STAGE_PROBE_RESULT_DRAIN", diag_main)
        self.assertIn("PLL_ARMED", diag_main)
        self.assertIn("PLL_WRITE_OK", diag_main)
        self.assertIn("PLL_TIMEOUT", diag_main)
        self.assertIn("CALL_DEVICE_INIT", diag_main)
        self.assertIn("H417_USBSS_PROBE_USBSS_DEVICE_INIT_ARMED", diag_main)
        self.assertIn("H417_USBSS_PROBE_USBSS_DEVICE_INIT_RETURNED", diag_main)
        self.assertIn("H417_USBSS_STAGE_WAIT_ENUM", diag_main)
        self.assertIn("USBSS_DevEnumStatus != 0U", diag_main)
        self.assertIn("usbss_link_int_ctrl", diag_main)
        self.assertIn("usbss_lmp_rx_data0", diag_main)
        self.assertIn("usbss_lmp_port_cap", diag_main)
        self.assertNotIn("FS state=READY", diag_main)
        self.assertIn("NVIC_SetPriority(USBSS_LINK_IRQn, 0x00U)", diag_main)
        self.assertIn("NVIC_SetPriority(USBSS_IRQn, 0x00U)", diag_main)
        self.assertIn("NVIC_SetPriority(TIM12_IRQn, 0x00U)", diag_main)
        self.assertIn("NVIC_SetPriority(USBFS_IRQn, 0x80U)", diag_main)
        self.assertIn("h417_usbfs_cdc_diag_poll_control", diag_main)
        self.assertIn("h417_usbfs_cdc_diag_take_advance", diag_main)
        self.assertIn("USBHS_QUIESCED", diag_main)
        self.assertIn("h417_usbfs_cdc_diag_try_write", diag_main)
        self.assertIn("memset(USBSS_EP0_Buf", diag_main)
        self.assertIn("ch32h417_usbhs_hid_nkro_init", fs_adapter)
        self.assertIn("ch32h417_usbfs_hid_nkro_init", fs_adapter)
        self.assertIn("NVIC_SetPriority(USBHS_IRQn, 0x00U)", fs_adapter)
        self.assertIn("NVIC_SetPriority(USBFS_IRQn, 0x80U)", fs_adapter)
        self.assertIn("h417_usbfs_cdc_diag_tx_idle", fs_adapter)
        self.assertIn("NVIC_DisableIRQ(USBFS_IRQn);", fs_adapter)
        self.assertIn("USBFS_CDC_Debug_Send", fs_adapter)
        self.assertIn("USBFS_RingBuffer_Comm", fs_adapter)
        self.assertIn('memcmp(&USBFS_Data_Buffer', fs_adapter)
        self.assertIn("#define MyDevDescr H417_Product_USBHS_MyDevDescr", product_usbhs)
        self.assertIn('#include "usb_desc.c"', product_usbhs)
        self.assertIn('#include "ch32h417_usbhs_device.c"', product_usbhs)
        self.assertIn(".usbhs_state 0x2011148C", diag_linker)
        self.assertIn(".usbhs_dma 0x201114E0", diag_linker)
        self.assertIn("SIZEOF(.usbhs_dma) == 0x267C", diag_linker)
        self.assertIn(".usbfs_dma 0x20113BF0", diag_linker)
        self.assertIn("SIZEOF(.usbfs_dma) == 0x514", diag_linker)
        self.assertIn(".stack ORIGIN(RAM_LOW) + LENGTH(RAM_LOW)", diag_linker)
        self.assertIn(".usbss_dma (NOLOAD)", diag_linker)

    def test_acquisition_schedule_covers_full_left_and_right_halves(self):
        makefile = read_text("hw_tests", "ch585", "Makefile")
        common_h = read_text("hw_tests", "ch585", "src", "ch585_common.h")
        source = read_text(
            "hw_tests", "ch585", "src", "ch585_ads7948_32k_pipeline.c"
        )

        self.assertIn("ch585_ads7948_32k_pipeline", makefile)
        self.assertIn("FREQ_SYS=78000000", makefile)
        self.assertIn("void ch585_ads7948_32k_pipeline_run(void);", common_h)
        self.assertIn("ACQ32_TARGET_HZ 32000U", source)
        self.assertIn("ACQ32_PERIOD_REMAINDER", source)
        self.assertIn("ACQ32_CH0_ADC0_COUNT 10U", source)
        self.assertIn("ACQ32_CH1_ADC1_COUNT 11U", source)
        self.assertIn("ACQ32_CH0_ADC0_COUNT 9U", source)
        self.assertIn(
            '#define ACQ32_HALF_NAME "right"\n'
            "#define ACQ32_ADC0_CS GPIO_Pin_15\n"
            "#define ACQ32_ADC1_CS GPIO_Pin_14",
            source,
        )
        self.assertIn(
            '#define ACQ32_HALF_NAME "left"\n'
            "#define ACQ32_ADC0_CS GPIO_Pin_14\n"
            "#define ACQ32_ADC1_CS GPIO_Pin_15",
            source,
        )
        self.assertIn("ACQ32_RESULT", source)
        self.assertIn("overruns", source)

    def test_host_validation_tools_and_combined_guide_exist(self):
        paths = (
            ("hw_tests", "h417", "tools", "usbss_fs_diag_monitor.py"),
            ("hw_tests", "h417", "tools", "ch585_spi_32k_budget.py"),
            ("hw_tests", "ch585", "tools", "ch585_32k_acq_monitor.py"),
            ("hw_tests", "README.md"),
        )
        for parts in paths:
            self.assertTrue(os.path.exists(os.path.join(ROOT, *parts)), parts)

        diag_tool = read_text(*paths[0])
        spi_tool = read_text(*paths[1])
        acq_tool = read_text(*paths[2])
        guide = read_text(*paths[3])
        self.assertIn("PID = 0xFE17", diag_tool)
        self.assertIn("USBFS diagnostic CDC", diag_tool)
        self.assertIn("advance_dtr(port, after=\"START\")", diag_tool)
        self.assertIn("advance_dtr(port, after=step)", diag_tool)
        self.assertIn('if "SS19 boot=" in decoded', diag_tool)
        self.assertIn('if "SS19 step=" in decoded', diag_tool)
        self.assertIn("PASS_USBSS_ENUMERATED", diag_tool)
        self.assertIn("SESSION_SILENCE_SECONDS", diag_tool)
        self.assertIn('"--reconnect-timeout"', diag_tool)
        self.assertIn("waiting_for=1A86:FE17", diag_tool)
        self.assertNotIn("port.write", diag_tool)
        self.assertIn('"--no-start"', diag_tool)
        self.assertIn("two_half_state_us", spi_tool)
        self.assertIn("31.25", spi_tool)
        self.assertIn("total_overruns", acq_tool)
        self.assertIn("USB 3.0 communication", guide)
        self.assertIn("h417_v3f_usbss_ch372", guide)
        self.assertIn("VID_1A86&PID_5537", guide)
        self.assertIn("CH37x", guide)
        self.assertIn("usbss_fs_diag_monitor.py", guide)
        self.assertIn("H417 to CH585 SPI speed", guide)
        self.assertIn("CH585 full-half 32K acquisition", guide)

    def test_spi_speed_path_uses_valid_mode0_dma_and_78mhz_slave(self):
        ch585_makefile = read_text("hw_tests", "ch585", "Makefile")
        slave_link = read_text(
            "firmware", "ch585", "drivers", "ch585_spi0_slave_link.c"
        )
        h417_test = read_text(
            "hw_tests", "h417", "passed", "v5f_rtthread", "src", "v5f_hw_test.c"
        )
        budget_tool = read_text(
            "hw_tests", "h417", "tools", "ch585_spi_32k_budget.py"
        )

        speed_section = ch585_makefile.split(
            "else ifeq ($(TEST),ch585_spi0_speed_slave)", 1
        )[1].split("else ifeq", 1)[0]
        self.assertIn("FREQ_SYS=78000000", speed_section)
        self.assertIn("ch585_spi0_slave_link_wait_tx_only_done", slave_link)
        self.assertIn('#include "ch32h417_dma.h"', h417_test)
        self.assertIn("CH585_SPI_SPEED_TX_DMA_REQ 63U", h417_test)
        self.assertIn("DMA1_FLAG_TC2", h417_test)
        self.assertIn("mode=mode0-dma", h417_test)
        self.assertNotIn("SPI_CPHA_2Edge", h417_test)
        self.assertIn('"core"', budget_tool)
        self.assertIn("--cycle-hz", budget_tool)


if __name__ == "__main__":
    unittest.main()
