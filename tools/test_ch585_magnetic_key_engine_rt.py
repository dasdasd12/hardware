import os
import shutil
import subprocess
import tempfile
import textwrap
import unittest


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))


class Ch585MagneticKeyEngineRtTest(unittest.TestCase):
    def test_default_rt_triggers_on_300_per_mille_travel(self):
        source = textwrap.dedent(
            r"""
            #include <stdint.h>
            #include <stdio.h>

            #include "magnetic_key_engine.h"

            static int require_true(int condition, const char *message)
            {
                if (!condition)
                {
                    printf("%s\n", message);
                    return 1;
                }
                return 0;
            }

            static int update_one(mag_key_engine_t *engine,
                                  uint16_t raw,
                                  uint8_t expected_down,
                                  const char *message)
            {
                uint16_t sample[1] = { raw };
                const mag_key_state_t *state;

                if (mag_key_engine_update(engine, sample) != MAG_KEY_STATUS_OK)
                {
                    printf("update failed: %s\n", message);
                    return 1;
                }

                state = mag_key_engine_state(engine, 0U);
                if ((state == 0) || (state->is_down != expected_down))
                {
                    printf("%s raw=%u pos=%u down=%u expected=%u\n",
                           message,
                           raw,
                           (state == 0) ? 0U : state->position_pm,
                           (state == 0) ? 0U : state->is_down,
                           expected_down);
                    return 1;
                }

                return 0;
            }

            int main(void)
            {
                mag_key_config_t cfg;
                mag_key_engine_t engine;

                mag_key_default_config(&cfg);
                if (require_true(cfg.mode == MAG_KEY_MODE_RAPID_TRIGGER,
                                 "default mode must be rapid trigger") != 0)
                {
                    return 1;
                }
                if (require_true(cfg.rt_press_delta_pm == 300U,
                                 "default RT press delta must be 300/1000 travel") != 0)
                {
                    return 1;
                }
                if (require_true(cfg.rt_release_delta_pm == 300U,
                                 "default RT release delta must be 300/1000 travel") != 0)
                {
                    return 1;
                }
                if (require_true(cfg.filter_shift == 0U,
                                 "default RT path must use direct ADC samples") != 0)
                {
                    return 1;
                }
                if (require_true(mag_key_engine_init(&engine, 1U, &cfg) == MAG_KEY_STATUS_OK,
                                 "engine init failed") != 0)
                {
                    return 1;
                }

                /*
                 * With released=490 and pressed=330, raw=474 is about 100/1000
                 * travel. raw=427 is +293/1000 and raw=426 is +300/1000.
                 */
                if (update_one(&engine, 474U, 0U, "baseline must not press") != 0) return 1;
                if (update_one(&engine, 427U, 0U, "less than 300/1000 must not press") != 0) return 1;
                if (update_one(&engine, 426U, 1U, "300/1000 downward travel must press") != 0) return 1;
                if (update_one(&engine, 473U, 1U, "less than 300/1000 upward travel must stay down") != 0) return 1;
                if (update_one(&engine, 474U, 0U, "300/1000 upward travel must release") != 0) return 1;
                if (update_one(&engine, 427U, 0U, "second less-than-delta move must not press") != 0) return 1;
                if (update_one(&engine, 426U, 1U, "RT re-press must not require static press_pm") != 0) return 1;

                return 0;
            }
            """
        )

        tmpdir = tempfile.mkdtemp()
        try:
            test_c = os.path.join(tmpdir, "test_magnetic_key_engine_rt.c")
            exe = os.path.join(tmpdir, "test_magnetic_key_engine_rt.exe")
            with open(test_c, "w", encoding="ascii") as handle:
                handle.write(source)

            compile_cmd = [
                "gcc",
                "-std=c99",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                os.path.join(ROOT, "firmware", "ch585", "applications"),
                test_c,
                os.path.join(ROOT, "firmware", "ch585", "applications", "magnetic_key_engine.c"),
                "-o",
                exe,
            ]
            subprocess.check_call(compile_cmd, cwd=ROOT)
            process = subprocess.Popen(
                [exe],
                cwd=ROOT,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
            )
            stdout, stderr = process.communicate()
            self.assertEqual(process.returncode, 0, stdout + stderr)
        finally:
            shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == "__main__":
    unittest.main()
