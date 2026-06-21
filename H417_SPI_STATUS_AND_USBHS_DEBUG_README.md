# H417 SPI 状态记录与 USBHS 调试计划

本文用于交接当前 H417-CH585 SPI 调试结论，并记录下一步 USBHS 调试入口。

## 1. 当前 SPI 结论

当前 H417 已经从软件 SPI 回到硬件 SPI2，只有 CS 用 GPIO 控制。这样符合队友的设计意见：SPI 时钟和 MOSI/MISO 走硬件外设，多个 CH585 只需要用不同 GPIO 片选。

当前一块 CH585 的接线是：

| H417 | CH585 | 方向 | 作用 |
| --- | --- | --- | --- |
| PB12 | PA12 | H417 -> CH585 | GPIO CS |
| PB13 | PA13 | H417 -> CH585 | SPI2 SCK |
| PC1 | PA14 | H417 -> CH585 | SPI2 MOSI |
| PC2 | PA15 | CH585 -> H417 | SPI2 MISO |
| GND | GND | 双向 | 共地 |
| 3V3 | 3V3 | 电源 | 同电平 |

当前协议是 request-only 短帧：

```text
H417 拉低 CS
H417 clock 16 bytes
CH585 返回已经准备好的 16B 状态帧
H417 拉高 CS
H417 校验、统计、处理，再请求下一帧
```

16B 状态帧格式：

| offset | size | field |
| --- | --- | --- |
| 0 | 1 | magic = 0xD7 |
| 1 | 1 | type = 0x11 |
| 2 | 1 | source_id |
| 3 | 1 | seq |
| 4 | 1 | ack_seq, 当前 request-only 下为 0xff |
| 5 | 1 | flags, bit7 表示 READY |
| 6 | 8 | down_bits[8], 64 键 0/1 状态 |
| 14 | 2 | crc16-ccitt over bytes 0..13 |

CH585 侧已经改成 ping-pong 双缓冲：当前 DMA 发送 front buffer，后台准备 next buffer。这样避免在 SPI CS 临界路径里做 ADC 模拟、CRC 和帧组装。

## 2. SPI 实测状态

最后一次稳定结果：

```text
125us 请求周期
每档训练 64 帧
50MHz: bad=64/64
33MHz: bad=64/64
25MHz: bad=64/64
20MHz HSRX2: bad=48/64 seq=8
20MHz HSRX1: bad=41/64 seq=10
16.7MHz HSRX2: bad=0/64 seq=0
normal poll crc/seq_drop 不继续增加
```

结论：

- 当前一块 CH585、16B 短帧、125us 请求周期下，稳定工作档是约 16MHz SCK。
- 20MHz 以上仍然不稳定，问题更像物理连线、MISO 建立时间、采样相位、驱动能力或板级布线问题，而不是帧格式本身。
- 如果以后必须上 40MHz 以上，建议用示波器或逻辑分析仪确认 SCK/MISO 波形和采样点，再考虑缩短线长、多根地线、调驱动能力、端接、换更合适的引脚或重新画板。

## 3. SPI 后续要做

1. 把 CH585 的真实 ADC 扫描和按键 0/1 判断接入后台缓冲，不要放回 SPI 发送临界路径。
2. 第二块 CH585 接入时，继续共用 SCK/MOSI，用独立 CS 和独立 MISO，H417 轮询两块。
3. 如果产品最终只需要 0/1 按键状态，优先保留 16B 短帧；raw ADC 大帧只作为调试或校准模式。
4. 决定 `seq` 是否保留。现在 `seq` 对查丢帧和重复帧很有用，但正式极限带宽模式可以再评估是否删除。
5. 高速问题不要继续只靠改协议猜测，下一步需要电气测量。

## 4. USBHS 当前代码入口

H417 V5F 工程位置：

```text
F:\嵌赛\hardware\rtthread_port
```

关键文件：

| 文件 | 作用 |
| --- | --- |
| `applications/main.c` | RT-Thread 主循环，初始化 SPI 和 USB CDC，周期打印诊断 |
| `applications/usb_cdc_dual.c` | CherryUSB CDC 描述符、FS/HS/SS 初始化、主动发送接口 |
| `rt-thread/components/drivers/usb/cherryusb/port/ch32h417/usb_dc_ch32h417_usbhs.c` | CH32H417 USBHS device controller 驱动 |
| `rt-thread/components/drivers/usb/cherryusb/port/ch32h417/usb_dc_ch32h417.c` | USB bus 分发和统一诊断入口 |
| `Makefile` | 当前默认打开 USBHS CDC，关闭 USBFS CDC |

当前 Makefile 默认：

```makefile
APP_ENABLE_USB2_HS_CDC ?= 1
APP_ENABLE_USB2_FS_CDC ?= 0
```

需要回退到 USBFS 调试时，可以这样编译：

```powershell
make APP_ENABLE_USB2_HS_CDC=0 APP_ENABLE_USB2_FS_CDC=1 PREFIX='F:/嵌赛/.codex_toolchain_wch12/bin/riscv-wch-elf-'
```

## 5. USBHS 观察方法

评估板资料里 USB 口分两组：

| 接口 | MCU 引脚 | 说明 |
| --- | --- | --- |
| USBFS/ISP | PA11 = DM, PA12 = DP | WCHISP USB 下载和之前 USBFS CDC 调试用 |
| USBHS | PB9 = USB2DM, PB8 = USB2DP | 本轮要调的 USB2.0 High-Speed |

注意：`PB8/PB9` 同时也是 `SWCLK/SWDIO` 复用脚。也就是说，用 WCH-Link 烧录时可以接调试器，但烧录完成后要观察 USBHS，建议拔掉 WCH-Link 调试连接，再插 USBHS 口并复位。否则 WCH-Link 可能会占用或加载 USBHS 的 D+/D- 线，PC 侧不会给 USBHS bus reset。

2026-06-21 已补一项代码对齐官方例程：V5F 的 USBHS 驱动初始化时会主动执行 `GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE)`，释放 PB8/PB9 给 USBHS。这样即使 V3F 唤醒代码没有先执行禁用 SWJ，USBHS 仍会自己抢回 D+/D- 引脚。

烧录时建议只保留 WCH-Link，先不要插 H417 USBHS 数据线。烧录完成后拔掉 WCH-Link，再插 USBHS 口，然后看两个现象：

1. UART8/WCH-Link COM 日志里应出现：

```text
Initializing USB2.0 HS CDC loopback on USBHS port...
USBHS CDC init ok
[USBHS] device init done ...
```

插上 USBHS 线后，诊断里应看到 `rst/rdy/setup/xfer` 等计数变化。

2. Windows 设备管理器里应出现新的 USB CDC 串口，产品字符串类似：

```text
CH32H417 USBHS CDC
```

如果只看到 WCH-Link 的 COM 口，说明 PC 还没有枚举到 H417 USBHS。

如果拔掉 WCH-Link 后就看不到 UART8 日志，这是正常的。此时优先用 Windows 设备管理器看 USBHS CDC 是否出现；如果必须同时看日志，需要另接一个独立 USB-TTL 到 UART8，不要占用 PB8/PB9。

## 6. 本轮 USBHS 调试目标

先不追求正式 8K HID，只验证 USBHS CDC 能枚举和传输文本：

```text
H417 V5F RT-Thread
  -> USBHS CDC 枚举
  -> PC 出现新 COM
  -> 打开 COM 后能看到 KS/KE 或 loopback 文本
```

确认 USBHS CDC 稳定后，再把正式键盘 HID 8K 上报接到同一 USBHS device 侧。

## 7. 2026-06-21 USBHS 最新排查记录

现象：
- Windows 能看到插入/拔出事件，但显示为 `Unknown USB Device (Invalid Device Descriptor)`，常见实例类似 `USB\VID_0000&PID_0005`。
- 这说明 USBHS 物理口至少有连接事件，不是单纯“完全没接上”；当前主要怀疑点是 EP0 控制传输/描述符阶段。

本次代码修正方向：
- `PB8/PB9` 已在 USBHS 初始化时通过 `GPIO_Remap_SWJ_Disable` 释放给 USBHS。
- USBHS EP0 初始状态改为：RX ACK、TX NAK，和 WCH 官方 USBHS 例程一致。
- SETUP 包之后，EP0 IN/OUT toggle 改为从 DATA1 开始，和 USB 控制传输规范、现有 USBFS 驱动、WCH 官方 USBHS 例程一致。
- EP0 OUT 不再强制加 `TOG_MATCH`，避免 SETUP/状态阶段被 toggle 判断误伤。
- 普通 `start_write/start_read` 不再每次重置 endpoint toggle，避免 bulk/interrupt 端点后续传输 toggle 错乱。

下一步验证：
1. 烧录新版 V5F/V3F，烧录时只接 WCH-Link，不接 USBHS 数据线。
2. 烧录完成后拔掉 WCH-Link，只插 USBHS 口，然后按 reset。
3. Windows 应该从 `Invalid Device Descriptor` 变为 CDC 设备或至少出现明确的 `VID_1A86` 设备。
4. 如果仍然是 `VID_0000&PID_0005`，下一步改用 WCH 官方 USBHS 例程最小化验证硬件口，再回迁到 CherryUSB 驱动。

## 8. 2026-06-21 official USBHS result

Official WCH example tested:

```text
F:\嵌赛\EVT\EXAM\USBHS\DEVICE\SimulateCDC
```

Build command that worked:

```powershell
make -j CORE=both EVT_ROOT=../../.. PREFIX='F:/嵌赛/.codex_toolchain_wch12/bin/riscv-wch-elf-'
```

Flash images:

```text
build\v3f\SimulateCDC_V3F.elf
build\v5f\SimulateCDC_V5F.elf
```

Windows result after flashing official example:

```text
USB Serial Device (COM6)
USB\VID_1A86&PID_FE0C\0123456789
```

Conclusion:

- H417 USBHS hardware path is OK.
- Cable/host/board USBHS connector are OK enough for enumeration.
- Our current failure is in `rtthread_port` USBHS/CherryUSB device-controller driver, not in the physical USBHS port.

Patch currently prepared in our driver:

- Removed the long delay inside `USBHS_UDIF_BUS_RST`.
- Removed the second `USBHS_UD_RST_LINK` pulse inside BUS reset interrupt.
- BUS reset now follows the official style more closely: reset software state, clear address, reinitialize endpoints, then let CherryUSB handle reset.

## 9. 2026-06-21 USBHS CDC success

After the latest `rtthread_port` firmware was rebuilt and flashed, Windows
enumerated the H417 USBHS port successfully.

Observed device:

```text
USB Composite Device
USB\VID_1A86&PID_FE31\2026060903
USB Serial Device (COM7)
USB\VID_1A86&PID_FE31&MI_00\6&3BE8D75&0&0000
```

This means the previous `VID_0000&PID_0002 Device Descriptor Request Failed`
problem has been fixed for the current test board/cable/host combination.

Code changes that led to this pass:

- V3F wakeup firmware no longer initializes the USBFS/USBHS PLL by default.
  V3F now mainly wakes V5F and stays in the idle loop.
- V5F USBHS RCC init now follows the official USBHS example more closely: if
  SYSPLL is not sourced from USBHS PLL, it reconfigures and enables USBHS PLL
  instead of skipping just because `USBHS_PLLRDY` was already set.
- The experimental USBHS SWJ remap and 5-second delay were removed from the
  USBHS DCD path. The official USBHS example does not need this remap.
- EP0 interrupt handling now clears `UEP*_TX_DONE` and `UEP*_RX_DONE`, and SETUP
  detection uses the saved RX control register value before clearing DONE.

Current validation target:

1. Open `COM7` or the newly assigned `USB Serial Device` COM port.
2. Confirm that USB CDC text output appears.
3. If CDC stays stable, the next step is to route the H417 scan/keyboard report
   stream through USBHS CDC first, then later replace it with the formal 8K USB
   keyboard/HID report path.

## 10. 2026-06-21 USBHS CDC + SPI status output

Latest firmware was rebuilt and flashed to both H417 cores:

```text
V5F: hardware/rtthread_port/build/v5f/rtthread_ch32h417_v5f.elf
V3F: hardware/rtthread_port/v3f_wakeup/build/v3f_wakeup.elf
```

Test wiring sequence:

1. Flash with only WCH-Link connected. Do not plug USBHS during flashing.
2. After flash success, unplug WCH-Link.
3. Plug H417 USBHS.
4. Press reset.

Windows result:

```text
COM7
USB Serial Device (COM7)
USB\VID_1A86&PID_FE31&MI_00\6&3BE8D75&0&0000
```

PC-side read helper:

```powershell
hardware/rtthread_port/tools/read_usbhs_cdc.ps1 -Port COM7 -Seconds 60 -LogPath hardware/rtthread_port/.usbhs_logs/usbhs_cdc_60s.log
```

Observed output includes both scan reports and status reports:

```text
KS f=1 i=000 1000 1000 1000 1000 1000 1000 3000 1000
SS hb=89 s0ok=90 s0fetch=0 s0crc=0 s0seq=0 s1ok=90 raw0=1000 raw1=1000 raw63=1000 raw64=1000
```

Meaning:

- `KS` is the current keyboard scan value stream sent over USBHS CDC.
- `SS` is the compact debug status line.
- `s0ok` increasing means H417 is receiving valid source0 frames from CH585.
- `s0fetch=0`, `s0crc=0`, `s0seq=0` means no fetch/CRC/sequence errors were observed in this 60 s test.
- `s1ok` currently comes from the fake source1 path, because only one CH585 is connected right now.

Current conclusion:

- The chain `CH585 short-frame SPI -> H417 parser -> H417 USBHS CDC -> PC` is now observable from the PC.
- The current test confirms communication correctness at the present SPI setting, but it does not yet prove the final 40 MHz+ target.
- Next work should focus on raising the SPI clock in controlled steps while watching `SS` error counters, then moving USBHS CDC debug output toward the final 8K keyboard/HID report path.

## 11. 2026-06-21 SPI auto-train observation

Important correction:

- The H417 code already has an automatic SPI training path.
- The training path is enabled by default:

```c
APP_CH585_SPI_AUTO_TRAIN = 1
APP_CH585_SPI_AUTO_TRAIN_AFTER_POLLS = 8
APP_CH585_SPI_TRAIN_FRAMES = 64
APP_CH585_SPI_TRAIN_INTERFRAME_US = 125
```

The previous plan of manually stepping the prescaler is still useful, but it
must be interpreted together with the auto-train result. If auto-train is
enabled, the firmware may override the default prescaler after startup.

Latest flashed firmware added these fields to the USBHS CDC `SS` line:

```text
sck=<source0 estimated SCK kHz>
p=<selected SPI prescaler register value>
h=<selected high-speed RX mode>
c=<selected CPHA edge number>
tr=<train_done>/<train_errors>/<train_frames>
```

Observed PC log after flashing that version:

```text
SS hb=17 s0ok=18 s0fetch=0 s0crc=0 s0seq=0 sck=16000.0 p=0020 h=1 c=1 tr=1/0/64 s1ok=18 raw0=1000 raw1=3000 raw63=1000 raw64=1000
SS hb=25 s0ok=26 s0fetch=0 s0crc=0 s0seq=0 sck=16000.0 p=0020 h=1 c=1 tr=1/0/64 s1ok=26 raw0=1000 raw1=1000 raw63=1000 raw64=1000
SS hb=33 s0ok=34 s0fetch=0 s0crc=0 s0seq=0 sck=16000.0 p=0020 h=1 c=1 tr=1/0/64 s1ok=34 raw0=1000 raw1=1000 raw63=1000 raw64=3000
```

Interpretation:

- Auto-train completed: `tr=1/...`.
- The selected training result had zero errors across 64 frames: `tr=1/0/64`.
- Runtime source0 remained clean in this sample: `s0fetch=0`, `s0crc=0`, `s0seq=0`.
- The selected SPI clock was about `16 MHz`: `sck=16000.0`.
- The selected configuration was `p=0020`, `h=1`, `c=1`.

This means the current automatic training code did not select a 40 MHz+ mode on
the present board/wiring/CH585 firmware combination. It selected the highest
candidate that passed according to its current scoring rule, or the first
zero-error candidate in its current candidate order.

Current auto-train candidate order in `rtthread_port/applications/ch585_spi_scan.c`:

```text
mode0-hsrx2-cpha1
mode0-hsrx1-cpha1
mode1-hsrx2-cpha1
mode1-hsrx1-cpha1
mode2-hsrx2-cpha1
mode2-hsrx1-cpha1
mode2-hsrx1-cpha2
mode3-hsrx2-cpha1
mode3-hsrx1-cpha2
mode3-hsrx1-cpha1
mode4-hsrx2-cpha1
mode4-hsrx1-cpha1
mode5-hsrx1-cpha1
```

The exact MHz value depends on HCLK and the H417 high-speed SPI divider mode.
The firmware prints the selected result as `sck=<kHz>`, which is the value to
trust during bring-up.

## 12. 2026-06-21 pending train-candidate visibility patch

A newer local firmware revision has been compiled but not yet flashed at the
time of this note.

Purpose:

- Keep the same SPI protocol.
- Keep the same auto-train policy.
- Add USBHS CDC visibility for every train candidate.

New expected USBHS CDC line:

```text
TR i=<index>/<count> p=<prescaler> h=<hsrx> c=<cpha_edges> bad=<bad_frame_count> seq=<sequence_error_count>
```

Example format:

```text
TR i=0/13 p=0002 h=2 c=1 bad=64 seq=0
TR i=1/13 p=0002 h=1 c=1 bad=64 seq=0
TR i=10/13 p=0020 h=2 c=1 bad=3 seq=0
TR i=11/13 p=0020 h=1 c=1 bad=0 seq=0
```

What to look for after flashing this patch:

- If all high-speed candidates show `bad=64`, the H417 is clocking faster than
  the CH585-side response or wiring can currently tolerate.
- If high-speed candidates show small nonzero `bad`, the next target is timing
  margin: CPHA, CS setup/hold, command-to-data delay, and signal integrity.
- If high-speed candidates show `bad=0` but `seq>0`, payload decoding is fine
  but frame pacing/sequence alignment is wrong.
- If a 40 MHz+ candidate is clean but auto-train still selects 16 MHz, the
  selection rule or candidate ordering should be changed.

Immediate next workflow:

1. Flash the pending firmware with `TR` output.
2. Boot with USBHS CDC connected.
3. Capture at least 60 seconds from `COM7` using:

```powershell
hardware/rtthread_port/tools/read_usbhs_cdc.ps1 -Port COM7 -Seconds 60 -LogPath hardware/rtthread_port/.usbhs_logs/usbhs_cdc_train_candidates_60s.log
```

4. Inspect `SS` for the selected runtime config.
5. Inspect `TR` for why faster candidates did or did not pass.

Current known-good PC observation before the pending `TR` patch:

```text
USB Serial Device (COM7)
USB\VID_1A86&PID_FE31&MI_00\6&3BE8D75&0&0000
```

Current known-good runtime chain:

```text
CH585 short-frame SPI
  -> H417 source0 parser
  -> H417 scan/raw merge
  -> USBHS CDC text report
  -> PC COM7 log
```

Current limitation:

- Only one CH585 is connected.
- Source0 is real CH585 over H417 SPI2.
- Source1 is still fake data.
- USBHS CDC is a debug transport, not the final 8K HID keyboard transport.
- The final 40 MHz+ SPI target has not been proven yet.

## 13. 2026-06-21 train-candidate result after Core-both flash

The pending `TR` patch was flashed successfully using the stable dual-core
OpenOCD path:

```text
Core both flash:
  V5F verified OK
  V3F verified OK
```

Important flashing note:

- `Core both` works reliably for the current USBHS firmware.
- Separate-core flashing was unstable for this test flow: V5F/V3F both could
  verify OK, but USBHS did not enumerate afterward.
- For now, use `Core both` for H417 USBHS/SPI debug unless there is a specific
  reason to isolate one core.

USBHS result after `Core both` flash:

```text
USB Serial Device (COM7)
USB\VID_1A86&PID_FE31&MI_00\6&3BE8D75&0&0000
USB Composite Device
USB\VID_1A86&PID_FE31\2026060903
```

Captured log:

```text
hardware/rtthread_port/.usbhs_logs/usbhs_cdc_train_candidates_80s.log
```

Auto-train selected result:

```text
SS hb=57 s0ok=58 s0fetch=0 s0crc=0 s0seq=0 sck=16000.0 p=0020 h=2 c=1 tr=1/0/64 s1ok=58 raw0=1000 raw1=1000 raw63=1000 raw64=1000
```

Candidate results observed:

```text
TR i=0/11  p=0000 h=2 c=1 bad=64 seq=0
TR i=1/11  p=0000 h=1 c=1 bad=64 seq=0
TR i=2/11  p=0008 h=2 c=1 bad=64 seq=0
TR i=3/11  p=0008 h=1 c=1 bad=64 seq=0
TR i=4/11  p=0010 h=2 c=1 bad=64 seq=0
TR i=5/11  p=0010 h=1 c=1 bad=64 seq=0
TR i=6/11  p=0010 h=1 c=2 bad=64 seq=0
TR i=7/11  p=0018 h=2 c=1 bad=60 seq=2
TR i=8/11  p=0018 h=1 c=2 bad=64 seq=0
TR i=9/11  p=0018 h=1 c=1 bad=61 seq=2
TR i=10/11 p=0020 h=2 c=1 bad=0  seq=0
```

Approximate interpretation with current clock tree:

```text
p=0000: about 48 MHz, fails completely
p=0008: about 32 MHz, fails completely
p=0010: about 24 MHz, fails completely
p=0018: about 19.2 MHz, mostly fails
p=0020: about 16 MHz, passes
```

Conclusion from this run:

- The 16 MHz selection is data-driven, not just a conservative default.
- Current wiring/CH585 firmware/H417 timing does not pass 24 MHz+.
- The failure cliff is currently between about 16 MHz and 19.2 MHz.
- Because high-speed candidates mostly show `bad` frame errors rather than only
  `seq` errors, the likely issue is physical/timing/frame-validity level, not
  just host-side sequence bookkeeping.

Recommended next debug direction:

1. Keep `Core both` flashing for repeatability.
2. Add or try slower timing margins around the CH585 response:
   `CS setup/hold`, `command-to-data delay`, and possibly one extra dummy/drain
   frame before measuring.
3. Add missing candidate combinations if needed, especially `p=0018 h=2 c=2`
   and `p=0020 h=1/c=1`, to make the cliff more complete.
4. If timing changes do not move the cliff, inspect signal integrity with a
   logic analyzer or oscilloscope: SCK edge quality, MISO setup/hold at H417,
   common ground, wire length, and CH585 drive strength.
5. Only after the 16->19.2 MHz cliff moves should we spend time on 40 MHz+.

## 14. 2026-06-21 architecture decision: magnetic algorithm on CH585

Latest decision:

- The final magnetic-key algorithm should run on each CH585 front-end.
- H417 should not depend on receiving full raw ADC arrays for normal operation.
- H417 should mainly aggregate the two CH585 reports, handle USB reporting, and
  keep debug/diagnostic paths.

Reason:

- Current H417-CH585 SPI link is proven stable at about 16 MHz.
- Auto-train shows the current failure cliff is between about 16 MHz and
  19.2 MHz; 24 MHz+ candidates fail completely.
- Sending full raw ADC data for 64 keys per CH585 every scan frame would create
  much higher bandwidth pressure than the current short state frame.
- If CH585 produces final key state locally, the normal frame can stay compact:
  `down_bits[8]`, status flags, sequence, optional counters.

Impact on the other agent's proposed modules:

- `firmware/ch585_frontend/ads7948.*` is still useful and should be considered
  the low-level ADS7948 bring-up driver.
- `firmware/ch585_frontend/ch585_ads7948_mux_scan.*` is still useful, but it
  should evolve from "threshold-to-down_bits only" into the CH585-side magnetic
  scan/decision layer.
- `firmware/common/magnetic_key_engine.*` should not be treated as the final
  H417-side runtime engine. It can remain as a reference, simulator, or possible
  unit-test model, but the product architecture should move the real-time
  magnetic algorithm to CH585.
- Current H417 `keyboard_engine.*` should also remain a prototype/debug helper
  while the CH585-side algorithm is developed.

Recommended CH585-side algorithm responsibilities:

1. Sample ADS7948 through the 16:1 MUX lanes.
2. Maintain per-key released/pressed calibration.
3. Filter raw ADC and normalize to travel/position if needed.
4. Apply hysteresis, rapid-trigger, and final press/release state decisions.
5. Fill the current short frame `down_bits[8]` for normal H417 reporting.
6. Optionally expose raw ADC or position data only in a slower debug/calibration
   frame, not in every high-rate normal frame.

Recommended H417 responsibilities after this decision:

1. Poll/request one frame from each CH585.
2. Verify magic/type/source/seq/CRC.
3. Merge two CH585 `down_bits[8]` blocks into the full keyboard state.
4. Send final keyboard state over USBHS.
5. Provide debug visibility through USBHS CDC while bring-up continues.

Short-term path:

- Do not upgrade the normal SPI frame to full raw ADC yet.
- First connect ADS7948/MUX code on CH585 and use it to generate real
  `down_bits[8]`.
- Keep using the current H417 short-frame parser so the SPI/USBHS chain remains
  stable while the CH585 scan algorithm is developed.
