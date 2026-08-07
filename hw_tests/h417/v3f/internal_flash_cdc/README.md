# H417 internal Flash diagnostic over USB CDC

This new-board diagnostic runs on the CH32H417 V3F core only. It does not
start V5F. It reuses the product USBFS HID + CDC implementation and keeps
PE12 configured as a push-pull low output for the entire run.

The test is deliberately more extensive than a single erase/write/read:

1. records chip ID, 480/960 KiB capacity selection, dual-bank mode, reset
   flags, Flash controller registers, option bytes and write protection;
2. scans all 8 KiB before modifying it and reports erased/zero/other word
   counts, CRC32, and the first/last word;
3. checks the known final signature left by the previous run, so a second
   reset or power cycle tests data retention;
4. records a CRC32 of the adjacent 256-byte guard range;
5. tries the WCH fast 8 KiB erase API and verifies every one of the 2048
   words is the CH32 CodeFlash erased value `0xE339E339`;
6. if the fast erase verification fails, retries with the bounded standard
   page-erase API;
7. programs all 8 KiB in 32 fast-program pages using all-zero,
   `0xAAAAAAAA`, `0x55555555`, walking-zero, walking-one, and deterministic
   pseudo-random patterns;
8. verifies every programmed word, expected/read CRC32, first mismatch,
   both `0x080xxxxx` and `0x000xxxxx` Flash aliases, and 16 repeated full
   reads for instability;
9. performs another standard erase, then programs and verifies a separate
   256-byte signature one word at a time with the standard API;
10. verifies that signature again after 250 ms and confirms the rest of the
    erase page stayed erased;
11. confirms the adjacent guard CRC did not change;
12. protects potentially unbounded fast Flash calls with the independent
    watchdog. If a call hangs, the next boot reports the exact retained
    stage instead of leaving CDC permanently silent.

## Destructive Flash range

Only this range is erased:

```text
0x080EE000 .. 0x080EFFFF  (8 KiB)
```

This is the final erase page of the H417 960 KiB CodeFlash. It is outside:

- the V3F image at `0x08000000 .. 0x0800FFFF`;
- the V5F linker region starting at `0x08010000`;
- the product profile storage at `0x08090000 .. 0x080B1FFF`.

Do not place other data in the test page while using this program.

## Build

From the `hardware` repository:

```powershell
make -B -C hw_tests/h417/v3f/internal_flash_cdc
```

The generated V3F images are:

```text
hw_tests/h417/v3f/internal_flash_cdc/build/h417_internal_flash_cdc.hex
hw_tests/h417/v3f/internal_flash_cdc/build/h417_internal_flash_cdc.bin
```

Flash the V3F image at its linked address. This test does not need a V5F
image.

## Read the result

Open the enumerated `VID:PID 1A86:FE17` CDC COM port at any baud rate
(115200 is conventional). The test waits for DTR, so opening the port after
the Flash operation has finished still prints the complete result. Closing
and reopening the port prints it again.

The included reader opens the port with DTR enabled, waits up to 45 seconds,
and uses these exit codes:

- `0`: `RESULT PASS`
- `3`: `RESULT DEGRADED`
- `2`: `RESULT FAIL`
- `1`: timeout or serial-port error

```powershell
powershell -ExecutionPolicy Bypass `
  -File hw_tests/h417/v3f/internal_flash_cdc/read_result.ps1 `
  -Port COM8 -TimeoutSeconds 45
```

The report banner must be `H417 INTERNAL FLASH DIAG v3`. Version 2 used
the STM32-style `0xFFFFFFFF` erased-state assumption and therefore falsely
reported a normal CH32 erased page as failed. Do not use a v2 result to
judge Flash health.

The report contains 27 lines. The most important fields are:

- `api=4` means the WCH function returned `FLASH_COMPLETE`; readback
  verification still determines PASS or FAIL.
- Complete API status values are: `1=BUSY`, `2=ERROR_PG`,
  `3=ERROR_WRP`, `4=COMPLETE`, `5=TIMEOUT`, `253=OP_RANGE`,
  `254=ALIGN`, and `255=ADDRESS_RANGE`.
- `bad` is the number of mismatching 32-bit words.
- `off`, `exp`, and `got` identify the first mismatch.
- `wrperr=1` is hardware write-protection evidence.
- `wp119=1` means the active-low WPR option bit covering sectors 31..119
  already marks the final test page write-protected.
- `WATCHDOG_RECOVERY` means the controller call did not return before the
  watchdog reset. `RESET ... stage/detail` identifies the operation/page or
  word.
- `TEST_INTERRUPTED_RESET` uses the same retained stage data but means the
  reset was not identified as an independent-watchdog reset.
- `FLASH_ALIVE_FAST_PATH_FAIL` means the standard erase/program path proved
  the Flash array works, but the WCH fast path failed.
- `ERASE_BLOCKED_OR_DEAD` means neither fast nor standard erase produced an
  erased page and no write-protection error was observed.
- `READ_UNSTABLE`, `FLASH_ALIAS_MISMATCH`, or
  `ADJACENT_PAGE_CORRUPTED` indicate more serious read/array/addressing
  faults.

Retained stage values useful for a recovery report are: `10` fast erase,
`20` standard fallback erase, `30` fast program (`detail` is page 0..31),
`50` repeated read, `60` final standard erase, `70` word program (`detail`
is word 0..63), and `80` delayed verification.

In particular, a page containing 2048 copies of `0xE339E339` after erase
is correctly erased on CH32; it is not a dead-read signature. A v3
`ERASE_* FAIL` means the page did not reach that value. If both v3 erase
paths fail verification with `wp119=0` and `wrperr=0`, or a watchdog
repeatedly recovers from the same Flash-controller stage, the evidence for
a defective Flash/controller is much stronger.

After any run that reaches the standard word-program stage, reset or
power-cycle the board once and capture the report again. The second report
should show `retention=PASS`. The test is destructive and repeats the Flash
erase/program cycle on every normal boot.
