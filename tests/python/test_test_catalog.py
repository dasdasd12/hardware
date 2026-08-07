import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CHECKER = ROOT / "tools" / "check_test_catalog.py"


def test_catalogs_are_complete_and_consistent():
    spec = importlib.util.spec_from_file_location("check_test_catalog", str(CHECKER))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    assert module.main([]) == 0


def test_all_32k_firmware_entries_are_preserved():
    hardware = module_json(ROOT / "hw_tests" / "catalog.json")
    entries = {entry["id"]: entry for entry in hardware["tests"]}
    for test_id in (
        "h417_v3f_usbss_ch372_baseline",
        "h417_v3f_usbss_fs_diag",
        "h417_v5f_ch585_spi_speed",
        "ch585_ads7948_32k_pipeline",
        "ch585_spi0_speed_slave",
    ):
        assert entries[test_id]["status"] == "feasibility"
        assert entries[test_id]["preserve"] is True


def module_json(path):
    import json

    return json.loads(path.read_text(encoding="utf-8"))
