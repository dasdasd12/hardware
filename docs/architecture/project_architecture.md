# 磁轴键盘项目总览与结构设计

更新时间：2026-06-21

本文档记录当前有效的软硬件架构。后续如果调试记录里有旧结论，以本文档的“当前有效方案”为准。

## 1. 当前目标

目标是做一套约 128 键的磁轴键盘采集、判定与 USB 上报系统：

```text
128 个磁轴
  -> 8 个 16:1 MUX
  -> ADC 采样链路
  -> 2 个 CH585M 前端 MCU
  -> CH32H417 主控
  -> USB2.0 / USBHS 上报 PC
```

当前默认分工是每个 CH585M 管 64 键：

```text
CH585M_0: MUX0 ~ MUX3, key 0  ~ 63
CH585M_1: MUX4 ~ MUX7, key 64 ~ 127
```

当前前端采样模型是：

```text
每个 CH585M:
  4 条 ADC lane
  4 个 16:1 MUX
  默认可由 2 颗 ADS7948 x 2 通道组成 4 lane
  合计 4 x 16 = 64 键
```

如果实际硬件的 ADC/MUX 数量或映射不同，需要优先修正 CH585 侧 lane map 和 key map。

## 2. 当前有效职责划分

### CH585M

CH585M 是前端键轴处理器。它不只是采样，还负责每个本地键的磁轴算法：

- ADS7948 + MUX 扫描。
- ADC 稳定处理：MUX/ADC settle、切换后丢样、过采样、滤波。
- 每键标定：released / pressed / min / max。
- 每键位置计算：raw ADC -> position。
- 每键触发算法：普通触发、Rapid Trigger、滞回。
- 生成本地 64 键 key state / event / 可选 position。
- 作为 SPI slave 等待 H417 拉取结果。

也就是说，正常运行时不应把每键 raw ADC 全量高频传给 H417 后再做磁轴算法。raw ADC 更适合调试、校准、低频诊断。

### H417

H417 是主控和 USB 上报端：

- 作为 SPI master 主动拉取两个 CH585M 的结果。
- 校验 CH585M 帧：magic/version/source/ack/flags/CRC。
- 合并 2 x 64 键为全键盘状态。
- 做跨 CH585 的全局功能：SOCD、组合键、宏、层、配置同步。
- 通过 USBHS 做正式 HID / Vendor HID；当前调试阶段用 USBFS CDC 打印日志。
- 把 PC 端配置同步给两个 CH585M。

H417 不再作为正常运行时的最终磁轴算法主处理器。

## 3. 数据流

推荐正常工作流：

```text
CH585 后台持续扫描 ADC/MUX
CH585 在本地完成滤波、标定、位置、触发判断
CH585 准备好下一帧 key state

H417 需要下一帧
H417 拉低对应 CH585 的 CS
H417 通过 SPI clock 一次事务
CH585 返回已经准备好的状态帧
H417 拉高 CS
H417 校验、合并、做全局逻辑
H417 通过 USBHS 上报 PC
```

关键原则：

- H417 控制 SPI 时钟和拉取节奏。
- CH585 不主动“推送”数据；SPI slave 只有在 H417 给 SCK 时才移出数据。
- 不增加硬件 READY 握手线，优先使用 SPI 内的软件 command/ack/seq/CRC。
- CH585 侧应使用前后台缓冲，避免在 CS 临界路径里临时扫描 ADC 或组帧。

## 4. H417 与 CH585 的 SPI 连接

当前已经从旧的软件 SPI 方案切回硬件 SPI。正确理解是：

```text
H417 使用硬件 SPI2 跑 SCK/MOSI/MISO。
CS 使用 GPIO 控制，方便一条 SPI 总线挂两个 CH585M。
```

当前单块 CH585M 已测试接线：

| H417 | CH585M | 方向 | 作用 |
| --- | --- | --- | --- |
| PB12 | PA12 | H417 -> CH585 | GPIO CS0，低有效 |
| PB13 | PA13 | H417 -> CH585 | SPI2 SCK |
| PC1 | PA14 | H417 -> CH585 | SPI2 MOSI |
| PC2 | PA15 | CH585 -> H417 | SPI2 MISO |
| GND | GND | 双向参考 | 共地 |
| 3V3 | 3V3 | 电源/电平 | 同电平 |

第二块 CH585M 的推荐方向：

```text
PB13/SCK  -> 两块 CH585 PA13 共用
PC1/MOSI  -> 两块 CH585 PA14 共用
PC2/MISO  <- 两块 CH585 PA15 共用
CS0       -> CH585M_0 PA12
CS1       -> CH585M_1 PA12
GND/3V3   -> 共地、同电平
```

注意：共享 MISO 的前提是 CH585 在 CS 无效时能可靠释放 MISO。这个需要实测确认。若不能可靠三态，需要改成独立 MISO、外部隔离或缓冲。

旧的 PD2/PD3/PD4/PD6 软件 SPI 接线只属于早期调试方案，当前不作为正式架构。

## 5. CH585 -> H417 通信帧规划

### 高频正常帧

正常 8K 工作路径应优先使用短帧，只上传键状态和必要诊断：

```text
down_bits[8]   64 键 0/1 状态
flags          READY / ADC_ERROR / STALE / OVERRUN 等
seq 或 ack_seq 丢帧、重复帧、请求响应校验
crc16          帧完整性校验
可选 events    少量按下/释放事件
```

这样每个 CH585 每帧只需要几十字节，适合高速轮询。

### 低频调试/配置帧

raw ADC、position、released/pressed/min/max 等信息不建议作为高频默认帧。它们应该作为低频调试、标定、配置同步帧：

```text
GET_RAW_ADC
GET_POSITION
GET_CALIBRATION
SET_CALIBRATION
SET_TRIGGER_CONFIG
```

这样可以同时满足最终速度和调试可观测性。

## 6. USB 规划

当前 H417 USBFS CDC 已经能枚举并输出调试文本。正式产品路径应逐步切到：

```text
USBHS HID / Vendor HID
```

建议分层：

- HID：正式键盘输入，上报频率优先保证。
- Vendor HID：配置、标定、状态读取。
- CDC：仅调试阶段使用，当前走 USBFS COM5，后续应降频或可关闭。

当前主要调试通道已经切到 USBFS CDC，Windows 上枚举为 `USB 串行设备 (COM5)`。USBHS/正式 HID 仍是后续产品路径。

## 7. 当前已经验证的状态

截至 2026-06-21：

- H417 双核烧录可用；当前稳定方法是 V5F/V3F core both 一次烧录。
- H417 USBFS CDC 已能枚举，PC 可读到 `KD`、`KS`、`SS`、`TR` 等调试行。
- H417 与一块 CH585M 的硬件 SPI2 + GPIO CS 链路已跑通。
- 当前只实接一块 CH585M，第二块 CH585M 仍未接入；H417 侧 source1 仍可用假数据路径补齐调试。
- 当前 SPI 自动训练显示约 16 MHz 稳定，19.2 MHz 附近已经明显不稳，24 MHz 以上基本失败。
- 杜邦线阶段暂时把约 16 MHz 作为稳定工程档位，不继续强行拉到 40 MHz+。40 MHz 以上目标等 PCB 到后，再结合示波器/逻辑分析仪看 SCK/MISO 建立时间、线长、地线、驱动能力、采样相位、共享 MISO 三态等硬件问题。
- `firmware/ch585/spi_slave_test` 已加入仓库，默认从模拟 ADC 进入 CH585 本地键轴算法，再输出 `KEY_STATE/down_bits[8]` 和低频 `KEY_DEBUG` 短帧；`CH585_FAST_SIM_FRAME=1` 仍可回退到旧 pattern。
- `firmware/ch585/frontend/ads7948.*` 和 `ch585_ads7948_mux_scan.*` 已有独立原型代码，但尚未接入 CH585 正式工程。
- `firmware/ch585/common/magnetic_key_engine.*` 可作为 CH585 侧磁轴算法参考/仿真原型。

## 8. 当前关键代码入口

H417 侧：

| 文件 | 当前作用 |
| --- | --- |
| `firmware/h417/v5f_rtthread/applications/main.c` | RT-Thread 主循环，SPI/USBFS CDC 调试输出入口 |
| `firmware/h417/v5f_rtthread/applications/ch585_spi_scan.c/.h` | H417 拉取 CH585 短帧、训练 SPI、合并调试数据 |
| `firmware/h417/v5f_rtthread/applications/usb_cdc_dual.c` | USBFS/USBHS CDC 调试输出 |
| `firmware/h417/v5f_rtthread/tools/read_usbhs_cdc.ps1` | PC 侧读取 CDC 串口日志 |

CH585 侧：

| 文件 | 当前作用 |
| --- | --- |
| `firmware/ch585/spi_slave_test/src/main.c` | CH585 SPI slave 测试工程，当前模拟 ADC、本地按键算法、`KEY_STATE` 和 `KEY_DEBUG` 都在这里 |
| `firmware/ch585/frontend/ads7948.c/.h` | ADS7948 保守读驱动原型 |
| `firmware/ch585/frontend/ch585_ads7948_mux_scan.c/.h` | CH585 侧 ADS7948 + MUX 扫描层原型 |
| `firmware/ch585/common/magnetic_key_engine.c/.h` | CH585 侧磁轴算法参考模块 |

## 9. 后续优先级

1. 用 `firmware/ch585/spi_slave_test` 持续验证模拟 ADC -> CH585 本地算法 -> `KEY_STATE/KEY_DEBUG` -> H417 USBFS `KD/KS/SS` 这条链路。
2. 在 CH585 上接入 ADS7948 单通道读取，先确认固定通道 raw code 稳定。
3. 接入 CH585 的 MUX 扫描，确认 64 键 raw code 和 lane/key map。
4. 把完整磁轴算法迁移到 CH585：滤波、标定、position、普通触发、RT、滞回。
5. CH585 正常帧继续发短帧 key state，raw/position 改为低频调试帧。
6. H417 保持硬件 SPI2 + GPIO CS，先完成单 CH585 稳定，再接第二块 CH585。
7. 用 USBHS Vendor HID/HID 替代长期 CDC 调试输出。
8. PCB 到板后再重新做 40 MHz+ SPI 提速、电气测量和两块 CH585 共享 MISO 验证。
