# CH32H417 键盘 AI 终端

一个基于沁恒 CH32H417 双核 RISC-V MCU 的桌面 AI Agent 监控伴侣。它将 2.4 寸全彩大屏、三模磁轴键盘与 AI Agent 控制界面融为一体，让开发者无需切换窗口即可实时监控 Claude Code、Codex CLI 等 AI 编程助手的执行状态、审批权限请求、查看终端日志。

> **状态**: Phase 1 早期开发中 —— RT-Thread 已移植，USB3.0 HID 复合设备已跑通

---

## 硬件架构

```
┌─────────────────────────────────────────────────────────────┐
│                        系统拓扑图                            │
├──────────────────────┬──────────────────────────────────────┤
│     CH32H417         │          CH585（无线协处理器）         │
│  ┌────────────────┐  │    ┌──────────────────────────────┐  │
│  │ V5F@400MHz     │  │    │ RISC-V3C 78MHz               │  │
│  │ V3F@150MHz     │  │    │ 512KB Flash / 128KB SRAM     │  │
│  │ 896KB SRAM     │  │    │ BLE 5.4 + 2.4G(8K)           │  │
│  │ 64MB SDRAM     │  │    └──────────────────────────────┘  │
│  ├────────────────┤  │                     │                │
│  │ USB3.0 Device  │──┼──USB3.0──► PC（默认8K，32K待开发验证） │
│  │ 450MB/s        │  │                     │                │
│  ├────────────────┤  │              ┌──────┴──────┐         │
│  │ USB2.0 HS Host │──┼─USB HS 480Mbps         BT射频         │
│  │ （连接CH585）   │  │                     │                │
│  ├────────────────┤  │              ▼              ▼        │
│  │ 百兆以太网      │──┼─► PC/局域网桥接服务器 ─► AI Agent服务 │
│  ├────────────────┤  │                                      │
│  │ LTDC 800x480   │──┼─► 2.4寸 RGB屏幕（可竖向安装）          │
│  ├────────────────┤  │                                      │
│  │ ADC + MUX      │──┼─◄ 磁轴矩阵 / EC11旋钮 / 摇杆          │
│  ├────────────────┤  │                                      │
│  │ GPIO + SPI     │──┼─► WS2812 RGB灯                       │
│  ├────────────────┤  │                                      │
│  │ QSPI Flash     │  │ ≥1Gbit，容量待定                      │
│  ├────────────────┤  │                                      │
│  │ FMC 64MB SDRAM │  │                                      │
│  └────────────────┘  │                                      │
└──────────────────────┴──────────────────────────────────────┘
```

### 关键参数

| 组件 | 规格 |
|:---|:---|
| 主控 | CH32H417QEU6, V5F 乱序双发射 400MHz + V3F 顺序单发 150MHz |
| 算力 | 2292 CoreMark @400MHz (5.73 CoreMark/MHz) |
| 内存 | 128KB ITCM + 256KB DTCM + 512KB 共享 SRAM + 64MB 外接 SDRAM |
| 显示 | 2.4" 800x480 RGB LCD, LTDC + GPHA 2D 加速 |
| 有线连接 | USB3.0 OTG (450MB/s) + 百兆以太网 MAC+内置 PHY |
| 无线连接 | CH585 协处理器: BLE 5.4 + 2.4G 私有协议 |
| 键盘 | 磁轴矩阵，支持 RT / DKS / SpeedTap / SOCD / 模拟输入 |
| 扩展 | EC11 旋钮、摇杆、WS2812 灯带、温湿度/光照/IMU 传感器 |

---

## 软件架构

```
┌────────────────────────────────────────┐
│ 应用层：AI终端界面 / 本地命令行          │  ← V5F
├────────────────────────────────────────┤
│ 路由层：HID输出 / 无线发送 / 本地分发     │  ← V5F
├────────────────────────────────────────┤
│ 逻辑层：键值映射 / 层切换 / 宏 / DKS    │  ← V5F
├────────────────────────────────────────┤
│ 磁轴引擎：RT / 触发花样 / 模拟输入       │  ← V5F
├────────────────────────────────────────┤
│ IPC Mailbox ←── 共享SRAM ──►           │
├────────────────────────────────────────┤
│ 采集层：MUX扫描 / ADC / DMA / 校准      │  ← V3F
├────────────────────────────────────────┤
│ 设备层：GPIO / USB3.0 OTG / USB HS Host │  ← V3F/V5F
├────────────────────────────────────────┤
│ 扩展层：EC11 / 摇杆ADC / WS2812 / I2C   │  ← V3F
└────────────────────────────────────────┘
```

### 技术栈

| 层级 | 选型 | 说明 |
|:---|:---|:---|
| RTOS | RT-Thread Standard (V5F) + Nano/裸机 (V3F) | 官方 BSP 支持，最强 GUI 生态 |
| 图形 | LVGL v9 | 预估 800x480 下 45-60 FPS |
| 网络 | lwIP + mbedtls | 百兆以太网，TLS 1.2/1.3 |
| AI 通信 | WebSocket + JSON Lines | 连接桥接服务器 |
| USB 栈 | CherryUSB | HID+CDC 复合设备 |
| 配置软件 | WebHID 网页驱动 | 免安装、跨平台 |
| 脚本生态 | MicroPython / Lua | 利用 64MB SDRAM 承载运行时 |

---

## 当前进展

- [x] RT-Thread Standard 移植到 CH32H417 V5F
- [x] 自定义启动代码、链接脚本、RISC-V 上下文切换与中断处理
- [x] UART8 串口驱动（调试控制台）与 GPIO 驱动
- [x] CherryUSB 集成：USB3.0 HID + CDC 复合设备
- [x] 评估板验证：按键触发 HID 键盘报告发送，LED 心跳指示
- [ ] V3F 核启动与 AMP 双核分工验证
- [ ] SDRAM 初始化与帧缓冲配置
- [ ] 终端模拟器引擎
- [ ] lwIP 网络协议栈
- [ ] WebSocket 客户端
- [ ] 桥接服务器原型
- [ ] LVGL 图形界面
- [ ] 磁轴引擎与键盘矩阵扫描

---

## 快速开始

### 环境准备

1. **工具链**: 沁恒 RISC-V 工具链 `riscv-wch-elf-gcc`
   - 默认路径: `/c/riscv-wch-toolchain/bin/`
   - 或安装 [MounRiver Studio 2](http://www.mounriver.com/)

2. **WCH 底层库**: 仓库内已保留主固件使用的 CH32H417 EVT 底层，路径为 `basic/ch32h417/wch/SRC`

3. **烧录工具**: WCH-Link + OpenOCD，或 MounRiver Studio 内置下载

### 编译

```bash
# 编译 H417 双核固件（V3F + V5F）
cd firmware/h417
make

# 或使用顶层 Makefile（支持 CH32H417 / CH585 双芯片）
cd ../..
make CHIP=CH32H417
```

### 烧录

```bash
# OpenOCD + WCH-Link
cd firmware/h417
make flash

# 或使用 WCH 官方工具
wchisp -f build/V5F/rtthread_ch32h417_V5F.bin
```

### 验证

连接串口（默认 UART8，115200 8N1），上电后应看到：

```
Hello, RT-Thread on CH32H417 V5F!
Initializing USB3.0 HID+CDC Composite Device...
USB device initialized.
```

按评估板按键 PB0，USB HID 将发送按键 'a'，同时 LED（PB1）闪烁。

---

## 项目结构

```
.
├── docs/
│   ├── pre_design_report/          # 三份深度预研报告
│   │   ├── report_1_hardware_architecture.md
│   │   ├── report_2_network_ai_integration.md
│   │   └── report_3_software_product.md
│   ├── user_manual/                # 原厂数据手册 PDF
│   │   ├── CH32H417DS0.PDF         # 数据手册
│   │   ├── CH32H417RM.PDF          # 参考手册
│   │   └── KD024WVFPD102A SPEC V1.0.pdf  # 屏幕规格书
│   └── development_plan.md         # 产品开发计划
│
├── basic/
│   └── ch32h417/
│       ├── drivers/
│       │   └── rgb1w_pioc/         # PIOC RGB 1-wire/WS2812 可复用驱动
│       └── wch/SRC                 # CH32H417 WCH EVT 底层库
│
├── firmware/
│   └── h417/                       # CH32H417 双核固件总目录
│       ├── v3f/                    # V3F 固件入口、采集和协同任务
│       ├── v5f_rtthread/           # V5F RT-Thread 主固件
│       ├── build/                  # V3F/V5F 构建产物
│       ├── flash_dualcore.ps1      # 双核烧录脚本
│       └── Makefile                # 双核统一构建入口
│
├── hw_tests/
│   ├── h417/                       # CH32H417 单项硬件测试
│   └── ch585/                      # CH585 单项硬件测试
│
├── tools/                          # 跨工程检查脚本
├── skills/                         # 本仓库自动化技能/脚本
│
├── Makefile                        # 顶层 Makefile
└── README.md                       # 本文件
```

---

## 文档索引

| 文档 | 内容 |
|:---|:---|
| [docs/pre_design_report/report_1_hardware_architecture.md](docs/pre_design_report/report_1_hardware_architecture.md) | 硬件平台深度解析、RTOS 选型对比、性能瓶颈分析 |
| [docs/pre_design_report/report_2_network_ai_integration.md](docs/pre_design_report/report_2_network_ai_integration.md) | 网络架构、TLS/mbedTLS、AI Agent 协议栈、桥接服务器方案 |
| [docs/pre_design_report/report_3_software_product.md](docs/pre_design_report/report_3_software_product.md) | 键盘固件架构、磁轴引擎、配置软件调研、AI 功能嵌入、传感器扩展 |
| [docs/development_plan.md](docs/development_plan.md) | 分阶段实施路径、里程碑、风险登记册、资源需求 |

---

## 产品路线

| 阶段 | 周期 | 核心目标 |
|:---|:---|:---|
| **Phase 1: MVP 验证** | 第 1-3 月 | 终端模拟器 + WebSocket + 桥接服务器，验证"AI Agent 硬件监控"假设 |
| **Phase 2: GUI + AI 控制** | 第 4-5 月 | LVGL 图形化界面 + 13 状态 FSM + AI Agent 快捷操作 |
| **Phase 3: 生态 + 扩展** | 第 6-7 月 | MicroPython 脚本 + 传感器扩展 + 社区功能 |

详见 [docs/development_plan.md](docs/development_plan.md)。

---

## 许可

- 本项目固件代码采用 [Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0) 许可证（与 RT-Thread 一致）。
- CH32H417 原厂 SDK 和驱动遵循沁恒微电子相关许可条款。

## 致谢

- [RT-Thread](https://www.rt-thread.io/) — 优秀的国产开源实时操作系统
- [沁恒微电子 (WCH)](https://www.wch.cn/) — CH32H417 及 RISC-V 工具链
- [CherryUSB](https://github.com/cherry-embedded/CherryUSB) — 轻量级 USB 协议栈
