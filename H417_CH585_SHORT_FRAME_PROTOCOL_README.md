# H417 + CH585 短帧协议与回退方式

记录时间：2026-06-20

本文记录当前新增的短帧协议，以及如何回退到旧的 12B 命令 + 24B 状态帧协议。

## 当前默认协议

当前默认使用短帧协议：

```text
H417 命令帧: 8B
CH585 状态帧: 16B
单块 CH585 一轮正常收发: 24B = 192 bit
```

注意：当前 H417 仍保留调试开关 `APP_CH585_SPI_RESYNC_EVERY_POLL=1`，所以实测每轮还会多一次 16B drain/resync：

```text
当前调试轮次: 16B drain + 8B command + 16B state = 40B
目标正常轮次: 8B command + 16B state = 24B
```

相比旧协议：

```text
旧命令帧: 12B
旧状态帧: 24B
单块 CH585 一轮正常收发: 36B = 288 bit
```

短帧把单块 CH585 的单轮 SPI 传输量减少了 1/3。

## 短命令帧

H417 发给 CH585：

```text
offset  size  field
0       1     magic     0xA7
1       1     cmd       0x01 = GET_STATE
2       1     host_seq  H417 请求序号低 8 bit
3       1     ack_seq   H417 已接收的 CH585 seq 低 8 bit，无历史则 0xff
4       1     flags     reserved
5       1     reserved  reserved
6       2     crc16     CRC-CCITT over bytes 0..5
```

## 短状态帧

CH585 返回给 H417：

```text
offset  size  field
0       1     magic       0xD7
1       1     type        0x11 = SHORT_KEY_STATE
2       1     source_id   当前单板为 0
3       1     seq         CH585 状态帧序号低 8 bit
4       1     ack_seq     回显 H417 host_seq 低 8 bit
5       1     flags       bit7=READY, bit0..bit4=错误标志
6       8     down_bits   64 个按键状态，bit=1 表示按下
14      2     crc16       CRC-CCITT over bytes 0..13
```

## 当前仍然是两段式

这次改动只做短帧，不直接改流水式。

当前流程仍然是：

```text
H417 drain/resync
H417 发送 8B GET_STATE
H417 等待短时间
H417 读取 16B 状态帧
```

短帧验证稳定后，先尝试把 H417 的 `APP_CH585_SPI_RESYNC_EVERY_POLL` 设为 `0`。如果再次出现隔帧错位，再进入流水式改造。

后续再改成流水式：

```text
第 N 次事务：H417 发请求 N，同时读响应 N-1
```

## 回退方法

H417 回退到旧协议：

```text
APP_CH585_SPI_WIRE_SHORT=0
```

CH585 回退到旧协议：

```text
CH585_USE_SHORT_FRAME=0
```

当前默认值：

```text
APP_CH585_SPI_WIRE_SHORT=1
CH585_USE_SHORT_FRAME=1
```

注意：H417 和 CH585 必须使用同一种协议。
一个短帧、一个旧帧时，SPI 会有时钟，但 CRC/magic/ack 都不会通过。

## 固件路径

H417 短帧固件：

```text
F:\嵌赛\hardware\rtthread_port\build\v5f\rtthread_ch32h417_v5f.hex
```

CH585 短帧固件：

```text
F:\嵌赛\CH585M_SPI_SLAVE_TEST\build\ch585m_spi_slave_test.hex
```

旧协议测试编译目录：

```text
F:\嵌赛\hardware\rtthread_port\build_legacy_test\v5f\rtthread_ch32h417_v5f.hex
F:\嵌赛\CH585M_SPI_SLAVE_TEST\build_legacy_test\ch585m_spi_slave_test.hex
```

## 已验证

- 默认短帧 H417 编译通过。
- 默认短帧 CH585 编译通过。
- 旧协议 H417 独立编译通过。
- 旧协议 CH585 独立编译通过。
- H417 + CH585 短帧实测能通信，状态帧长度确认为 16B。

当前短帧实测结果：

```text
src0 speed: 16 bytes 128 bits
head=d7 11 ...
crc=0
ack_err=0
seq_drop=0
cmd_wait_us=100
```

启动初期可能出现少量 `magic`，但在 `cmd_wait_us=100` 下，后续观察中 `magic` 停止增长，`ok` 继续增长。

下一步：

```text
1. 已验证两段式短帧在无每轮 resync 时会隔帧错位。
2. 已验证全双工流水式实验会导致 CH585 返回帧 CRC 每帧错误。
3. 当前采用 request-only 单事务短帧：
   H417 每轮只 clock 16B，CH585 DMA 返回已经准备好的 16B 状态帧。
4. 下一步可以基于 request-only 单事务方案逐档提 SPI 时钟。
```

request-only 单事务实测结果：

```text
poll=17  src0 ok=17  magic=0 crc=0 seq_drop=0 ack_err=0 resync=1
poll=25  src0 ok=25  magic=0 crc=0 seq_drop=0 ack_err=0 resync=1
src0 speed: 16 bytes 128 bits
```

提频实测：

```text
Mode5, HSRX=1: measured about 14.2 MHz, stable
Mode4, HSRX=1: measured about 16.0 MHz, stable
  poll=17  src0 ok=17  magic=0 crc=0 seq_drop=0 ack_err=0 resync=1
  poll=25  src0 ok=25  magic=0 crc=0 seq_drop=0 ack_err=0 resync=1
Mode3, HSRX=1: measured about 18.3 MHz, failed
  poll=17  src0 ok=2 magic=0 ver=2 src=5 crc=8 seq_drop=1 resync=15
  poll=25  src0 ok=2 magic=0 ver=6 src=6 crc=11 seq_drop=1 resync=23
```

当前实际每轮通信量：

```text
单块 CH585: 16B = 128 bit
两块 CH585: 32B = 256 bit
8kHz 两块总线载荷: 2.048 Mbit/s
```
