# ADS7948 / MUX / Magnetic Engine Agent Notes

更新时间: 2026-06-21

本次只新增独立文件, 没有修改任何现有 `.c/.h/Makefile/main` 文件。原因是当前 SPI417/CH585 调试链路正在由其他 agent 调试。

## 当前仓库里已经有的初步代码

结论: 已经有初步代码, 但还不是 ADS7948 实采样闭环。

1. H417 侧 SPI417/CH585 接收:
   - `rtthread_port/applications/ch585_spi_scan.h`
   - `rtthread_port/applications/ch585_spi_scan.c`
   - 当前默认 `APP_CH585_SPI_WIRE_SHORT=1`, 接收 CH585 短帧 `0xD7/0x11`。
   - 短帧只带 `down_bits[8]`, H417 侧会把按下映射为模拟 raw `3000`, 松开映射为模拟 raw `1000`。
   - 所以当前 H417 侧还没有从 CH585 收到真实 ADS7948 raw ADC 数组。

2. H417 侧原型磁轴算法:
   - `rtthread_port/applications/keyboard_engine.h`
   - `rtthread_port/applications/keyboard_engine.c`
   - 已有固定阈值/IIR/位置 0..1000/按下释放事件队列。
   - 目前是原型: 默认 released=1000, pressed=3000, press=450pm, release=350pm。

3. CH585 测试工程里的临时按下判断:
   - `../CH585M_SPI_SLAVE_TEST/src/main.c`
   - 里面有 `update_key_state_from_adc()` 这样的阈值滞回逻辑。
   - 但默认 `CH585_FAST_SIM_FRAME=1` 时会直接发 pattern bits, 会绕过 ADC 阈值判断。
   - 如果要临时用 CH585 做 down_bits, 需要让 CH585 采样层产出 down_bits, 再塞进当前短帧。

4. ADS7948:
   - 本地仓库此前没有搜到 `ADS7948`/`ads7948` 相关驱动。

## 新增文件

1. `firmware/ch585_frontend/ads7948.h`
2. `firmware/ch585_frontend/ads7948.c`

ADS7948 保守读驱动。ADS7948 是 TI ADS794x 系列的 10-bit 双通道 SAR ADC, 不是 SPI 命令选通道, 而是通过 `CH SEL` 引脚选择 AIN0/AIN1。驱动通过回调接入 CH585 的 GPIO/SPI/delay:

- `set_cs(level, user)`
- `set_ch_sel(level, user)`
- `read16(&word, user)`
- `delay_us(us, user)`

默认时序是:

1. 切 `CH SEL`
2. 等待 `input_settle_us`
3. 读并丢弃 `discard_frames` 个 16-clock frame
4. 再读 1 个 frame, 取 bits `[15:6]` 作为 10-bit code

这样速度不是最高, 但适合先把 MUX/ADC 链路调稳。

3. `firmware/ch585_frontend/ch585_ads7948_mux_scan.h`
4. `firmware/ch585_frontend/ch585_ads7948_mux_scan.c`

CH585 前端扫描层。默认逻辑:

- 1 个 CH585 管本地 64 键
- 4 条 lane, 每条 lane 接 1 个 16:1 MUX
- 默认用 2 颗 ADS7948, 每颗 2 通道, 合计 4 lane
- key map: lane0=0..15, lane1=16..31, lane2=32..47, lane3=48..63

扫描处理:

- 设置 MUX 地址
- 等 `mux_settle_us`
- 可选丢弃一次切换后的读数
- 每 lane 过采样平均
- IIR 滤波
- 用 raw code 阈值和滞回生成 `down_bits[8]`
- 同时保留 front buffer raw ADC, 后续可升级为 raw ADC 帧

5. `firmware/common/magnetic_key_engine.h`
6. `firmware/common/magnetic_key_engine.c`

H417 侧最终磁轴算法候选模块。它不依赖 RT-Thread, 也不依赖当前 `keyboard_engine`。

功能:

- 每键 released/pressed ADC 标定
- 自动支持按下 ADC 增大或减小两种方向
- raw ADC -> IIR filtered ADC -> position 0..1000pm
- 静态触发模式
- Rapid Trigger 模式
- press/release event queue

## ADS7948 时序要点

官方资料: https://www.ti.com/lit/ds/symlink/ads7948.pdf

关键点:

- ADS7948 是 10-bit, dual-channel, SPI-compatible, 16-clock frame。
- `CH SEL=0` 选择 channel 0, `CH SEL=1` 选择 channel 1。
- 16-clock frame 输出上一帧完成的转换结果。
- ADS7948 的转换在第 11 个 SCLK rising edge 结束并进入 acquisition。
- 外部 MUX 切换、ADS7948 `CH SEL` 切换、前端 RC/运放稳定都会影响第一拍读数, 所以前期建议丢弃切换后的样本。

## 建议接入顺序

1. CH585 先接入 `ads7948.*`
   - 给每颗 ADS7948 写 CS/CH_SEL/SPI16/delay 回调。
   - 用单通道固定电压或手按单键验证 10-bit code 是否稳定。

2. CH585 再接入 `ch585_ads7948_mux_scan.*`
   - 提供 `set_mux_addr(mux_channel)`。
   - 先用保守配置: `mux_settle_us=5..20`, `oversample_count=4`, `discard_after_mux_switch=1`。
   - 串口或调试帧观察 64 个 raw code。

3. 如果暂时不改 H417 通信协议:
   - 继续使用当前短帧。
   - CH585 用 `ch585_mux_scan_front_down_bits()` 生成 `down_bits[8]`。
   - H417 当前 `ch585_spi_scan` 可以继续把 down_bits 映射成 1000/3000。

4. 如果要做真正最终磁轴:
   - 需要把 CH585->H417 帧从 down_bits 升级为 raw ADC 数据。
   - H417 收到 2 个 CH585 的 64 raw 后合并为 128 raw。
   - 再把 128 raw 喂给 `mag_key_engine_update()`。
   - 当前短帧只带按下位, 不足以在 H417 做精细磁轴行程、RT、DKS、SOCD。

## 重要注意

- 本次新增模块没有被 Makefile 编译, 需要等当前 SPI 调试稳定后再由接入 agent 选择性加入工程。
- 新模块的默认阈值是 ADS7948 10-bit code 级别, 只是 bring-up 默认值, 必须用实测 released/pressed ADC 重新标定。
- 如果实际硬件不是 "2 ADS7948 x 2ch = 4 lane", 修改 `lane_adc_index[]` 和 `lane_adc_channel[]` 即可。
- 如果实际 key map 不是 lane 连续布局, 接入时在 CH585 侧重排 raw/down_bits, 或在 H417 merge 层重排。

## 2026-06-21 架构修正：磁轴算法放在 CH585

后续架构以这个结论为准：

- CH585 负责 ADS7948/MUX 采样和最终磁轴按键判断。
- H417 不再作为正常运行时的 raw ADC 磁轴算法主处理器。
- H417 负责轮询两颗 CH585、校验帧、合并按键状态、通过 USBHS 上报。

原因：

- 当前 H417-CH585 SPI 链路实测稳定点约为 16 MHz。
- 自动训练结果显示 19.2 MHz 基本失败，24 MHz 以上完全失败。
- 如果每帧都把 64 路 raw ADC 从每颗 CH585 发给 H417，会明显增加 SPI 带宽压力。
- 将磁轴算法放到 CH585 后，正常帧可以继续保持短帧：`down_bits[8]` + flags + seq + CRC。

因此，本文件里原本提到的 `firmware/common/magnetic_key_engine.*` 应降级为：

- 算法参考实现；
- PC/H417 侧仿真或测试模型；
- 后续可迁移到 CH585 的逻辑原型。

它不应作为当前产品架构中的 H417 常规运行时最终算法。

推荐接入路径也相应调整：

1. CH585 先接入 `ads7948.*`，验证单颗 ADS7948 固定通道 raw code。
2. CH585 再接入 `ch585_ads7948_mux_scan.*`，验证 64 键扫描 raw code。
3. 在 CH585 内部完成滤波、标定、滞回、Rapid Trigger/最终按下判断。
4. CH585 继续用当前短帧输出 `down_bits[8]` 给 H417。
5. raw ADC/position 数据只作为低速调试或校准帧，不作为高频正常帧。
