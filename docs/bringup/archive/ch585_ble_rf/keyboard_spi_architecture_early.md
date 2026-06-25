# 磁轴键盘 CH585M 到 H417 SPI 架构建议

本文记录当前磁轴键盘方案中，`CH585M -> H417 -> USB 8K -> 上位机` 这一段应该如何分工、如何传数据、H417 如何解算，以及需要优先验证的风险点。

## 1. 当前硬件想法

当前设想：

```text
128 个磁轴
  -> 8 个 16:1 MUX
  -> 4 路 ADC 采样
  -> 分给 2 个 CH585M
  -> 2 个 CH585M 通过 SPI 把采样数据交给 H417
  -> H417 统一解算按键状态
  -> H417 通过 8K USB 上报给上位机
```

每个 CH585M 可以理解为管理半边键盘：

```text
CH585M_0:
  MUX0 ~ MUX3
  64 个磁轴原始 ADC 数据
  base_key = 0

CH585M_1:
  MUX4 ~ MUX7
  64 个磁轴原始 ADC 数据
  base_key = 64
```

如果实际 ADC/MUX 分配不同，也建议最后统一抽象成：

```text
每个 CH585M 每帧输出 64 个按键的 raw ADC。
H417 按 base_key 把两个 64 键帧合并成 128 键 raw 数组。
```

## 2. 推荐总体分工

核心建议：

```text
CH585M 只做采样前端。
H417 才是键盘主控和算法中心。
```

### CH585M 负责

- 控制本侧 MUX。
- 采集本侧 ADC。
- 做非常轻量的采样稳定处理，例如丢弃切换后的第一拍、简单平均、限幅。
- 维护双缓冲 raw ADC 数据。
- 在 H417 读取时，通过 SPI slave 返回完整扫描帧。
- 提供帧序号、状态 flags、CRC 等用于 H417 判断数据是否可靠。

### CH585M 不建议负责

- 不建议判断按下/释放。
- 不建议做 Rapid Trigger 最终状态机。
- 不建议做 SOCD、SpeedTap、DKS、宏、层切换等全局行为。
- 不建议只发“变化事件”。

原因是这些算法很多是跨键、跨左右手、跨配置层的。放在两个 CH585M 上会让同步和配置管理变复杂。

### H417 负责

- 作为 SPI master 主动轮询两个 CH585M。
- 用 SPI DMA 读取完整 raw frame。
- 校验帧头、版本、长度、序号、CRC。
- 把两个 64 键帧合并成 128 键 raw ADC 数组。
- 做校准、归一化、滤波、Rapid Trigger、静态触发、DKS、SOCD、SpeedTap 等算法。
- 生成 USB HID NKRO 报告。
- 通过 USB 8K 上报 PC。
- 低频把 raw ADC 或诊断数据传给上位机，用于调试和校准 UI。

## 3. 推荐 SPI 主从关系

推荐：

```text
H417 = SPI master
CH585M_0 = SPI slave, CS0
CH585M_1 = SPI slave, CS1
```

如果 H417 SPI 外设数量足够：

```text
H417 SPI_A -> CH585M_0
H417 SPI_B -> CH585M_1
```

这样两个 CH585M 可以并行读，时序余量最大。

如果只能用一路 SPI：

```text
H417 SPI -> CH585M_0 / CH585M_1 共总线
CS0 / CS1 分时片选
```

这样也可以，但 SPI 频率和 DMA 调度要留足余量。

## 4. 推荐 8K 时序

8K USB 上报意味着每 125us 一轮。

推荐节拍：

```text
H417 8K 定时器 tick
  1. 拉起 SYNC 或发送 READ_FRAME 命令
  2. 读取 CH585M_0 front buffer
  3. 读取 CH585M_1 front buffer
  4. 校验并合并 raw[128]
  5. 运行 H417 键盘算法
  6. 生成 HID report
  7. USB IN 上报
```

CH585M 侧：

```text
CH585M 周期性扫描本侧 64 键
  1. 写入 back buffer
  2. 一轮扫描完成后，front/back buffer 交换
  3. front buffer 只给 SPI 读取
  4. back buffer 只给 ADC 扫描写入
```

必须使用双缓冲，否则 H417 读取时 CH585M 正在写，会读到半新半旧的数据。

## 5. SYNC 和 DRDY 选择

推荐优先采用 H417 主同步：

```text
H417 每 125us 给两个 CH585M 一个同步节拍。
两个 CH585M 根据同步节拍准备下一帧。
H417 在固定时间点读走上一帧。
```

优点：

- 两个 CH585M 的时间基准统一。
- 和 USB 8K 上报天然对齐。
- H417 容易判断丢帧、延迟和过期数据。

也可以增加 DRDY 引脚作为辅助：

```text
CH585M 扫描完成后拉高 DRDY。
H417 如果在读帧前发现 DRDY 未就绪，则标记 stale 或 overrun。
```

推荐最终形态：

```text
SYNC: H417 -> CH585M，用于统一 8K 节拍
DRDY0: CH585M_0 -> H417，表示新帧已准备好
DRDY1: CH585M_1 -> H417，表示新帧已准备好
SPI: H417 master 读取完整帧
```

## 6. SPI 数据帧格式建议

不要一键一键发，也不要只发变化事件。建议每帧固定发送 64 个 `uint16_t` raw ADC。

```c
typedef struct __attribute__((packed)) {
    uint16_t magic;        // 固定 0x4B53, "KS"
    uint8_t  version;      // 协议版本，当前 1
    uint8_t  source_id;    // 0 或 1，对应两个 CH585M

    uint16_t seq;          // 扫描帧序号，每完成一帧 +1
    uint16_t flags;        // bit0=overrun, bit1=adc_error, bit2=stale, bit3=sync_lost

    uint32_t tick;         // CH585M 本地扫描 tick，或跟随 H417 sync 计数
    uint16_t base_key;     // CH585M_0 为 0，CH585M_1 为 64
    uint16_t key_count;    // 固定 64

    uint16_t payload_len;  // adc 数组字节数，固定 128
    uint16_t adc[64];      // 64 个按键的 raw ADC，小端序

    uint16_t crc16;        // 从 magic 到 adc 末尾的 CRC16
} SpiKeyScanFrame;
```

固定顺序建议：

```text
adc[mux_id * 16 + mux_channel]
```

例如 CH585M_0：

```text
adc[0]  = MUX0 CH0
adc[1]  = MUX0 CH1
...
adc[15] = MUX0 CH15
adc[16] = MUX1 CH0
...
adc[63] = MUX3 CH15
```

H417 合并时只需要：

```c
for (int i = 0; i < frame.key_count; i++) {
    raw[frame.base_key + i] = frame.adc[i];
}
```

## 7. SPI 带宽估算

每个 CH585M 每帧大约：

```text
header 约 20 字节
adc[64] = 128 字节
crc = 2 字节
总计约 150 字节
```

8K 下单个 CH585M：

```text
150 bytes * 8000 = 1.2 MB/s
约 9.6 Mbps
```

两个 CH585M：

```text
约 19.2 Mbps
```

如果单 SPI 总线分时读两个 CH585M：

```text
20 MHz SPI:
  读两个完整帧大约 120us，仅剩很小余量，不推荐。

40 MHz SPI:
  读两个完整帧大约 60us，比较合理。

更高 SPI 或双 SPI:
  余量更好。
```

所以建议：

- SPI 频率优先按 40MHz 级别设计和验证。
- 必须使用 SPI DMA。
- 不要在 125us 周期里靠 CPU 字节搬运。
- 如果调试初期 SPI 不稳定，可以先降到 1K/2K 验证协议，再拉到 8K。

## 8. ADC/MUX 采样风险

每个 CH585M 如果管理 64 键，并且每侧有 2 路 ADC 并行，则每 125us 要完成：

```text
64 个 key sample
2 路 ADC 并行
=> 32 组 MUX 切换 + ADC 转换
=> 125us / 32 = 3.9us 每组
```

这 3.9us 里包含：

- 设置 MUX 地址。
- 等待 MUX 输出稳定。
- ADC 采样保持。
- ADC 转换。
- 数据搬运。

这里是最大风险点之一。

需要实测：

- MUX 切换后的稳定时间。
- Hall 传感器输出阻抗是否能快速驱动 ADC 采样电容。
- MUX 导通电阻和 ADC 输入电容造成的 RC 延迟。
- 是否需要运放 buffer。
- 是否需要丢弃 MUX 切换后的第一拍 ADC。
- 8K 下噪声是否可接受。

如果 8K 全量 raw 采样压力过大，可以考虑：

- 先做 4K raw scan，USB 8K 重复上一帧。
- 每键多采样改为每键单采样。
- 降低 CH585M 发给 H417 的诊断数据量。
- 把高频 HID 和低频 raw monitor 分开。

## 9. H417 解算流程建议

H417 侧建议拆成这些层：

```text
SPI Receiver
  -> Frame Validator
  -> Raw Merger
  -> Calibration / Normalization
  -> Filter
  -> Key State Machine
  -> Behavior Resolver
  -> HID Report Builder
  -> USB 8K Sender
```

### 9.1 收帧校验

H417 对每个 CH585M 分别维护状态：

```c
typedef struct {
    uint16_t last_seq;
    uint32_t last_tick;
    uint8_t  online;
    uint8_t  stale;
    uint32_t crc_error_count;
    uint32_t seq_drop_count;
    uint32_t overrun_count;
} ScanSourceState;
```

收到帧后检查：

- `magic == 0x4B53`
- `version` 是否支持
- `source_id` 是否匹配 CS
- `key_count == 64`
- `payload_len == 128`
- `crc16` 是否正确
- `seq` 是否连续

CRC 错误或帧不合法时：

- 不更新对应 64 键 raw。
- 标记该 source stale。
- HID 状态可以保持上一帧，或者按安全策略释放该半边按键。

### 9.2 归一化

每个键需要校准参数：

```c
typedef struct {
    uint16_t raw_released;
    uint16_t raw_pressed;
    uint16_t deadzone_bottom;
    uint16_t deadzone_top;
    uint8_t  invert;
} KeyCalibration;
```

归一化成 0.0 到 1.0：

```c
float normalize_key(uint16_t raw, const KeyCalibration *cal)
{
    int32_t lo = cal->raw_released;
    int32_t hi = cal->raw_pressed;

    if (cal->invert) {
        int32_t tmp = lo;
        lo = hi;
        hi = tmp;
    }

    float pos = (float)((int32_t)raw - lo) / (float)(hi - lo);

    if (pos < 0.0f) pos = 0.0f;
    if (pos > 1.0f) pos = 1.0f;
    return pos;
}
```

后续可以把浮点改成定点数，例如 Q15/Q16，以减少实时开销。

### 9.3 滤波

初期可以用简单 IIR：

```c
filtered = filtered * 0.75f + pos * 0.25f;
```

也可以对 raw ADC 做限幅：

```c
if (raw > last_raw + max_step) raw = last_raw + max_step;
if (raw + max_step < last_raw) raw = last_raw - max_step;
```

注意：

- 滤波越强，延迟越大。
- RT 键盘不能过度滤波。
- 可对不同模式使用不同滤波强度。

### 9.4 静态触发

```c
if (!pressed && pos >= actuation_point) {
    pressed = true;
}

if (pressed && pos <= release_point) {
    pressed = false;
}
```

要求：

```text
release_point < actuation_point
```

### 9.5 Rapid Trigger

每个键维护：

```c
typedef struct {
    uint8_t pressed;
    float peak;
    float valley;
} RtState;
```

逻辑：

```c
if (state->pressed) {
    if (pos > state->peak) {
        state->peak = pos;
    }

    if ((state->peak - pos) >= rt_release_delta) {
        state->pressed = 0;
        state->valley = pos;
    }
} else {
    if (pos < state->valley) {
        state->valley = pos;
    }

    if (pos >= min_actuation && (pos - state->valley) >= rt_press_delta) {
        state->pressed = 1;
        state->peak = pos;
    }
}
```

这只是基础 RT，后续还要处理：

- 噪声门限。
- 底部死区。
- 重新按下最小深度。
- 每键参数。
- 校准异常时的降级策略。

### 9.6 HID 报告

H417 根据 128 个键的 pressed 状态生成 NKRO bitmap：

```text
key_state[128]
  -> keymap/layer
  -> HID usage
  -> NKRO bitmap
  -> USB IN report
```

PC 上位机可以接收 raw ADC 作为调试数据，但不应该依赖 PC 做实时按键解算。

## 10. H417 双核分工建议

如果沿用当前 H417 产品架构，推荐最终分工：

```text
V3F:
  8K 实时链路
  SPI DMA 收 CH585M raw frame
  raw 合并
  归一化 / 滤波 / RT / 按键状态机
  生成 RuntimeIntent 或 key_state bitmap

V5F:
  RT-Thread
  USB HID / Vendor HID
  配置管理
  上位机通信
  屏幕 UI
  Flash 持久化
```

如果早期 bring-up 为了简单，也可以先：

```text
V5F 先同时做 SPI 读取 + 算法 + USB HID
```

等链路跑通后，再把实时部分下沉到 V3F。

不要一开始就把多核、USB、SPI、配置、屏幕全部同时拉满。MVP 先把最小闭环跑通。

## 11. MVP 建议步骤

### 第一步：假数据协议

目标：先不接 MUX/ADC。

- CH585M_0 每帧发 64 个递增假 ADC。
- CH585M_1 每帧发 64 个递增假 ADC。
- H417 SPI DMA 能稳定读两个 slave。
- H417 能检查 seq/crc。
- H417 能把 raw[128] 通过低频调试口或 USB CDC 打印。

验收：

```text
8K 下连续运行 10 分钟
seq_drop_count = 0
crc_error_count = 0
overrun_count = 0
```

### 第二步：真实 ADC 低频

目标：验证 MUX/ADC 采样，不追 8K。

- 先 1K 扫描。
- 上位机显示 128 个 raw ADC。
- 手按磁轴能看到对应通道变化。
- 检查串扰、噪声、通道顺序。

### 第三步：真实 ADC 8K

目标：验证完整实时吞吐。

- 提升到 8K。
- H417 统计每路 CH585M 的 frame age。
- 上位机记录噪声和丢帧。
- 观察 MUX 切换后是否需要额外 settle delay。

### 第四步：H417 基础按键算法

目标：H417 本地解算。

- 每键校准 raw_released/raw_pressed。
- 静态触发。
- 基础 RT。
- 生成 NKRO HID。
- PC 能作为键盘识别。

### 第五步：配置闭环

目标：上位机可调参数。

- PC 通过 Vendor HID/CDC 写入 actuation、release、RT 参数。
- H417 修改运行时参数。
- 参数影响实际按键行为。
- 后续再做 Flash 持久化。

## 12. 最重要的设计原则

1. CH585M 发 raw frame，不发最终按键事件。
2. H417 当 SPI master，统一节拍，统一解算。
3. SPI 必须 DMA，CH585M 必须双缓冲。
4. 每帧必须有 `source_id + seq + flags + crc`。
5. 先把 64 键假数据帧跑稳，再接真实 MUX/ADC。
6. 上位机只做配置和调试，实时键盘输入必须 H417 本地完成。
7. 如果 8K 全量 raw 链路压力太大，优先保证 HID 8K，raw monitor 可以低频。

## 13. 当前建议采用的最小协议

如果要先快速开工，可以先定这个最小版：

```c
#define SPI_KEY_MAGIC 0x4B53
#define SPI_KEY_COUNT 64

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  version;
    uint8_t  source_id;
    uint16_t seq;
    uint16_t flags;
    uint16_t base_key;
    uint16_t key_count;
    uint16_t adc[SPI_KEY_COUNT];
    uint16_t crc16;
} SpiKeyScanFrameV1;
```

先不加复杂命令，H417 每次片选后直接 clock 出一整个 `SpiKeyScanFrameV1`。

等这个稳定后，再扩展：

- `READ_STATUS`
- `READ_CAPS`
- `SET_SCAN_RATE`
- `SET_SYNC_MODE`
- `RESET_COUNTERS`
- `ENTER_BOOTLOADER`
