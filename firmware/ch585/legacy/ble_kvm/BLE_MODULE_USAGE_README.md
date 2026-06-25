# CH585M 蓝牙模块用法与文件索引

更新时间：2026-05-10

这份文档用于防止后续遗忘当前 BLE / HID / 调试链路的关键内容。后续我或队友的 AI 重新接手时，建议先读这一份，再读 `KVM_STATUS_README.md`。

## 1. 当前一句话状态

当前 `CH585M` 工程已经跑通：

```text
PC Python 控制台 -> USB CDC/COM3 -> CH585M -> BLE HID Keyboard -> PC 真实输入
```

已实测：

- 电脑能连接 BLE 设备 `CH585M_HIDBLE`。
- 电脑能通过 `COM3` 打开调试控制台。
- 串口命令能触发 BLE HID 键盘输入。
- 普通按键、组合键、字符串输入、按键队列、软件 KVM target 编号都已验证。

当前还不是完整产品键盘，因为还没有接真实按键、USB HS HID、RF、多主机连接管理或真实 KVM 硬件切换。

## 2. 重要文件索引

### BLE HID 层

```text
src/BLE/ble_hid.c
src/BLE/ble_hid.h
```

作用：

- 配置 BLE 广播名 `CH585M_HIDBLE`。
- 建立 BLE HID Keyboard 服务。
- 处理 BLE 连接、断开、通知打开等状态。
- 发送 HID Keyboard report。
- 实现按键点按队列。
- 支持普通按键和带修饰键按键。

关键接口：

```c
void BLE_HID_Init(void);
uint8_t BLE_HID_IsConnected(void);
uint8_t BLE_HID_IsKeyTapBusy(void);
uint8_t BLE_HID_GetQueuedTapCount(void);
uint8_t BLE_HID_SendKeyboard(const uint8_t *report8);
uint8_t BLE_HID_TriggerKeyTap(uint8_t keycode);
uint8_t BLE_HID_TriggerModifiedKeyTap(uint8_t modifier, uint8_t keycode);
```

最常用的是：

```c
BLE_HID_TriggerKeyTap(HID_KEYBOARD_A);
BLE_HID_TriggerModifiedKeyTap(0x02, HID_KEYBOARD_A); // Shift + A
```

### KVM 动作层

```text
src/KVM/kvm_control.c
src/KVM/kvm_control.h
```

作用：

- 把底层 HID 按键封装成更上层的动作。
- 支持组合键。
- 支持 ASCII 字符串输入。
- 支持软件 target 编号切换。

关键接口：

```c
void KVM_ControlInit(void);
uint8_t KVM_GetCurrentTarget(void);
uint8_t KVM_SwitchTarget(uint8_t target);
uint8_t KVM_SendCombo(uint8_t modifier, uint8_t keycode);
uint8_t KVM_TypeText(const char *text, uint8_t max_len);
uint8_t KVM_AsciiToKey(char ch, uint8_t *modifier, uint8_t *keycode);
```

注意：

- `KVM_SwitchTarget()` 现在只是软件记录 `target=1/2/3`。
- 它还没有真正切 BLE 主机、USB/RF 链路或视频外设。
- 以后真实按键、旋钮、霍尔、GPIO 都应该优先调用 KVM 层，而不是直接操作 BLE 细节。

### USB CDC 调试层

```text
src/USB/usb_cdc_debug.c
src/USB/usb_cdc_debug.h
```

作用：

- 提供 USB CDC/串口日志。
- 解析 PC 端命令。
- 把命令转成 BLE HID 或 KVM 动作。
- `status` 可查看 BLE、队列、KVM target 状态。

当前 Windows 里可能显示为：

```text
USB-SERIAL CH9340 (COM3)
```

实用判断标准不是名称，而是 Python 脚本能否打开端口并看到：

```text
CH585M USB CDC boot
USB CDC ready
CH585M_HIDBLE: connected
```

### PC Python 控制台

```text
tools/usb_cdc_console.py
```

作用：

- 打开 COM 口。
- 显示固件日志。
- 手动输入命令。
- 提供本地辅助命令 `after` / `afterfast`。

启动示例：

```powershell
D:\python\python.exe tools\usb_cdc_console.py COM3
```

如果不是 `COM3`，把 `COM3` 换成设备管理器里实际端口号。

### 文档

```text
README.md
KVM_STATUS_README.md
USB_CDC_DEBUG_README.md
BLE_HID_STATUS_README.md
BLE_MODULE_USAGE_README.md
```

建议阅读顺序：

1. `BLE_MODULE_USAGE_README.md`
2. `KVM_STATUS_README.md`
3. `README.md`
4. `USB_CDC_DEBUG_README.md`
5. `BLE_HID_STATUS_README.md`

## 3. 当前串口命令

### 基础查询

```text
help
status
```

`status` 示例：

```text
status: usb_cfg=1 dtr=1 ready=1 ble_connected=1 tap_busy=0 tap_queue=0 kvm_target=2 baud=115200
```

字段含义：

- `usb_cfg=1`：USB 已配置。
- `dtr=1`：PC 串口已打开。
- `ready=1`：调试通道可用。
- `ble_connected=1`：BLE HID 已连接电脑。
- `tap_busy=1`：当前还有按键正在发送。
- `tap_queue=7`：还有 7 个按键排队等待发送。
- `kvm_target=2`：当前软件 KVM target 为 2。
- `baud=115200`：串口参数。

### 普通按键

```text
tap b
tap 1
tap enter
tap space
tap esc
tap tab
tap backspace
tap left
tap right
tap up
tap down
tap f1
```

### 原始 HID report

```text
report 02 00 04 00 00 00 00 00
release
```

含义：

- `02` 是左 Shift。
- `04` 是 HID keycode `A`。
- 合起来是 `Shift + A`。
- `release` 发送全 0 报告，释放所有按键。

注意：手动发 `report` 后一定要发 `release`，否则可能出现按键被认为一直按住。

### KVM 动作层命令

```text
combo shift a
combo ctrl c
combo ctrl v
combo ctrl alt del
type Hello123
kvm switch 1
kvm switch 2
kvm switch 3
```

注意：

- `combo ctrl alt del` 可能触发 Windows 安全界面，不建议随便测试。
- `type` 当前适合 ASCII 文本，不适合中文输入。
- 测试输出时建议切到英文输入法，避免中文输入法把 `b`、`1` 等解释成拼音候选。

## 4. Python 控制台本地辅助命令

这些命令由 `tools/usb_cdc_console.py` 自己处理，不是固件命令。

### after

```text
after 3 tap b
after 3 combo shift a
after 3 type Hello123
```

用途：

- 给你 3 秒时间切到记事本或其他文本框。
- 字符会输入到文本框。
- 终端仍然显示固件日志。

### afterfast

```text
afterfast 3 tap b; tap 1; tap enter
afterfast 3 combo ctrl c; combo ctrl v
```

用途：

- 快速向板子发送多条命令。
- 用于压测固件按键队列。
- 已验证 `afterfast 3 tap b; tap 1; tap enter` 会在终端看到 `key tap queued` / `key tap dequeue`，文本框得到 `b1` 并换行。

## 5. 已经验证成功的测试记录

### 单键

命令：

```text
after 3 tap b
```

结果：

```text
tap b -> status=00
CH585M_HIDBLE: key tap down sent
CH585M_HIDBLE: key tap up status=0
```

文本框输出：

```text
b
```

### 队列

命令：

```text
afterfast 3 tap b; tap 1; tap enter
```

结果：

```text
key tap queued
key tap dequeue
```

文本框输出：

```text
b1
```

并换行。

### 组合键

命令：

```text
after 3 combo shift a
```

文本框输出：

```text
A
```

### 字符串输入

命令：

```text
after 3 type Hello123
```

文本框输出：

```text
Hello123
```

### KVM target

命令：

```text
kvm switch 2
status
```

结果：

```text
kvm switch 2 -> status=00
kvm_target=2
```

## 6. 编译和烧录注意事项

因为现在新增了：

```text
src/KVM/kvm_control.c
src/KVM/kvm_control.h
```

MRS 中建议：

1. `Refresh` 工程。
2. `Clean Project`。
3. `Build`。
4. 烧录新的 `obj/CH585M.hex`。
5. 重新运行 Python 控制台。

如果 Build 后没有编进 KVM 文件，优先检查：

- `.cproject` 是否包含 `src/KVM` include path。
- MRS 是否已经 Refresh。
- `src/KVM` 是否被 MRS 当成 source folder 参与构建。

## 7. 常见问题

### 7.1 为什么记事本里出现中文？

因为 Windows 当前是中文输入法。

例如：

```text
tap b; tap 1; tap enter
```

在中文输入法下可能被解释成拼音候选选择，出现“吧”等中文。

测试固件时请切到英文输入法。

### 7.2 为什么 `tap_queue` 不是 0？

说明还有按键正在排队发送。

例如 `type Hello123` 会把多个字符依次排队，所以发送中途 `status` 可能看到：

```text
tap_busy=1 tap_queue=7
```

这是正常的。

### 7.3 为什么手机 BLE App 收不到字符？

因为我们发送的是 HID Keyboard report，不是普通 BLE 文本通知。

正确验证方式是：

- 电脑蓝牙连接 `CH585M_HIDBLE`。
- 打开记事本。
- 用 Python 控制台触发按键。
- 看记事本是否输入字符。

### 7.4 现在的 KVM 是真的吗？

现在是“固件动作层”和“软件 target 编号”，还不是真正完整 KVM。

已经完成：

- 键盘动作抽象。
- target 编号切换。
- 组合键和字符串输入。

尚未完成：

- 多主机 BLE 绑定/切换。
- USB/RF 输出链路。
- 视频或外设硬件切换。

## 8. 后续最自然的工作

由于当前开发板没有额外外设，下一步可以先继续用 USB CDC 模拟输入源。

后续有外设或 GPIO 后，建议接入顺序：

1. 找一个临时 GPIO/跳线作为触发输入。
2. 触发 `KVM_SwitchTarget(1/2/3)`。
3. 再触发 `KVM_SendCombo(...)`。
4. 最后接真实键盘扫描。

核心思想：

```text
真实输入源 -> KVM 动作层 -> BLE_HID 队列 -> BLE HID report -> PC
```

目前我们已经完成的是中间和后半段，前半段“真实输入源”还需要硬件条件。
