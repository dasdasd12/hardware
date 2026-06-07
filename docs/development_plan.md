# CH32H417 键盘 AI 终端 — 产品开发计划

> **版本**: v1.0
> **日期**: 2026-05-18
> **周期**: 6-7 个月，分 3 个阶段推进
> **核心目标**: 验证并交付"桌面 AI Agent 硬件监控伴侣"的最小可行产品，随后迭代至图形化 GUI 与开源生态。

---

## 1. 产品愿景与里程碑总览

### 1.1 产品愿景

为每天使用 Claude Code、Codex CLI、Cursor 等 AI 编程工具的专业开发者，打造一款桌面固定使用的硬件伴侣。它通过 2.4 寸全彩大屏实时显示 AI Agent 的执行状态、终端日志和权限请求，让开发者无需切换窗口即可审批操作、监控进度，从根本上消除"上下文切换"带来的效率损耗。

### 1.2 里程碑总览

| 阶段 | 时间 | 核心交付物 | 成功标准 |
|:---|:---|:---|:---|
| **Phase 1** MVP 验证 | 第 1-3 月 | 终端模拟器 + WebSocket 通信 + 桥接服务器原型 | 设备能实时显示 Codex/Claude 状态，支持一键审批权限 |
| **Phase 2** GUI + AI 控制 | 第 4-5 月 | LVGL 图形界面 + 13 状态 FSM + 快捷操作面板 | 图形化任务卡片、实时日志流、Fn+组合键快捷操作 |
| **Phase 3** 生态 + 扩展 | 第 6-7 月 | MicroPython + 传感器扩展 + 用户脚本 | 屏幕运行 Python REPL，传感器数据可视化，社区配置分享 |

---

## 2. Phase 1：MVP 验证（第 1-3 月）

Phase 1 的核心目标是在最短时间内构建可工作的最小可行产品（MVP），验证"AI Agent 硬件监控"这一产品假设。技术路线聚焦三项并行工作：基础系统启动、终端模拟器引擎、网络与 AI 通信栈。

### 2.1 里程碑 1.1：基础系统启动（第 1 个月）

**目标**: 建立稳定的双核开发基线，验证硬件最小系统。

| 任务 | 交付物 | 验收标准 | 风险 |
|:---|:---|:---|:---|
| V5F RT-Thread Standard 完整启动 | `main.c` 运行多线程 | Finsh Shell 可用，心跳线程正常 | 低 |
| V3F 裸机/RT-Thread Nano AMP 启动 | V3F 独立运行扫描循环 | V3F 能控制 GPIO 闪烁，与 V5F 通过 IPC Mailbox 通信 | **中** — 双核 AMP 适配文档较少 |
| SDRAM 初始化与帧缓冲配置 | 双缓冲帧地址映射 | SDRAM 读写测试通过，LTDC 输出彩条 | 低 |
| 串口调试环境完善 | UART8 控制台稳定 | 115200 波特率无乱码，支持 `rt_kprintf` 和 Finsh 命令 | 低 |
| 项目构建系统优化 | 统一 Makefile / VS Code 任务 | 一键编译、一键烧录、一键调试 | 低 |

**关键决策**: V3F 暂时以裸机运行键盘扫描循环，V5F 运行 RT-Thread Standard。后续评估是否迁移至 RT-Thread Nano。

### 2.2 里程碑 1.2：终端模拟器引擎（第 2 个月）

**目标**: 在屏幕上实现可用的文本终端，作为 MVP 的 UI 核心。

| 任务 | 交付物 | 验收标准 | 备注 |
|:---|:---|:---|:---|
| ANSI/VT100 终端模拟器移植 | `term/` 模块 | 支持光标移动、清屏、颜色、滚动 | 参考 ATMega/STM32F103 成熟实现，预估 45KB 代码 |
| 字体渲染与缓存 | 8x16 / 12x24 点阵字体 | DTCM 存放字体缓存，800x480 下 60 FPS 文本刷新 | 脏矩形优化，仅重绘变化区域 |
| 屏幕竖向安装适配 | 坐标旋转或竖屏 UI | 字体、边框、进度条在竖向布局下可读 | 产品形态倾向于竖屏 |
| 键盘本地输入路径 | 本地 Input 子系统 | 评估板按键可直接输入字符到终端 | 打通 V3F 扫描 → IPC → V5F 应用的全链路 |
| 板载快捷键操作 | Fn+组合键映射 | 支持切换配置、亮度调节等基础功能 | 不依赖 PC 端软件 |

**性能目标**: CPU 占用率 < 25%，文本刷新 > 60 FPS，端到端输入延迟 < 20ms。

### 2.3 里程碑 1.3：网络 + AI 通信（第 3 个月）

**目标**: 设备通过局域网连接桥接服务器，实时显示 AI Agent 状态。

| 任务 | 交付物 | 验收标准 | 备注 |
|:---|:---|:---|:---|
| lwIP 协议栈集成 | TCP/IP 可用 | DHCP 获取 IP，能 ping 通网关 | 利用 WCH SDK 已有示例 |
| WebSocket 客户端 | `ws_client/` 模块 | 成功握手并收发文本帧 | 基于 libwebsockets 或 lwIP 内置实现 |
| JSON Lines 解析与事件路由 | cJSON + 事件分发器 | 正确解析 `task_update`、`permission_request` 等事件 | 峰值内存 < 150KB |
| 13 状态 FSM 框架 | `ai_fsm.c` | IDLE → SUBMITTED → WORKING → ... 状态转换正确 | 支持非终止态与终止态分组判断 |
| 桥接服务器原型 | Python 脚本 | 能将 Codex `remote-control` / Claude `stream-json` 转为统一事件流 | 参考 Happy 项目三层架构 |
| Codex CLI 状态显示验证 | 端到端演示 | 终端实时显示 Codex 任务状态（运行中/等待/完成） | 一键批准/拒绝权限请求 |

**网络指标**: TCP 吞吐 > 70Mbps（优化后），WebSocket 首次连接 < 3s，会话恢复 < 1s。

**MVP 冻结标准**: 设备能在终端界面实时显示 AI Agent 状态，支持一键批准或拒绝权限请求，面向 3-5 位早期开发者收集反馈。

---

## 3. Phase 2：GUI + AI 控制（第 4-5 月）

Phase 2 将纯文本终端升级为图形化界面，并深度集成 AI Agent 控制能力。

### 3.1 里程碑 2.1：LVGL 图形界面（第 4 月上旬）

| 任务 | 交付物 | 验收标准 | 备注 |
|:---|:---|:---|:---|
| LVGL v9 集成到 RT-Thread | `lvgl/` 软件包 | 官方软件包配置一键启用 | RT-Thread 官方维护 LVGL 包 |
| 显示驱动适配 | LTDC + GPHA | 双缓冲无撕裂，RGB565 格式 | 预估 45-60 FPS（中等复杂度 UI） |
| 状态卡片 UI 组件 | 任务状态面板 | 显示 Agent 名称、任务进度、耗时、状态色标 | 蓝色脉动=WORKING，红色闪烁=WAITING_PERMISSION |
| 实时日志滚动视图 | 终端组件 | 支持彩色高亮、自动滚动、手势缩放 | 流式追加，内存上限 10KB |
| 确认弹窗 + 蜂鸣器 | 模态对话框 | 权限请求弹窗 + 蜂鸣器/LED 提醒 | 30 秒超时自动拒绝 |

**渲染性能**: 若 GPHA 可用，参考 DMA2D 将帧率从 15-20 FPS 提升至 40+ FPS；若不可用，V5F@400MHz 软件渲染保底 30+ FPS。

### 3.2 里程碑 2.2：AI Agent 控制（第 4 月下旬 - 第 5 月上旬）

| 任务 | 交付物 | 验收标准 | 备注 |
|:---|:---|:---|:---|
| Codex CLI 协议适配 | `agent_codex.c` | 支持 `codex remote-control` 和 `codex exec --json` | JSON-RPC 2.0 事件流 |
| Claude Code 协议适配 | `agent_claude.c` | 支持 `--output-format stream-json` | NDJSON 流，适配 `--sdk-url` WebSocket 模式 |
| MCP 协议层 | `agent_mcp.c` | 暴露设备硬件为 MCP Tools | 基于 EmbedMCP 或自研 C 实现 |
| 13 状态 FSM 完整实现 | 状态机 + UI 映射 | 所有状态转换正确，UI 实时同步 | 宏 `TASK_STATE_ACTIVE` / `TASK_STATE_TERMINAL` |
| 快捷操作绑定 | Fn+组合键 | Fn+F1=确认操作, Fn+F2=暂停, Fn+F3=查看任务 | 屏幕实时显示当前绑定 |
| 屏幕实时状态刷新 | < 50ms 端到端延迟 | 从消息接收到界面更新 < 50ms | cJSON 解析 + LVGL 脏矩形 |

### 3.3 里程碑 2.3：配置软件原型（第 5 月）

| 任务 | 交付物 | 验收标准 | 备注 |
|:---|:---|:---|:---|
| WebHID 网页驱动框架 | 单页应用（PWA） | Chrome/Edge 直接识别设备，无需安装 | WebHID API + WebSocket |
| 磁轴参数可视化调节 | 触发点/RT/DKS 编辑器 | 实时预览行程条、阈值线 | 对标 Wootility |
| 板载配置管理 | 16 套配置 + 导入导出 | 网页端编辑后一键写入键盘 | 配置持久化到 QSPI Flash |
| AI Agent 快捷面板（P0） | 配置软件内嵌 Agent 控制 | 显示任务列表、点击跳转详情、一键绑定按键 | 核心卖点 |

---

## 4. Phase 3：生态 + 扩展（第 6-7 月）

Phase 3 面向开源生态建设，通过脚本语言降低二次开发门槛。

### 4.1 里程碑 3.1：MicroPython 移植（第 6 月）

| 任务 | 交付物 | 验收标准 | 备注 |
|:---|:---|:---|:---|
| MicroPython 集成到 RT-Thread | `micropython/` 软件包 | 基于 CH32V307 先例移植 | 896KB SRAM 是 CH32V307 的 14 倍 |
| 标准库适配 | `json`, `re`, `collections` 等 | 纯 Python 模块可直接 import | 利用 64MB SDRAM 空间 |
| 屏幕 REPL 界面 | 交互式解释器 | 在 800x480 屏上运行 MicroPython REPL | 支持串口 + 屏幕双通道输入 |
| 用户脚本执行框架 | `scripts/` 目录 + 加载器 | 上电自动运行 `main.py`，支持事件钩子 | 脚本可操控 GPIO、屏幕、网络 |
| Lua 备选运行时 | `lua/` 模块 | 核心 25KB RAM，更轻量 | 适合资源敏感场景 |

### 4.2 里程碑 3.2：传感器扩展（第 6-7 月）

| 任务 | 交付物 | 验收标准 | 备注 |
|:---|:---|:---|:---|
| EC11 旋钮驱动完善 | 旋转编码器 + 按键 | 菜单导航、音量调节、参数微调 | 状态机处理格雷码 + 软件消抖 |
| 接近传感器唤醒 | VCNL4040 | 手部靠近 < 10cm 自动亮屏 | 深度休眠 → 浅休眠 → 活跃 三级功耗 |
| 环境光自适应亮度 | VEML7700 | 自动调节屏幕背光 | 暗光降低功耗，强光提升可读性 |
| 温湿度/气压显示 | BME280 | 屏幕显示环境数据 | 可编写脚本读取并上报 |
| IMU 手势交互 | MPU6050 | 摇晃唤醒/撤销、倾斜翻页、双击确认 | DMP 直接输出四元数 |
| I2S 麦克风 + 关键词唤醒 | INMP441 | 本地运行轻量级 KWS 算法 | 语音交互入口 |
| 多模态交互验证 | 综合演示 | 接近+语音+按键+手势协同工作 | 参考 Rabbit R1 多模态设计 |

### 4.3 里程碑 3.3：社区功能（第 7 月）

| 任务 | 交付物 | 验收标准 | 备注 |
|:---|:---|:---|:---|
| 配置分享平台 | 网页端配置库 | 用户上传/下载键盘配置，带评分和标签 | 对标 Wooting 社区 |
| 脚本市场框架 | 脚本仓库 + 加载器 | 一键安装社区脚本，自动解决依赖 | 类似 VS Code 扩展市场 |
| 完整文档与示例 | README + API 文档 + 示例代码 | 新开发者 30 分钟内完成第一个脚本 | 包含磁轴、屏幕、网络、传感器示例 |
| 开源发布准备 | GitHub Release + 许可证 | 代码清理、CI 构建、Release Note | Apache-2.0 许可证 |

---

## 5. 风险登记册

| 风险项 | 影响 | 概率 | 缓解策略 | 残余风险 |
|:---|:---:|:---:|:---|:---:|
| **GPHA 性能/SDK 示例不足** | 高 | 中 | ① 移植初期准备纯软件渲染 fallback，V5F@400MHz 预估 30+ FPS；② 主动联系 WCH 获取 GPHA 示例；③ 仅依赖已确认基础能力（填充/拷贝/PFC/Alpha） | 低 |
| **RISC-V 第三方库/中间件不足** | 中 | 高 | ① MicroPython/Lua 引入高级语言生态；② RT-Thread 450+ 软件包补充；③ 64MB SDRAM 直接移植 POSIX 兼容项目 | 低 |
| **AI Agent CLI API 不兼容变更** | 中 | 高 | ① 桥接服务器统一抽象，设备端仅消费标准 JSON Lines；② 支持多 Agent 后端插件化；③ 事件格式版本化 | 极低 |
| **RT-Thread 双核 AMP 适配延迟** | 中 | 中 | ① V5F 运行 RT-Thread Standard + V3F 裸机/RT-Thread Nano；② 自定义共享内存 + IPC 中断 | 低 |
| **SDRAM 信号完整性与带宽瓶颈** | 中 | 低 | ① 参考官方评估板 PCB；② RGB565 降低带宽至 46MB/s；③ 双缓冲分放不同 Bank | 极低 |
| **AI Agent 市场热度下降** | 高 | 中 | ① 同时支持通用 SSH 终端和系统监控，不绑定单一 Agent；② 开源社区驱动迭代 | 低 |

---

## 附录：术语表

| 术语 | 说明 |
|:---|:---|
| AMP | 非对称多处理（Asymmetric Multi-Processing），V5F 与 V3F 运行不同 OS/固件 |
| DKS | Dynamic Keystroke，动态键程，单键多触发点 |
| FSM | 有限状态机（Finite State Machine） |
| GPHA | Graphics Processing Hardware Accelerator，CH32H417 内置 2D 图形加速器 |
| IPC | 核间通信（Inter-Processor Communication） |
| LTDC | LCD-TFT Display Controller |
| MCP | Model Context Protocol，AI Agent 工具连接标准 |
| MVP | 最小可行产品（Minimum Viable Product） |
| RT | Rapid Trigger，抬起即重置的磁轴触发模式 |
| SOCD | Simultaneous Opposing Cardinal Directions，互斥方向键清理 |
| TCM | 紧耦合存储器（Tightly Coupled Memory），零等待访问 |

---

*本计划基于 [report_1_hardware_architecture.md](pre_design_report/report_1_hardware_architecture.md)、[report_2_network_ai_integration.md](pre_design_report/report_2_network_ai_integration.md)、[report_3_software_product.md](pre_design_report/report_3_software_product.md) 三份预研报告制定，将在每个 Sprint 结束后回顾并调整。*
# Deprecated Development Plan

This file is historical and appears to contain mojibake from an older plan. It
is not an implementation contract. Use
`docs/architecture/keyboard_config_site/` as the current architecture source,
and use `hardware/docs/architecture/` only after the conflicts there have been
aligned to that HTML architecture.
