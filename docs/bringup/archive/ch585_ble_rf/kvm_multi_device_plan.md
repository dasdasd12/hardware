# CH585M KVM 多设备切换方案

记录时间：2026-05-16

## 当前状态

已经验证成功：

```text
CH585M_RF_TX
    -> 2.4G RF
    -> CH585M_RF_RX_USBHS
    -> USBHS HID Keyboard
    -> Windows 输入字符
```

稳定备份：

```text
CH585M_RF_RX_USB
    -> USB FS HID Keyboard
```

之前 BLE 工程 `CH585M` 已经跑通过：

```text
USB CDC 命令
    -> KVM/action 层
    -> BLE HID Keyboard
    -> Windows 输入字符
```

## 队友提出的 KVM 目标

目标可以拆成三句话：

```text
1. 多设备同时连接。
2. 可以实时切换当前输入发给谁。
3. 可以切换 2.4G 或 BLE 通道。
```

其中，“2.4G 多目标切换”比“BLE 多主机同时连接”容易很多，所以我们先做 2.4G。

## 已完成：2.4G target_id 过滤第一版

当前已经在 RF 帧中加入：

```text
[20] target_id
[21] sequence
```

接收端逻辑：

```text
如果 target_id == RF_LOCAL_TARGET_ID
    上报 USB HID
否则如果 target_id == 0
    当作广播，上报 USB HID
否则
    丢弃
```

默认接收端目标 ID：

```c
// F:\嵌赛\CH585M_RF_RX_USBHS\APP\rf_receiver.h
#define RF_LOCAL_TARGET_ID  1
```

TX 当前自动演示：

```text
target 1: a1 + Enter
target 2: b2 + Enter
target 1: c1 + Enter
```

因此：

- RX 目标 1 应该只看到 `a1` 和 `c1`。
- RX 目标 2 应该只看到 `b2`。
- 如果有两块 RX 板，就可以同时插两台电脑或同一台电脑的两个输入焦点环境，验证“发给谁”已经可以区分。

TX 端也已经有第一版切换状态：

```c
static volatile uint8_t gTxCurrentTarget = KVM_TARGET_1;
static void KVM_SwitchTarget(uint8_t target_id);
```

现在的自动演示序列是：

```text
KVM_SwitchTarget(1)
tap a / tap 1 / tap enter
KVM_SwitchTarget(2)
tap b / tap 2 / tap enter
KVM_SwitchTarget(1)
tap c / tap 1 / tap enter
```

这一步的意义是：后面有真实按键后，不需要重写 RF 协议，只要把 `Fn+1/Fn+2` 或其它触发源接到 `KVM_SwitchTarget()`。

## BLE 当前现实约束

BLE HID 当前代码在：

```text
F:\嵌赛\CH585M\src\BLE\ble_hid.c
F:\嵌赛\CH585M\src\BLE\hiddev.c
```

当前实现是单连接思路：

```c
static uint16_t g_conn_handle = GAP_CONNHANDLE_INIT;
```

并且 `hiddev.c` 里也维护单个：

```c
static uint16_t gapConnHandle;
```

`HidDev_Report()` 最终用这个单连接 handle 做通知：

```c
GATT_Notification(gapConnHandle, ...)
```

所以 BLE 要做“多个电脑/手机同时连接，并指定发给某一个”，不是简单改目标数量，而是要重做 BLE HID 连接表、CCCD 状态和通知发送路径。

## 推荐分阶段实现

### 阶段 1：统一 KVM 路由抽象

后续建议在 TX 或统一输入层定义目标表：

```c
typedef enum {
    KVM_TRANSPORT_RF_24G,
    KVM_TRANSPORT_BLE,
    KVM_TRANSPORT_USB_LOCAL,
} kvm_transport_t;

typedef struct {
    uint8_t id;
    kvm_transport_t transport;
    uint8_t online;
    uint16_t ble_conn_handle;
    uint8_t rf_target_id;
} kvm_target_t;
```

统一发送接口：

```c
KVM_SendKeyboardReport(report16);
KVM_SendConsumerReport(usage);
KVM_SwitchTarget(target_id);
KVM_SwitchTransport(transport);
```

输入层只产生 HID report，不关心最后发给谁。

### 阶段 2：2.4G 多目标

第一版已经完成：

```text
TX 帧带 target_id
RX_USBHS / RX_USB 按 RF_LOCAL_TARGET_ID 过滤
```

还需要继续补：

```text
把固定测试序列改成“当前目标 + 当前输入”
把 KVM_SwitchTarget(target_id) 从自动演示接到真实触发源
```

### 阶段 3：真实切换命令

在没有真实按键前，可以先让 TX 自动演示：

```text
target 1: type "T1"
target 2: type "T2"
target 1: type "BACK"
```

等有按键后，再映射成：

```text
Fn + 1 -> target 1
Fn + 2 -> target 2
Fn + B -> BLE
Fn + R -> 2.4G
```

### 阶段 4：BLE 单连接通道切换

先不要一步到位做 BLE 多连接。

更稳的 BLE 第一版：

```text
BLE 只保持一个当前连接主机。
KVM transport=BLE 时，把输入发给当前 BLE 主机。
KVM transport=RF 时，把输入发给 2.4G RX_USBHS。
```

这样可以先证明：

```text
同一个输入事件可以选择走 BLE 或 2.4G。
```

### 阶段 5：BLE 多连接

最后再做 BLE 多主机同时连接，需要研究：

```text
PERIPHERAL_MAX_CONNECTION
hiddev.c 的 gapConnHandle 单连接设计
HidDev_Report() 是否改成 HidDev_ReportTo(connHandle, ...)
各 HID report CCCD 是否支持每连接独立配置
BLE 连接状态表
```

这一步风险最大，建议放在 2.4G 多目标稳定之后。

## 当前最建议马上做的任务

下一步建议：

```text
1. 编译 CH585M_RF_TX 和 CH585M_RF_RX_USBHS。
2. 默认 RF_LOCAL_TARGET_ID=1 时验证只输出 a1/c1。
3. 把 RX_USBHS 的 RF_LOCAL_TARGET_ID 改成 2。
4. 只重编译/重烧 RX_USBHS。
5. 验证只输出 b2。
6. 如果有第二块 RX 板，再分别烧 target 1 和 target 2，实现同时在线分流。
```

## 推荐工程分工

当前主线工程：

```text
CH585M_RF_TX
CH585M_RF_RX_USBHS
```

保底工程：

```text
CH585M_RF_RX_USB
CH585M
```

建议先不要把 BLE 和 2.4G 强行合并到一个工程里，先把协议和路由层做清楚。
