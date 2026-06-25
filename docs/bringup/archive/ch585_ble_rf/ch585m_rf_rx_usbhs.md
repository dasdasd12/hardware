# CH585M_RF_RX_USBHS 使用说明

记录时间：2026-05-16

## 工程定位

`CH585M_RF_RX_USBHS` 是当前 2.4G 接收端主线工程：

```text
CH585M_RF_TX
    -> CH585M 2.4G RF
    -> CH585M_RF_RX_USBHS
    -> USBHS HID Keyboard
    -> Windows 输入字符
```

它从已经跑通的 `CH585M_RF_RX_USB` 复制而来，主要区别是把接收端 USB 输出从 USB FS HID 改成 USBHS HID。

稳定备份工程仍然是：

```text
F:\嵌赛\CH585M_RF_RX_USB
```

如果 USBHS 调试出问题，可以先烧回 FS 版本验证 RF 链路和 HID 上报是否仍然正常。

## 当前已经加入的 KVM 目标过滤

当前 RF 帧已经带有 `target_id`：

```text
[0]      0x55
[1]      payload length = 23
[2..17]  16B NKRO keyboard report
[18..19] consumer usage
[20]     target_id
[21]     sequence
[22..23] reserved
[24]     XOR checksum of bytes [0..23]
```

接收端只处理两类帧：

```text
target_id == RF_LOCAL_TARGET_ID
target_id == RF_TARGET_BROADCAST(0)
```

默认本机目标 ID 在这里配置：

```c
// F:\嵌赛\CH585M_RF_RX_USBHS\APP\rf_receiver.h
#define RF_LOCAL_TARGET_ID  1
```

如果要把某块接收板作为目标 2，把它改成：

```c
#define RF_LOCAL_TARGET_ID  2
```

然后只需要重新 Clean + Build + 烧录 RX_USBHS 接收板，TX 不需要改。

## 当前 TX 演示序列

`F:\嵌赛\CH585M_RF_TX\APP\rf_keyboard_tx.c` 当前上电约 3 秒后自动发送：

```text
target 1: a
target 1: 1
target 1: Enter

target 2: b
target 2: 2
target 2: Enter

target 1: c
target 1: 1
target 1: Enter
```

所以默认 `RF_LOCAL_TARGET_ID=1` 的 RX_USBHS 应该只输出：

```text
a1
c1
```

如果把 RX_USBHS 改成 `RF_LOCAL_TARGET_ID=2`，它应该只输出：

```text
b2
```

如果以后 TX 发 `target_id=0`，则表示广播，所有 RX 都会接收。

## 编译和烧录

建议在 MounRiver Studio 里分别 Clean + Build：

```text
F:\嵌赛\CH585M_RF_TX
F:\嵌赛\CH585M_RF_RX_USBHS
```

烧录文件：

```text
TX 板：
F:\嵌赛\CH585M_RF_TX\obj\CH585M_RF_TX.hex

RX_USBHS 板：
F:\嵌赛\CH585M_RF_RX_USBHS\obj\CH585M_RF_RX_USBHS.hex
```

WCHISPTool 里一次只勾选一个 hex。不要同时勾选 BLE、TX、RX 的多个 hex，否则会出现 “HEX 文件数据有重叠”。

## 测试步骤

1. RX_USBHS 板烧录 `CH585M_RF_RX_USBHS.hex`。
2. TX 板烧录 `CH585M_RF_TX.hex`。
3. RX_USBHS 板插电脑 USB HS 口。
4. 打开记事本或任意文本框，把光标点进去。
5. 复位或重新上电 TX 板。
6. 等待约 3 秒。

默认 `RF_LOCAL_TARGET_ID=1` 的预期结果：

```text
a1
c1
```

目标 2 测试方法：

1. 把 `F:\嵌赛\CH585M_RF_RX_USBHS\APP\rf_receiver.h` 里的 `RF_LOCAL_TARGET_ID` 改成 `2`。
2. Clean + Build `CH585M_RF_RX_USBHS`。
3. 只重烧 RX_USBHS 板。
4. 复位 TX 板。
5. 预期只看到：

```text
b2
```

## USBHS 自检开关

`F:\嵌赛\CH585M_RF_RX_USBHS\APP\rf_receiver_main.c` 里保留了 USBHS 本机自检，默认关闭：

```c
#define USBHS_SELF_TEST_ENABLE 0
```

如果怀疑 USBHS 枚举或 HID 上报本身有问题，可以临时改成：

```c
#define USBHS_SELF_TEST_ENABLE 1
```

打开后，RX_USBHS 枚举成功会先自动输出：

```text
hs1
```

判断方法：

- 能看到 `hs1`，说明 USBHS HID 枚举和键盘上报正常。
- 看不到 `hs1`，优先查 USB HS 口、线缆、设备管理器和 USBHS 代码。
- 能看到 `hs1` 但看不到 `a1/c1` 或 `b2`，问题优先看 RF 接收、TX 烧录、目标 ID 是否匹配。

## 当前限制

这个工程目前是 HID Keyboard，不是 COM 口，所以设备管理器里不一定出现串口。它的现象应该是 Windows 识别到 HID Keyboard 或 USB Composite Device，然后能在记事本里打字。
