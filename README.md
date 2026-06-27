# AI_keyb_wch Hardware

本目录保存硬件侧固件、单项硬件验证工程、芯片底层依赖和本地辅助脚本。当前仓库的完整资料、原理图、LaTeX、屏幕资料和规格文档优先维护在 [`dasdasd12/Docs-For-AI-Keyboard`](https://github.com/dasdasd12/Docs-For-AI-Keyboard)。本仓库内 `docs/` 是早期架构设计方案，大部分内容已经过时，暂时不要作为当前实现依据。

## 关联仓库

- [`dasdasd12/software`](https://github.com/dasdasd12/software)：项目软件侧关联仓库。
- [`dasdasd12/Docs-For-AI-Keyboard`](https://github.com/dasdasd12/Docs-For-AI-Keyboard)：项目文档、原理图、规格书和设计资料主仓库。

## 目录原则

- 芯片相关内容放在 `firmware/<chip>/` 下；H417 使用的 WCH EVT 底层库放在 `firmware/h417/basic/wch/SRC/`。
- 可被同一芯片多个固件目标复用的自写驱动放在 `firmware/<chip>/drivers/`。
- 只属于某个核或某个固件目标的代码放在对应目标内部，例如 `firmware/h417/v3f/drivers/`。
- `hw_tests/` 只放单项硬件验证代码和测试资产，可以引用 `firmware/` 下的底层库和主驱动，但不把测试 runner 或大资产混入产品应用目录。
- `build/`、日志、临时 dump、IDE 元数据和本地缓存不属于仓库结构。

## 当前文件树

```text
hardware/
|-- firmware/
|   |-- h417/
|   |   |-- Makefile
|   |   |-- basic/wch/SRC/                  # H417 本地 WCH 底层库
|   |   |-- drivers/
|   |   |   |-- gd5f1g_spi_nand/            # GD5F1G SPI-NAND 底层驱动与 H417 SPI1 适配
|   |   |   |-- gpha_2d/                    # V5F GPHA 阻塞式 2D helper
|   |   |   `-- ltdc_rgb/                   # RGB LCD/LTDC 面板与 framebuffer helper
|   |   |-- v3f/                            # V3F 启动、基础初始化和 V3F 侧驱动
|   |   `-- v5f_rtthread/                   # V5F RT-Thread 产品固件
|   `-- ch585/
|       `-- basic/wch/SRC/                  # CH585 本地 WCH 底层库
|-- hw_tests/
|   |-- h417/
|   |   |-- Makefile                        # H417 单项测试统一入口
|   |   `-- passed/
|   |       |-- v3f_standalone/             # 已验证的 V3F standalone 测试
|   |       `-- v5f_rtthread/               # 已验证的 V5F RT-Thread 测试 runner 和资产
|   `-- ch585/
|       |-- Makefile
|       `-- src/
|-- tools/
|   |-- check_hw_tests.py                   # 硬件测试边界检查
|   `-- test_check_hw_tests_policy.py
`-- README.md
```

## H417 主驱动

- `firmware/h417/drivers/ltdc_rgb/`：800x480 RGB 面板时序、PA9 DISP、PA10 CTRL、RGB565、L8+CLUT、旋转 framebuffer helper。驱动不分配 framebuffer，调用方负责内存。
- `firmware/h417/drivers/gpha_2d/`：GPHA R2M 填充、L8 到 RGB565 PFC、ARGB4444 over RGB565 blend、L8 4-byte quad fill。GPHA 不能原生输出 L8，L8 fill 通过 ARGB8888 R2M 写 4 个相邻索引实现，x 和 width 必须 4 字节对齐。
- `firmware/h417/drivers/gd5f1g_spi_nand/`：GD5F1G SPI-NAND core 和 CH32H417 SPI1 板级适配，只负责外置 NAND 的 ID、reset、feature、erase、program、read、坏块标记读取等底层操作。图片/LUT 写入读取属于当前硬件测试流程，不放入主驱动区。

## 构建入口

```bash
# H417 双核产品固件
make -B -C firmware/h417
make -B -C firmware/h417 v3f
make -B -C firmware/h417 v5f

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

- 单项测试尽量保持一个硬件项一个 `HW_TEST`，可以共享 runner 和驱动，但不要让测试互相依赖。
- 主代码区驱动要求即插即用：少依赖、不分配大块内存、头文件写清初始化顺序、内存归属和硬件限制。
- V5F 测试资产放在 `hw_tests/h417/passed/v5f_rtthread/assets/`；不要放回 `firmware/h417/v5f_rtthread/applications/`。
- 图片/LUT 存入外置 flash 只是当前通路验证测试，相关 manifest、LZSS、checksum 和 display flow 保留在 `hw_tests/`，不要抽象成主驱动接口。
- 需要擦写 flash 的测试必须在测试代码和注释里明确说明擦写范围。
- 移动文件时同步更新 Makefile、README 和 `tools/check_hw_tests.py`。

## 提交规范

提交前先看 `git status --short`，确认没有误带构建产物、日志、临时 dump 或无关分支残留。提交信息使用简短英文：

```text
type(scope): summary
```

示例：

```text
refactor(h417): split v5f hardware test drivers
fix(hw-tests): build v5f tests from passed tree
docs: update hardware tree overview
```
