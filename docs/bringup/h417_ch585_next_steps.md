# H417 + CH585 后续任务清单

Updated: 2026-06-21

本文记录当前 H417 + CH585 键盘前端的代码进展、下一步任务和验收标准。整体方向以 `contest_report_template.pdf` 的硬件架构为准。

## 目标架构

```text
每半区磁轴
  -> 4 个 16:1 MUX
  -> 2 个 ADS7948, 共 4 条 ADC lane
  -> 1 个 CH585 做本半区采样和按键判断
  -> SPI 短帧给 CH32H417
  -> H417 合并两颗 CH585 状态
  -> USB HID / Vendor HID / 调试 CDC 上报给 PC
```

分工原则：

- CH585 负责 ADS7948/MUX 扫描、settle、丢样、过采样、滤波、标定、position、普通触发、Rapid Trigger、输出 key state/event/debug。
- H417 负责轮询两个 CH585、校验和合并状态、全局功能、USB 上报、屏幕/灯效/存储、配置下发。
- 正常高频帧不传完整 raw ADC；raw/position 只作为低频调试或校准数据。

## 当前已验证状态

当前板上已经验证：

```text
CH585 模拟 ADC / 本地按键算法
  -> CH585 KEY_STATE + 低频 KEY_DEBUG
  -> H417 hardware SPI2 + GPIO CS 拉取
  -> H417 USBFS CDC 日志
```

当前 H417 调试输出默认配置：

```text
APP_ENABLE_USB2_HS_CDC=0
APP_ENABLE_USB2_FS_CDC=1
```

当前开发板临时接线：

```text
H417 PB12 -> CH585 PA12 / CS0
H417 PB13 -> CH585 PA13 / SCK
H417 PC1  -> CH585 PA14 / MOSI
H417 PC2  <- CH585 PA15 / MISO
GND       <-> GND
3V3       <-> 3V3
```

最终 PCB 报告引脚：

```text
H417 PB3 -> CH585 SCK0
H417 PB5 -> CH585 MOSI0
H417 PB4 <- CH585 MISO0
H417 PF2 -> CH585 #1 CS
H417 PD9 -> CH585 #2 CS
```

这两套引脚需要在代码中做成可切换 profile，不能混在一起。

## 当前帧格式

正常状态短帧 `KEY_STATE`，16B：

```text
offset  size  field
0       1     magic       0xD7
1       1     type        0x11
2       1     source_id
3       1     seq
4       1     ack_seq
5       1     flags       bit7=READY
6       8     down_bits   64 key bitmap
14      2     crc16
```

低频调试短帧 `KEY_DEBUG`，16B：

```text
offset  size  field
0       1     magic       0xD7
1       1     type        0x12
2       1     source_id
3       1     seq
4       1     key_id
5       1     flags       bit7=READY, bit0=down, bit1=rt_armed
6       2     raw_adc
8       2     filtered_adc
10      2     position_pm
12      2     peak_pm
14      2     crc16
```

当前通信模型是 request-only：

```text
H417 处理完上一帧
H417 拉低 CS
H417 clock 16 bytes
CH585 返回已经准备好的 KEY_STATE 或 KEY_DEBUG
H417 拉高 CS
H417 校验、合并、打印或上报
```

## 下一步优先级

### P0: 固化当前可复现调试点

- 保持 USBFS CDC 作为主调试口，不急着切 USBHS。
- 保持单 CH585、16B 短帧、request-only 模式。
- H417 继续打印 `SS / KS / KD / TR`。
- 确认仓库默认代码能让同学复现当前现象。

验收标准：

```text
s0ok 持续增加
s0fetch = 0
s0crc = 0
s0seq 不持续增加
KD 周期出现
KS 能看到模拟 key 在 1000/3000 之间变化
```

### P1: 定义 CH585 本地算法配置结构

把 CH585 里的按键算法参数集中成结构体，后续方便 H417 下发配置：

```text
released_adc
pressed_adc
min_adc
max_adc
press_position
release_position
rt_press_delta
rt_release_delta
filter_shift
rt_enable
valid_key_mask
local_to_global_key_id
```

目标：

- 当前模拟 ADC 先使用这套结构。
- 后续 ADS7948 实采可以直接复用。
- 每颗 CH585 支持本地 64 个槽位，但只启用真实焊接的键。

状态：

- 初版已接入 `firmware/ch585_spi_slave_test/src/main.c`。
- 当前仍使用默认配置，尚未接入 H417 下发命令。

### P2: 扩展 H417 -> CH585 命令

在不破坏当前 `KEY_STATE` 短帧的前提下，增加低频命令原型：

```text
GET_STATE
GET_DEBUG
GET_CONFIG
SET_CONFIG
CALIBRATE_KEY
CALIBRATE_ALL
```

原则：

- 高频按键状态仍走 16B `KEY_STATE`。
- raw ADC、position、校准数据只走低频 debug/config。
- 命令和响应都保留 CRC、seq、source_id。

状态：

- 初版 16B 命令结构已加入 H417 和 CH585 测试固件。
- H417 侧已提供 source0 命令排队 API。
- CH585 侧已能识别 `GET_STATE / GET_DEBUG / GET_CONFIG / SET_CONFIG / CALIBRATE_KEY / CALIBRATE_ALL`。
- `SET_CONFIG` 已能修改 CH585 本地每键配置结构。
- 默认高速状态链路仍保持 `KEY_STATE / KEY_DEBUG` 返回帧不变。

### P3: 接入 ADS7948 单通道

先只让 CH585 读一个 ADS7948 固定通道，不接完整 MUX 扫描。

步骤：

```text
实现 ADS7948 CS / CH_SEL / SPI16 / delay 回调
固定读取一个通道
丢弃切换后的第一帧
通过 KEY_DEBUG 输出 raw_adc / filtered_adc / position
确认手按或固定电压时 raw code 稳定变化
```

验收标准：

```text
raw_adc 不再是模拟值
静止时抖动范围可接受
按压时 position 方向正确
H417 KD 能稳定看到变化
```

### P4: 接入 MUX 扫描

在 ADS7948 单通道稳定后，再逐步接入 MUX：

```text
单 ADS7948 单通道 + 单 MUX
单 ADS7948 双通道
2 个 ADS7948 / 4 lane
一颗 CH585 的 64 个本地槽位
```

需要补齐：

- MUX 地址线控制。
- `mux_settle_us`。
- 切换后丢样。
- 过采样次数。
- lane/key map。
- valid mask，跳过未焊接键位。

### P5: 双 CH585 轮询和键位合并

在第一颗 CH585 真实采样稳定后，再接第二颗。

H417 需要：

- 支持 source0/source1 都是真实 CH585。
- 两颗 CH585 使用同一组 SCK/MOSI。
- 两颗 CH585 使用独立 CS。
- MISO 是否共线要实测；如果未选中 CH585 不能可靠三态，则使用独立 MISO 或外部三态。
- 按报告映射：右半区 key 1..41，左半区 key 42..77。

### P6: USB 上报路径

调试阶段继续用 USBFS CDC。

之后按顺序做：

```text
USBFS CDC 调试保持
USB HID boot keyboard
Vendor HID 配置通道
USBHS CDC/HID 重新 bring-up
USB3/SS 路径后置
```

USBHS 之前出现过未知设备，先不要让它阻塞 ADC/SPI 主链路。

### P7: SPI 速率

开发板杜邦线阶段以稳定为主，不再硬拉 40 MHz / 70 MHz。

当前策略：

```text
16 MHz 作为稳定参考点
PCB 到板后再按最终引脚重测
用 CRC/seq/fetch 计数器判断，不靠肉眼看日志
必要时用示波器或逻辑分析仪看 SCK/MISO 建立保持时间
```

PCB 阶段再测试：

```text
16 MHz
20 MHz
30 MHz
40 MHz
50 MHz+
```

## 暂缓项

这些现在不要作为主线任务：

- USBHS 正式上报。
- USB3/SS。
- RGB 灯效。
- 屏幕 UI。
- SDRAM/Flash 文件系统。
- BLE / 2.4G 正式无线功能。
- 宏、SOCD、DKS 完整产品功能。

它们等“真实采样 -> CH585 判键 -> H417 合并 -> USBFS/HID 上报”主链路稳定后再接。

## Git 协作约定

后续提交按同学要求走 fork + PR 流程：

```text
1. 只在自己的功能分支上改代码和文档
2. 本地确认 git status / diff / build 或必要检查
3. commit 到自己的分支
4. push 到自己的 fork
5. 确认 GitHub 上 fork 分支已经看到最新 commit
6. 再从自己的 fork 分支向同学仓库 main 发 pull request
```

注意事项：

- 不直接 push 到同学仓库主线。
- 不在还没 push 完整之前提前发 PR。
- PR 发出后，如果还要补东西，继续 push 到同一个分支，PR 会自动更新。
- 每次 PR 前尽量整理 md 和调试产物，避免把临时 build/log 文件带进去。

## 最短行动路线

```text
1. 保持当前单 CH585 模拟链路可复现
2. 整理 CH585 算法配置结构
3. 扩展低频 config/debug 命令
4. 接 ADS7948 单通道
5. 接单 MUX
6. 接 4 lane / 64 本地槽位
7. 接第二颗 CH585
8. 做 USB HID / Vendor HID
9. PCB 到板后重测 SPI 高频
```
