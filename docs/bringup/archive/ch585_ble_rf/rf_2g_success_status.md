# CH585M 2.4G 当前成功状态记录

记录时间：2026-05-16

## 已经验证成功的链路

USB FS 备份链路已经跑通：

```text
CH585M_RF_TX
    -> CH585M 内置 2.4G RF
    -> CH585M_RF_RX_USB
    -> USB FS HID Keyboard
    -> Windows 记事本输入字符
```

USBHS 主线链路也已经跑通：

```text
CH585M_RF_TX
    -> CH585M 内置 2.4G RF
    -> CH585M_RF_RX_USBHS
    -> USBHS HID Keyboard
    -> Windows 记事本输入字符
```

已经实测过：

- TX 可以通过 2.4G RF 发出键盘帧。
- RX_USB 可以接收 RF 帧并通过 USB FS HID 打字。
- RX_USBHS 可以接收 RF 帧并通过 USBHS HID 打字。
- USBHS 本机自检 `hs1` 正常。
- RF 到 USBHS 的实际输入正常。

## 当前正在做的 KVM 目标路由

现在已经把 RF 帧里的 reserved 字节改成 KVM 路由字段：

```text
[20] target_id
[21] sequence
[22] reserved
[23] reserved
```

接收端只处理：

```text
target_id == 本接收器 RF_LOCAL_TARGET_ID
target_id == 0
```

其中 `target_id=0` 是广播，方便兼容或调试。

TX 端已经有第一版 `KVM_SwitchTarget()` 和 `gTxCurrentTarget`：

```text
自动测试序列先切 target 1，再发 a1/Enter；
再切 target 2，发 b2/Enter；
最后切回 target 1，发 c1/Enter。
```

这说明当前代码已经不是“每个按键硬编码一个接收端”的结构，而是更接近真实 KVM 的“先切目标，再把后续输入发给当前目标”。

## 当前需要编译的工程

当前主线演示 USBHS 版本，编译：

```text
F:\嵌赛\CH585M_RF_TX
F:\嵌赛\CH585M_RF_RX_USBHS
```

稳定 USB FS 备份版本，编译：

```text
F:\嵌赛\CH585M_RF_TX
F:\嵌赛\CH585M_RF_RX_USB
```

暂时不需要编译：

```text
F:\嵌赛\CH585M
F:\嵌赛\CH585M_RF_RX
```

说明：

- `CH585M` 是之前已经跑通的 BLE HID + USB CDC 调试工程。
- `CH585M_RF_RX` 是原始 RF 接收测试工程，目前不是主线。
- `CH585M_RF_RX_USB` 是稳定 USB FS 备份工程。
- `CH585M_RF_RX_USBHS` 是当前主线 USBHS 接收工程。

## 当前烧录文件

TX 发射板：

```text
F:\嵌赛\CH585M_RF_TX\obj\CH585M_RF_TX.hex
```

RX_USBHS 接收板：

```text
F:\嵌赛\CH585M_RF_RX_USBHS\obj\CH585M_RF_RX_USBHS.hex
```

RX_USB 接收板备份：

```text
F:\嵌赛\CH585M_RF_RX_USB\obj\CH585M_RF_RX_USB.hex
```

WCHISPTool 里一次只勾选一个 hex，不要同时勾选多个目标程序文件。

## 当前 TX 行为

`F:\嵌赛\CH585M_RF_TX\APP\rf_keyboard_tx.c` 当前行为：

```text
上电约 3 秒后自动发送 KVM 目标测试序列：

target 1: a -> 1 -> Enter
target 2: b -> 2 -> Enter
target 1: c -> 1 -> Enter
```

每个键都会发送按下帧和释放帧，避免一直刷屏。

## 测试结果预期

默认 RX_USBHS 的 `RF_LOCAL_TARGET_ID=1`，所以记事本应该出现：

```text
a1
c1
```

如果把 RX_USBHS 的 `RF_LOCAL_TARGET_ID` 改成 `2` 并重烧 RX，记事本应该只出现：

```text
b2
```

如果有两块接收板：

- RX1 编译 `RF_LOCAL_TARGET_ID=1`，应该看到 `a1` 和 `c1`。
- RX2 编译 `RF_LOCAL_TARGET_ID=2`，应该看到 `b2`。
- 这就能演示“多设备同时在线，但只有目标设备响应”。

## 下一步方向

更适合继续做：

- 给 TX 增加真实触发源，把 `KVM_SwitchTarget()` 接到按键、旋钮或队友键盘矩阵。
- 等有按键/旋钮/外设后，把 `Fn+1/Fn+2` 映射成目标切换。
- 在 TX 或统一 KVM 层加入 transport 概念，后续支持 RF 和 BLE 之间切换。
- BLE 多主机同时连接风险更大，建议放在 2.4G 多目标稳定之后。
