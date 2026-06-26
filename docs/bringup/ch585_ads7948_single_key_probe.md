# CH585 ADS7948 单键探针调试

更新时间: 2026-06-24

## 当前目标

现在 PCB、ADS7948 和 MUX 已经装上，但只有右半边最下排左边第一个键接好了。当前调试不要直接扫全键盘，先用一个固定通道验证:

```text
Hall sensor -> MUX -> ADS7948 -> CH585 SPI1
  -> CH585 本地滤波/position/按键判断
  -> CH585 KEY_DEBUG 短帧
  -> H417 SPI 拉取
  -> H417 USBFS 串口日志
```

## 默认探针通道

根据 `contest_report_template.pdf` 里的键位和原理图表格:

- 右半边从右到左、从上到下编号。
- 右半边最下排左边第一个键是 Space，对应全局 key 41。
- 右半边 MUX4 的 D11 是 Space。
- 当前 CH585 本地 64 键映射是 `lane * 16 + mux`。
- 若 D1 对应 mux 0，则 MUX4/D11 对应 `lane=3, mux=10, key_id=58`。

所以默认编译参数使用:

```text
CH585_ADC_PROBE_DEBUG_KEY=58
```

如果日志里 raw ADC 不随按键变化，优先试:

```text
CH585_ADC_PROBE_DEBUG_KEY=59
```

这可以排除 D11 是否按 1-based/0-based 映射接错一位。

## CH585 使用到的 PCB 引脚

右半边 CH585 是报告中的 U3。

| 功能 | CH585 引脚 |
| --- | --- |
| ADS7948 SPI1 SCK | PA0 |
| ADS7948 SPI1 MOSI | PA1 |
| ADS7948 SPI1 MISO | PA2 |
| MUX SEL0 | PB0 |
| MUX SEL1 | PB1 |
| MUX SEL2 | PB2 |
| MUX SEL3 | PB3 |
| ADS7948 CH SEL | PB18 |
| MUX/ADC PDEN | PB19 |
| 右半边 ADS7948 CS1 | PB15 |
| 右半边 ADS7948 CS2 | PB14 |

左半边 U2 的 CS1/CS2 与右半边相反，后续如果换左半边测试，需要编译:

```powershell
make DEFS_EXTRA="-DCH585_ADC_PROBE_MODE=1 -DCH585_ADC_PROBE_RIGHT_HALF=0"
```

## 编译 CH585 探针固件

在 CH585 测试工程下编译:

```powershell
cd F:\嵌赛\hardware\firmware\ch585_spi_slave_test
make clean
make DEFS_EXTRA="-DCH585_ADC_PROBE_MODE=1"
```

烧录文件:

```text
F:\嵌赛\hardware\firmware\ch585_spi_slave_test\build\ch585m_spi_slave_test.hex
```

## 不接 H417，直接看 CH585 串口

如果 H417 没接好，或者只想先确认 ADS7948/MUX/磁轴算法，可以编译 UART 版探针:

```powershell
cd F:\嵌赛\hardware\firmware\ch585_spi_slave_test
make clean
make DEFS_EXTRA="-DCH585_ADC_PROBE_MODE=1 -DCH585_ADC_PROBE_UART_MODE=1"
```

烧录文件仍然是:

```text
F:\嵌赛\hardware\firmware\ch585_spi_slave_test\build\ch585m_spi_slave_test.hex
```

UART 版默认使用键盘主板中间靠右的 CH585 串口下载/调试口。根据 `Docs-For-AI-Keyboard/latex/contest_report_template.tex` 的 CH585 下载调试接口表:

| CH585 | 作用 | 连接到 USB 转串口 |
| --- | --- | --- |
| PA9 | UART1 TXD / TX1 | RX |
| PA8 | UART1 RXD / RX1 | TX |
| GND | 共地 | GND |

当前默认:

```text
baud = 115200
8N1
period_ms = 10
CH585 SYSCLK = 78 MHz
ADS7948 SPI1 SCK = 39 MHz
ADS7948 frame rate = 39 MHz / 16 clocks = 2.4375 MSPS
```

预期串口输出:

```text
CH585 ADS7948 UART probe start
baud=115200 key=58 right=1 spi1_div=78 period_ms=100
line: AP seq key lane mux raw filt pos word down rt st
AP 0 58 3 10 512 2048 500 0x8000 0 1 0
```

字段说明:

- `raw`: ADS7948 10-bit code, 范围 `0..1023`。
- `filt`: CH585 磁轴算法滤波后的 ADC。
- `pos`: CH585 磁轴位置 `0..1000`。
- `word`: ADS7948 原始 16-bit SPI word。
- `st`: ADS7948 读取状态，`0` 表示成功。

如果必须临时回退到 UART0，可改编译端口，例如默认 UART0 引脚:

```powershell
make clean
make DEFS_EXTRA="-DCH585_ADC_PROBE_MODE=1 -DCH585_ADC_PROBE_UART_MODE=1 -DCH585_ADC_PROBE_UART_PORT=0"
```

如果 UART0 还需要 remap 到 `PA14/PA15`:

```powershell
make clean
make DEFS_EXTRA="-DCH585_ADC_PROBE_MODE=1 -DCH585_ADC_PROBE_UART_MODE=1 -DCH585_ADC_PROBE_UART_PORT=0 -DCH585_ADC_PROBE_UART0_REMAP=1"
```

注意: `PA14/PA15` 和当前 H417 SPI 调试线中的 `MOSI/MISO` 冲突。所以 UART0 remap 版只用于 CH585 单板 ADC 调试；要回到 H417 拉取时，重新编译普通探针版或默认 SPI 版。

## H417 日志如何看

H417 侧 USBFS CDC 日志中的 debug 行会多显示 `lane` 和 `mux`:

```text
src0 debug frames=... seq=... key=58 lane=3 mux=10 raw=... filt=... pos=... peak=... down=... rt=...
```

字段含义:

- `key/lane/mux`: 当前正在读的 CH585 本地通道。
- `raw`: ADS7948 解出的 10-bit ADC code，范围应为 `0..1023`。
- `filt`: CH585 当前磁轴算法中的滤波 ADC。当前 ADS7948 探针路径已经直接使用 10-bit raw 量纲，不再把 raw 左移到 12-bit-ish 量纲。
- `pos`: CH585 当前磁轴算法输出的位置，范围 `0..1000`。
- `peak`: ADS7948 原始 16-bit SPI word，用来排查 bit 对齐和 SPI 读数。
- `down/rt`: CH585 当前按键判断与 Rapid Trigger 状态。

正常现象:

- 不按和按下时，`raw` 应该有稳定且可重复的差值。
- `filt` 应该跟随 `raw` 缓慢变化。
- 如果 released/pressed 默认值不合适，`pos/down` 不准是正常的；先确认 `raw` 会动。

异常判断:

- `raw=65535`: ADS7948 读失败或探针路径返回错误。
- `raw` 一直 0 或 1023: 优先检查 MISO、CS、CH SEL、PDEN、ADC 供电/参考电压。
- `raw` 有噪声但按键不变: 优先确认 `key_id` 是否对，试 `58`、`59` 或扫全通道。
- `raw` 按键会动但 `down` 不动: 先做单键 released/pressed 标定，再调 press/release position。

## 扫全通道定位

如果不确定唯一焊好的键到底在哪个 MUX 通道，可以让 CH585 每个 debug 帧轮询一个通道:

```powershell
make clean
make DEFS_EXTRA="-DCH585_ADC_PROBE_MODE=1 -DCH585_ADC_PROBE_DEBUG_KEY=0xFF"
```

然后按住那颗键，看哪个 `key/lane/mux/raw` 出现明显变化。定位后再把 `CH585_ADC_PROBE_DEBUG_KEY` 固定到对应值。

## 右半区 41 键事件扫描

根据 `Docs-For-AI-Keyboard/latex/contest_report_template.tex`，当前右半区不是满 64 键，而是 41 个实际霍尔键:

```text
右 MUX1: D1..D10  -> 右半区霍尔  1..10
右 MUX2: D1..D10  -> 右半区霍尔 11..20
右 MUX3: D1..D10  -> 右半区霍尔 21..30
右 MUX4: D1..D11  -> 右半区霍尔 31..41
```

固件里需要区分两个编号:

```text
hall: 报告/键位表里的右半区霍尔序号，1..41。
slot: CH585 本地 MUX 槽位号，slot = lane * 16 + mux_index，范围 0..63。
```

例如当前已验证的右半区最下排左边第一个键是:

```text
hall = 41
key  = Space
右 MUX4 D11
slot = 3 * 16 + 10 = 58
lane = 3
mux_index = 10
```

编译右半区事件扫描 UART 版:

```powershell
cd F:\嵌赛\hardware\firmware\ch585_spi_slave_test
make clean
make DEFS_EXTRA="-DCH585_ADC_PROBE_MODE=1 -DCH585_ADC_PROBE_UART_MODE=1 -DCH585_ADC_PROBE_DEBUG_KEY=0xFF -DCH585_ADC_PROBE_UART_EVENT_MODE=1"
```

烧录文件:

```text
F:\嵌赛\hardware\firmware\ch585_spi_slave_test\build\ch585m_spi_slave_test.hex
```

串口仍用右半区 CH585 的 UART1:

```text
PA9 = TX
PA8 = RX
115200 8N1
```

启动后会显示:

```text
event_mode=on
right_half_valid_keys=41 slots: MUX1 D1-D10, MUX2 D1-D10, MUX3 D1-D10, MUX4 D1-D11
line: EV seq hall slot lane mux d key raw filt pos word down rt st
```

按下/松开任意有效键时，串口输出 `EV`:

```text
EV seq hall slot lane mux d key raw filt pos word down rt st
```

字段:

- `hall`: 右半区霍尔序号，1..41。
- `slot`: 当前 H417/CH585 短帧仍使用的 0..63 本地槽位。
- `lane`: 0..3，对应右 MUX1..4。
- `mux`: 1..4，给人看的 MUX 序号。
- `d`: 1..16，给人看的 MUX D 输入序号。
- `key`: 报告中的默认键名。
- `raw/filt/pos/down`: CH585 本地磁轴判断结果。
- `st`: ADS7948 读取状态，`0` 表示正常。

当前事件扫描版只打印状态变化，不连续打印全部 raw。每 200 轮会打印一次 `ST seq down=n err=n` 作为心跳。

## 当前单键实测标定

已确认右半边最下排左边第一个键对应:

```text
hall = 41
key = Space
右 MUX4 D11
key_id = 58
lane = 3
mux = 10
```

当前 UART 探针读到的 ADS7948 10-bit raw 约为:

```text
松开: raw = 499..500
按下: raw = 349..350
```

CH585 ADS7948 探针路径现在直接使用 ADS7948 的 10-bit raw 量纲。因此当前默认标定为:

```text
released_adc = 500
pressed_adc  = 350
press_pm     = 500
release_pm   = 350
filter_shift = 0
rt_enable    = 0
```

预期现象:

- 松开时，`pos` 应接近 `0`，`down=0`。
- 按下时，`pos` 应接近 `1000`，`down=1`。
- 这版 UART 边沿测试固件使用 `filter_shift=0`，基本没有滤波延迟，适合先验证按放边沿。

## 接入 CH585 SPI 短帧

UART 单键探针已验证通过后，可以编译 SPI 短帧版，让 H417 直接拉取这颗键的状态:

```powershell
cd F:\嵌赛\hardware\firmware\ch585_spi_slave_test
make clean
make DEFS_EXTRA="-DCH585_ADC_PROBE_MODE=1"
```

烧录文件仍然是:

```text
F:\嵌赛\hardware\firmware\ch585_spi_slave_test\build\ch585m_spi_slave_test.hex
```

这个版本的行为:

- CH585 每次构造 key-state 短帧前，实时采样 `key_id=58`。
- CH585 在本地完成 `raw -> position -> down` 判断。
- CH585 把结果写进 `down_bits[58]`。
- ADC 探针模式下不再把 `down_bits[0..7]` 当 SPI 诊断字段覆盖。
- ADS7948 时钟按高速测试档配置: `SYSCLK=78MHz`，`SPI1_DIV=2`，即 `SCK=39MHz`、单 16-clock frame 约 `2.4375MSPS`。这高于 ADS7948 标称 2MSPS，用于余量测试；如果不稳定，回退到 `SYSCLK=62.4MHz`。
- 固定同一颗键连续采样时，只读 1 个 16-clock frame；切换 MUX/ADS 通道时仍会先执行 settle + dummy frame。

H417 侧当前也按真实按键位图解析 source0 的 `down_bits`。在 H417 日志里重点看:

```text
CH585 scan poll=... raw[58]=...
src0 debug ... key=58 ... raw=... pos=... down=...
```

预期:

- 松开时 debug `down=0`，CH585 侧 raw 接近 `500`。
- 按下时 debug `down=1`，CH585 侧 raw 接近 `350`。

## 下一步

1. 烧录右半区 41 键事件扫描 UART 版。
2. 按右半区不同按键，确认 `hall/slot/mux/d/key` 与实际键位一致。
3. 如果某些键无事件，优先检查对应 MUX D 线、霍尔供电、焊接和默认阈值。
4. 事件扫描稳定后，编译 SPI 短帧版，让 H417 拉取真实 `down_bits`。
5. 最后再恢复适合实机的滤波/RT 参数，而不是继续使用 `filter_shift=0` 的边沿测试参数。
