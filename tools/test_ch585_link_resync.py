import os
import shutil
import subprocess
import tempfile
import unittest


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))


class Ch585LinkResyncTest(unittest.TestCase):
    def test_host_link_state_machine(self):
        tmpdir = tempfile.mkdtemp()
        try:
            exe = os.path.join(tmpdir, "host_ch585_link_test.exe")
            compile_cmd = [
                "gcc",
                "-std=gnu99",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                os.path.join(ROOT, "firmware", "tests", "host_stubs"),
                "-I",
                os.path.join(ROOT, "firmware", "common"),
                "-I",
                os.path.join(
                    ROOT, "firmware", "h417", "v3f", "applications"
                ),
                os.path.join(
                    ROOT, "firmware", "tests", "host_ch585_link_test.c"
                ),
                os.path.join(
                    ROOT,
                    "firmware",
                    "h417",
                    "v3f",
                    "applications",
                    "ch585_link.c",
                ),
                "-o",
                exe,
            ]
            subprocess.check_call(compile_cmd, cwd=ROOT)
            process = subprocess.run(
                [exe],
                cwd=ROOT,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                check=False,
            )
            self.assertEqual(
                process.returncode, 0, process.stdout + process.stderr
            )
            self.assertIn("host_ch585_link_test: PASS", process.stdout)
        finally:
            shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == "__main__":
    unittest.main()
