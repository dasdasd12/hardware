# AI_keyb_wch Hardware

本仓库保存硬件侧资料、CH32H417/CH585 固件、单项硬件测试、底层库和本地自动化脚本。

README 只描述仓库维护规则和当前文件结构。产品方案、协议和长期架构设计放在 `docs/architecture/`、`docs/development_plan.md` 和 `docs/pre_design_report/` 中维护。

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

- `feat`: 新增硬件测试、驱动能力或固件功能。
- `fix`: 修复编译、刷写、波形、引脚或路径问题。
- `chore`: 文件树整理、构建脚本调整、依赖位置调整。
- `docs`: README、架构说明、测试记录等文档更新。

示例：

```text
chore(h417-v3f): organize firmware layout
fix(hw-tests): build ws2812 from v3f pioc driver
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

# H417 单项硬件测试
make -B -C hw_tests/h417 HW_TEST=h417_ws2812
make -B -C hw_tests/h417 HW_TEST=h417_lcd_signal

# CH585 单项硬件测试
make -B -C hw_tests/ch585 HALF=u2 TEST=ch585_u2_eeprom_i2c
make -B -C hw_tests/ch585 HALF=u3 TEST=ch585_u3_max17048_i2c

# 仓库级边界检查
python tools/check_hw_tests.py
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
│   ├── architecture/                         # 架构和协议设计文档
│   ├── pre_design_report/                    # 早期调研报告和图片
│   ├── user_manual/                          # 芯片、屏幕等原厂资料
│   └── development_plan.md                   # 开发计划
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
├── hw_tests/
│   ├── h417/
│   │   ├── Makefile                          # H417 单项硬件测试构建入口
│   │   ├── Link_h417_v3f.ld                  # H417 测试链接脚本
│   │   └── src/                              # H417 测试源码
│   └── ch585/
│       ├── Makefile                          # CH585 单项硬件测试构建入口
│       └── src/                              # CH585 测试源码
│
├── skills/
│   ├── pdf-reader/                           # 本地 PDF 读取辅助 skill
│   └── wch-mrs-automation/                   # WCH/MounRiver 自动化脚本和说明
│
├── tools/
│   └── check_hw_tests.py                     # 硬件测试边界检查
│
├── read_serial.ps1                           # 本地串口读取辅助脚本
└── README.md
```

`build/`、`.wch-skill-logs/`、`.tmp_pdf_text/`、OpenOCD 日志和临时 dump 文件由 `.gitignore` 忽略，不属于仓库结构。

## 目录职责

### `basic/`

只放底层硬件库。当前保留 `basic/ch32h417/wch/SRC`，作为 H417 V3F、V5F 和硬件测试共同引用的 WCH EVT 底层库。

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

### `hw_tests/`

硬件单项测试目录。每个测试应能单独构建、单独刷写、单独运行，避免为了测一个外设拉起整套系统。

H417 测试当前走 V3F 裸机构建，结果主要通过 SWD、示波器/逻辑分析仪、屏幕或灯效确认。CH585 测试通过 `TX1 PA9 / RX1 PA8` 串口输出状态。

测试边界：

- 不测 H417-CH585 SPI。
- 不测 ADS7948、CH585 内部 ADC。
- 不测 USB 上报。
- 不测蓝牙/2.4G 无线功能。
- 不测 SDRAM。

### `docs/`

文档按用途放置：

- `docs/architecture/`: 架构、协议、设备模型和工程设计。
- `docs/user_manual/`: 原厂资料和手册。
- `docs/pre_design_report/`: 早期调研报告。
- `docs/development_plan.md`: 阶段计划。

README 不再承载完整产品架构设计，只作为仓库入口和维护规范。

### `skills/` 和 `tools/`

`skills/` 放本仓库专用自动化能力和参考资料。`tools/` 放可直接运行的检查脚本。

脚本更新后应同步运行对应验证：

```bash
python tools/check_hw_tests.py
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

# H417 单项测试
make -C hw_tests/h417 HW_TEST=h417_ws2812
make -C hw_tests/h417 HW_TEST=h417_ws2812_breath
make -C hw_tests/h417 HW_TEST=h417_ws2812_chase
make -C hw_tests/h417 HW_TEST=h417_ws2812_rainbow_band
make -C hw_tests/h417 HW_TEST=h417_lcd_signal

# CH585 单项测试
make -C hw_tests/ch585 HALF=u2 TEST=ch585_u2_eeprom_i2c
make -C hw_tests/ch585 HALF=u2 TEST=ch585_u2_controls_gpio
make -C hw_tests/ch585 HALF=u3 TEST=ch585_u3_max17048_i2c
make -C hw_tests/ch585 HALF=u3 TEST=ch585_u3_charge_gpio
make -C hw_tests/ch585 HALF=u3 TEST=ch585_u3_ec11_gpio
```

## 维护原则

- 新驱动优先放到实际拥有它的固件或测试目录；只有芯片原厂底层库放 `basic/`。
- 单项硬件测试不要依赖完整产品固件，不要引入无关外设。
- 移动文件时同步更新 README、Makefile、检查脚本和自动化脚本。
- 不提交构建产物、日志、临时 dump、工具缓存。
- 修改 RT-Thread 或 WCH EVT 底层库前先确认是否确实需要改 vendor 代码。
