# 嵌赛工作区 Git 仓库功能与入口总览

本文整理 `F:\嵌赛` 下已经拉取的 Git 仓库。当前识别到 3 个仓库：

| 仓库 | 远端 | 当前定位 |
| --- | --- | --- |
| `hardware` | `https://github.com/dasdasd12/hardware.git` | CH32H417 双核硬件/固件 bring-up 与新架构验证 |
| `keyboard_USBHS` | `https://github.com/dasdasd12/keyboard_USBHS.git` | 早期磁轴分体键盘固件原型，已有较完整的键盘扫描和 HID 输出 |
| `software` | `https://github.com/dasdasd12/software.git` | PC 侧 Local Core Service，负责 AI Agent、键盘配置、设备抽象和本地 API |

说明：本文基于本地仓库文件静态阅读整理，没有重新编译固件或连接硬件验证。

## 1. `hardware`

### 项目定位

`hardware` 是新的 CH32H417 键盘 AI 终端仓库，目标是把 CH32H417 双核 MCU、屏幕 UI、键盘引擎、USB 设备协议和 CH585 无线协处理器整合起来。

仓库文档里的最终产品形态是：

- CH32H417 V5F 跑 RT-Thread，负责应用、UI、USB 协议、配置管理和诊断。
- CH32H417 V3F 做实时键盘引擎，负责扫描、滤波、磁轴算法和运行时输入意图。
- CH585 负责 BLE HID、2.4G 私有协议、无线状态和非键盘输入协处理。
- PC 软件通过 USB Vendor HID 或后续 BLE/2.4G 通道配置设备。

### 当前已有功能

- 已移植 RT-Thread 到 CH32H417 V5F。
- 已有自定义启动代码、链接脚本、RISC-V 上下文切换和中断处理。
- 已有 UART8 调试控制台和 GPIO 驱动。
- 已集成 CherryUSB，并在当前代码中重点调试 USBSS/USBFS CDC loopback。
- V3F 侧已有唤醒 V5F、USBSS PLL/PHY 初始化、USBSS 链路探测、USBFS 时钟初始化等 bring-up 代码。
- V3F 侧已有 Hall ADC 初始化和 DMA 扫描基础函数，但完整磁轴矩阵扫描引擎仍是 TODO。
- 文档中已经把目标架构从早期“以太网/WebSocket 上板”收敛为“USB Vendor HID 控制面 + 配置读写 + 设备持久化 + V5F/V3F RuntimeTable 下发”。

### 主要入口

| 入口 | 作用 |
| --- | --- |
| `hardware\README.md` | 项目总体介绍、硬件/软件架构、当前进展、编译烧录说明 |
| `hardware\rtthread_port\applications\main.c` | V5F RT-Thread 主入口，初始化 USB CDC 测试和心跳 |
| `hardware\rtthread_port\applications\usb_cdc_dual.c` | USBSS/USBFS/USBHS CDC 描述符、端点、初始化和 loopback 逻辑 |
| `hardware\rtthread_port\v3f_wakeup\main.c` | V3F 裸机入口，初始化时钟、USBSS PLL/PHY，唤醒 V5F |
| `hardware\rtthread_port\v3f_wakeup\adc.c` | V3F Hall ADC 和 DMA 采样基础 |
| `hardware\rtthread_port\bsp\board.c` | RT-Thread 板级初始化 |
| `hardware\rtthread_port\drivers\drv_usart.c` | UART8 串口驱动 |
| `hardware\rtthread_port\drivers\drv_gpio.c` | GPIO/EXTI 驱动 |
| `hardware\docs\architecture\hardware_architecture.md` | 当前硬件产品架构，说明 H417/V3F/CH585 分工 |
| `hardware\docs\architecture\usb_vendor_hid_architecture.md` | 目标 USB Vendor HID 控制面设计 |
| `hardware\docs\architecture\device_protocol.md` | 设备协议设计 |

### 当前未完成或待接入

- V3F 完整磁轴矩阵扫描、滤波、RuntimeIntent 输出。
- V5F 到 V3F 的 RuntimeTable 下发和共享 SRAM/IPC 闭环。
- USB Vendor HID 控制面。
- DeviceSettings/ProfilePackage 槽位读写。
- 外部 Flash 持久化。
- 屏幕 UI/LVGL。
- CH585 SPI ingest 和无线协同。

## 2. `keyboard_USBHS`

### 项目定位

`keyboard_USBHS` 是早期的“Clear & Fail”多功能 HID 复合设备项目，目标是基于沁恒 MCU 做高性能磁轴分体双键盘。

这个仓库比 `hardware` 更接近一个已有键盘原型：它已经包含 CH32F207 版本、CH585 版本、2.4G 接收器和 PC 调试工具。

### 当前已有功能

- F207 侧已有主循环：ADC 扫描、触发算法处理、NKRO 键盘报告生成、USB HID 发送。
- 支持 9 个 Hall 磁轴按键的 ADC 采集。
- 支持键盘 NKRO 报告和 Consumer Control 媒体键报告。
- 支持编码器旋转映射到音量加减，编码器按键映射到静音。
- 支持 `Swich` 按键映射到 F14。
- 支持 USB EP2 配置命令，用于设置轮询率、触发模式、死区、校准值、按键映射等。
- 支持 ADC 实时监控数据上报，PC 工具可以读取 ADC 原始值。
- 已有配置系统、Flash 存储接口、自适应校准模块。
- CH585 侧已有 BLE HID、USB HID、RF 私有传输、模式管理、LED、调试串口等模块。
- 2.4G 接收器侧已有 RF 接收、NKRO 转 Boot 键盘报告、USB FS HID 输出。
- `PC_Tool` 下有多种 Python 调试工具，例如实时监控、按键测试、配置写入、固件测试。

注意：仓库 README 仍把“三种触发模式完整实现、Flash 存储、PC GUI、三模设计”等列为路线图项。代码里已经有不少接口和局部实现，但整体产品化还没完成。

### 主要入口

| 入口 | 作用 |
| --- | --- |
| `keyboard_USBHS\README.md` | 项目介绍、键盘配列、配置说明、路线图 |
| `keyboard_USBHS\debug_guide.md` | 调试指南 |
| `keyboard_USBHS\docs\ARCHITECTURE.md` | 计划中的 Electron 配置器架构 |
| `keyboard_USBHS\f207\User\Main.c` | CH32F207 主入口，核心扫描循环和 USB 报告发送 |
| `keyboard_USBHS\f207\User\Algo\algo_trigger.c` | 磁轴触发算法 |
| `keyboard_USBHS\f207\User\Hal\hal_adc.c` | ADC 扫描和采样滤波 |
| `keyboard_USBHS\f207\User\Hal\hal_gpio.c` | GPIO、编码器、按键读取 |
| `keyboard_USBHS\f207\User\Config\config_system.c` | 配置系统和 USB 配置命令处理 |
| `keyboard_USBHS\f207\User\Config\config_calibration.c` | 自适应校准 |
| `keyboard_USBHS\f207\User\USB_Device\ch32_usbhs_device.c` | USB HS HID 设备栈 |
| `keyboard_USBHS\ch585\CH585M\src\Main.c` | CH585 主入口，扫描、报告、BLE/RF/USB 模式调度 |
| `keyboard_USBHS\ch585\CH585M\src\Mode\mode_manager.c` | 有线、BLE、2.4G 模式管理 |
| `keyboard_USBHS\ch585\CH585M\src\Mode\rf_transport.c` | 2.4G 发送端 |
| `keyboard_USBHS\ch585\receiver\rf_receiver_main.c` | 2.4G USB 接收器主入口 |
| `keyboard_USBHS\PC_Tool\realtime_monitor.py` | PC 端 ADC 实时监控工具 |
| `keyboard_USBHS\PC_Tool\key_tester.py` | PC 端按键测试工具 |
| `keyboard_USBHS\PC_Tool\flash_profile_writer.py` | PC 端配置/Flash 写入辅助 |

### 当前未完成或待打磨

- 完整三模产品闭环。
- PC 配置工具正式 GUI，README 中计划从旧 Flet 方案切到 Electron。
- Flash 存储的稳定性和配置版本迁移。
- 高级功能：多击、组合键、宏、完整按键层。
- 触发算法参数和自适应校准仍需实机打磨。

## 3. `software`

### 项目定位

`software` 是 CH32H417 AI Keyboard 的 PC 侧软件仓库。当前核心是 Python Local Core Service MVP，用来把本地 UI、键盘配置、设备传输抽象、Claude Code/Codex 等 AI Agent CLI 串起来。

它不是固件仓库，而是以后桌面应用和键盘设备之间的本地中枢。

### 当前已有功能

- Local Core Service WebSocket API。
- 本地客户端 `hello` 握手、token、origin 校验、client identity 和 capability gate。
- 结构化 command/event/snapshot 模型。
- Agent 会话管理：启动、恢复、中断、关闭、会话列表。
- Claude Code/Codex 适配器骨架，其中当前文档把 Claude Code 作为主要参考 provider，Codex 作为兼容/回归路径。
- 权限请求处理：permission request、permission response、ACK、forwarded 证据模型。
- Claude Code hook 和 Codex RPC 相关本地 API 消息处理。
- SQLite 持久化：profile、device、session、run、permission history、approval policy、UI preferences 等。
- 键盘 profile/keymap/lighting/backend validation。
- DeviceTransport 抽象、SimulatedTransport、虚拟设备、虚拟输入、slot mapping、设备投影。
- diagnostics/health 检查和大量 pytest 测试。
- smoke 脚本用于本地 API、真实 agent、foreground approval、virtual input 等场景验证。

### 主要入口

| 入口 | 作用 |
| --- | --- |
| `software\README.md` | 项目说明、快速启动、当前状态、测试方式 |
| `software\src\bridge\server.py` | Local Core Service 主入口和 WebSocket API 处理 |
| `software\src\bridge\config.yaml` | Local Core Service 配置 |
| `software\src\bridge\agent_proxy.py` | Agent 进程/事件代理 |
| `software\src\bridge\session_manager.py` | Agent session 缓存和状态管理 |
| `software\src\agents\runtime.py` | Agent 生命周期运行时 |
| `software\src\agents\commands.py` | 结构化 Agent 命令处理 |
| `software\src\agents\foreground_cli.py` | 前台 CLI 启动命令构造 |
| `software\src\devices\device_transport.py` | DeviceFrame、DeviceCapabilities、SimulatedTransport |
| `software\src\devices\protocol_codec.py` | 设备协议 codec |
| `software\src\devices\projection_runtime.py` | Agent/设备状态投影到设备帧 |
| `software\src\keyboard\profile.py` | 键盘 Profile 数据模型和校验 |
| `software\src\keyboard\compiler.py` | Profile 编译为设备可下发 payload |
| `software\src\persistence\repositories.py` | SQLite app store 和 repository |
| `software\scripts\monitor-bridge.ps1` | 本地服务健康检查 |
| `software\scripts\local-api-smoke.py` | Local API smoke 测试 |
| `software\scripts\local-agent-cli.py` | 本地前台 Agent CLI 辅助入口 |
| `software\docs\architecture\software_architecture.md` | 软件侧产品架构 |
| `software\docs\architecture\implementation_status_v1.md` | 当前后端实际实现状态 |

### 当前未完成或待接入

- 正式桌面前端或浏览器 UI。
- 打包后的服务生命周期和安装器。
- 真实 USB HID、CDC、BLE、2.4G 设备 transport。
- 与 `hardware` 固件中的真实 Vendor HID/设备协议闭环。
- 物理键盘输入目前还主要通过 simulator/virtual input 表示。

## 4. 三个仓库之间的关系

可以把它们理解成三个阶段/三条线：

```text
keyboard_USBHS
  早期可用键盘固件原型
  已验证磁轴扫描、HID、配置命令、BLE/RF 方向

hardware
  新硬件平台迁移和底层 bring-up
  目标是 CH32H417 双核 + 屏幕 + CH585 + Vendor HID 控制面

software
  PC 侧本地核心服务
  目标是桌面 UI、Agent 控制、键盘配置、设备协议和持久化
```

当前整体状态：

- `keyboard_USBHS` 更像旧原型，功能最贴近“键盘能扫描、能发 HID”。
- `hardware` 是新平台迁移，重点在 CH32H417 双核、USB、RT-Thread 和架构文档。
- `software` 是 PC 侧服务骨架，测试和后端模块较完整，但还没接真实硬件 transport。

下一步最关键的打通链路通常是：

```text
software Local Core Service
  -> USB Vendor HID / CDC 真实 transport
  -> hardware V5F 设备协议
  -> V5F 配置管理/Flash
  -> RuntimeTable 下发给 V3F
  -> V3F 磁轴扫描行为真的变化
```
