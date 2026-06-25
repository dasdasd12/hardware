# CH585M_RF_RX_USB 使用说明

记录时间：2026-05-16

## 工程定位

`CH585M_RF_RX_USB` 是 2.4G 接收端的 USB FS 稳定备份工程：

```text
CH585M_RF_TX
    -> CH585M 2.4G RF
    -> CH585M_RF_RX_USB
    -> USB FS HID Keyboard
    -> Windows 输入字符
```

当前主线已经转到 USBHS：

```text
F:\嵌赛\CH585M_RF_RX_USBHS
```

但 `CH585M_RF_RX_USB` 不要删。USBHS 出问题时，它可以用来判断 RF 协议和 HID 上报是否仍然正常。

## 当前协议

TX 和 RX_USB/RX_USBHS 必须保持一致：

```c
#define RF_CHANNEL          16
#define RF_SYNC_WORD        0xA55A1234UL
#define RF_FRAME_MAGIC      0x55
#define RF_FRAME_LEN        25
```

帧格式：

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

接收端只处理：

```text
target_id == RF_LOCAL_TARGET_ID
target_id == RF_TARGET_BROADCAST(0)
```

默认本机目标 ID：

```c
// F:\嵌赛\CH585M_RF_RX_USB\APP\rf_receiver.h
#define RF_LOCAL_TARGET_ID  1
```

## 当前 TX 测试序列

`F:\嵌赛\CH585M_RF_TX\APP\rf_keyboard_tx.c` 上电约 3 秒后自动发送：

```text
target 1: a -> 1 -> Enter
target 2: b -> 2 -> Enter
target 1: c -> 1 -> Enter
```

所以默认 `RF_LOCAL_TARGET_ID=1` 的 RX_USB 应该输出：

```text
a1
c1
```

如果把 `RF_LOCAL_TARGET_ID` 改成 `2` 并重烧 RX_USB，应该输出：

```text
b2
```

## 编译和烧录

需要 Build：

```text
F:\嵌赛\CH585M_RF_TX
F:\嵌赛\CH585M_RF_RX_USB
```

烧录：

```text
TX 板：
F:\嵌赛\CH585M_RF_TX\obj\CH585M_RF_TX.hex

RX_USB 板：
F:\嵌赛\CH585M_RF_RX_USB\obj\CH585M_RF_RX_USB.hex
```

不要烧旧的 `RF_Basic.hex`，也不要烧 `CH585M_RF_RX.hex`。WCHISPTool 里一次只勾选一个 hex。

## 测试步骤

1. RX_USB 板烧录 `CH585M_RF_RX_USB.hex`。
2. TX 板烧录 `CH585M_RF_TX.hex`。
3. RX_USB 板 USB FS 插电脑。
4. 电脑应识别到 HID Keyboard 或 USB Composite Device，不是 COM 口。
5. 打开记事本或任意文本框。
6. 复位 TX 板。
7. 等待约 3 秒。

默认预期：

```text
a1
c1
```

## 如果没有输出

优先检查：

- RX_USB 是否真的烧了 `CH585M_RF_RX_USB.hex`。
- TX 是否烧了新版 `CH585M_RF_TX.hex`。
- 下载工具里是否只勾选了一个 hex。
- 记事本是否获得焦点。
- 两块板距离先放近一点测试。
- 如果目标 ID 改过，确认 TX 发送的 `target_id` 和 RX 的 `RF_LOCAL_TARGET_ID` 匹配。

当前 `CH585M_RF_TX` 是自动测试发射器，不需要按键，也不需要 USB 输入命令。
