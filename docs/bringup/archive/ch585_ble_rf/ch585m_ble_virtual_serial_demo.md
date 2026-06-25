# BLE + USB 虚拟串口演示步骤

本文用于给同学演示之前已经实现的蓝牙键盘功能：

```text
电脑 Python 串口命令
        ↓ USB FS CDC 虚拟串口
CH585M 板子
        ↓ BLE HID
电脑蓝牙键盘输入
```

注意：这份演示使用的是 `CH585M` 工程，不是现在调 2.4G 的 `CH585M_RF_TX`、`CH585M_RF_RX`、`CH585M_RF_RX_USB`。

## 1. 需要编译的工程

只编译：

```text
F:\嵌赛\CH585M
```

不要编译/烧录这些 RF 工程：

```text
F:\嵌赛\CH585M_RF_TX
F:\嵌赛\CH585M_RF_RX
F:\嵌赛\CH585M_RF_RX_USB
```

MounRiver Studio 中选择 `CH585M`，执行：

```text
Clean Project
Build Project
```

Build 成功后应生成：

```text
F:\嵌赛\CH585M\obj\CH585M.hex
```

## 2. 下载程序

打开 WCHISPTool，芯片选择：

```text
芯片系列：CH58x
芯片型号：CH585
下载接口：USB
```

下载文件只勾选一个：

```text
目标程序文件1：
F:\嵌赛\CH585M\obj\CH585M.hex
```

不要同时勾选 RF 工程的 hex，否则会出现：

```text
HEX文件数据有重叠
```

## 3. 连接 USB 虚拟串口

烧录后，板子的 USB FS 插到电脑。

在设备管理器中查看：

```text
端口 (COM 和 LPT)
```

可能显示为：

```text
USB-SERIAL CH9340 (COMx)
```

或者类似的 USB CDC/串口设备。

记下 `COMx`，例如：

```text
COM3
```

## 4. 连接蓝牙 HID

在电脑蓝牙设置里搜索并连接：

```text
CH585M_HIDBLE
```

如果以前配对过但连接异常，可以先在 Windows 蓝牙设备里删除旧设备，再重新搜索连接。

连接成功后，串口 `status` 里应看到：

```text
ble_connected=1
```

## 5. 运行 Python 串口脚本

打开 PowerShell 或 CMD：

```powershell
cd F:\嵌赛\CH585M
```

推荐使用你之前成功用过的 Python：

```powershell
D:\python\python.exe tools\usb_cdc_console.py COM3
```

如果你的 COM 口不是 `COM3`，把 `COM3` 换成设备管理器里看到的端口。

如果提示没有 `serial` 模块，安装：

```powershell
D:\python\python.exe -m pip install pyserial
```

再次运行：

```powershell
D:\python\python.exe tools\usb_cdc_console.py COM3
```

正常打开后会看到类似：

```text
Opened COM3. Try: help, status, tap a, combo ctrl c, type hello, kvm switch 2
CH585M USB CDC boot
Type 'help' for commands.
USB CDC ready
```

## 6. 演示前检查

在 Python 串口窗口输入：

```text
help
```

再输入：

```text
status
```

理想状态类似：

```text
status: usb_cfg=1 dtr=1 ready=1 ble_connected=1 tap_busy=0 tap_queue=0 kvm_target=1 baud=115200
```

重点看：

```text
usb_cfg=1
ready=1
ble_connected=1
```

如果 `ble_connected=0`，说明电脑还没有连上 `CH585M_HIDBLE`。

## 7. 演示单个按键

先打开记事本或任意文本框。

在串口窗口输入：

```text
after 3 tap a
```

输入后 3 秒内把鼠标点到记事本文本框。

预期现象：

```text
记事本出现 a
串口窗口出现 key tap down / key tap up 日志
```

## 8. 演示连续按键队列

在串口窗口输入：

```text
afterfast 3 tap b; tap 1; tap enter
```

3 秒内切到记事本。

预期现象：

```text
b1
```

并且光标换到下一行。

串口里会看到类似：

```text
key tap queued
key tap dequeue
key tap down sent
key tap up status=0
```

这个演示说明我们已经实现了 BLE HID 按键排队发送。

## 9. 演示组合键

输入：

```text
after 3 combo shift a
```

3 秒内切到记事本。

预期现象：

```text
A
```

这个演示说明组合键 `modifier + keycode` 正常。

## 10. 演示字符串输入

输入：

```text
after 3 type Hello123
```

3 秒内切到记事本。

预期现象：

```text
Hello123
```

这个演示说明 KVM/action 层可以把 ASCII 字符串转成 HID 按键队列。

## 11. 演示原始 HID report

输入：

```text
after 3 report 02 00 04 00 00 00 00 00; release
```

3 秒内切到记事本。

预期现象：

```text
A
```

解释：

```text
02 = Left Shift
04 = HID keycode a
release = 发送全 0 报文，释放按键
```

## 12. 演示 KVM 目标切换状态

输入：

```text
kvm switch 2
status
```

预期串口输出：

```text
kvm_target=2
```

注意：目前这个阶段的 `kvm switch` 主要是状态层/命令层演示，还没有接真实外设切换硬件。

## 13. 广播开关命令

如果需要重新广播蓝牙设备名，可以输入：

```text
adv off
adv on
```

手机或电脑重新扫描时应能看到：

```text
CH585M_HIDBLE
```

## 14. 推荐演示顺序

给同学展示时建议按这个顺序：

```text
1. status
2. after 3 tap a
3. afterfast 3 tap b; tap 1; tap enter
4. after 3 combo shift a
5. after 3 type Hello123
6. after 3 report 02 00 04 00 00 00 00 00; release
7. kvm switch 2
8. status
```

这样可以完整说明：

```text
USB 虚拟串口可控
BLE HID 已连接
单键可发
连续队列可发
组合键可发
字符串可发
原始 HID report 可发
KVM 状态层可切换
```
