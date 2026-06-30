# -*- coding: utf-8 -*-
from __future__ import print_function

import io
import os
import re
import unittest


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))


def path(*parts):
    return os.path.join(ROOT, *parts)


def read_text(*parts):
    with io.open(path(*parts), "r", encoding="utf-8", errors="ignore") as handle:
        return handle.read()


class V3fProfileFlashContract(unittest.TestCase):
    def test_v3f_build_uses_profile_runtime_and_external_flash_store(self):
        makefile = read_text("firmware", "h417", "v3f", "Makefile")

        self.assertIn("applications/profile_runtime.c", makefile)
        self.assertIn("applications/profile_store_gd5f1g.c", makefile)
        self.assertIn("drivers/gd5f1g_spi_nand/include", makefile)
        self.assertIn("drivers/gd5f1g_spi_nand/src/gd5f1g_spi_nand.c", makefile)
        self.assertIn("drivers/gd5f1g_spi_nand/src/ch32h417_gd5f1g_spi1.c", makefile)
        self.assertNotIn("applications/default_profile.c", makefile)

    def test_compiled_profile_format_is_shared_and_fits_one_nand_page(self):
        header = read_text("firmware", "common", "aik_profile_runtime.h")

        self.assertIn("AIK_PROFILE_RUNTIME_MAGIC", header)
        self.assertIn("AIK_PROFILE_RUNTIME_VERSION", header)
        self.assertIn("aik_profile_runtime_v1_t", header)
        self.assertIn("AIK_KEY_COUNT_TOTAL", header)
        self.assertIn("AIK_PROFILE_RUNTIME_SIZE", header)
        self.assertTrue(
            re.search(r"AIK_PROFILE_RUNTIME_SIZE\s*<=\s*2048", header) is not None
        )

    def test_v3f_main_loads_external_profile_before_ch585_spi_link(self):
        main_c = read_text("firmware", "h417", "v3f", "applications", "main.c")

        self.assertIn('#include "profile_runtime.h"', main_c)
        self.assertIn('#include "profile_store_gd5f1g.h"', main_c)
        self.assertIn("v3f_profile_store_load_fixed", main_c)
        self.assertIn("v3f_profile_runtime_apply", main_c)
        self.assertNotIn("default_profile.h", main_c)
        self.assertNotIn("v3f_default_profile_build_nkro16", main_c)

        load_index = main_c.index("v3f_profile_store_load_fixed")
        link_index = main_c.index("v3f_ch585_link_init")
        self.assertLess(load_index, link_index)

    def test_runtime_report_path_has_no_hardcoded_keymap_table(self):
        app_dir = path("firmware", "h417", "v3f", "applications")
        combined = ""
        for name in os.listdir(app_dir):
            if name.endswith((".c", ".h")):
                combined += "\n/* {0} */\n".format(name)
                combined += read_text("firmware", "h417", "v3f", "applications", name)

        self.assertNotIn("s_default_key_outputs", combined)
        self.assertNotIn("HID_USAGE_F12", combined)
        self.assertNotIn("v3f_default_profile_build_nkro16", combined)
        self.assertIn("v3f_profile_runtime_build_nkro16", combined)

    def test_profile_store_reads_fixed_external_flash_location_without_fallback_write(self):
        store = read_text("firmware", "h417", "v3f", "applications", "profile_store_gd5f1g.c")

        self.assertIn("V3F_PROFILE_FLASH_BLOCK", store)
        self.assertIn("gd5f1g_read_page", store)
        self.assertIn("ch32h417_gd5f1g_spi1_init", store)
        self.assertIn("ch32h417_gd5f1g_spi1_release", store)
        self.assertNotIn("gd5f1g_program_page", store)
        self.assertNotIn("gd5f1g_block_erase", store)
        self.assertTrue(
            re.search(r"factory|fallback", store, flags=re.IGNORECASE) is None
        )

    def test_gd5f1g_spi1_adapter_releases_shared_spi1_before_ch585_link(self):
        header = read_text(
            "firmware", "h417", "v3f", "drivers",
            "gd5f1g_spi_nand", "include", "ch32h417_gd5f1g_spi1.h"
        )
        source = read_text(
            "firmware", "h417", "v3f", "drivers",
            "gd5f1g_spi_nand", "src", "ch32h417_gd5f1g_spi1.c"
        )

        self.assertIn("ch32h417_gd5f1g_spi1_release", header)
        self.assertIn("void ch32h417_gd5f1g_spi1_release", source)
        self.assertIn("SPI_I2S_DeInit(SPI1)", source)
        self.assertIn("GPIO_Mode_IN_FLOATING", source)


if __name__ == "__main__":
    unittest.main()
