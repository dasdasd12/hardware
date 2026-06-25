# AI_keyb_wch Hardware

本仓库保存硬件侧主线固件、随固件管理的芯片底层依赖和本地自动化脚本。

`main` 保持主线固件结构，不混入临时 bring-up 输出；硬件单项测试放在 `hardware-test` / `origin/test` 分支维护。硬件资料、规格书、原理图、LaTeX 和完整项目文档优先放在 [`dasdasd12/Docs-For-AI-Keyboard`](https://github.com/dasdasd12/Docs-For-AI-Keyboard)。

## 关联仓库

- [`dasdasd12/software`](https://github.com/dasdasd12/software): 本项目的软件侧关联仓库。
- [`dasdasd12/Docs-For-AI-Keyboard`](https://github.com/dasdasd12/Docs-For-AI-Keyboard): 项目文档、原理图、规格书和设计资料主仓库。

## 目录原则

- 顶层不维护通用 `basic/`。底层依赖必须跟随实际使用它的芯片固件目录。
- H417 相关内容放在 `firmware/h417/`，H417 使用的 WCH 底层库放在 `firmware/h417/basic/wch/SRC/`。
- CH585 相关内容进入 `firmware/ch585/`，CH585 底层依赖放在 `firmware/ch585/basic/` 下。
- 项目自写驱动放在对应固件目标内部，例如 `firmware/h417/v3f/drivers/` 或后续 `firmware/ch585/.../drivers/`。
- 单项硬件测试分支可以引用对应固件目录中的底层依赖和自写驱动，但不拥有芯片基础库。
- `docs/` 只保留早期设计和调研资料；芯片手册、屏幕规格书等用户手册不放在本仓库。

## 当前文件树

```text
hardware/
├── .gitignore                               # 忽略构建产物、日志、临时 dump 和本地工具缓存
├── docs/
│   ├── architecture/                         # 早期架构方案，暂不作为当前实现依据
│   ├── pre_design_report/                    # 早期调研报告和图片
│   └── development_plan.md                   # 早期开发计划，暂不维护
│
├── firmware/
│   ├── h417/
│   │   ├── Makefile                          # H417 双核统一构建入口
│   │   ├── flash_dualcore.ps1                # H417 双核烧录脚本
│   │   ├── unlock.tcl                        # OpenOCD 辅助脚本
│   │   ├── wch-dual-core.cfg                 # 双核 OpenOCD 配置
│   │   ├── wch-v5f-only.cfg                  # V5F OpenOCD 配置
│   │   ├── basic/
│   │   │   └── wch/
│   │   │       └── SRC/                      # H417 固件本地依赖的 WCH 底层库
│   │   │           ├── Core/
│   │   │           ├── Debug/
│   │   │           ├── Ld/
│   │   │           ├── Peripheral/
│   │   │           └── Startup/
│   │   ├── v3f/
│   │   │   ├── Makefile                      # V3F 固件构建入口
│   │   │   ├── applications/                 # V3F 应用入口
│   │   │   ├── bsp/                          # 启动文件、系统初始化和链接脚本
│   │   │   │   └── linker_scripts/
│   │   │   └── drivers/                      # V3F 固件侧自写驱动
│   │   │       ├── gd5f1g_spi_nand/          # GD5F1G SPI-NAND 驱动
│   │   │       │   ├── include/
│   │   │       │   └── src/
│   │   │       └── rgb1w_pioc/               # PIOC RGB 1-wire / WS2812 驱动
│   │   │           ├── include/
│   │   │           └── src/
│   │   └── v5f_rtthread/
│   │       ├── Makefile                      # V5F RT-Thread 构建入口
│   │       ├── applications/                 # V5F 应用入口
│   │       ├── bsp/                          # V5F 板级支持
│   │       │   └── linker_scripts/
│   │       ├── drivers/                      # RT-Thread 设备驱动适配
│   │       ├── libcpu/                       # RISC-V CPU 适配
│   │       ├── rt-thread/                    # RT-Thread 源码
│   │       ├── tools/                        # V5F 相关检查/读取脚本
│   │       ├── rtconfig.h                    # RT-Thread 配置
│   │       └── usb_config.h                  # USB 配置
│   └── ch585/
│       ├── basic/
│       │   └── wch/
│       │       ├── SRC/                      # CH585 本地依赖的 WCH 底层库
│       │       │   ├── Ld/
│       │       │   ├── RVMSIS/
│       │       │   ├── Startup/
│       │       │   └── StdPeriphDriver/
│       │       │       └── inc/
│       │       └── LIB/                      # CH585 BLE/RF 相关 WCH 二进制库和头文件
│       ├── common/                           # CH585 侧通用磁轴算法
│       ├── frontend/                         # CH585 前端采样、ADS7948 和 MUX 代码
│       ├── spi_slave_test/                   # CH585 SPI slave bring-up 固件目标
│       ├── legacy/                           # CH585 BLE/RF/USB 历史源码快照，不携带重复 SDK
│       └── tools/                            # CH585 本地调试工具
│
├── skills/
│   ├── pdf-reader/                           # 本地 PDF 读取辅助 skill
│   └── wch-mrs-automation/                   # WCH/MounRiver 自动化脚本和说明
│
├── read_serial.ps1                           # 本地串口读取辅助脚本
└── README.md
```

`build/`、`.wch-skill-logs/`、`.tmp_pdf_text/`、OpenOCD 日志和临时 dump 文件由 `.gitignore` 忽略，不属于仓库结构。

## 构建入口

根目录不维护通用 Makefile。按芯片和固件目标进入对应目录构建：

```bash
# H417 双核固件
make -B -C firmware/h417
make -B -C firmware/h417 v3f
make -B -C firmware/h417 v5f
make -C firmware/h417 clean

# H417 烧录
make -C firmware/h417 flash

# CH585 SPI slave bring-up
make -B -C firmware/ch585/spi_slave_test
```

## 维护规则

- 新芯片进入仓库时先建立 `firmware/<chip>/` 边界，再放该芯片自己的 `basic/`、固件目标、烧录脚本和工具。
- 新驱动优先放到实际拥有它的固件或测试目录；不要放回顶层 `basic/`。
- 单项硬件测试留在 `hardware-test` / `origin/test` 分支，不合入 `main`。
- CH585 legacy 目录只保留手写历史源码；不要在 legacy 内重复放 WCH SDK 或 MounRiver 元数据。
- 移动文件时同步更新 README、Makefile 和自动化脚本。
- 不提交构建产物、日志、临时 dump、工具缓存、规格书 PDF 和本地手册归档。
- 修改 RT-Thread 或 WCH 底层库前先确认是否确实需要改 vendor 代码。

## 提交规范

提交前先看 `git status --short`，确认没有误带本地日志、构建产物或其他分支的未完成改动。

提交信息使用简短英文，推荐格式：

```text
type(scope): summary
```

示例：

```text
chore(h417): move basic library under firmware
chore(ch585): add firmware-owned basic library
docs: document firmware-owned basic layout
```
