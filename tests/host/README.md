# Host C tests

These tests compile selected firmware logic with desktop GCC. They do not
access hardware and are safe to run on a development PC.

```powershell
make -C tests/host run
```

The suite covers:

- approval control and the fixed V3F-to-V5F approval mailbox;
- H417-to-CH585 transaction and resynchronization behavior;
- PC profile upload, commit, activation, and runtime parsing;
- profile shortcuts;
- cross-MCU profile synchronization for both CH585 halves.

Generated executables stay under `tests/host/build/`.
