# CH585M KVM / BLE HID Current Status

更新时间：2026-05-09

这份文档给队友和后续 AI 快速接手用，记录当前已经验证的链路、刚新增的 KVM 动作层、以及下一轮测试步骤。

## 1. 已经实测通过的链路

当前已经实测通过：

- `CH585M_HIDBLE` 可以被电脑通过蓝牙连接。
- USB FS CDC 调试口可以通过 `COM3` 打开。
- Python 控制台可以收发固件日志和命令。
- `tap b` 可以让电脑真实输入 `b`。
- `report 02 00 04 00 00 00 00 00; release` 可以输入大写 `A`。
- `afterfast 3 tap b; tap 1; tap enter` 已验证固件按键队列：
  - 终端看到 `key tap queued` / `key tap dequeue`
  - 记事本看到 `b1` 并换行

核心链路已经成立：

```text
Python console -> USB CDC -> CH585M firmware -> BLE HID -> PC text input
```

## 2. 当前关键文件

- `src/BLE/ble_hid.c`
  - BLE HID 广播、连接、HID report 发送。
  - 已支持单键点按队列。
  - 新增 `BLE_HID_TriggerModifiedKeyTap(modifier, keycode)`，支持 `Ctrl/Shift/Alt/GUI + key`。

- `src/USB/usb_cdc_debug.c`
  - USB CDC 描述符、串口日志、命令解析。
  - 已支持 `tap/report/release/combo/type/kvm switch/status`。

- `src/KVM/kvm_control.c`
  - 新增 KVM 动作层。
  - 将“组合键、输入字符串、目标切换”封装成上层动作。

- `tools/usb_cdc_console.py`
  - PC 端串口控制台。
  - 支持本地延迟命令 `after` 和队列压测命令 `afterfast`。

## 3. 当前固件命令

基础命令：

```text
help
status
tap b
tap enter
report 02 00 04 00 00 00 00 00
release
```

KVM / 动作层命令：

```text
combo ctrl c
combo ctrl v
combo ctrl alt del
combo shift a
type hello123
kvm switch 1
kvm switch 2
kvm switch 3
```

PC 控制台本地命令，不会直接发给板子，而是辅助测试：

```text
after 3 tap b
afterfast 3 tap b; tap 1; tap enter
after 3 combo ctrl c
after 3 type Hello123
```

`after` 用来给你时间切到记事本，`afterfast` 用来快速发送多条命令，压测固件按键队列。

## 4. 新增代码说明

### 4.1 BLE HID 队列升级

原来队列只保存 `keycode`，现在队列保存：

```c
typedef struct
{
    uint8_t modifier;
    uint8_t keycode;
} bleHidKeyTap_t;
```

因此现在既能排队普通按键，也能排队组合键：

```c
BLE_HID_TriggerKeyTap(HID_KEYBOARD_A);
BLE_HID_TriggerModifiedKeyTap(KVM_MOD_LEFT_CTRL, HID_KEYBOARD_C);
```

### 4.2 KVM 动作层

当前 KVM 层是“动作抽象层”，还不是真正完整 KVM 硬件切换。

已经实现：

- `KVM_SendCombo(modifier, keycode)`
- `KVM_TypeText(text, max_len)`
- `KVM_SwitchTarget(target)`
- `KVM_GetCurrentTarget()`
- `KVM_AsciiToKey(ch, &modifier, &keycode)`

其中 `KVM_SwitchTarget()` 当前只记录目标编号，后续可以接入真实的多主机策略，比如 BLE 多连接、USB/BLE/RF 模式管理、GPIO 控制外部切换芯片等。

## 5. 下一轮测试步骤

因为本次新增了 `src/KVM` 目录，建议 MRS 里先：

1. `Refresh` 工程。
2. `Clean Project`。
3. `Build`。
4. 烧录新的 `CH585M.hex`。
5. 重新运行：

```powershell
D:\python\python.exe tools\usb_cdc_console.py COM3
```

然后按顺序测试：

```text
status
after 3 combo shift a
after 3 type Hello123
afterfast 3 combo ctrl c; combo ctrl v
kvm switch 2
status
```

预期：

- `combo shift a` 在英文输入法下输出 `A`。
- `type Hello123` 输出 `Hello123`。
- `kvm switch 2` 返回 `status=00`。
- `status` 里能看到 `kvm_target=2`。

## 6. 当前边界

已完成的是：

- BLE HID 基础链路。
- USB CDC 调试链路。
- PC Python 辅助控制台。
- 单键、组合键、字符串输入的动作层。
- 软件目标编号 `kvm_target`。

还没有完成的是：

- 真实按键扫描。
- USB HS HID 键盘输出。
- RF 输出。
- 真正多主机连接管理。
- 真正视频/外设 KVM 硬件切换。

下一步最建议做：把一个真实板载输入源，哪怕只是临时 GPIO/跳线，接到 `KVM_SendCombo()` 或 `KVM_SwitchTarget()`，完成“真实触发 -> 动作层 -> BLE HID 输出”的闭环。
