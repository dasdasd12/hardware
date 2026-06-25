# CH585M USB CDC Debug README
更新时间：2026-04-26

## 0. CH9340 / UART0 调试入口

当前调试命令已经同时支持两条入口：

- `USB FS CDC`：CH585M 自己枚举出来的虚拟串口
- `UART0 / CH9340`：电脑看到的 `USB-SERIAL CH9340 (COMx)`

如果设备管理器里只能看到 `USB-SERIAL CH9340 (COM3)`，也可以直接使用这个串口调试。固件会从 UART0 读取命令，并把响应写回 UART0。

推荐先用：

```powershell
D:\python\python.exe tools\usb_cdc_console.py COM3
```

然后输入：

```text
help
status
tap a
```

说明：

- `CH9340` 是外部 USB 转 UART 芯片
- 它不是 CH585M 内部 USB FS CDC
- 但现在两条通道复用了同一套命令

## 1. 当前功能

当前工程已经补上一个最小可用的 `USB FS CDC ACM` 调试/控制通道。

它现在能做这些事：

- 板子通过 `USB FS` 枚举成一个虚拟串口
- 固件日志会同时输出到：
  - 原来的 `UART0 PRINT`
  - 新增的 `USB CDC`
- 电脑可以通过串口命令控制当前 BLE HID 测试接口

当前已经接好的命令有：

- `help`
- `status`
- `tap a`
- `tap enter`
- `tap space`
- `tap esc`
- `adv on`
- `adv off`

## 2. 关键文件

### 2.1 USB CDC 固件

- [usb_cdc_debug.h](/F:/嵌赛/CH585M/src/USB/usb_cdc_debug.h)
- [usb_cdc_debug.c](/F:/嵌赛/CH585M/src/USB/usb_cdc_debug.c)

作用：

- 负责 USB FS CDC 描述符
- 负责 USB 控制传输和端点收发
- 负责日志缓冲
- 负责命令解析
- 负责把命令转成 BLE HID 测试动作

### 2.2 主程序入口

- [Main.c](/F:/嵌赛/CH585M/src/Main.c)

当前已经接入：

- `USB_CDC_DebugInit();`
- `USB_CDC_DebugProcess();`

也就是说现在主循环已经持续处理 USB CDC。

### 2.3 BLE HID 层

- [ble_hid.c](/F:/嵌赛/CH585M/src/BLE/ble_hid.c)
- [ble_hid.h](/F:/嵌赛/CH585M/src/BLE/ble_hid.h)

当前 USB CDC 会调用这些函数：

- `BLE_HID_IsConnected()`
- `BLE_HID_TriggerKeyTap(uint8_t keycode)`
- `BLE_HID_StartAdvert()`
- `BLE_HID_StopAdvert()`

另外，BLE 关键日志现在也会转发到 USB CDC。

### 2.4 PC 侧脚本

- [usb_cdc_console.py](/F:/嵌赛/CH585M/tools/usb_cdc_console.py)

作用：

- 打开 CDC 串口
- 持续显示板子发来的日志
- 支持手动输入命令

## 3. 关键接口

### 3.1 `USB_CDC_DebugInit`

原型：

```c
void USB_CDC_DebugInit(void);
```

作用：

- 初始化 USB FS CDC 设备
- 分配端点 RAM
- 开启 USB 中断

### 3.2 `USB_CDC_DebugProcess`

原型：

```c
void USB_CDC_DebugProcess(void);
```

作用：

- 处理接收到的串口命令
- 发送日志缓冲中的数据
- 处理 CDC 状态通知

说明：

- 这个函数需要在主循环里持续调用

### 3.3 `USB_CDC_DebugLog`

原型：

```c
void USB_CDC_DebugLog(const char *fmt, ...);
```

作用：

- 把格式化日志写进 USB CDC 发送缓冲区

### 3.4 `USB_CDC_DebugIsReady`

原型：

```c
uint8_t USB_CDC_DebugIsReady(void);
```

作用：

- 判断当前 CDC 是否已经配置完成并且主机端已经打开串口

## 4. 当前支持的命令

### 4.1 `help`

显示可用命令列表。

### 4.2 `status`

打印当前状态，至少包括：

- `usb_cfg`
- `dtr`
- `ready`
- `ble_connected`
- `baud`

### 4.3 `tap a`

调用：

```c
BLE_HID_TriggerKeyTap(HID_KEYBOARD_A);
```

效果：

- 如果电脑当前已经通过 BLE 连上板子
- 会发送一次按下再释放
- 电脑文本框里应该出现一个 `a`

### 4.4 `tap enter`

调用：

```c
BLE_HID_TriggerKeyTap(HID_KEYBOARD_RETURN);
```

### 4.5 `adv on`

调用：

```c
BLE_HID_StartAdvert();
```

### 4.6 `adv off`

调用：

```c
BLE_HID_StopAdvert();
```

## 5. 推荐测试流程

### 5.1 编译烧录

1. 在 MRS 里 `Refresh`
2. `Build`
3. 烧录新的 `CH585M.hex`

### 5.2 连接 USB CDC

前提：

- 板子的 `USB FS` 线路已经实际接到电脑

然后在电脑上查看新的串口号，比如 `COM5`。

### 5.3 启动 Python 控制台

先确保装了 `pyserial`，然后运行：

```bash
python tools/usb_cdc_console.py COM5
```

把 `COM5` 换成你电脑实际枚举出来的端口号。

### 5.4 验证命令

先输入：

```text
help
status
```

如果 BLE 已经连接到电脑，再输入：

```text
tap a
```

预期：

- 控制台里会看到命令响应
- 电脑蓝牙连接的文本框里会收到一个 `a`

## 6. 当前边界

当前这版是“最小调试通道”，不是完整调试系统。

已完成：

- USB FS CDC 枚举
- CDC 日志输出
- 基本文本命令
- 通过 CDC 软件触发 BLE 发键

未完成：

- 更完整的命令集
- 更详细的状态统计
- 自动保存日志
- Python 侧协议封装
- 和真实按键扫描的联动

## 7. 后续最自然的扩展方向

接下来可以继续做这几件事：

1. 增加更多 `tap <key>` 映射
2. 增加 `send report` 类命令，直接下发 8 字节 HID 报告
3. 把 ADC / 霍尔 / 矩阵扫描数据通过 CDC 持续发给电脑
4. 让 Python 脚本自动记录日志并打标签
5. 再把真实按键输入接到 `BLE_HID_TriggerKeyTap()`
