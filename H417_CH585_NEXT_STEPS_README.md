# H417 + CH585 SPI 下一步优化计划

记录时间：2026-06-21

本文只记录当前 H417 和 CH585 通信调试的下一步工作，避免后续烧录和改线时忘记主线。

## 当前状态

- H417 使用硬件 SPI2 做 SCK/MOSI/MISO。
- H417 使用 GPIO 控制 CH585 的 CS，不额外加硬件 READY 线。
- CH585 使用 SPI0 从机。
- 当前只接了一块 CH585，第二块后续再扩展。
- 现在手头没有 ADS7948/ADC 板，因此真实 ADC/MUX 扫描先暂停。
- CH585 测试固件已经能用模拟 ADC 进入本地按键算法，再输出 `down_bits[8]` 短帧。
- H417 已切到 USBFS CDC 调试版，当前主要看 `COM5`；WCH-Link `COM4` 可以同时保留看心跳/辅助日志。
- 杜邦线阶段 SPI 不继续硬拉 40MHz+，先使用当前稳定档，等 PCB 到后再重新做高速验证。
- 当前通信是软件握手：

```text
H417 发 GET_STATE 命令
CH585 校验命令
CH585 返回 64 个按键的 0/1 状态 bitmap
H417 校验 CRC / ack_seq / seq
H417 再通过 USB/串口调试看结果
```

## 2026-06-21 无 ADC 阶段要做的事

当前没有 ADC，下一步先把“可观测、可配置、可回退”的 CH585 前端算法框架做好。

优先级：

1. 保持当前链路稳定：

```text
CH585 模拟 ADC/本地算法
  -> down_bits[8] 短帧
  -> H417 SPI 拉取
  -> USBFS COM5 打印 KS/SS/TR
```

验收标准：

```text
s0ok 持续增加
s0fetch=0
s0crc=0
s0seq 不持续增加
KS 前几个键能在 1000/3000 变化
```

2. 增加低频调试输出或调试帧，用来看 CH585 内部算法状态：

```text
key_id
sim/raw_adc
filtered_adc
position 0..1000
down
rt_armed / peak / valley
```

高频正常帧仍然只发 `down_bits[8]`，调试信息低频输出，避免拖慢 SPI。

3. 把 CH585 算法参数整理成结构，后续真实 ADC 到了直接复用：

```text
released_adc
pressed_adc
press_position
release_position
rt_press_delta
rt_release_delta
filter_shift
rt_enable
```

4. 做 H417 -> CH585 配置命令雏形：

```text
GET_STATE       正常拉取键状态
GET_DEBUG       低频读取某几个 key 的算法内部状态
SET_CONFIG      下发触发点/RT/滤波参数
GET_CONFIG      回读当前参数
```

5. 等 ADS7948/ADC 硬件到位后，再把模拟 ADC 替换成真实单通道 ADC：

```text
ADS7948 单通道
  -> key0 状态
  -> ADS7948 双通道
  -> 单个 16:1 MUX
  -> 4 lane / 64 键
```

## 当前帧格式

当前是调试帧，不是最终高速帧。

```text
命令帧: 12B
状态帧: 24B
单块 CH585 一次请求合计: 36B = 288 bit
两块 CH585 在 8kHz 下约: 4.608 Mbit/s
```

结论：当前 bit 数会影响每帧耗时，但不是 SPI 时钟跑不上 40MHz 的主要原因。
SPI 时钟上不去更可能是采样相位、MISO 输出建立时间、线材/地线/驱动能力、两段式事务等待等问题。

## seq 字段含义

当前协议里有 3 个和序号相关的字段：

```text
host_seq:
  H417 每发一次 GET_STATE 命令加 1。

response.ack_seq:
  CH585 把收到的 host_seq 回显给 H417。
  H417 用它确认这一帧确实响应当前请求。

response.seq:
  CH585 自己的数据帧序号。
  H417 用它判断是否漏帧、重复帧、错位。
```

这些字段在调试阶段有价值，但最终高速帧可以压缩：

- `host_seq` 可以保留低 8 bit。
- `ack_seq` 可以保留低 8 bit，或在流水式协议稳定后弱化。
- `response.seq` 可以保留低 8 bit，用于发现漏帧。

## 下一步 1：先确认当前链路稳定

插上 USB/WCH-Link 后先看 H417 控制台日志。

重点观察这些计数是否继续增加：

```text
magic
crc
ack_err
seq_drop
resync
cmd_timeout
```

理想现象：

```text
启动或刚复位时允许少量错误
随后 magic/crc/ack_err/seq_drop/resync 基本停止增加
ok 持续增加
```

如果 `resync` 或 `magic` 持续增加，优先检查：

```text
CS 是否接对
SCK/MOSI/MISO 是否接反
GND 是否足够
CH585 是否烧录了最新测试固件
H417 是否烧录了最新 V5F 固件
```

## 下一步 2：把调试帧压缩成高速短帧

当前 H417 不需要 CH585 上报模拟 ADC 原始值。
CH585 只需要在本地判断按键按下/松开，然后发送 64 个按键的 bitmap。

建议高速状态帧：

```text
offset  size  field
0       1     magic/simple header
1       1     seq
2       1     ack
3       1     flags
4       8     down_bits[8]
12      2     crc16
14      2     reserved/diag
```

总长度 16B。

建议高速命令帧：

```text
offset  size  field
0       1     magic/simple header
1       1     cmd
2       1     host_seq
3       1     ack
4       2     crc16
```

总长度 6B，也可以为了 DMA/对齐扩到 8B。

目标：

```text
单块 CH585: 约 24B 或更少
两块 CH585: 约 48B 或更少
8kHz 总线占用明显下降
```

## 下一步 3：改成流水式 SPI 事务

当前方式：

```text
事务 1: H417 发命令
等待 CH585 准备
事务 2: H417 读状态帧
```

这个方式清晰，但中间有等待时间，不适合冲很高的扫描效率。

建议后续改成流水式：

```text
第 N 次事务:
  H417 发送请求 N
  同时读取 CH585 已准备好的响应 N-1

第 N+1 次事务:
  H417 发送请求 N+1
  同时读取 CH585 对请求 N 的响应
```

优点：

- 一次 SPI 事务同时完成 MOSI 命令和 MISO 响应。
- 去掉 `命令 -> 等待 -> 读数据` 的中间等待。
- 更接近硬件 SPI 的高速使用方式。

需要验证：

- CH585 SPI0 从机是否能稳定全双工：一边接收 MOSI 命令，一边发送上一帧 MISO 响应。
- CH585 当前代码是否正确处理 RX FIFO 和 TX FIFO 同时工作。
- CS 拉低期间帧长度固定，避免 CH585/H417 双方错位。

## 下一步 4：再逐档提高 SPI 频率

不要直接跳到 70MHz。先按档位测：

```text
14MHz 当前稳定档
16MHz / 18MHz
20MHz
30MHz
40MHz+
```

每一档只看错误计数，不凭肉眼感觉判断。

通过标准：

```text
连续运行一段时间
crc 不增加
magic 不增加
ack_err 不增加
seq_drop 不持续增加
ok 持续增加
```

如果高频失败，优先尝试：

```text
调整 SPI CPHA/采样相位
调整 H417 HSRX 模式
降低线长
增加 GND 线
确认 MISO 强驱动只用于单 CH585 测试
用示波器/逻辑分析仪看 SCK/MISO 边沿
```

## 下一步 5：第二块 CH585 扩展

第二块 CH585 加入前，先把单块通信稳定。

最终一条 SPI 总线：

```text
SCK  共用
MOSI 共用
MISO 共用或分别接入后再决定
CS0  单独控制 CH585_0
CS1  单独控制 CH585_1
GND  必须共地
```

如果两块 CH585 共用 MISO，必须验证：

```text
未选中的 CH585 在 CS 无效时是否真正释放 MISO
两个 CH585 不会同时驱动 MISO
```

如果不能保证，就改成：

```text
两路独立 MISO
或外部三态缓冲/隔离
```

## 优先级总结

1. 先看当前链路是否稳定，确认错误计数是否停住。
2. 把 12B + 24B 调试协议压成短帧。
3. 把两段式请求改成流水式单事务。
4. 再逐档提高 SPI 时钟。
5. 单块稳定后再接第二块 CH585。

核心判断：

```text
压帧解决的是总线占用问题。
流水式解决的是事务等待问题。
采样相位/信号质量解决的是 SPI 时钟上限问题。
```
