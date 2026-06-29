import importlib.util
import os
import unittest


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
TUI_PATH = os.path.join(ROOT, "hw_tests", "h417", "tools", "ch585_adc_key_cal_tui.py")


def load_tui_module():
    spec = importlib.util.spec_from_file_location("ch585_adc_key_cal_tui", TUI_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Ch585AdcKeyCalTuiTest(unittest.TestCase):
    def test_sample_parser_keeps_user_selected_key_while_old_samples_arrive(self):
        tui = load_tui_module()
        state = tui.CalState(side="left", key_count=36)
        state.current_key = 9

        changed = tui.parse_line(
            state,
            "CAL_SAMPLE side=left key=8 seq=1 raw=490 min=490 max=490 count=1 status=0 spi=0 diag=0x000002c0",
        )

        self.assertTrue(changed)
        self.assertEqual(state.current_key, 9)
        self.assertEqual(state.last_sample_key, 8)
        self.assertEqual(state.keys[8].raw, 490)

    def test_sample_parser_updates_only_on_pc_owned_minmax_change(self):
        tui = load_tui_module()
        state = tui.CalState(side="left", key_count=36)

        first = tui.parse_line(
            state,
            "CAL_SAMPLE side=left key=8 seq=1 raw=490 min=000 max=000 count=1 status=0 spi=0 diag=0x000002c0",
        )
        new_min = tui.parse_line(
            state,
            "CAL_SAMPLE side=left key=8 seq=2 raw=480 min=000 max=000 count=2 status=0 spi=0 diag=0x000002c0",
        )
        in_range = tui.parse_line(
            state,
            "CAL_SAMPLE side=left key=8 seq=3 raw=485 min=000 max=000 count=3 status=0 spi=0 diag=0x000002c0",
        )

        self.assertTrue(first)
        self.assertTrue(new_min)
        self.assertFalse(in_range)
        self.assertEqual(state.keys[8].raw, 485)
        self.assertEqual(state.keys[8].min_raw, 480)
        self.assertEqual(state.keys[8].max_raw, 490)

    def test_command_builder_uses_line_protocol(self):
        tui = load_tui_module()

        self.assertEqual(tui.build_key_command(9), "key 9\n")
        self.assertEqual(tui.build_reset_command(), "reset\n")
        self.assertEqual(tui.build_stop_command(), "stop\n")
        self.assertEqual(tui.build_start_command(), "start\n")

    def test_command_ack_redraws_dashboard(self):
        tui = load_tui_module()
        state = tui.CalState(side="left", key_count=36)

        changed = tui.parse_line(
            state,
            "CAL_CMD ok side=left key=9 key_count=36 stream=1 reset=0",
        )

        self.assertTrue(changed)
        self.assertIn("CAL_CMD ok", state.last_line)

    def test_render_contains_current_key_and_table(self):
        tui = load_tui_module()
        state = tui.CalState(side="right", key_count=41)
        state.current_key = 1
        tui.parse_line(
            state,
            "CAL_SAMPLE side=right key=1 seq=6 raw=856 min=856 max=856 count=32 status=0 spi=0 diag=0x000002c4",
        )
        tui.parse_line(
            state,
            "CAL_SAMPLE side=right key=1 seq=7 raw=353 min=353 max=856 count=33 status=0 spi=0 diag=0x000002c4",
        )

        screen = tui.render_screen(state, width=96, max_rows=18)

        self.assertIn("CH585 ADC Key Cal", screen)
        self.assertIn("side=right", screen)
        self.assertIn("key=1/40", screen)
        self.assertIn("raw=0353", screen)
        self.assertIn("span=503", screen)
        self.assertIn("K01", screen)

    def test_demo_lines_are_valid_samples(self):
        tui = load_tui_module()
        state = tui.CalState(side="left", key_count=36)
        changed = [tui.parse_line(state, line) for line in tui.demo_lines()]

        self.assertTrue(any(changed))
        self.assertIsNotNone(state.keys[0].min_raw)
        self.assertIsNotNone(state.keys[1].max_raw)


if __name__ == "__main__":
    unittest.main()
