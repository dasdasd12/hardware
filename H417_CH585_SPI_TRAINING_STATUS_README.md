# H417-CH585 SPI 训练与提速状态

## 当前固件

H417:

```text
F:\嵌赛\hardware\rtthread_port\build\v5f\rtthread_ch32h417_v5f.hex
```

CH585:

```text
F:\嵌赛\CH585M_SPI_SLAVE_TEST\build\ch585m_spi_slave_test.hex
```

## 当前协议

- H417 使用 SPI2 硬件 SPI，PB12 作为 GPIO CS。
- CH585 使用 SPI0 从机 DMA。
- 当前是 request-only 短帧：H417 拉低 CS 并 clock 16 字节，CH585 返回已经准备好的 16B 状态帧。
- 帧内有 `magic/type/source/seq/ack/flags/down_bits/crc16`。
- `seq` 现在主要用于判断是否丢帧或重复帧；真正产品化后可以评估是否保留。

## 训练算法现状

H417 已加入 SPI 档位训练：

- 初始先用已知可通信的 Mode4 + HSRX2，约 16.7MHz。
- 不在初始化瞬间训练，而是在第 8 次 poll 后训练，避免 CH585 尚未稳定。
- 训练候选从 50MHz、33.3MHz、25MHz、20MHz、16.7MHz、14.3MHz 依次测试。
- 每个候选先 warmup 8 帧，再统计 16 帧。
- 每帧之间保留 125us 间隔，用来模拟 8kHz 同一 CH585 的请求周期。
- 串口会输出 `CH585 SPI train:` 和 `src0 train candidates:`。

## 2026-06-20 实测现象

慢速主循环 poll 时，16MHz 档可以正常通信：

```text
src0 ok 持续增加
magic/crc/seq_drop 不持续增加
src0 speed: 16 bytes 128 bits xfer=8 us sck=16000.0 kHz
```

但 8kHz 训练条件下，旧 CH585 模拟 ADC 帧生成版本表现为：

```text
50MHz: bad=16/16
33MHz: bad=16/16
25MHz: bad=16/16
20MHz: bad=15~16/16
16.7MHz: bad=8/16
14.3MHz: bad=8/16
```

这说明问题不只是 SPI 线速或 H417 采样相位。因为 500ms 慢速 poll 稳定，而 125us 周期训练坏一半，更像 CH585 在每帧结束后重新准备下一帧的时间太长。

## 新增 CH585 快速模拟帧

CH585 已加入：

```c
#define CH585_FAST_SIM_FRAME 1
```

快速模式只更新少量 `down_bits`，仍保留 16B 短帧和 CRC16。用途是把“SPI 通信能力”和“64 路模拟 ADC/按键计算耗时”拆开测试。

下一步需要把这个 CH585 hex 烧进 CH585，然后重启/重烧 H417，让 H417 重新训练。

## 下一步判断

如果快速模拟帧后 20MHz/25MHz 变成 0 错：

- 主要瓶颈在 CH585 帧生成/按键计算。
- 后续应把 CH585 改成双缓冲：一边给 SPI DMA 发上一帧，一边在后台准备下一帧。
- 按键判断不要放在 SPI CS 间隔的临界路径里。

如果快速模拟帧后仍然只能 16MHz 或更低：

- 主要瓶颈更可能是物理连线、MISO 建立时间、采样相位或 H417/CH585 SPI 模式匹配。
- 继续用 CPHA、HSRX、线长、多根 GND、示波器/逻辑分析仪排查。

## 快速模拟帧补充实测

CH585 快速模拟帧烧录后，H417 在 125us 帧间隔训练：

```text
50MHz:   bad=16/16
33MHz:   bad=16/16
25MHz:   bad=16/16
20MHz:   bad=12~13/16, seq=2
16.7MHz: bad=3/16 best
14.3MHz: bad=5/16
```

H417 改为 250us 帧间隔训练后：

```text
16.7MHz Mode4 HSRX2: bad=0/16 seq=0
20MHz 仍然不稳定
```

结论：

- CH585 原先 64 路模拟 ADC 计算确实会恶化 125us 训练结果。
- 即使快速模拟帧，125us 仍不能 0 错；250us 可以 0 错，说明 CH585 每帧结束后的 DMA/帧准备路径仍有时序余量问题。
- 下一步需要静态帧测试：CH585 不更新 seq、不重算 CRC，只重复发送同一帧，用来判断 125us 下纯 SPI0 DMA 重装是否足够。

静态帧测试版 CH585 hex：

```text
F:\嵌赛\CH585M_SPI_SLAVE_TEST\build\ch585m_spi_slave_test.hex
```

对应 H417 已重新编译回 125us 训练间隔，但需要在 CH585 静态版烧好后重烧/重启 H417 才会重新训练。

## 静态帧 125us 实测

CH585 静态帧测试版烧录后，H417 125us 训练结果：

```text
50MHz:   bad=16/16
33MHz:   bad=16/16
25MHz:   bad=16/16
20MHz:   bad=10~14/16
16.7MHz: bad=0/16
14.3MHz: bad=0/16
```

静态帧不更新 `seq`，所以这里 `seq=15` 是预期现象；判断链路只看 `bad`。

结论：

- CH585 的 SPI0 DMA 每 125us 重装一次，在 16.7MHz 可以做到 `bad=0`。
- 快速动态帧在 125us 下仍有坏帧，说明 CH585 帧更新/CRC/数据准备路径仍然太贴边。
- 20MHz 即使静态帧也不稳定，说明 20MHz 以上还有物理连线、MISO 建立时间或 H417 采样相位问题。
- 当前可工作的通信上限应先按 16.7MHz 规划；要到 40MHz+，需要示波器/逻辑分析仪和更短更规范的连线，必要时调整驱动能力、端接、采样相位或换更合适的 SPI 引脚/布线。

## 动态帧双缓冲版本

CH585 request-only 短帧路径已经改成 ping-pong 双缓冲：

- `g_frame_pingpong[0/1]` 两个 4 字节对齐帧缓冲。
- 当前缓冲只给 SPI0 DMA 发送。
- 另一缓冲在本轮 CS 拉低后生成下一帧。
- 下一轮可以直接装载已经准备好的帧，减少 125us 帧间临界路径。

当前待烧录 CH585 hex：

```text
F:\嵌赛\CH585M_SPI_SLAVE_TEST\build\ch585m_spi_slave_test.hex
```

注意：这个 hex 是动态帧双缓冲版，`CH585_STATIC_FRAME_TEST=0`。烧好 CH585 后，需要重新烧录或复位 H417，让 H417 重新跑 125us 训练。

## 动态帧双缓冲实测

CH585 双缓冲动态帧版烧录后，H417 先用 16 帧训练：

```text
20MHz Mode3 HSRX2: bad=5/16 seq=3
20MHz Mode3 HSRX1: bad=12/16 seq=2
16.7MHz Mode4 HSRX2: bad=1/16 seq=0
16.7MHz Mode4 HSRX1: bad=0/16 seq=0
```

随后将 H417 训练帧数提高到 64 帧，125us 帧间隔不变：

```text
50MHz:   bad=64/64
33MHz:   bad=64/64
25MHz:   bad=64/64
20MHz HSRX2: bad=48/64 seq=8
20MHz HSRX1: bad=41/64 seq=10
16.7MHz HSRX2: bad=0/64 seq=0
```

H417 最终自动选择：

```text
SPI2 prescaler Mode4, HSRX2, 16.7MHz expected
measured about 16.0MHz
train_err=0/64
```

结论：

- 双缓冲解决了 CH585 动态帧 125us 准备不足的问题。
- 当前单 CH585、16B 短帧、125us 请求周期下，16.7MHz 档已经可以作为稳定工作档。
- 20MHz 即使在双缓冲后仍然失败，后续若要上 40MHz+，重点不再是帧格式，而是硬件时序/连线/采样相位/驱动能力。
- 下一步可以把 CH585 的真实 ADC 扫描和按键判断接入双缓冲后台路径，注意不要把 ADC 计算放回 SPI DMA 发送前的临界路径。

## 给同学看的短结论

我们现在不是软件 SPI，H417 已回到硬件 SPI2，只是 CS 用 GPIO 控制。当前 16B request-only 短帧慢速通信稳定；但是按 8kHz 节奏训练时，旧 CH585 模拟帧生成会导致 16MHz 也坏一半。下一步用 CH585 快速模拟帧验证瓶颈是否来自 CH585 每帧计算和 DMA 重装，而不是单纯 SPI 时钟。
