# CH585M 2.4G Bring-up Notes

本文记录当前 2.4G 一发一收调试工程的来源、角色和烧录顺序。

## 当前工程

- `CH585M`
  - 已跑通 BLE HID、USB CDC、KVM 命令层。
  - 暂时不要改名、不要混入 2.4G bring-up 代码。

- `CH585M_RF_TX`
  - 从 `CHC585/EXAM/BLE/RF_Basic` 复制而来。
  - 用作 2.4G 发射端。
  - `APP/RF_basic.c` 中保持：

```c
#define  TEST_MODE     MODE_TX
```

- `CH585M_RF_RX`
  - 从 `CHC585/EXAM/BLE/RF_Basic` 复制而来。
  - 用作 2.4G 接收端。
  - `APP/RF_basic.c` 中已切换为：

```c
#define  TEST_MODE     MODE_RX
```

## 重要参数

TX 和 RX 必须保持一致：

- `TEST_FREQUENCY`
- `gParm.accessAddress`
- `gParm.crcInit`
- `gParm.properties`

当前例程默认关键值在 `APP/RF_basic.c`：

```c
#define  TEST_FREQUENCY   16
gParm.accessAddress = 0x71762345;
gParm.crcInit = 0x555555;
gParm.properties = LLE_MODE_PHY_2M;
```

## MounRiver Studio 使用顺序

1. Import/Open Existing Project，分别导入：
   - `F:\嵌赛\CH585M_RF_TX`
   - `F:\嵌赛\CH585M_RF_RX`
2. 确认 Project Explorer 中显示：
   - `CH585M_RF_TX`
   - `CH585M_RF_RX`
3. 先分别 Build 一次，再下载。
4. 不要直接使用刚复制过来的旧 `obj` 产物；复制目录里的旧 hex 可能不是当前配置重新编译出来的。
5. 给两块板贴标签：
   - TX 板：烧 `CH585M_RF_TX`
   - RX 板：烧 `CH585M_RF_RX`

## 如果出现 invalid link

`HAL`、`LIB`、`Ld`、`RVMSIS`、`Startup`、`StdPeriphDriver` 是 MounRiver/Eclipse 的链接目录，不是工程里真实复制出来的普通文件夹。

当前两个 RF 工程已把这些链接指向：

```text
F:\嵌赛\CHC585\EXAM\BLE\HAL
F:\嵌赛\CHC585\EXAM\BLE\LIB
F:\嵌赛\CHC585\EXAM\SRC\Ld
F:\嵌赛\CHC585\EXAM\SRC\RVMSIS
F:\嵌赛\CHC585\EXAM\SRC\Startup
F:\嵌赛\CHC585\EXAM\SRC\StdPeriphDriver
```

如果 MounRiver Studio 左侧仍显示 `invalid link`，先右键工程 `Refresh`，不行就关闭该工程再重新打开 `.wvproj`。

## 当前目标

第一阶段只验证裸 2.4G 链路：

```text
TX 板发送 RF_Basic 数据包
RX 板接收 RF_Basic 数据包
```

确认一发一收稳定后，再做第二阶段：

```text
TX 板发送键盘 HID 报文
RX 板接收 2.4G 数据并通过 USB FS HID 上报电脑
```

第二阶段可参考队友工程：

```text
keyboard_USBHS-main/ch585/receiver
keyboard_USBHS-main/ch585/CH585M/src/Mode/rf_transport.c
```

## 当前第二阶段状态

已创建：

```text
F:\嵌赛\CH585M_RF_RX_USB
```

用途：

```text
2.4G RF 接收 -> USB FS HID 键盘上报电脑
```

同时 `CH585M_RF_TX` 已加入：

```text
APP/rf_keyboard_tx.c
```

它会周期性发送键盘 `a` 的按下/释放 RF 帧，和 `CH585M_RF_RX_USB` 配套测试。

后续使用说明见：

```text
F:\嵌赛\CH585M_RF_RX_USB_README.md
```
