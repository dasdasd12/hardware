# AI_keyb_wch Hardware

本仓库保存硬件侧主线固件、随固件管理的芯片底层依赖和本地自动化脚本。

`main` 保持主线固件结构，不混入临时 bring-up 输出；硬件单项测试放在 `hardware-test` / `origin/test` 分支维护。`hardware-test` 必须保持包含 `main` 的代码，再额外保留测试工程、测试资产和测试检查脚本。硬件资料、规格书、原理图、LaTeX 和完整项目文档优先放在 [`dasdasd12/Docs-For-AI-Keyboard`](https://github.com/dasdasd12/Docs-For-AI-Keyboard)。

## 关联仓库

- [`dasdasd12/software`](https://github.com/dasdasd12/software)：本项目的软件侧关联仓库。
- [`dasdasd12/Docs-For-AI-Keyboard`](https://github.com/dasdasd12/Docs-For-AI-Keyboard)：项目文档、原理图、规格书和设计资料主仓库。

## 目录原则

- 顶层不维护通用 `basic/`。底层依赖必须跟随实际使用它的芯片固件目录。
- H417 相关内容放在 `firmware/h417/`，H417 使用的 WCH 底层库放在 `firmware/h417/basic/wch/SRC/`。
- CH585 相关内容进入 `firmware/ch585/`，CH585 底层依赖放在 `firmware/ch585/basic/` 下。
- 主线产品驱动优先放在实际拥有它的固件目录；测试分支中可被同一芯片多个测试复用的驱动可以放在 `firmware/<chip>/drivers/`。
- `hw_tests/` 只属于 `hardware-test` / `origin/test`，用于单项硬件验证代码和测试资产，不合入 `main`。
- `docs/` 只保留早期设计和调研资料；芯片手册、屏幕规格书等用户手册不放在本仓库。
- `build/`、`.wch-skill-logs/`、`.tmp_pdf_text/`、OpenOCD 日志和临时 dump 文件由 `.gitignore` 忽略，不属于仓库结构。

## 当前文件树

```text
hardware/
|-- firmware/
|   |-- h417/
|   |   |-- Makefile                       # H417 双核统一构建入口
|   |   |-- basic/wch/SRC/                 # H417 固件本地依赖的 WCH 底层库
|   |   |-- drivers/                       # hardware-test 分支共享测试驱动
|   |   |   |-- gd5f1g_spi_nand/           # GD5F1G SPI-NAND 底层驱动与 H417 SPI1 适配
|   |   |   |-- gpha_2d/                   # V5F GPHA 阻塞式 2D helper
|   |   |   `-- ltdc_rgb/                  # RGB LCD/LTDC 面板与 framebuffer helper
|   |   |-- v3f/                           # V3F 固件目标
|   |   `-- v5f_rtthread/                  # V5F RT-Thread 固件目标
|   `-- ch585/
|       |-- Makefile
|       |-- basic/wch/                     # CH585 WCH 底层库和 BLE/RF 库
|       |-- applications/
|       |-- bsp/
|       `-- drivers/
|-- hw_tests/                              # 仅 hardware-test/test 分支维护
|   |-- h417/
|   |   |-- Makefile
|   |   `-- passed/
|   |       |-- v3f_standalone/
|   |       `-- v5f_rtthread/
|   `-- ch585/
|       |-- Makefile
|       `-- src/
|-- skills/
|-- tools/                                 # hardware-test 中包含测试边界检查
`-- README.md
```

## H417 测试驱动

- `firmware/h417/drivers/ltdc_rgb/`：800x480 RGB 面板时序、PA9 DISP、PA10 CTRL、RGB565、L8+CLUT、旋转 framebuffer helper。驱动不分配 framebuffer，调用方负责内存。
- `firmware/h417/drivers/gpha_2d/`：GPHA R2M 填充、L8 到 RGB565 PFC、ARGB4444 over RGB565 blend、L8 4-byte quad fill。GPHA 不能原生输出 L8，L8 fill 通过 ARGB8888 R2M 写 4 个相邻索引实现，x 和 width 必须 4 字节对齐。
- `firmware/h417/drivers/gd5f1g_spi_nand/`：GD5F1G SPI-NAND core 和 CH32H417 SPI1 板级适配，只负责外置 NAND 的 ID、reset、feature、erase、program、read、坏块标记读取等底层操作。图片/LUT 写入读取属于当前硬件测试流程，不放入主驱动区。

## 构建入口

```bash
# H417 双核主线固件
make -B -C firmware/h417
make -B -C firmware/h417 v3f
make -B -C firmware/h417 v5f
make -C firmware/h417 clean

# CH585 RF basic smoke 固件
make -B -C firmware/ch585
make -C firmware/ch585 clean

# H417 V5F 单项硬件测试，会同时生成 V3F 启动固件和 V5F 测试固件
make -B -C hw_tests/h417 HW_TEST=h417_v5f_ltdc
make -B -C hw_tests/h417 HW_TEST=h417_v5f_ltdc_l8_palette_image
make -B -C hw_tests/h417 HW_TEST=h417_v5f_ltdc_rgb565_diag
make -B -C hw_tests/h417 HW_TEST=h417_v5f_flash
make -B -C hw_tests/h417 HW_TEST=h417_v5f_flash_l8_assets
make -B -C hw_tests/h417 HW_TEST=h417_v5f_gpha_r2m_fill
make -B -C hw_tests/h417 HW_TEST=h417_v5f_gpha_pfc_l8_rgb565
make -B -C hw_tests/h417 HW_TEST=h417_v5f_gpha_blend_rgb565
make -B -C hw_tests/h417 HW_TEST=h417_v5f_gpha_l8_ltdc_fullscreen

# H417 V3F standalone 单项测试
make -B -C hw_tests/h417 HW_TEST=h417_ws2812
make -B -C hw_tests/h417 HW_TEST=h417_lcd_backlight
make -B -C hw_tests/h417 HW_TEST=h417_ltdc
make -B -C hw_tests/h417 HW_TEST=h417_flash_image

# 边界检查
python tools/check_hw_tests.py
```

## 维护规则

- `main` 不保留 `hw_tests/` 和测试资产；`hardware-test` / `origin/test` 保持包含 `main`，再叠加测试工程。
- 单项测试尽量保持一个硬件项一个 `HW_TEST`，可以共享 runner 和驱动，但不要让测试互相依赖。
- 测试驱动要求即插即用：少依赖、不分配大块内存、头文件写清初始化顺序、内存归属和硬件限制。
- V5F 测试资产放在 `hw_tests/h417/passed/v5f_rtthread/assets/`；不要放回 `firmware/h417/v5f_rtthread/applications/`。
- 图片/LUT 存入外置 flash 只是当前通路验证测试，相关 manifest、LZSS、checksum 和 display flow 保留在 `hw_tests/`，不要抽象成主驱动接口。
- 需要擦写 flash 的测试必须在测试代码和注释里明确说明擦写范围。
- 移动文件时同步更新 README、Makefile 和自动化脚本。

## 提交规范

提交前先看 `git status --short`，确认没有误带本地日志、构建产物或其他分支的未完成改动。提交信息使用简短英文，推荐格式：

```text
type(scope): summary
```

示例：

```text
chore(h417): move basic library under firmware
refactor(h417): split v5f hardware test drivers
docs: update hardware tree overview
```

## Firmware hardware contract

`../latex/contest_report_template.tex` is the source of truth for board
hardware. A default firmware path must not initialize or depend on a device,
peripheral instance, pin role, or board resource that is not described there.

Run this before changing default firmware hardware bring-up code:

```bash
python tools/check_firmware_hardware_contract.py
```

The check intentionally fails while default firmware still contains stale
hardware assumptions, such as undeclared H417 UART8 console use, eval-board
LED pins, fake CH585 halves, or old SPI fallback pins.
