# AI Keyboard Hardware

本仓库包含 AI Keyboard 的产品固件和与芯片绑定的底层依赖。项目规格、原理图、引脚分配和发布文档以 [Docs-For-AI-Keyboard](https://github.com/dasdasd12/Docs-For-AI-Keyboard) 为准；本仓库只说明固件构建、烧录和联调入口。

## V1.0 固件组成

V1.0 由三个独立 MCU 固件组成：

- H417 V3F：键盘主控，负责 USB、Profile 运行时、左右半键盘汇聚、RGB、旋钮和五向键。
- 左 CH585：左半键盘扫描和 RF/BLE 无线链路。
- 右 CH585：右半键盘扫描和 RF/BLE 无线链路。

H417 的 USBFS HID 和 USBHS 通道可通过同一物理 USB 口工作。Profile 上传使用 H417 USBFS CDC 控制通道；不要用诊断串口代替该通道。

## 目录

```text
firmware/
  common/                         跨 MCU 的键盘、RF 和 Profile 协议
  h417/
    v3f/                           V3F 启动、平台代码和键盘主控应用
      applications/               H417 V1.0 键盘功能
    Makefile
  ch585/
    applications/                 左右半键盘扫描应用
    tools/                         CH585 诊断工具
    Makefile
README.md
```

## 构建和烧录

在仓库根目录构建产品固件：

```powershell
make -C firmware/h417 v3f_keyboard
make -C firmware/ch585 half_scan_left_keyboard
make -C firmware/ch585 half_scan_right_keyboard
```

对应产物：

```text
firmware/h417/build/V3F_keyboard/h417_V3F_keyboard.bin
firmware/ch585/build/half_scan_left_keyboard/ch585_half_scan_left_keyboard.bin
firmware/ch585/build/half_scan_right_keyboard/ch585_half_scan_right_keyboard.bin
```

分别将 H417 主控、左 CH585 和右 CH585 的产物烧录到对应芯片。烧录后先确认键盘默认出厂 Profile 的按键行为正常，再进行运行时 Profile 验证。

诊断固件只用于定位问题，不是产品烧录目标：

```powershell
make -C firmware/ch585 half_scan_left_uart_diag
make -C firmware/ch585 half_scan_right_uart_diag
```

## 运行时更新 Profile

Profile JSON 的编译、上传和激活工具位于相邻的 `software` 仓库。以下命令从 `software` 根目录执行，并假设 H417 已烧录 `h417_V3F_keyboard.bin`、PC 已枚举到其 USBFS CDC 端口。

```powershell
cd ..\software
pip install -r src\bridge\requirements.txt
$PORT = 'COM5'                    # 替换为 H417 USBFS CDC 的实际端口
python scripts\upload-profile.py --port $PORT --info
```

先从出厂 JSON 创建待修改的 Profile。编辑 `build\my_profile.json` 中需要变更的 `keymap` 项；例如为一个已知按键换成不同的键值，以便验证结果。

```powershell
New-Item -ItemType Directory -Force build | Out-Null
Copy-Item config\factory_default_profile.json build\my_profile.json
# 编辑 build\my_profile.json

# 上传到槽位 1，但暂不切换，避免上传失败影响当前键盘行为。
python scripts\upload-profile.py --port $PORT --slot 1 --chunk 64 --no-activate build\my_profile.json
python scripts\upload-profile.py --port $PORT --info

# 确认 slots 中槽位 1 已存在后，激活它。
python scripts\upload-profile.py --port $PORT --activate 1
python scripts\upload-profile.py --port $PORT --info
```

验证顺序：确认改动的按键已生效，断开并重新上电后再次确认，并运行 `--info` 检查活动槽位仍为 `1`。恢复内置出厂 Profile：

```powershell
python scripts\upload-profile.py --port $PORT --factory
```

要修改固件内置的出厂回退 Profile，而非运行时槽位，请编辑 `software\config\factory_default_profile.json`，执行 `python scripts\compile-factory-akpk.py`，然后重新构建并烧录 H417 固件。

## 分支约定

- `main` 只保留产品固件和必要工具。
- `test` 包含 `main`，并额外承载单项硬件 bring-up、诊断资产和试验代码。
- `build/`、日志、烧录工具输出和临时抓包文件不提交。

提交前至少执行对应的三个产品构建目标，并检查 `git status --short`，避免把构建产物或本地日志带入提交。
