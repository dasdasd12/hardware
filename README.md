# AI_keyb_wch Hardware

本仓库保存硬件侧资料、CH32H417/CH585 固件、底层库和本地自动化脚本。

README 只描述仓库维护规则和当前文件结构。`docs/` 下多为早期方案和历史资料，当前阶段不作为实现依据；引用前需要重新核对。

## 关联仓库

- [`dasdasd12/software`](https://github.com/dasdasd12/software): 本项目的软件侧关联仓库。
- [`dasdasd12/Docs-For-AI-Keyboard`](https://github.com/dasdasd12/Docs-For-AI-Keyboard): 本项目所需文档的主仓库，查阅原理图、LaTeX、规格书和设计资料时优先使用该仓库。

## 提交规范

### 协作约定

- 不在同一个提交里混入无关改动。比如 README 更新、V3F 文件树调整、WS2812 驱动修改应分别提交。
- 提交前先看 `git status --short`，确认没有误带本地日志、构建产物或别人的未完成改动。
- 构建入口以各子目录 Makefile 为准，不再维护根目录通用 Makefile。

### 提交信息

使用简短英文提交信息，推荐格式：

```text
type(scope): summary
```

常用类型：

- `feat`: 新增驱动能力或固件功能。
- `fix`: 修复编译、刷写、波形、引脚或路径问题。
- `chore`: 文件树整理、构建脚本调整、依赖位置调整。
- `docs`: README、架构说明、测试记录等文档更新。

示例：

```text
chore(h417-v3f): organize firmware layout
fix(h417-v3f): build ws2812 from v3f pioc driver
docs: update hardware repository structure
```

### 提交前检查

按改动范围选择最小必要验证：

```bash
# H417 双核固件
make -B -C firmware/h417

# H417 单核目标
make -B -C firmware/h417 v3f
make -B -C firmware/h417 v5f

# WCH/MounRiver 自动化脚本
powershell -ExecutionPolicy Bypass -File skills/wch-mrs-automation/tests/verify-wch-skill.ps1
```

构建产物、OpenOCD 日志、MounRiver 临时文件、本地 PDF 文本缓存不提交。

## 当前文件树

```text
hardware/
├── basic/
│   └── ch32h417/
│       └── wch/SRC/                         # CH32H417 WCH EVT 底层库
│
├── docs/
│   ├── architecture/                         # 早期架构方案，暂不作为当前依据
│   ├── pre_design_report/                    # 早期调研报告和图片
│   ├── user_manual/                          # 历史资料归档，引用前需核对
│   └── development_plan.md                   # 早期开发计划，暂不维护
│
├── firmware/
│   └── h417/
│       ├── Makefile                          # H417 双核统一构建入口
│       ├── flash_dualcore.ps1                # H417 双核烧录脚本
│       ├── unlock.tcl                        # OpenOCD 辅助脚本
│       ├── wch-dual-core.cfg                 # 双核 OpenOCD 配置
│       ├── wch-v5f-only.cfg                  # V5F OpenOCD 配置
│       ├── v3f/
│       │   ├── Makefile                      # V3F 固件构建入口
│       │   ├── applications/                 # V3F 应用入口
│       │   ├── bsp/                          # 启动文件、系统初始化、链接脚本
│       │   └── drivers/                      # V3F 固件侧驱动
│       │       └── rgb1w_pioc/               # PIOC RGB 1-wire / WS2812 驱动
│       └── v5f_rtthread/
│           ├── Makefile                      # V5F RT-Thread 构建入口
│           ├── applications/                 # V5F 应用入口
│           ├── bsp/                          # V5F 板级支持
│           ├── drivers/                      # RT-Thread 设备驱动适配
│           ├── libcpu/                       # RISC-V CPU 适配
│           ├── rt-thread/                    # RT-Thread 源码
│           └── tools/                        # V5F 相关检查/读取脚本
│
├── skills/
│   ├── pdf-reader/                           # 本地 PDF 读取辅助 skill
│   └── wch-mrs-automation/                   # WCH/MounRiver 自动化脚本和说明
│
├── read_serial.ps1                           # 本地串口读取辅助脚本
└── README.md
```

`build/`、`.wch-skill-logs/`、`.tmp_pdf_text/`、OpenOCD 日志和临时 dump 文件由 `.gitignore` 忽略，不属于仓库结构。

## 目录职责

### `basic/`

只放底层硬件库。当前保留 `basic/ch32h417/wch/SRC`，作为 H417 V3F 和 V5F 固件共同引用的 WCH EVT 底层库。

不要把板级业务驱动放回 `basic/`。例如 WS2812 使用的 PIOC RGB1W 驱动属于当前 H417 V3F 固件侧驱动，放在 `firmware/h417/v3f/drivers/rgb1w_pioc/`。

### `firmware/h417/`

CH32H417 双核固件主目录。

- `firmware/h417/Makefile` 是统一入口，负责转调 V3F 和 V5F。
- `v3f/` 表示 V3F 固件职责，不再用 `wakeup` 这类只描述启动片段的目录名。
- `v5f_rtthread/` 表示 V5F 的 RT-Thread 主固件。
- `build/V3F` 和 `build/V5F` 是构建产物目录，不提交。

V3F 子目录约定：

- `applications/`: V3F 应用入口和任务流程。
- `bsp/`: V3F 启动文件、系统初始化、链接脚本、芯片配置头。
- `drivers/`: V3F 独占或 V3F 首先验证的固件侧驱动。

V5F 子目录约定：

- `applications/`: V5F 应用和 bring-up 入口。
- `bsp/`: V5F 板级初始化、启动文件、链接脚本。
- `drivers/`: RT-Thread 设备驱动适配。
- `rt-thread/`: RT-Thread 源码，不做无关格式化或大范围重排。

### `docs/`

`docs/` 目前主要是早期架构设计、调研报告和历史资料归档，大部分内容已经落后于当前硬件验证和固件目录结构。

完整项目文档放在 [`dasdasd12/Docs-For-AI-Keyboard`](https://github.com/dasdasd12/Docs-For-AI-Keyboard)。当前开发以 `firmware/`、`basic/`、本 README 和关联文档仓库为准。暂时不维护 `docs/` 下的旧方案；如果后续需要引用其中内容，应先按当前原理图、LaTeX、芯片手册和已有固件重新核对。

### `skills/`

`skills/` 放本仓库专用自动化能力和参考资料。

脚本更新后应同步运行对应验证：

```bash
powershell -ExecutionPolicy Bypass -File skills/wch-mrs-automation/tests/verify-wch-skill.ps1
```

## 常用命令

```bash
# H417 双核固件
make -C firmware/h417
make -C firmware/h417 v3f
make -C firmware/h417 v5f
make -C firmware/h417 clean

# H417 烧录
make -C firmware/h417 flash

# WCH/MounRiver 自动化脚本检查
powershell -ExecutionPolicy Bypass -File skills/wch-mrs-automation/tests/verify-wch-skill.ps1
```

## 维护原则

- 新驱动优先放到实际拥有它的固件目录；只有芯片原厂底层库放 `basic/`。
- 硬件验证内容先走 `test` 分支，不在 `main` 混入临时 bring-up 测试。
- 移动文件时同步更新 README、Makefile、检查脚本和自动化脚本。
- 不提交构建产物、日志、临时 dump、工具缓存。
- 修改 RT-Thread 或 WCH EVT 底层库前先确认是否确实需要改 vendor 代码。
