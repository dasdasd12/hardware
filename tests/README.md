# PC-side tests

This directory contains tests that run without flashing a board.

- `host/`: C behavior tests compiled with desktop GCC.
- `python/`: pytest contract, parser, policy, and tooling tests.

Run everything:

```powershell
make -C tests/host run
python -m pytest -q
```

Flashable board tests live under `../hw_tests/`.
