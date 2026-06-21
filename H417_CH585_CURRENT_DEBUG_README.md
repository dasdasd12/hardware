# H417 + CH585 当前 SPI 调试状态

本文记录当前已经验证过的 H417 与单块 CH585M 调试状态，方便后续和队友对齐。

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
F:\嵌赛\CH585M_SPI_SLAVE_TEST\build\ch585m_spi_slave_test.hex
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
F:\嵌赛\CH585M_SPI_SLAVE_TEST\src\main.c
```

## 下一步建议

1. 先继续观察单块 CH585 是否长时间保持 `crc` 不继续增长。
2. 接第二块 CH585M：共用 SCK/MOSI，单独接 PD5/MISO1 和 PD7/CS1。
3. H417 把 `source1` 从假数据改成真实 soft SPI source1。
4. CH585M 把当前模拟 ADC 值替换成真实 MUX/ADC 扫描值。
5. H417 把 raw[128] 交给磁轴算法层，做校准、滤波、触发点、RT 等逻辑。
6. USB 从当前 CDC 调试输出切到正式 8K HID 上报。
