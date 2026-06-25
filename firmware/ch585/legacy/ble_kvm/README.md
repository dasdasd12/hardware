# CH585M 工作记录

更新时间：2026-04-26

## 1. 项目背景

当前比赛方向是先把 `CH585M` 的无线与输入链路调通，再逐步做成可展示的三模键盘能力。

当前工作区中和本项目直接相关的目录：

- `CH585DS1.PDF`
  - CH585 系列芯片手册。
- `CHC585`
  - 沁恒官方例程与公开资料。
- `keyboard_USBHS-main`
  - 队友仓库，可参考代码结构、开发规范、三模设计思路。
- `CH585M`
  - 我们自己的工作目录，后续代码以这里为主。

## 2. 已确认结论

- `CH585M` 的 BLE 相关硬件资源是强约束的，至少 `ANT`、`X32MI/X32MO`、`VINTA`、`VDCIA` 不能按普通 GPIO 理解。
- `USB FS`、`USB HS` 也有各自固定功能脚，板级排查时要按专用口处理。
- 官方 BLE bring-up 最直接的参考是 `CHC585\EXAM\BLE\HID_Keyboard`。
- 队友仓库里已经有较完整的三模代码骨架，重点可参考：
  - `keyboard_USBHS-main\ch585\CH585M\src\BLE`
  - `keyboard_USBHS-main\ch585\CH585M\src\Mode`
  - `keyboard_USBHS-main\ch585\CH585M\src\USB`
- 我们自己的 `CH585M` 工程已经完成“最小 BLE 广播版”，广播名为 `CH585M_MINBLE`。
- 当前已经接入“最小可连接 BLE HID 版本”代码，新的设备名为 `CH585M_HIDBLE`，等待烧录验收。
- 该最小版本已经实测可被扫描到，这说明：
  - BLE 协议栈初始化链路基本正常。
  - 射频链路至少已经达到“能广播”的状态。
  - 板子的 `ANT`、高频时钟、BLE 基本供电没有出现立刻致命的问题。

## 3. 当前工程状态

当前 `CH585M` 目录内已经有以下基础内容：

- `src\Main.c`
  - 已切到最小可连接 BLE HID 入口。
- `src\BLE\ble_hid.c`
  - 最小可连接 BLE HID 任务封装。
- `src\BLE\ble_hid.h`
  - BLE HID 任务头文件。
- `src\broadcaster.c`
  - 保留的最小广播版本参考代码。
- `src\broadcaster.h`
  - 广播版本头文件。
- `src\BLE`
  - 已接入 `battservice / devinfoservice / hiddev / hidkbdservice / scanparamservice`。
- `HAL`
  - 已接入最小 BLE/HAL 支撑文件。
- `LIB`
  - 已接入 BLE 相关库。
- `obj\CH585M.hex`
  - 当前可下载镜像。

当前还没有完成的部分：

- 可连接 `BLE HID` 的实机验收
- `USB HS HID Keyboard`
- `RF` 接收端联调
- 三模切换管理
- KVM 相关控制逻辑
- 统一调试日志通道

## 4. 芯片与板级注意事项

从芯片手册和官方硬件资料目前可以先固定记住这些点：

- `ANT`
  - 蓝牙射频专用口，直接关系到天线与匹配网络。
- `X32MI / X32MO`
  - 外部 `32MHz` 高频晶振两端，BLE 工作强依赖它。
- `VINTA / VDCIA`
  - 模拟电源相关节点，去耦和布局不能随意处理。
- `PB10 / PB11`
  - `USB FS` 相关专用功能脚。
- `PB12 / PB13`
  - `USB HS` 相关专用功能脚。

如果后面出现“程序看起来没问题，但无线完全不起来”的情况，优先检查：

- 天线与匹配网络
- `32MHz` 晶振参数与布局
- 模拟电源去耦
- 地平面与回流路径
- USB 专用口是否被错误复用

## 5. 可直接参考的资料

官方参考：

- `CHC585\EXAM\BLE\HID_Keyboard`
- `CHC585\EXAM\BLE\Peripheral`
- `CHC585\EXAM\USB\Device\COM`
- `CHC585\EXAM\BLE\BLE_USB`
- `CHC585\PUB\CH585SCH.pdf`
- `CHC585\PUB\蓝牙芯片的电路及PCB设计的重要注意事项.pdf`

队友仓库参考：

- `keyboard_USBHS-main\ch585\CH585M\src\Main.c`
- `keyboard_USBHS-main\ch585\CH585M\src\BLE\ble_hid.c`
- `keyboard_USBHS-main\ch585\CH585M\src\Mode\mode_manager.c`
- `keyboard_USBHS-main\ch585\CH585M\src\USB\usb_hid.c`
- `keyboard_USBHS-main\ch585\引脚定义.txt`
- `keyboard_USBHS-main\ch585\proj.md`

## 6. 连接 / 切换 / KVM 的项目内含义

为了避免后续沟通混乱，先统一术语：

- `连接`
  - 指某一种传输链路真的能工作。
  - 对本项目来说主要是 `USB`、`BLE`、`2.4G RF` 三条链路。
- `切换`
  - 指键盘当前把按键报告发到哪一条链路上。
  - 队友仓库里已经有 `USB -> BLE -> RF` 的模式切换思路。
- `KVM`
  - 在本项目里更可能指“多主机切换”。
  - 如果只是“键盘在多台设备之间切换控制目标”，主要靠固件即可推进。
  - 如果包含 `USB-C / HDMI / 视频切换`，那就不是 CH585 固件单独能完成的，必须确认板上是否有额外切换芯片。

## 7. 比赛可执行任务清单

下面这份清单按“能尽快出效果、能逐步验收”的思路排列。

### 7.1 P0：建立稳定基线

- [x] 最小 BLE 广播工程可编译、可烧录。
- [x] `CH585M_MINBLE` 已能被扫描到。
- [x] 在 `CH585M` README 固化当前结论与后续计划。
- [ ] 增加一个最简单的板级活性指示。
- [ ] 方案二选一：板载 LED 心跳灯，或 USB FS 调试日志。
- [ ] 固化一份“烧录与验收步骤”，避免后续每次重新摸索。

完成标志：
只要重新烧录后，队内任意一人都能独立完成“上电、扫描、确认程序活着”。

### 7.2 P1：先把连接能力做出来

- [ ] 烧录当前 `CH585M_HIDBLE` 固件并验收。
- [ ] 让电脑或手机可以连接到板子，而不只是扫描到广播。
- [ ] 确认主机能识别到 HID 服务。
- [ ] 在 BLE 模式下补最小 `HID Keyboard` 报告验收链路。
- [ ] 单独拉起 `USB HS HID Keyboard` 最小版本，验证插线后可枚举成键盘。
- [ ] 梳理 `RF` 方向是否要进入本次比赛主线。
- [ ] 如果要做 `RF`，补最小接收端联调，至少验证一条按键报告能送达接收器。

完成标志：

- BLE：能连接，能发一个最小按键。
- USB：能枚举，能发一个最小按键。
- RF：如果纳入比赛范围，至少要有一次成功联调记录。

### 7.3 P2：做三模切换

- [ ] 在我们自己的 `CH585M` 工程中引入 `mode_manager` 思路。
- [ ] 定义统一模式枚举：`USB / BLE / RF`。
- [ ] 定义统一发送接口，例如 `Mode_SendKeyboardReport()`。
- [ ] 短按模式键切换当前链路。
- [ ] 切换前先发“全释放报告”，避免主机认为某个键一直按下。
- [ ] 切换时停掉旧链路，再启动新链路。
- [ ] 当前模式保存到 Flash，上电恢复上次模式。
- [ ] 用 LED 或日志明确显示当前模式。

完成标志：
同一套按键扫描结果，不改上层业务逻辑，只靠模式切换就能分别走 `USB / BLE / RF` 三条输出路径。

### 7.4 P3：定义并实现 KVM

这一项必须先和队友对齐需求，否则很容易做偏。

- [ ] 明确“你们说的 KVM”到底是：
- [ ] 方案 A：键盘在多台主机之间切换控制目标。
- [ ] 方案 B：键盘加扩展坞，再切 `USB-C / 视频 / 外设` 路径。
- [ ] 如果是方案 A，定义主机槽位。
- [ ] 如果是方案 A，建议先做 `USB 主机 + BLE 主机 1 + BLE 主机 2` 的软切换模型。
- [ ] 如果是方案 A，补“当前主机编号”与“切换按键逻辑”。
- [ ] 如果是方案 B，先确认原理图里是否存在真正的切换硬件。
- [ ] 如果是方案 B，确认 CH585 是通过 GPIO、I2C 还是别的方式去控制切换芯片。

完成标志：

- 软 KVM：按下指定按键后，键盘能切到另一台主机继续发按键。
- 真 KVM：除键鼠外，外设或视频链路也能同步切换，并且有明确硬件控制路径。

### 7.5 P4：调试与验收体系

- [ ] 保留最少一种稳定日志出口。
- [ ] 优先候选：`USB FS CDC` 文本日志。
- [ ] Python 脚本可自动接收日志并落盘。
- [ ] 日志至少覆盖：启动、模式切换、BLE 广播、BLE 连接、USB 枚举、异常码。
- [ ] 补一份最小验收表，记录每一阶段是否通过。

完成标志：
出现问题时，队内可以先看日志和验收表，而不是完全靠猜。

## 8. 建议的比赛推进顺序

建议按下面顺序推进，性价比最高：

1. 先把 `BLE` 从“能广播”升级到“能连接、能发最小 HID 报告”。
2. 再单独做通 `USB HS HID`。
3. 然后把 `USB / BLE / RF` 接到统一的模式管理层。
4. 再确认 `KVM` 的真实范围，只做本次比赛必须演示的那一层。
5. 最后补 `USB FS` 调试日志和 Python 接收脚本，提高联调效率。

## 9. 当前最值得继续做的下一步

当前最推荐的下一步不是直接做 KVM，而是先把“连接”补齐：

- 优先级 1：把当前最小 BLE 广播版升级成可连接的 `BLE HID Peripheral`
- 优先级 2：拉起最小 `USB HS HID Keyboard`
- 优先级 3：把队友仓库里的模式切换骨架迁到我们自己的 `CH585M`

原因很简单：

- 没有稳定连接，就没有稳定切换。
- 没有稳定切换，就没有可演示的 KVM。
- 先把链路跑通，再谈高级功能，比赛节奏最稳。
## 10. 2026-05-09 当前进展快照

这部分用于快速恢复项目上下文，详细说明见 `KVM_STATUS_README.md`。

当前已经实测通过：

- `CH585M_HIDBLE` 可以通过电脑蓝牙稳定连接。
- `COM3` 调试口可以运行 `tools/usb_cdc_console.py`。
- `tap b` 可以让电脑真实输入 `b`。
- `report 02 00 04 00 00 00 00 00; release` 可以输入大写 `A`。
- `afterfast 3 tap b; tap 1; tap enter` 已验证按键队列，记事本输出 `b1` 并换行。

本轮新增：

- `src/KVM/kvm_control.c`
- `src/KVM/kvm_control.h`
- `BLE_HID_TriggerModifiedKeyTap(modifier, keycode)`
- 串口命令：`combo ctrl c`、`combo ctrl alt del`、`combo shift a`、`type Hello123`、`kvm switch 1/2/3`
- `status` 新增 `kvm_target`

当前推荐测试命令：

```text
status
after 3 combo shift a
after 3 type Hello123
kvm switch 2
status
```

注意：因为新增了 `src/KVM` 目录，MRS 里建议先 `Refresh`，再 `Clean Project`，然后重新 `Build` 和烧录。
