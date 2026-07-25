import imp
import os
import sys
import types
import unittest

try:
    import StringIO
except ImportError:
    import io as StringIO


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
TUI_SCRIPT = os.path.join(
    ROOT, "firmware", "ch585", "tools", "ch585_half_scan_debug_tui.py"
)


START_LINE = "half_scan start half=0 sys=78000000 keys=36 rf=1 uart_dbg=1"
STATUS_LINE = (
    "hs half=0 seq=120 scan=345 raw_min=5:330 pos_max=5:1000 "
    "down=200000000000 first=5 spi=119 abort=1 last=0 host=1 cmd=1 hseq=88"
)
STATUS_LINE_WITH_HOST_RX = (
    STATUS_LINE + " rxcnt=32 rx=a6010000 hcrc=6c3a/6c3a"
)
RF_LINE = "half_scan rf tx tick=1000 done=998 reports=42 hseq=88 flags=0"
WIRED_ONLY_START_LINE = "half_scan start half=0 sys=78000000 keys=36 rf=0 uart_dbg=1"
WIRED_ONLY_STATUS_LINE = (
    "hs half=0 seq=120 scan=345 raw_min=5:330 pos_max=5:1000 "
    "down=200000000000 first=5 spi=119 abort=0 last=0 host=0 cmd=0 hseq=0 "
    "rxcnt=12 rx=00000000 hcrc=0000/84c0"
)
WIRED_ONLY_STATUS_LINE_WITH_PARTIAL_DUMMY_MOSI = (
    "hs half=0 seq=21700 scan=21700 raw_min=9:353 pos_max=9:856 "
    "down=000200000000 first=9 spi=21700 abort=0 last=0 host=0 cmd=0 hseq=0 "
    "rxcnt=7 rx=00000000 hcrc=0000/2a45"
)


def load_tui():
    return imp.load_source("ch585_half_scan_debug_tui", TUI_SCRIPT)


class Ch585HalfScanDebugTuiTests(unittest.TestCase):
    def test_parser_tracks_start_and_status_lines(self):
        tui = load_tui()
        state = tui.HalfScanState()

        self.assertTrue(tui.parse_line(state, START_LINE))
        self.assertTrue(tui.parse_line(state, STATUS_LINE))

        self.assertEqual(state.half, 0)
        self.assertEqual(state.sys_hz, 78000000)
        self.assertEqual(state.key_count, 36)
        self.assertEqual(state.rf_enabled, 1)
        self.assertEqual(state.seq, 120)
        self.assertEqual(state.scan, 345)
        self.assertEqual(state.raw_min_key, 5)
        self.assertEqual(state.raw_min, 330)
        self.assertEqual(state.pos_max_key, 5)
        self.assertEqual(state.pos_max, 1000)
        self.assertEqual(state.first_down, 5)
        self.assertEqual(state.spi_frames, 119)
        self.assertEqual(state.spi_aborts, 1)
        self.assertEqual(state.last_spi_result, 0)
        self.assertEqual(state.host_valid, 1)
        self.assertEqual(state.cmd, 1)
        self.assertEqual(state.hseq, 88)

    def test_parser_tracks_host_rx_debug_fields(self):
        tui = load_tui()
        state = tui.HalfScanState()

        self.assertTrue(tui.parse_line(state, STATUS_LINE_WITH_HOST_RX))

        self.assertEqual(state.host_rx_count, 32)
        self.assertEqual(state.host_rx_hex, "a6010000")
        self.assertEqual(state.host_crc, 0x6c3a)
        self.assertEqual(state.host_calc_crc, 0x6c3a)

    def test_down_bits_are_decoded_to_key_list(self):
        tui = load_tui()
        state = tui.HalfScanState()
        tui.parse_line(state, STATUS_LINE)

        self.assertEqual(state.down_hex, "200000000000")
        self.assertEqual(state.down_keys(), [5])
        self.assertEqual(state.down_count(), 1)

    def test_down_bits_decode_little_endian_bytes_and_key_count_limit(self):
        tui = load_tui()
        state = tui.HalfScanState()
        tui.parse_line(state, "half_scan start half=0 sys=78000000 keys=36 rf=0 uart_dbg=1")
        tui.parse_line(
            state,
            "hs half=0 seq=1 scan=2 raw_min=8:330 pos_max=35:1000 "
            "down=00010000080c first=8 spi=1 abort=0 last=0 host=1 cmd=0 hseq=1",
        )

        self.assertEqual(state.down_keys(), [8, 35])

    def test_parser_tracks_rf_tx_status(self):
        tui = load_tui()
        state = tui.HalfScanState()

        self.assertTrue(tui.parse_line(state, RF_LINE))

        self.assertEqual(state.rf_tick, 1000)
        self.assertEqual(state.rf_done, 998)
        self.assertEqual(state.rf_reports, 42)
        self.assertEqual(state.rf_hseq, 88)
        self.assertEqual(state.rf_flags, 0)

    def test_render_screen_contains_debug_fields(self):
        tui = load_tui()
        state = tui.HalfScanState()
        tui.parse_line(state, START_LINE)
        tui.parse_line(state, STATUS_LINE)
        tui.parse_line(state, RF_LINE)

        screen = tui.render_screen(state, 100, 24)

        self.assertTrue(screen.startswith("\x1b[H"))
        self.assertTrue(screen.endswith("\x1b[J"))
        self.assertIn("CH585 Half Scan Debug", screen)
        self.assertIn("sys=78000000", screen)
        self.assertIn("raw_min=K05:0330", screen)
        self.assertIn("pos_max=K05:1000", screen)
        self.assertIn("down=200000000000", screen)
        self.assertIn("keys=[5]", screen)
        self.assertIn("spi frames=119", screen)
        self.assertIn("host=valid", screen)
        self.assertIn("rf tick=1000", screen)

    def test_render_screen_contains_host_rx_debug_fields(self):
        tui = load_tui()
        state = tui.HalfScanState()
        tui.parse_line(state, STATUS_LINE_WITH_HOST_RX)

        screen = tui.render_screen(state, 100, 24)

        self.assertIn("host_rx count=32 head=a6010000", screen)
        self.assertIn("hcrc=6c3a/6c3a", screen)

    def test_wired_only_mode_does_not_treat_host_cmd_invalid_as_fault(self):
        tui = load_tui()
        state = tui.HalfScanState()
        tui.parse_line(state, WIRED_ONLY_START_LINE)
        tui.parse_line(state, WIRED_ONLY_STATUS_LINE)

        screen = tui.render_screen(state, 120, 24)

        self.assertIn("wired-only mode", screen)
        self.assertNotIn("host command CRC/magic is not valid", screen)

    def test_wired_only_dummy_mosi_without_start_line_is_not_spi_mode_fault(self):
        tui = load_tui()
        state = tui.HalfScanState()
        tui.parse_line(state, WIRED_ONLY_STATUS_LINE_WITH_PARTIAL_DUMMY_MOSI)

        screen = tui.render_screen(state, 120, 24)

        self.assertIn("wired-only mode", screen)
        self.assertNotIn("expected a601", screen)

    def test_demo_lines_include_start_and_status(self):
        tui = load_tui()
        lines = list(tui.demo_lines_once())

        self.assertTrue(any(line.startswith("half_scan start") for line in lines))
        self.assertTrue(any(line.startswith("hs half=") for line in lines))

    def test_raw_output_includes_rf_line(self):
        tui = load_tui()
        lines = [START_LINE, STATUS_LINE_WITH_HOST_RX, RF_LINE]
        old_stdout = sys.stdout
        try:
            output = StringIO.StringIO()
            sys.stdout = output
            tui.run_raw(lines)
        finally:
            sys.stdout = old_stdout

        self.assertIn("rf tick=1000", output.getvalue())
        self.assertIn("done=998", output.getvalue())

    def test_wrong_serial_package_reports_actionable_error(self):
        tui = load_tui()
        old_serial = sys.modules.get("serial")
        old_stderr = sys.stderr
        fake_serial = types.ModuleType("serial")
        fake_serial.__file__ = r"C:\fake\site-packages\serial\__init__.py"
        try:
            sys.modules["serial"] = fake_serial
            err = StringIO.StringIO()
            sys.stderr = err

            with self.assertRaises(SystemExit) as raised:
                next(tui.iter_serial_lines("COM5", 921600))
        finally:
            sys.stderr = old_stderr
            if old_serial is None:
                sys.modules.pop("serial", None)
            else:
                sys.modules["serial"] = old_serial

        self.assertEqual(raised.exception.code, 2)
        self.assertIn("wrong 'serial' package", err.getvalue())
        self.assertIn("pip uninstall serial", err.getvalue())


if __name__ == "__main__":
    unittest.main()
