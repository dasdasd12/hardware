# H417 SDRAM tests

Only two SDRAM test targets are maintained here:

| Target | State | Purpose |
| --- | --- | --- |
| `h417_v5f_sdram_memtest` | v70 hardware PASS; v71 full-x16 pending | 32 MiB, four-bank DMA256 write/read stress test comparing all 16 data bits (`0xFFFF`) |
| `h417_v5f_sdram_video` | In development | LTDC/video path debugging; its behavior is not a memory-test acceptance result |

## Validated memory test

The retained passing test identifies itself as:

```text
H417 SDRAM CDC TEST v65 SHARED DMA256 GAP0 E2E
```

The current revalidation build identifies itself as:

```text
H417 SDRAM CDC TEST v71 FULL16
```

It keeps PA9/PA10 high as GPIO outputs, moves their unused AFR fields from AF0
to AF15 so they cannot retain a second SDRAM D10/D11 route, and restores the
dedicated PD0/PD1 pins to AF1 with `PD0PD1_RM=0`. LTDC remains disabled. The
four complete 32 MiB DMA256 passes now write and compare all 16 data bits with
mask `0xFFFF`; the unmasked raw D10/D11 probe remains part of the final result.
The v70 board run proved the isolated D10/D11 combinations with zero errors;
v71 promotes those two bits into the full-capacity acceptance test.

The recorded hardware run completed four full 32 MiB write/read passes,
transferred 256 MiB in total, and ended with:

```text
DMA_SLICED STRESS END PASS mode=wide256 passes=4 transferred=268435456
FULL16 DMA RESULT PASS mask=ffff dma_rw=pass
SUMMARY bytes=33554432 ok=1 fail=0 stage=PATTERN
RESULT PASS
```

Build it with:

```powershell
make HW_TEST=h417_v5f_sdram_memtest -j4
```

Read a new hardware result with:

```powershell
powershell -ExecutionPolicy Bypass `
  -File tools/read_sdram_result.ps1 `
  -Port COM7
```

Use the actual enumerated COM port if it is not `COM7`.

## Video debug test

The current diagnostic is a single-variable LTDC sampling-edge experiment and
identifies itself as:

```text
H417 SDRAM LTDC TEST v29 ARGB8888 IPC STATIC INTERNAL
```

It embeds the first video frame as standard little-endian ARGB8888, scales it to
320x192, verifies its CRC, copies it to the
main-validated internal shared-SRAM framebuffer, and displays it centered on a
black 800x480 background. SDRAM, DMA, USB upload, frame switching, and vblank
updates do not participate. Panel-control outputs are asserted once during boot
and LTDC is started once. The only LTDC timing change from the shared driver is
the test-local pixel-clock polarity override from `IIPC` to `IPC`; the shared
main driver is not modified.

Build and read this visual diagnostic with:

```powershell
make HW_TEST=h417_v5f_sdram_video -j4
powershell -ExecutionPolicy Bypass `
  -File tools/read_sdram_result.ps1 `
  -Port COM7
```

Expected display: a stable centered 320x192 full-color Miku frame with a black border.
The firmware reports `RESULT PASS` after five seconds when the embedded frame
CRC, LTDC scan counter, IPC register state, and FIFO status are all valid. The
visual judgement is still required: compare false color around the neck/collar
and the horizontal line pattern against the previous IIPC result.

Do not use a video diagnostic result as the SDRAM production acceptance
result; the retained `h417_v5f_sdram_memtest` target is the acceptance test.
