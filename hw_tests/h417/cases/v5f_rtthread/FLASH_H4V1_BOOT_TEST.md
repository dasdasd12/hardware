# H417 H4V1 Flash isolation test

The qualified `h417_v5f_sdram_video_h4v1` target remains unchanged.
`h417_v5f_flash_h4v1_install` reuses its V3F image and V5F object files, then
adds four read-only Flash stages in an isolated ITCM tail.

Stage 1 runs only after the original H4V1 test has printed `RESULT PASS`. It
only issues NAND `READ ID (0x9F)` and `GET FEATURE (0x0F)` commands.

After Stage 1 succeeds, Stage 2 scans reserved blocks 768..1015 for the first
factory-good block, then reads its pages 0..3 twice using `PAGE READ (0x13)`
and `READ FROM CACHE (0x03)`. It requires the already-enabled internal ECC,
rejects uncorrectable pages, records corrected-page severity, checks
repeatability and reports an 8 KiB CPU byte-loop sanity rate. That short rate
is not the final sustained-bandwidth measurement. Neither stage resets,
configures, erases or programs the Flash, and neither uses DMA or SDRAM.

After Stage 2 succeeds, Stage 3 reuses its final verified blank page already
resident in the NAND cache. It issues only `READ FROM CACHE (0x03)` plus
read-only ID/config probes, then measures CPU and DMA1 cache throughput at
12.5, 25, 33.3 and 50 MHz. The 256 KiB DMA sample uses DMA1 channels 2/3 and
shared SRAM; it does not use DMA2 or SDRAM. This stage measures the SPI/cache
bus ceiling, not NAND-array page-load latency.

After Stage 3 succeeds, Stage 4 performs the real NAND-array end-to-end
measurement. It reads all 64 pages of the already-qualified blank block 768
eight times. Every page uses `PAGE READ (0x13)`, OIP/ECC polling and a 2 KiB
DMA1 `READ FROM CACHE (0x03)` at 50 MHz with HSRX disabled. The test verifies
the entire 1 MiB as `0xff`, requires repeat-stable hashes, reports array,
cache and wall throughput plus ECC statistics, and remains strictly
read-only. It does not reset or configure the NAND and does not touch DMA2,
SDRAM, LTDC or USB while measuring.

Stage 5 is deliberately excluded from that read-only target. The separate
`h417_v5f_flash_h4v1_write_probe` target is destructive and chains Stage 5
only after Stages 1 through 4 have passed. It owns fixed scratch block 1015
(page 0 row `0x0000fdc0`); blocks 1016..1023 remain reserved for L8 assets.
Stage 5 requires a good `0xff` bad-block marker, ECC enabled, and the normal
fully locked protection value `A0=0x38` before it temporarily sets A0 to
`0x0a`. It then erases the
block, verifies page 0 erased, programs and ECC-verifies a deterministic 2
KiB pattern, and erases/verifies the block again. Every exit attempts WRDI,
A0 restoration and DMA/SPI restoration. A Stage 5 PASS therefore promises
the scratch block was returned blank; a FAIL line explicitly reports whether
dirty data may remain.

## Build

```powershell
cd C:\program1\Program\AI_keyb_wch\hardware\hw_tests\h417
make HW_TEST=h417_v5f_flash_h4v1_install
```

Program both images:

- `build/h417_v5f_flash_h4v1_install/V3F/h417_V3F.bin` at `0x08000000`
- `build/h417_v5f_flash_h4v1_install/V5F/rtthread_ch32h417_V5F.bin` at `0x08010000`

## Run Stage 4

```powershell
cd C:\program1\Program\AI_keyb_wch\hardware
powershell -ExecutionPolicy Bypass `
  -File hw_tests/h417/tools/upload_sdram_h4v1.ps1 `
  -Port COM7 `
  -FrameCount 90 `
  -PackedPath C:\program1\Program\AI_keyb_wch\.tmp\sdram_video\miku_h4v1_chunk16_90f_30fps.h4v `
  -ReusePacked `
  -FlashStage4
```

Expected final output:

```text
RESULT PASS
FLASH STAGE1 START ...
FLASH STAGE1 PASS mid=c8 did=91 mode=... a0=... b0=... c0=... f0=... spi_timeout=0
H4V1 Flash Stage1 PASS.
FLASH STAGE2 START ...
FLASH STAGE2 PASS block=... marker=ff hash=.../... ... e2e_KiBps=... cache_KiBps=... scope=sanity
H4V1 Flash Stage2 PASS.
FLASH STAGE3 START ...
FLASH STAGE3 RATE idx=... div=... hsrx=... valid=... cpu_KiBps=... dma_KiBps=...
FLASH STAGE3 PASS valid_mask=... best_idx=... best_dma_KiBps=... scope=cache_bus_not_array_e2e
H4V1 Flash Stage3 PASS.
FLASH STAGE4 START ...
FLASH STAGE4 PASS block=768 rows=49152..49215 pages=64 repeats=8 ... e2e_KiBps=... cache_KiBps=... wall_KiBps=... ecc=...
H4V1 Flash Stage4 PASS.
```

## Build and run destructive Stage 5

Only use this target when erasing/programming NAND block 1015 is authorized:

```powershell
cd C:\program1\Program\AI_keyb_wch\hardware\hw_tests\h417
make HW_TEST=h417_v5f_flash_h4v1_165_write_probe
```

The V3F image is byte-identical to the qualified baseline. If that V3F image
is already on the board, program only the write-probe V5F image at
`0x08010000` without erasing the V3F region; otherwise program both images.
Then run:

```powershell
cd C:\program1\Program\AI_keyb_wch\hardware
powershell -ExecutionPolicy Bypass `
  -File hw_tests/h417/tools/upload_sdram_h4v1.ps1 `
  -Port COM7 `
  -ChunkedAbsolute `
  -PackedPath C:\program1\Program\AI_keyb_wch\.tmp\sdram_video\xiaofengjin_h4v1_chunk16_165f_30fps.h4v `
  -ReusePacked `
  -FlashStage5
```

Expected tail output includes:

```text
FLASH STAGE4 PASS ...
FLASH STAGE5 START destructive=1 target=write_probe block=1015 ...
FLASH STAGE5 PASS block=1015 row=0000fdc0 ... blank=1 ...
H4V1 Flash Stage5 PASS.
```

If the H4V1 test fails, Stage 1 is never entered. If the Flash ID/status read
fails, Stage 2 is skipped. The original H4V1 `RESULT PASS` remains visible;
the script uses exit code 3 for Stage 1 failure, 4 for Stage 2 failure, 5 for
Stage 3 failure, 6 for Stage 4 failure and 7 for Stage 5 failure.

`-FlashStage5` only tells the host script to wait for the extra result; it is
not an erase authorization message. Authorization is represented by building
and programming the explicitly named destructive target. The normal install
target `h417_v5f_flash_h4v1_install` continues to contain only the four
read-only stages; the full 165-frame installer below has a different target
name and a separate host authorization.

## Install the qualified 165-frame image

The full installer is a separate destructive image named
`h417_v5f_flash_h4v1_165_install`. It is rebased on the exact 165-frame SDRAM
image and preserves that image's USB-to-SDRAM path. Before the original H4V1
test prints `RESULT PASS`, the installer performs no NAND access. Its startup
banner is exactly:

```text
H417 FLASH H4V1 INSTALL v001 CHUNK165 COMMIT V01
```

The host refuses to send the first payload byte unless that banner is seen,
the package matches the frozen 165-frame contract, and both independent host
switches are present. This makes the ordinary 165-frame image and the Stage 5
write-probe image fail closed before upload.

The installed package is fixed to ARGB1555, 800x480, 165 frames at 30 fps,
180-degree host rotation, and chunked-absolute 16 KiB records. The qualified
transfer is 30,965,760 bytes with CRC32 `e32a6c99`; the container and padded
transfer SHA-256 values are respectively:

```text
969E9292EB5F99897C2120F36978BCEA11D47AA6A8BC37C51970D932BBEDDFCE
4B21CFB3DF55BA07444EADA282DA93CDF315E362A10CDCB5922C0C68E793FA65
```

Build and program the dedicated installer:

```powershell
cd C:\program1\Program\AI_keyb_wch\hardware\hw_tests\h417
make HW_TEST=h417_v5f_flash_h4v1_165_install
```

- Program `build/h417_v5f_flash_h4v1_165_install/V3F/h417_V3F.bin` at
  `0x08000000` if the qualified V3F image is not already present.
- Program
  `build/h417_v5f_flash_h4v1_165_install/V5F/rtthread_ch32h417_V5F.bin` at
  `0x08010000`.

Run the install only when erasing/programming blocks 768..1014 is authorized:

```powershell
cd C:\program1\Program\AI_keyb_wch\hardware
powershell -ExecutionPolicy Bypass `
  -File hw_tests/h417/tools/upload_sdram_h4v1.ps1 `
  -Port COM7 `
  -ChunkedAbsolute `
  -ReusePacked `
  -FlashInstall `
  -AuthorizeBlocks768To1014
```

`-FlashInstall` selects the dedicated protocol. The separately named
`-AuthorizeBlocks768To1014` switch is the destructive authorization; neither
switch is accepted alone, and install mode cannot be combined with any
`-FlashStage1` through `-FlashStage5` switch.

The layout is transactional:

- block 768 holds the manifest; descriptor page 0 is written after payload
  verification and commit page 1 is written last;
- blocks 769..1014 are the only payload candidates; 15,120 pages require 237
  factory-good blocks, leaving capacity for at most nine factory-bad blocks;
- block 1015 remains the Stage 5 scratch block and is not authorized by the
  install command;
- blocks 1016..1023 remain reserved for the existing L8 assets.

After `RESULT PASS`, the host requires the exact install-hook identity and then
waits, in order, for `PLAN`, `INVALIDATE`, `PAYLOAD`, `VERIFY`, `MANIFEST`, and
`COMMIT` PASS records. One 600-second deadline covers those install records.
Any FAIL, missing record, out-of-contract block range, incomplete byte/page
count, CRC mismatch, missing commit, or failure to restore `A0=0x38` and
`B0=0x10` makes the host fail.

The successful tail has this fixed shape (map-dependent values are shown as
ellipses):

```text
FLASH INSTALL PLAN PASS candidates=246 good=... bad=... unreadable=... selected=237 first=... last=...
FLASH INSTALL INVALIDATE PASS manifest=768 committed=0 a0=38 wel=0
FLASH INSTALL PAYLOAD PASS bytes=30965760 pages=15120 blocks=237 source_crc=e32a6c99
FLASH INSTALL VERIFY PASS bytes=30965760 pages=15120 crc=e32a6c99 ecc_worst=...
FLASH INSTALL MANIFEST PASS block=768 descriptor=valid crc=........ map=237
FLASH INSTALL COMMIT PASS block=768 page=1 crc=........ descriptor=........ committed=1 a0=38 b0=10 wel=0
```

The cold-boot player remains a separate, read-only second image. It must accept
the Flash package only when manifest descriptor page 0 and commit page 1 agree
on the format, dimensions, frame count, transfer length, CRC and bad-block map.
An erased, partial, stale or mismatched manifest is invalid. The first cold-boot
implementation should preload the complete package from NAND into SDRAM and
then reuse the qualified SDRAM decoder/LTDC path; direct Flash streaming is not
part of this install step.

## Verify the installed image after a cold boot

The cold-boot player is a separate read-only target:

```powershell
cd C:\program1\Program\AI_keyb_wch\hardware\hw_tests\h417
make HW_TEST=h417_v5f_flash_h4v1_165_coldboot
```

Program
`build/h417_v5f_flash_h4v1_165_coldboot/V5F/rtthread_ch32h417_V5F.bin`
at `0x08010000` and power-cycle the board. If the matching qualified V3F image
is not already present, also program
`build/h417_v5f_flash_h4v1_165_coldboot/V3F/h417_V3F.bin` at `0x08000000`.
The monitor
does not open, hash or upload a video file; its only host-to-MCU write is the
ASCII command `STATUS\r\n`, used once per second as a harmless status probe.
Playback does not need to consume that command: while the H4C loop is running,
the firmware periodically replays the exact banner and the three PASS records.
That passive replay is what lets a monitor recover after attaching late or
after USB re-enumeration.

```powershell
cd C:\program1\Program\AI_keyb_wch\hardware
powershell -ExecutionPolicy Bypass `
  -File hw_tests/h417/tools/monitor_flash_h4v1_coldboot.ps1 `
  -Port COM7
```

The firmware identity line must match exactly before any PASS record is
accepted:

```text
H417 FLASH H4V1 COLD BOOT v001 READONLY PLAY V01
```

The monitor then requires these stages in order:

```text
FLASH COLD BOOT MANIFEST PASS ... bytes=30965760 crc=e32a6c99 map=237 frames=165 fps=30 descriptor=........ commit=........
FLASH COLD BOOT LOAD PASS ... bytes=30965760 crc=e32a6c99 map=237 frames=165 fps=30 descriptor=........ commit=........
FLASH COLD BOOT PLAY PASS ... bytes=30965760 crc=e32a6c99 map=237 frames=165 fps=30 descriptor=........ commit=........
```

Field order and extra diagnostics may vary, but each required field must occur
exactly once. `descriptor` and `commit` are eight-digit lowercase CRC values
captured from `MANIFEST PASS`; `LOAD PASS` and `PLAY PASS` must repeat those
same two values. They are deliberately not hard-coded because the descriptor
contains the factory-bad-block map.

Before the exact banner has been witnessed, any `FLASH COLD BOOT <stage> PASS`
record is counted and ignored: it is never validated, accepted, or used to
advance the state machine. The monitor keeps waiting for the next passive
replay, then starts a fresh strict `MANIFEST -> LOAD -> PLAY` transaction at
the exact banner. This covers the valid late-attach sequence in which the
monitor first sees the original `PLAY PASS` and only later sees the replayed
banner. Repeated already-passed records after the banner are accepted only
when they do not skip a prerequisite and still satisfy the full contract.

An unexpected H417 banner, a future stage arriving before its prerequisite
after identity has been established, a missing or duplicated contract token,
a CRC/map/geometry mismatch, or any `FLASH COLD BOOT <stage> REJECT|FAIL` line
fails immediately. `REJECT` and `FAIL` are fatal even before the banner. A
successful monitor exit reports `upload_bytes=0`.
