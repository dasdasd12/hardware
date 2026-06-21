# H417 与两块 CH585 软件 SPI 接线说明

本文记录当前建议采用的 H417 到两块 CH585 的软件 SPI 接线方案。

## 结论

H417 端不用硬件 SPI 外设，不使用 `SPI1/SPI2/SPI3/SPI4` 和 DMA，而是用普通 GPIO 模拟 SPI master。

CH585 端可以继续使用自己的 SPI slave，或者后续也用 GPIO 模拟 slave，但当前接线只约定电气连接和协议方向。

推荐 H417 使用 `PD2 ~ PD7` 这 6 根 GPIO：

```text
H417 PD2  -> SCK
H417 PD3  -> MOSI
H417 PD4  <- MISO0
H417 PD5  <- MISO1
H417 PD6  -> CS0
H417 PD7  -> CS1
```

## 接线表

| 功能 | H417 引脚 | CH585_0 | CH585_1 | 说明 |
| --- | --- | --- | --- | --- |
| SCK | PD2 | SCK | SCK | 两块 CH585 共用时钟 |
| MOSI | PD3 | MOSI | MOSI | H417 发给两块 CH585 |
| MISO0 | PD4 | MISO | 不接 | CH585_0 单独返回数据 |
| MISO1 | PD5 | 不接 | MISO | CH585_1 单独返回数据 |
| CS0 | PD6 | CS | 不接 | 低有效，选中 CH585_0 |
| CS1 | PD7 | 不接 | CS | 低有效，选中 CH585_1 |
| GND | GND | GND | GND | 必须共地 |
| 电源 | 3.3V | VCC | VCC | 确认两侧都是 3.3V 电平 |

## 一个 H417 SCK 怎么接两块 CH585

`PD2 -> 两块 CH585 的 SCK` 的意思是：`PD2` 这一个输出脚作为同一个 SCK 信号网，分成两路接到两块 CH585 的 SCK 输入脚。

原型调试时可以这样接：

```text
             +-> CH585_0 SCK
H417 PD2 SCK |
             +-> CH585_1 SCK
```

这在电气上是允许的，因为 H417 的 `PD2` 是输出，两个 CH585 的 `SCK` 都是输入。一个数字输出驱动多个 CMOS 输入，负载主要是输入电容，两个 CH585 输入对低速软件 SPI 来说问题不大。

同理，`PD3 / MOSI` 也可以这样一分二：

```text
              +-> CH585_0 MOSI
H417 PD3 MOSI |
              +-> CH585_1 MOSI
```

但是 `MISO` 不建议一开始共用，因为 MISO 是 CH585 输出、H417 输入。两个输出直接并在一起可能会互相打架，所以当前方案用两根 MISO：

```text
CH585_0 MISO -> H417 PD4
CH585_1 MISO -> H417 PD5
```

实际飞线时可以用面包板同一排、转接板同一个焊盘网络，或者在 H417 的 `PD2` 处焊出两根线。线尽量短，SCK 线上可以预留 22R~100R 串联电阻位置，方便后面处理边沿振铃。

## 为什么 MISO 分两根

软件 SPI 不受硬件 SPI 外设的 MISO 引脚限制，所以建议两块 CH585 的 MISO 分开接：

```text
CH585_0 MISO -> H417 PD4
CH585_1 MISO -> H417 PD5
```

这样可以避免两块 CH585 在 MISO 上互相驱动。调试阶段更容易判断是哪一块 CH585 的数据有问题。

如果以后要省 GPIO，可以把两块 CH585 的 MISO 共用到一根 H417 GPIO，但必须确认 CH585 在 CS 拉高时 MISO 是高阻态，否则会发生总线冲突。

## 初始电平

H417 上电初始化时建议设置：

```text
PD2 / SCK  = 输出低
PD3 / MOSI = 输出低
PD6 / CS0  = 输出高
PD7 / CS1  = 输出高
PD4 / MISO0 = 输入
PD5 / MISO1 = 输入
```

`CS0` 和 `CS1` 都是低有效。空闲时必须保持高电平。

## SPI 时序约定

第一版建议用最常见、最容易验证的模式：

```text
SPI Mode 0
SCK 空闲低电平
上升沿采样 MISO
下降沿更新 MOSI
MSB first，高位先发
CS 低有效
```

软件 SPI 初始频率建议先低速：

```text
100 kHz ~ 500 kHz
```

等假帧、CRC、序号都稳定后，再逐步提高频率。

## 暂时不要占用的 H417 引脚

当前工程里这些引脚已经有用途，调试阶段不要拿来接 CH585：

```text
PA11 / PA12：USB2.0 FS
PB4 / PB5：UART8 日志
PB1：板载 LED
PA13 / PA14：调试相关风险较高
```

另外，虽然 `PB13 / PC1 / PC2 / PB12` 是官方例程常用的 `SPI2` 引脚，但这次同学要求 H417 不使用硬件 SPI，所以当前接线先避开这组硬件 SPI 引脚。

## 调试顺序

建议不要一开始两块 CH585 全接满。按下面顺序来：

1. 只接 CH585_0：

```text
H417 PD2  -> CH585_0 SCK
H417 PD3  -> CH585_0 MOSI
H417 PD4  <- CH585_0 MISO
H417 PD6  -> CH585_0 CS
GND 共地
3.3V 供电
```

2. H417 用软件 SPI 低速读取 CH585_0 的固定测试帧。
3. 在 PC 端通过 H417 USBFS CDC COM 口观察 `magic / source_id / seq / crc` 是否稳定。
4. 再接 CH585_1：

```text
H417 PD2  -> CH585_1 SCK
H417 PD3  -> CH585_1 MOSI
H417 PD5  <- CH585_1 MISO
H417 PD7  -> CH585_1 CS
GND 共地
3.3V 供电
```

5. H417 分别拉低 `CS0` 和 `CS1`，轮询读取两块 CH585。
6. H417 合并两块 CH585 的 64 键数据，继续通过 USBFS CDC 上报到 PC。

## 后续代码命名建议

H417 侧建议把这套 GPIO 固定成宏：

```c
#define H417_CH585_SOFT_SPI_SCK_PIN    "PD.2"
#define H417_CH585_SOFT_SPI_MOSI_PIN   "PD.3"
#define H417_CH585_SOFT_SPI_MISO0_PIN  "PD.4"
#define H417_CH585_SOFT_SPI_MISO1_PIN  "PD.5"
#define H417_CH585_SOFT_SPI_CS0_PIN    "PD.6"
#define H417_CH585_SOFT_SPI_CS1_PIN    "PD.7"
```

后续实现时，H417 读 CH585_0 使用 `MISO0 + CS0`，读 CH585_1 使用 `MISO1 + CS1`，`SCK/MOSI` 共用。
