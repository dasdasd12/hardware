# CH585M SPI 从机短帧测试固件

本工程用于当前 H417 + 单块 CH585M 的 SPI 链路和前端按键算法 bring-up。

当前定位：

```text
CH585 模拟 ADC
  -> CH585 本地滤波/position/触发/RT
  -> KEY_STATE/down_bits[8] + 低频 KEY_DEBUG 短帧
  -> H417 SPI2 主机拉取
  -> H417 USBFS CDC COM5 打印 KD/KS/SS/TR
```

现阶段因为仍然使用杜邦线，SPI 速率先以已验证稳定的约 16 MHz 为主，不继续强行拉到 40 MHz+。等 PCB 到后再重新做高速 SI/时序验证。

## 当前接线

| H417 | CH585M | 方向 | 作用 |
| --- | --- | --- | --- |
| PB12 | PA12 | H417 -> CH585 | GPIO CS0，低有效 |
| PB13 | PA13 | H417 -> CH585 | SPI2 SCK |
| PC1 | PA14 | H417 -> CH585 | SPI2 MOSI |
| PC2 | PA15 | CH585 -> H417 | SPI2 MISO |
| GND | GND | 双向参考 | 共地 |
| 3V3 | 3V3 | 电源/电平 | 同电平 |

第二块 CH585 暂时不接，H417 侧仍可用 fake source1 补齐调试。

## 当前协议

默认使用短帧：

```text
magic      0xD7
type       0x11
source_id  0
seq        8-bit frame seq
ack_seq    当前 request-only 模式下保留
flags      bit7 READY, bit4 CMD_ERROR
down_bits  8 bytes, 64 键状态，bit=1 表示按下
crc16      CRC-CCITT over bytes before crc16
```

每 `CH585_DEBUG_FRAME_INTERVAL` 个 `seq` 还会插入一个低频 debug 帧：

```text
magic        0xD7
type         0x12 = KEY_DEBUG
source_id    0
seq          8-bit frame seq
key_id       当前调试 key
flags        bit7 READY, bit0 down, bit1 rt_armed
raw_adc
filtered_adc
position_pm  0..1000
peak_pm
crc16        CRC-CCITT over bytes before crc16
```

H417 到 CH585 的低频命令原型同样使用 16B，和状态短帧等长：

```text
magic       0xA7
cmd         0x01 GET_STATE
            0x02 GET_DEBUG
            0x03 GET_CONFIG
            0x04 SET_CONFIG
            0x05 CALIBRATE_KEY
            0x06 CALIBRATE_ALL
host_seq    H417 command seq
ack_seq     H417 已收到的 CH585 seq
target_key  本地 key id, 0..63
param_id    配置项 id
value       配置值
flags       命令标志
aux         预留辅助参数
crc16       CRC-CCITT over bytes before crc16
```

当前 request-only 高速路径仍以 `KEY_STATE/KEY_DEBUG` 返回为准；命令结构和 `SET_CONFIG` 处理已经就绪，后续可以接入调试 shell、Vendor HID 或配置同步流程。

当前 `Makefile` 默认：

```makefile
CH585_USE_SHORT_FRAME=1
CH585_USE_REQUEST_ONLY_SHORT=1
CH585_FAST_SIM_FRAME=0
CH585_KEY_ENABLE_RAPID_TRIGGER=1
CH585_DEBUG_FRAME_INTERVAL=8
```

如果需要回退到最早的快速 pattern 测试，可临时编译：

```powershell
make DEFS_EXTRA="-DCH585_FAST_SIM_FRAME=1"
```

## 模拟按键算法

默认不再直接发送 pattern bits，而是每帧做：

```text
sim_adc_value(seq, key)
  -> IIR filter
  -> ADC released/pressed 归一化为 position 0..1000
  -> 普通 press/release 滞回
  -> 简化 Rapid Trigger release/re-press
  -> g_key_down[key]
  -> down_bits[8]
```

当前只有前 4 个 key 生成模拟运动，其余 key 保持松开，用于在 H417 `KS` 日志里清楚观察状态变化。

当前代码已经把本地算法参数集中到每键配置结构中，后续可以由 H417 通过低频配置命令下发：

```text
released_adc
pressed_adc
min_adc / max_adc
press_position
release_position
rt_press_delta
rt_release_delta
filter_shift
rt_enable
valid
global_key_id
```

正常高速帧仍然只发送 `down_bits[8]`，这些配置和 raw/position 只用于 CH585 本地判键、调试和后续校准。

## 编译产物

编译：

```powershell
make
```

烧录到 CH585M 的文件：

```text
F:\嵌赛\hardware\firmware\ch585_spi_slave_test\build\ch585m_spi_slave_test.hex
```

## 期望现象

1. H417 烧录当前 USBFS CDC + CH585 SPI 固件。
2. CH585 手动烧录本工程 hex。
3. 接好 H417/CH585 SPI 线并共地。
4. H417 USBFS 接 PC，打开 COM5 或实际枚举出的 CDC 串口。
5. 正常时：
   - `KD` 周期出现，能看到 `raw/filt/pos/down/rt`。
   - `SS` 里 `s0ok` 持续增加。
   - `s0crc=0`、`s0seq=0` 或不再增长。
   - `KS` 里前几个键会在 `1000`/`3000` 间变化。

如果 `s0fetch` 或 `s0crc` 持续增长，优先检查 CS/SCK/MOSI/MISO/GND 接线、CH585 是否烧录了本工程 hex、H417 是否仍在稳定 SPI 档位。
