# CH32H417 双核下载与 MounRiver 配置说明

本文根据 `F:\嵌赛\EVT\PUB\CH32H417评估板说明书.pdf`、`CH32H417 Evaluation Board Reference-EN.pdf` 和当前 `hardware` 工程整理。当前板卡主控在仓库说明中标为 `CH32H417QEU6`，所以工具里芯片型号选 `CH32H417QEU`。

## 先看结论

CH32H417 有两个核：

- `V3F`：启动核，Flash 起始地址 `0x08000000`。它负责上电后先运行，并唤醒 V5F。
- `V5F`：应用核，Flash 起始地址 `0x08010000`。我们现在写的 RT-Thread、USB2.0 HS CDC、CH585 SPI scan 骨架都在这里。

所以只烧 V5F 不够。只烧 `rtthread_ch32h417_v5f.hex` 时，V5F 可能不会被唤醒，串口也可能完全没输出。当前调试应同时准备这两个文件：

```text
V3F:
F:\嵌赛\hardware\rtthread_port\v3f_wakeup\build\v3f_wakeup.hex

V5F:
F:\嵌赛\hardware\rtthread_port\build\v5f\rtthread_ch32h417_v5f.hex
```

当前调试分支里的 V3F 已改成最小启动核：上电后配置系统时钟、唤醒 V5F，然后空转。这样先保证 V5F 的 UART8 日志、RT-Thread 和 USBHS CDC 能稳定跑起来；USBSS 接管、ADC 扫描等 V3F 侧复杂逻辑后面再逐步加回。

## 编译当前工程

你的 MounRiver 工具链位置是：

```text
F:\MountRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC12\bin
```

`make.exe` 位置是：

```text
F:\MountRiver\MounRiver_Studio2\resources\app\resources\win32\others\Build_Tools\Make\bin
```

在 PowerShell 里先设置临时 `PATH`：

```powershell
cd F:\嵌赛\hardware

$env:PATH='F:\MountRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC12\bin;F:\MountRiver\MounRiver_Studio2\resources\app\resources\win32\others\Build_Tools\Make\bin;' + $env:PATH
```

编译 V5F：

```powershell
& 'F:\MountRiver\MounRiver_Studio2\resources\app\resources\win32\others\Build_Tools\Make\bin\make.exe' -C rtthread_port PREFIX=riscv-wch-elf-
```

编译 V3F：

```powershell
& 'F:\MountRiver\MounRiver_Studio2\resources\app\resources\win32\others\Build_Tools\Make\bin\make.exe' -C rtthread_port\v3f_wakeup PREFIX=riscv-wch-elf- EVT_ROOT=F:/嵌赛/EVT/EXAM
```

## 推荐下载方式：WCH-Link 分开下载

这是官方说明书里的“方式一：单独下载”，最不容易把两个核的地址弄错。

硬件连接：

```text
WCH-Link SWCLK -> H417 SWCLK
WCH-Link SWDIO -> H417 SWDIO
WCH-Link GND   -> H417 GND
WCH-Link 3V3/VTref -> H417 3V3 参考电平
NRST 可选，但建议接上
```

MounRiver/WCH-Link 下载配置：

1. 打开 MounRiver Studio 2。
2. 确认 WCH-Link 能识别目标芯片。
3. 芯片选择：

```text
芯片系列：CH32H41x
芯片型号：CH32H417QEU
```

如果是在 MounRiver 图形界面里操作，重点看这几个配置项：

```text
下载器/调试器：WCH-Link
接口：SWD
目标芯片：CH32H417QEU
下载文件：选择对应 hex/bin
下载地址：V3F 用 0x08000000，V5F 用 0x08010000
擦除方式：第一次可全擦，第二次不能全擦
```

不同版本的 MounRiver 菜单名字可能略有差别，常见入口是工程右键的下载/调试配置，或者工具栏下载按钮旁边的配置页。如果当前界面不能手动选择文件和地址，就使用 MounRiver 安装目录自带的 WCH-LinkUtility 做同样的 WCH-Link 下载配置。不要用 WCHISP 的串口下载界面去代替 WCH-Link 下载，二者进入芯片的方式不同。

4. 先下载 V3F：

```text
文件：F:\嵌赛\hardware\rtthread_port\v3f_wakeup\build\v3f_wakeup.hex
下载地址：0x08000000
Erase All：勾选
```

5. 再下载 V5F：

```text
文件：F:\嵌赛\hardware\rtthread_port\build\v5f\rtthread_ch32h417_v5f.hex
下载地址：0x08010000
Erase All：不要勾选
```

注意：下载第二个工程时不能全擦。否则会把刚烧进去的 V3F 启动程序擦掉，V5F 就不会被唤醒。

## MounRiver 合并下载方式

官方说明书里的“方式二：合并下载”适合标准 MounRiver 双工程。

流程是：

1. 正常编译 V3F 工程，生成 V3F target 文件。
2. 打开 V5F 工程属性。
3. 在属性里启用 `合并 target` 功能。
4. 设置合并文件名，例如 `Merge.bin`。
5. 选择 V3F 编译生成的 target 文件路径。
6. 重新编译 V5F 工程，V5F 工程目录下会生成 `Merge.bin`。
7. 选中 V5F 工程，下载合并后的 `Merge.bin`。

当前我们的 `hardware\rtthread_port` 主要是 Makefile 工程，不是完整 MounRiver 双工程。因此当前阶段更推荐“分开下载”。

## WCHISPTool 下载注意事项

你前面打开的 `WCHISPStudio` 也可以用于部分 CH32H417 芯片的 ISP 下载，但它和 WCH-Link 下载不是一回事。

如果使用 WCHISPTool：

```text
工具页签：WCHISPTool_CH32Vxxx
芯片系列：CH32H41x
芯片型号：CH32H417QEU
```

CH32H417QEU 的 ISP 管脚：

```text
USB ISP：PA11(DM), PA12(DP)
串口 ISP：PB6(TX), PB7(RX)
```

特别注意：

- `PB4/PB5` 是当前 RT-Thread 日志用的 UART8，不是 H417QEU 的串口 ISP 管脚。
- 如果 WCHISP 串口选择了 USB-TTL 的 `COMx`，但这个 USB-TTL 接在 PB4/PB5，上位机会显示等待或打开失败，下载也不会成功。
- USB ISP 用的是 USBFS 的 PA11/PA12，不是我们应用里要调的 USBHS/USBSS 接口。
- WCHISP 多文件合并时，只能选 H417 的 V3F/V5F 文件，不要把 CH585M 的 hex 一起勾选。

可选的 WCHISP 多 hex 方式：

```text
目标程序文件1：v3f_wakeup.hex
目标程序文件2：rtthread_ch32h417_v5f.hex
```

这两个 hex 内部已经分别带有 `0x00000000` 和 `0x00010000` 地址信息。若工具要求手动填地址，则按 MounRiver 说明书使用：

```text
V3F 地址：0x08000000
V5F 地址：0x08010000
```

## 下载后应该看到什么

下载完成并重新上电后，接 UART8：

```text
H417 PB4(TX) -> USB-TTL RX
H417 PB5(RX) -> USB-TTL TX
GND          -> GND
115200 8N1
```

如果手里的 WCH-Link 带 `TXD/RXD` 串口透传引脚，它也可以当 USB-TTL 用，接法一样要交叉：

```text
H417 PB4(TX) -> WCH-Link RXD
H417 PB5(RX) -> WCH-Link TXD
H417 GND     -> WCH-Link GND
波特率        -> 115200 8N1
```

只看启动日志时，`H417 PB4(TX) -> WCH-Link RXD` 和共地是最小必需连接；接上 `PB5/RXD` 后才方便后续给 RT-Thread shell 发命令。注意这里用的是 WCH-Link 的串口透传功能，不是 SWDIO/SWCLK 下载线。

串口应看到类似：

```text
[V5F] early-uart up
Hello, RT-Thread on CH32H417 V5F!
CH585 SPI scan ingest: fake source backend enabled
CH585 SPI frame size=142 bytes, sources=2, keys/source=64
Initializing USB2.0 CDC loopback; USBSS owned by V3F official stack...
USBHS CDC init ok
Dual CDC init completed.
rtthread heartbeat ...
CH585 scan poll=...
  src0 ok=... fetch=0 crc=0 seq_drop=0 ...
  src1 ok=... fetch=0 crc=0 seq_drop=0 ...
```

其中 `USBSS owned by V3F official stack` 是 V5F 当前残留的提示语。当前 V3F 实际是最小唤醒版，所以调试时优先看 `early-uart up`、`Hello, RT-Thread`、`USBHS CDC init ok` 和 `CH585 scan poll`。

PC 端 USBHS 口还应枚举出一个 CDC 串口，产品字符串类似：

```text
CH32H417 USBHS CDC
```

这个 USBHS CDC 当前是 loopback，打开后发什么回显什么；调试日志仍然主要走 UART8。

## 常见问题

### 只烧 V5F 后没任何串口输出

大概率是 V3F 没烧，或者 V3F 被后一次下载的 `Erase All` 擦掉了。按顺序重烧：

```text
1. V3F 0x08000000，勾选 Erase All
2. V5F 0x08010000，不勾选 Erase All
```

### WCHISP 显示打开 COM 失败

通常是 COM 口被串口助手/Python 脚本/MRS Serial Monitor 占用，也可能是选错接口。H417QEU 串口 ISP 是 `PB6/PB7`，不是 `PB4/PB5`。

### USB ISP 搜不到设备

USB ISP 需要芯片进入 BOOT。说明书写明：USB 下载时，上电检查需要 `PA12(DP)` 为高、`PA11(DM)` 为低，但 DP 不能一直上拉，否则正常 USB 功能会异常。评估板上更稳的日常方式仍然是 WCH-Link。

### 调试断点注意事项

官方说明书提示：

- CH32H417 调试过程中，V3F 工程不支持单步调试。
- V5F 工程断点需要等 V5 核被 V3F 唤醒后再设置。

当前阶段优先用 UART8 日志确认 V3F 已唤醒 V5F、V5F RT-Thread 已启动，再考虑 MounRiver 断点调试。
