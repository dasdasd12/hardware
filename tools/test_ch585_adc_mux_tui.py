import imp
import os
import sys
import unittest


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
TUI_SCRIPT = os.path.join(
    ROOT, "hw_tests", "ch585", "tools", "ch585_adc_mux_tui.py"
)


SAMPLE_LINES = [
    "DATA adc_mux_profile=left",
    "DATA mux_counts=9,9,9,9",
    "FRAME side=left seq=12 scale=1024 keys=36 sampled=36 flags=0x02 down=0x0000000000000000",
    " L1 CS1(PB14) CH0 mux=9",
    "  D01 raw=0512 travel=50.0% [########........]",
    "  D02 raw=0256 travel=25.0% [####............]",
    " L2 CS2(PB15) CH0 mux=9",
    "  D01 raw=0768 travel=75.0% [############....]",
]


def load_tui():
    return imp.load_source("ch585_adc_mux_tui", TUI_SCRIPT)


class Capture(object):
    def __init__(self):
        self.parts = []

    def write(self, text):
        self.parts.append(text)

    def flush(self):
        pass

    def isatty(self):
        return True


class Ch585AdcMuxTuiTests(unittest.TestCase):
    def test_parser_tracks_latest_frame_without_serial_dependency(self):
        tui = load_tui()
        state = tui.TuiState()

        for line in SAMPLE_LINES:
            tui.parse_line(state, line)

        self.assertEqual(state.side, "left")
        self.assertEqual(state.seq, 12)
        self.assertEqual(state.scale, 1024)
        self.assertEqual(state.keys, 36)
        self.assertEqual(state.flags, "0x02")
        self.assertEqual(state.lanes[0].label, "L1")
        self.assertEqual(state.lanes[0].cs, "CS1")
        self.assertEqual(state.lanes[0].pin, "PB14")
        self.assertEqual(state.lanes[0].channel, 0)
        self.assertEqual(state.lanes[0].samples[0].raw, 512)
        self.assertEqual(state.lanes[0].samples[0].travel, "50.0%")
        self.assertEqual(state.lanes[1].samples[0].raw, 768)

    def test_render_screen_uses_fixed_position_ansi_home_and_clear_tail(self):
        tui = load_tui()
        state = tui.TuiState()

        for line in SAMPLE_LINES:
            tui.parse_line(state, line)

        screen = tui.render_screen(state)

        self.assertTrue(screen.startswith("\x1b[H"))
        self.assertTrue(screen.endswith("\x1b[J"))
        self.assertIn("CH585 ADC MUX TUI", screen)
        self.assertIn("side=left", screen)
        self.assertIn("seq=12", screen)
        self.assertIn("scale=1024", screen)
        self.assertIn("L1 CS1(PB14) CH0", screen)
        self.assertIn("D01 raw=0512 travel=50.0% [########........]", screen)

    def test_demo_lines_produce_multiple_frames(self):
        tui = load_tui()
        frames = list(tui.demo_lines())

        self.assertTrue(any("FRAME side=left" in line for line in frames))
        self.assertTrue(any(" L1 " in line for line in frames))
        self.assertTrue(any("raw=" in line and "travel=" in line for line in frames))

    def test_run_tui_uses_alternate_screen_so_refresh_does_not_scroll(self):
        tui = load_tui()

        capture = Capture()
        old_stdout = sys.stdout
        try:
            sys.stdout = capture
            tui.run_tui(iter([None]), 10.0)
        finally:
            sys.stdout = old_stdout

        output = "".join(capture.parts)
        self.assertTrue(output.startswith("\x1b[?1049h"))
        self.assertIn(tui.ANSI_HOME, output)
        self.assertTrue(output.endswith(tui.ANSI_SHOW_CURSOR + "\x1b[?1049l"))

    def test_render_screen_fits_24_row_terminal_without_bottom_scroll(self):
        tui = load_tui()
        state = tui.TuiState()

        for line in tui.demo_lines():
            tui.parse_line(state, line)

        screen = tui.render_screen(state)
        body = screen[len(tui.ANSI_HOME):-len(tui.ANSI_CLEAR_TAIL)]
        rows = body.split("\n")

        self.assertLessEqual(len(rows), 24)
        self.assertFalse(body.endswith("\n"))

    def test_run_tui_keeps_static_layout_and_updates_dynamic_cells_only(self):
        tui = load_tui()
        capture = Capture()
        old_stdout = sys.stdout
        try:
            sys.stdout = capture
            tui.run_tui(iter(tui.demo_lines()), 1000.0)
        finally:
            sys.stdout = old_stdout

        output = "".join(capture.parts)
        self.assertEqual(output.count("L1 CS1(PB14) CH0 mux=9"), 1)
        self.assertEqual(output.count("L2 CS2(PB15) CH0 mux=9"), 1)
        self.assertEqual(output.count("CH585 ADC MUX TUI"), 1)
        self.assertIn("\x1b[", output)
        self.assertIn("0138", output)


if __name__ == "__main__":
    unittest.main()
