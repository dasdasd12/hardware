# H417 与 CH585M SPI 扫描帧分支说明

这个分支在 `rtthread_port` 里补了一个 H417 侧的 CH585M 扫描帧接收骨架，用来先验证“两个 CH585M 前端 -> H417 合并 -> 后续 USB 上报”的软件边界。当前还没有绑定真实 SPI 引脚和 DMA，中间先用可重复的 fake source 产出两路 64 键 raw ADC 数据，方便在 H417 串口日志里调通帧格式、CRC、序号和合并逻辑。

## 当前实现

- 入口：`rtthread_port/applications/main.c`
  - 启动时调用 `ch585_spi_scan_init()`。
  - 主循环里调用 `ch585_spi_scan_poll_once()`，每次轮询两个 CH585M source。
  - 心跳日志里周期性打印接收统计和 `raw[0] / raw[63] / raw[64] / raw[127]`。
- USB2.0 调试/上报通道：`rtthread_port/applications/usb_cdc_dual.c`
  - 当前分支默认打开 `APP_ENABLE_USB_TEST=1`。
  - 默认打开 `APP_ENABLE_USB2_HS_CDC=1`，走 H417 的 USBHS，也就是 USB2.0 High-Speed。
  - 默认关闭 `APP_ENABLE_USB2_FS_CDC=0`，避免同时枚举 FS/HS 两套 CDC 把调试复杂度拉高。
  - USBSS 仍按现有策略由 V3F official CH372 stack 持有，V5F 侧默认跳过 USBSS。
- 模块：`rtthread_port/applications/ch585_spi_scan.c`
  - 定义固定 v1 帧：magic、version、source_id、seq、flags、base_key、key_count、64 个 `uint16_t adc`、CRC16。
  - 校验 magic/version/source/key_count/CRC。
  - 检测每个 source 的 seq 断帧。
  - 合并为 H417 侧 128 键 raw ADC 数组。
- 头文件：`rtthread_port/applications/ch585_spi_scan.h`
  - 暴露帧结构、统计结构、初始化、轮询、统计打印和 raw ADC 访问接口。
- 构建：`rtthread_port/Makefile`
  - 已把 `applications/ch585_spi_scan.c` 加入 `APP_SRC`。

## 推荐的真实 SPI 帧格式

每个 CH585M 固定负责 64 个键。H417 作为 SPI master 主动拉取，每次 CS 低电平期间读取一整帧。

```c
typedef struct __attribute__((packed))
{
    uint16_t magic;      /* 0x4B53, "KS" */
    uint8_t version;     /* 1 */
    uint8_t source_id;   /* 0 或 1 */
    uint16_t seq;        /* 每帧递增 */
    uint16_t flags;      /* overrun/adc_error/stale/sync_lost */
    uint16_t base_key;   /* source0=0, source1=64 */
    uint16_t key_count;  /* 64 */
    uint16_t adc[64];    /* 原始 ADC 码值，先不要在 CH585M 上做按键判定 */
    uint16_t crc16;      /* 覆盖 crc16 前面的所有字节 */
} ch585_scan_frame_v1_t;
```

这样做的好处是 CH585M 只负责采样和封包，H417 负责去抖、磁轴行程映射、阈值、RT/DKS 等算法，后续调算法不需要反复改两个前端 MCU。

## 下一步要补的硬件相关代码

1. 确认 H417 到两个 CH585M 的 SPI 引脚、CS 引脚、SPI 外设实例和 DMA 通道。
2. 在 `ch585_scan_fetch_source()` 里替换 fake backend：
   - 拉低对应 CH585M 的 CS。
   - 用 SPI master + DMA 读 `sizeof(ch585_scan_frame_v1_t)` 字节。
   - 超时或 DMA 错误时返回非 0。
   - 拉高 CS。
3. 给 CH585M 侧实现同一个 `ch585_scan_frame_v1_t`，并用双缓冲保证 H417 拉取时读到的是完整帧。
4. 在 H417 上把 `ch585_spi_scan_raw()` 接到实际键盘算法层，再接 USB 8K 上报层。

## 调试顺序

先不要同时调所有链路。建议顺序是：

1. H417 本地 fake source：确认串口能看到两路 source 的 `ok` 持续增长，`crc` 和 `seq_drop` 为 0。
2. 单个 CH585M + H417 SPI：先只接 source0，确认 magic/version/source/key_count/CRC/seq 全部稳定。
3. 两个 CH585M + H417 SPI：确认 source0/source1 的 `ok` 都增长，且没有互相串 CS 或 base_key 错误。
4. H417 算法：用 raw ADC 做基线、滤波、触发阈值、释放阈值，再输出 key state。
5. H417 USB2.0 HS：当前先用 USBHS CDC 做调试通道；键盘正式上报时，再把 CDC 替换/扩展成 USB HID 8K 报告。
