# H417 + CH585 当前 SPI 调试状态

本文记录当前已经验证过的 H417 与单块 CH585M 调试状态，方便后续和队友对齐。

## 2026-06-21 最新状态

当前实际调试链路已经切到：

```text
CH585M 测试固件
  -> 本地模拟 ADC
  -> CH585 本地滤波/位置计算/普通触发/RT 判断
  -> SPI0 从机输出 16B 短帧
  -> H417 硬件 SPI2 + GPIO CS 主机读取
  -> H417 只合并 2 x 64 键状态，调试阶段通过 USBFS CDC 输出
```

H417 侧现在使用硬件 SPI2，不再使用最早的 PD2/PD3/PD4 软件 SPI 调试线。当前第一块 CH585 接线：

| H417 | CH585M | 方向 | 作用 |
| --- | --- | --- | --- |
| PB12 | PA12 | H417 -> CH585 | SPI2 GPIO CS0，低有效 |
| PB13 | PA13 | H417 -> CH585 | SPI2 SCK |
| PC1 | PA14 | H417 -> CH585 | SPI2 MOSI |
| PC2 | PA15 | CH585 -> H417 | SPI2 MISO |
| GND | GND | 双向参考 | 必须共地 |
| 3V3 | 3V3 | 供电/电平 | 3.3V |

当前仍只接第一块 CH585，第二块 CH585 后续接同一组 SCK/MOSI，另加独立 CS/MISO。

当前主状态帧为 16B `KEY_STATE` 短帧：

```text
offset  size  field
0       1     magic       0xD7
1       1     type        0x11 = KEY_STATE
2       1     source_id   0
3       1     seq
4       1     ack_seq     request-only 模式下暂不使用
5       1     flags       bit7=READY
6       8     down_bits   64 键 0/1 状态
14      2     crc16       CRC-CCITT over bytes 0..13
```

新增低频调试帧也是 16B，不改变 H417 每次 SPI 读取长度：

```text
offset  size  field
0       1     magic       0xD7
1       1     type        0x12 = KEY_DEBUG
2       1     source_id   0
3       1     seq
4       1     key_id
5       1     flags       bit7=READY, bit0=down, bit1=rt_armed
6       2     raw_adc
8       2     filtered_adc
10      2     position_pm 0..1000
12      2     peak_pm
14      2     crc16       CRC-CCITT over bytes 0..13
```

CH585 测试固件当前每 8 个 `seq` 插入 1 个 `KEY_DEBUG`，其余帧仍然发送 `KEY_STATE/down_bits[8]`。H417 收到 `KEY_DEBUG` 后不会刷新 64 键状态，只更新调试快照并通过 USB CDC 输出：

```text
KD n=3 seq=24 k=3 raw=3001 filt=2410 pos=705 peak=1000 down=1 rt=0
```

含义：

- `KD`：CH585 内部算法调试帧。
- `n`：H417 已收到的 debug 帧数。
- `seq`：CH585 帧序号。
- `k`：当前上报的 key id。
- `raw/filt/pos/peak`：CH585 本地算法状态。
- `down/rt`：CH585 已经判断出的按键状态和 RT armed 状态。

当前主要观测端口：

```text
COM5 = H417 USBFS CDC，看 KS / SS / TR / KD
COM4 = WCH-Link SERIAL，看 rtthread heartbeat 和详细 dump
```

CH585 测试工程已经整理进仓库：

```text
F:\嵌赛\hardware\firmware\ch585_spi_slave_test
```

默认编译产物：

```text
F:\嵌赛\hardware\firmware\ch585_spi_slave_test\build\ch585m_spi_slave_test.hex
```

## 当前结论

当前链路已经跑通：

```text
CH585M 测试固件
  -> PA15/MISO 输出 64 路模拟 ADC 帧
  -> H417 PD2/PD3/PD4/PD6 软件 SPI 主机读取
  -> H417 校验 magic/version/source/length/CRC
  -> H417 通过 USB2.0 FS CDC COM5 输出 raw 数据
```

H417 不是一直盲读 CH585，而是在主循环里按一次事务读取一帧：

```text
H417 处理当前数据
H417 拉低 CS0
H417 clock sizeof(ch585_scan_frame_v1_t) 字节
CH585 返回已经准备好的 64 键帧
H417 拉高 CS0
H417 校验并合并 raw 数据
H417 通过 USB CDC 输出调试数据
下一轮再请求下一帧
```

这符合“主机处理完，发出去之后，再请求下一帧数据”的设计方向。

## 当前接线

只接第一块 CH585M：

| H417 | CH585M | 方向 | 作用 |
| --- | --- | --- | --- |
| PD2 | PA13 | H417 -> CH585 | Soft SPI SCK |
| PD3 | PA14 | H417 -> CH585 | Soft SPI MOSI |
| PD4 | PA15 | CH585 -> H417 | Soft SPI MISO0 |
| PD6 | PA12 | H417 -> CH585 | CS0，低有效 |
| GND | GND | 双向参考 | 必须共地 |
| 3V3 | 3V3 | 供电/电平 | 3.3V |

第二块 CH585M 后续再接：

| H417 | CH585M_1 | 方向 | 作用 |
| --- | --- | --- | --- |
| PD2 | PA13 | H417 -> CH585 | 共用 SCK |
| PD3 | PA14 | H417 -> CH585 | 共用 MOSI |
| PD5 | PA15 | CH585 -> H417 | Soft SPI MISO1 |
| PD7 | PA12 | H417 -> CH585 | CS1，低有效 |

## 当前帧格式

H417 和 CH585 共用 V1 测试帧：

```c
typedef struct __attribute__((packed))
{
    uint16_t magic;
    uint8_t  version;
    uint8_t  source_id;
    uint16_t seq;
    uint16_t flags;
    uint16_t base_key;
    uint16_t key_count;
    uint16_t adc[64];
    uint16_t crc16;
} ch585_scan_frame_v1_t;
```

当前 `magic = 0x4BD3`，线上字节序是：

```text
d3 4b
```

之前用 `53 4b` 时，CH585 的 MISO 在 CS 拉低后的第一个采样位容易保持空闲高电平，导致 H417 把首字节读成 `d3`。把帧头第一字节设计成 MSB=1 的 `d3` 后，H417 不再需要首位修复，当前日志里 `repair=0`。

## 模拟 ADC 与 H417 解算

当前已经加入一条模拟解算链：

```text
CH585 生成模拟 ADC 波形
H417 读取 raw[128]
H417 keyboard_engine 做滤波/归一化/按下释放判断
H417 通过 USB CDC 输出 KE/EV 调试行
```

CH585 测试固件中，key0 ~ key3 会周期性模拟下压和松开：

```text
released_adc = 1000
pressed_adc  = 3000
period       = 32 frames
noise        = about +/-3
```

其它键保持在 released_adc 附近，只带小噪声。

H417 解算模块位置：

```text
F:\嵌赛\hardware\rtthread_port\applications\keyboard_engine.c
F:\嵌赛\hardware\rtthread_port\applications\keyboard_engine.h
```

当前使用定点数表示键程：

```text
position_pm = 0..1000
0    = 完全松开
1000 = 完全按下
```

滤波公式：

```text
filtered = filtered + (raw - filtered) / 4
```

归一化公式：

```text
position_pm = (filtered_adc - released_adc) * 1000
              / (pressed_adc - released_adc)
```

## 2026-06-22 SPI speed checkpoint

This section records the current reproducible H417 + one-CH585 board result.

Hardware wiring under test:

```text
H417 PB12 -> CH585 PA12 / CS0
H417 PB13 -> CH585 PA13 / SCK
H417 PC1  -> CH585 PA14 / MOSI
H417 PC2  <- CH585 PA15 / MISO
GND       <-> GND
3V3       <-> 3V3
```

Current stable baseline left on the board:

```text
APP_CH585_SPI_HW_SPI2_PRESCALER = SPI_BaudRatePrescaler_Mode4
APP_CH585_SPI_HW_SPI2_HIGHSPEED = 0
APP_CH585_SPI_CMD_TO_DATA_US    = 1000
APP_CH585_SPI_CS_SETUP_MS       = 1
Observed USBFS log: SP sck=3121.9 kHz p=0020 h=0 c=1
```

Observed comparison:

```text
Mode4, h=0: about 3.12 MHz, stable in short COM5 captures; rxbad did not grow during the final 30 s check.
Mode3, h=0: about 6.10 MHz, rxbad slowly increased.
Mode4, h=1: about 16.00 MHz, main link still worked, but rxbad slowly increased.
Older Mode4, h=2: about 16.00 MHz, worse than h=1 in previous captures.
CPHA=2: bad, reverted to CPHA=1.
```

Current interpretation:

- The protocol path is alive: H417 command queue, CH585 response, USBFS CDC logs, KEY_DEBUG, and calibration test all run.
- The clean speed on the current Dupont-wire setup is only around 3 MHz.
- Since CS setup is already 1 ms and command-to-data delay is 1000 us, the observed errors are not likely to be simple CS setup-time violations.
- The likely causes are signal integrity, SPI edge timing, CH585 SPI0 sampling margin, or transaction state recovery after a corrupted command.
- Do not use the current Dupont-wire result to conclude the final PCB cannot reach a higher SPI clock.

Next useful SPI work:

```text
1. Keep 3.12 MHz as the stable debug baseline.
2. Add clearer command-transaction diagnostics: distinguish corrupted command bytes from phase-recovery/dummy-read artifacts.
3. On PCB, retest normal divisors and high-speed modes with short traces and solid ground return.
4. After PCB is available, target >=40 MHz again; CH585/H417 datasheet limits alone do not rule it out.
```

## 2026-06-22 USBHS HID checkpoint

USBHS application-layer bring-up now has a first HID prototype in:

```text
rtthread_port/applications/usb_hs_hid_keyboard.c
rtthread_port/applications/usb_hs_hid_keyboard.h
```

Build switches:

```text
APP_ENABLE_USB2_FS_CDC = 1   ; keep USBFS CDC as debug console
APP_ENABLE_USB2_HS_CDC = 0   ; do not use USBHS as CDC in this profile
APP_ENABLE_USB2_HS_HID = 1   ; use USBHS as HID keyboard + vendor HID
```

The USBHS device currently enumerates as:

```text
VID_1A86 PID_FE32
USB Composite Device
MI_00: HID Keyboard Device
MI_01: HID-compliant vendor-defined device
```

Windows check result from the development PC:

```text
USB\VID_1A86&PID_FE32\2026062201                         OK
USB\VID_1A86&PID_FE32&MI_00\...                           OK
USB\VID_1A86&PID_FE32&MI_01\...                           OK
HID\VID_1A86&PID_FE32&MI_00\...  HID Keyboard Device       OK
HID\VID_1A86&PID_FE32&MI_01\...  vendor-defined HID        OK
```

Current scope:

- Keyboard report is an 8-byte boot keyboard report.
- Vendor HID report is a 64-byte in/out placeholder for future config and debug.
- The report producer currently maps raw ADC values above `APP_USBHS_HID_DOWN_ADC` to a small HID usage table for bring-up only.
- This is not yet the final 8K report scheduler and not yet the final keymap/config protocol.

Current caveat:

- When USBFS CDC is not present, COM5 logs are unavailable even though USBHS HID enumerates correctly.
- For log-assisted debugging, keep USBFS connected or use WCH-Link serial output.

当前按键判断：

```text
position_pm >= 450 -> 按下
position_pm <= 350 -> 松开
```

也就是用了一个简单迟滞窗口，避免临界点抖动导致反复触发。

COM5 新增两类调试行：

```text
KE f=12 k=0 raw=2428 filt=1880 pos=440 down=0
EV f=13 k=0 down=1 pos=504 raw=2714 filt=2008
```

含义：

```text
KE = 当前 key 状态快照
EV = 按下/释放事件
f  = H417 解算帧号
k  = key index
pos = 0..1000 的键程
down = 0/1
```

当前实测已经能看到完整解算输出，例如：

```text
EV f=3 k=2 down=1 pos=578 raw=2997 filt=2156
KE f=20 k=0 raw=3004 filt=2927 pos=963 down=1
EV f=21 k=1 down=0 pos=275 raw=1002 filt=1550
EV f=29 k=0 down=0 pos=275 raw=997 filt=1551
```

同时 COM4 显示 SPI 链路干净：

```text
src0 ok=25 fetch=0 magic=0 ver=0 src=0 len=0 crc=0 seq_drop=0 repair=0
```

这说明当前模拟链路已经跑通：

```text
CH585 模拟 ADC -> SPI 帧 -> H417 CRC/合并 -> H417 解算 -> USB CDC 输出 KE/EV
```

## 当前实测现象

PC 侧串口：

```text
COM4 = WCH-Link SERIAL，RT-Thread 控制台和详细统计
COM5 = H417 USBFS CDC，上位机可见 raw 数据
```

COM5 能看到类似：

```text
KS f=0 i=000 1022 1023 1024 1025 1026 1027 1028 1029
KS f=0 i=008 1041 1042 1043 1044 1045 1046 1047 1048
```

COM4 当前关键统计：

```text
CH585 scan poll=57 raw[0]=1076 raw[63]=1139 raw[64]=1056 raw[127]=1119
  src0 ok=56 fetch=0 magic=0 ver=0 src=0 len=0 crc=1 seq_drop=1 last_seq=76
  src1 ok=57 ...
  src0 req_rx=d3 4b 01 00 head=d3 4b 01 00 ... repair=0
```

解释：

- `src0 ok=56 / poll=57`：单块 CH585 已经稳定读到帧。
- `magic=0`：帧头没有再错。
- `repair=0`：新帧头已经解决首位污染问题。
- `crc=1` 和 `seq_drop=1`：刚启动或刚插 USB 后曾经丢过一帧，后续长时间读取没有继续增长。
- `src1` 当前还是 H417 内部假数据，因为第二块 CH585M 还没接。

## 固件路径

H417 V5F 当前固件：

```text
F:\嵌赛\hardware\rtthread_port\build\v5f\rtthread_ch32h417_v5f.hex
```

CH585M 当前测试固件：

```text
F:\嵌赛\hardware\firmware\ch585_spi_slave_test\build\ch585m_spi_slave_test.hex
```

注意：CH585M 需要手动用下载器烧录；H417 可以通过当前脚本和 WCH-Link 烧录。

## 当前代码位置

H417：

```text
F:\嵌赛\hardware\rtthread_port\applications\main.c
F:\嵌赛\hardware\rtthread_port\applications\ch585_spi_scan.c
F:\嵌赛\hardware\rtthread_port\applications\ch585_spi_scan.h
```

CH585M：

```text
F:\嵌赛\hardware\firmware\ch585_spi_slave_test\src\main.c
```

## 下一步建议

1. 先继续观察单块 CH585 是否长时间保持 `crc` 不继续增长。
2. 接第二块 CH585M：共用 SCK/MOSI，单独接 PD5/MISO1 和 PD7/CS1。
3. H417 把 `source1` 从假数据改成真实 soft SPI source1。
4. CH585M 把当前模拟 ADC 值替换成真实 MUX/ADC 扫描值。
5. H417 把 raw[128] 交给磁轴算法层，做校准、滤波、触发点、RT 等逻辑。
6. USB 从当前 CDC 调试输出切到正式 8K HID 上报。
