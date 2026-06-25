# CH585M BLE HID 状态说明

更新时间：2026-04-26

## 1. 这份文档的目的

这是一份单独给“协作者 / 其他 AI / 后续接手者”看的说明文档。

目标是快速回答下面几个问题：

- 目前 `CH585M` 已经做到什么程度
- 现在怎么验证它是正常工作的
- 目前应该调用哪些函数
- 后续如果要接真实按键，应该从哪里接入

## 2. 当前已经完成的功能

当前 `CH585M` 工程中的 BLE HID 已经完成以下能力：

- 能广播 BLE 设备名：`CH585M_HIDBLE`
- 能被电脑扫描到
- 能和电脑稳定建立 BLE 连接
- 连接后已经验证过可以向电脑发送 HID 键盘按键
- 现在已经去掉“自动发字符 a”的调试逻辑
- 现在改成“手动软件触发一次按键点按”

当前已经验证成功的主链路是：

`广播 -> 连接 -> 建立 HID 通知 -> 发送键盘报告 -> 电脑收到字符`

## 3. 当前还没有完成的功能

下面这些现在还没有接入：

- 板级真实按键输入
- 按键矩阵 / 霍尔 / ADC 键值扫描
- USB HS 键盘
- RF 链路
- 三模切换
- KVM 控制逻辑

当前这版 BLE HID 只是“链路已打通、接口已预留”的基础版本。

## 4. 当前最重要的文件

### 4.1 入口文件

- `src/Main.c`

作用：

- 初始化系统时钟
- 初始化 BLE 协议栈
- 初始化 HID 设备层
- 初始化当前 BLE HID 应用层

当前初始化顺序：

```c
CH58x_BLEInit();
HAL_Init();
GAPRole_PeripheralInit();
HidDev_Init();
BLE_HID_Init();
```

### 4.2 BLE HID 应用层

- `src/BLE/ble_hid.c`
- `src/BLE/ble_hid.h`

作用：

- 配置广播包和设备名
- 配置 GAP/Bond 参数
- 接管连接状态回调
- 提供对外的发键接口
- 实现“单次按键点按”的调度逻辑

### 4.3 HID Profile 层

- `src/BLE/hiddev.c`
- `src/BLE/hiddev.h`
- `src/BLE/hidkbdservice.c`
- `src/BLE/hidkbdservice.h`

作用：

- `hiddev.*`
  - HID 设备任务层
  - 维护连接安全状态
  - 负责最终走 `GATT Notification` 发 HID 报告
- `hidkbdservice.*`
  - HID 键盘 GATT 服务定义
  - HID Report Map
  - Keyboard Input / Output / Boot Report 等特征值

### 4.4 基础服务

- `src/BLE/devinfoservice.c`
- `src/BLE/battservice.c`
- `src/BLE/scanparamservice.c`

作用：

- 提供 Device Information
- 提供 Battery Service
- 提供 Scan Parameters Service

## 5. 目前对外可用的核心函数

下面这些函数是当前最重要、最值得直接调用和查看的。

### 5.1 `BLE_HID_Init`

文件：

- `src/BLE/ble_hid.c`

原型：

```c
void BLE_HID_Init(void);
```

作用：

- 初始化当前 BLE HID 应用任务
- 设置广播数据和扫描响应数据
- 设置设备名 `CH585M_HIDBLE`
- 配置配对参数
- 注册 HID 服务

### 5.2 `BLE_HID_IsConnected`

文件：

- `src/BLE/ble_hid.c`

原型：

```c
uint8_t BLE_HID_IsConnected(void);
```

作用：

- 返回当前是否已经建立 BLE 连接

返回值：

- `TRUE`：已连接
- `FALSE`：未连接

### 5.3 `BLE_HID_SendKeyboard`

文件：

- `src/BLE/ble_hid.c`

原型：

```c
uint8_t BLE_HID_SendKeyboard(const uint8_t *report8);
```

作用：

- 发送一份完整的 8 字节 HID 键盘报告

适用场景：

- 已经有上层按键扫描结果
- 上层自己组好了标准 8 字节键盘报告
- 需要直接发原始 HID 报告

标准 8 字节格式：

```text
Byte0: modifier
Byte1: reserved
Byte2: keycode1
Byte3: keycode2
Byte4: keycode3
Byte5: keycode4
Byte6: keycode5
Byte7: keycode6
```

### 5.4 `BLE_HID_TriggerKeyTap`

文件：

- `src/BLE/ble_hid.c`
- `src/BLE/ble_hid.h`

原型：

```c
uint8_t BLE_HID_TriggerKeyTap(uint8_t keycode);
```

作用：

- 软件触发一次“按下 + 自动释放”的单次点按
- 这是当前最推荐给上层调用的接口

特点：

- 不需要上层自己手动发“按下”和“释放”两次
- 内部已经带了重试逻辑
- 当前没有真实按键时，可以先拿它做功能接入

典型调用方式：

```c
BLE_HID_TriggerKeyTap(HID_KEYBOARD_A);
```

预期行为：

- 发送按下 `A`
- 短延时后自动发送全释放报告

返回值含义：

- `SUCCESS`
  - 成功接受这次点按请求
- `bleNotReady`
  - 当前没连接，不能发
- `INVALIDPARAMETER`
  - 传入了非法键值，比如 `HID_KEYBOARD_RESERVED`
- `bleAlreadyInRequestedMode`
  - 上一次点按流程还没结束，当前忙

## 6. 当前最推荐的上层接入方式

当前没有真实按键，所以建议所有后续输入源统一调用：

```c
BLE_HID_TriggerKeyTap(keycode);
```

后续谁来触发都可以：

- 板级按键中断
- 按键矩阵扫描
- ADC 键值识别
- 霍尔传感器判断
- 调试命令
- 模式切换逻辑

也就是说：

- 上层只需要决定“这次想发哪个 HID keycode”
- BLE 层负责把这次点击完整送出去

## 7. 推荐查看的关键内部函数

如果后续要继续改 BLE HID，最值得先看的函数是这些：

### 7.1 `BLE_HID_ProcessEvent`

文件：

- `src/BLE/ble_hid.c`

作用：

- 当前应用层事件处理函数
- 负责：
  - 连接参数更新
  - PHY 更新
  - 单次点按的按下/释放调度

### 7.2 `hidStateCB`

文件：

- `src/BLE/ble_hid.c`

作用：

- GAP 状态切换回调
- 负责处理：
  - 初始化完成
  - 开始广播
  - 建立连接
  - 断开连接
  - 断开后重新打开广播

### 7.3 `hidRptCB`

文件：

- `src/BLE/ble_hid.c`

作用：

- HID Report 回调
- 主机开启通知时会走到这里
- 目前主要用于观察 HID Input Notify 是否被主机正确打开

### 7.4 `HidDev_Report`

文件：

- `src/BLE/hiddev.c`

作用：

- HID 报告真正下发前的关键入口
- 会检查：
  - 当前是否已连接
  - 当前连接是否处于可发送状态
  - 对应报告的通知是否已经开启

### 7.5 `hidDevSendReport`

文件：

- `src/BLE/hiddev.c`

作用：

- 根据 `Report ID` / `Report Type` 找到正确的 GATT Handle
- 读取 CCCD 状态
- 只有主机打开通知时才真的发出 HID 报告

### 7.6 `Hid_AddService`

文件：

- `src/BLE/hidkbdservice.c`

作用：

- 注册 HID Keyboard 的 GATT 服务
- 构造 HID Report Map
- 建立 report 与 characteristic handle 的映射关系

## 8. 目前如何验证功能

### 8.1 验证连接

步骤：

1. 烧录当前工程
2. 用电脑扫描 BLE 设备
3. 找到 `CH585M_HIDBLE`
4. 建立连接

预期：

- 能稳定连接
- 不会频繁断开

### 8.2 验证发键

当前板子没有额外物理按键，所以默认不能靠硬件直接触发。

当前只能通过“软件调用”验证：

```c
BLE_HID_TriggerKeyTap(HID_KEYBOARD_A);
```

如果在连接状态下调用成功，电脑端文本框应看到一次字符输入。

## 9. 当前的限制和注意事项

### 9.1 这版不是完整键盘固件

它只是：

- BLE HID 链路稳定基线
- 可连接
- 可手动软件触发发键

它还不是：

- 真正完成的键盘输入固件

### 9.2 当前没有真实输入源

所以这版最缺的是：

- 谁来调用 `BLE_HID_TriggerKeyTap`

后续只要接入真实按键或调试命令，这个问题就解决了。

### 9.3 手机 BLE 调试 App 不一定能显示按键字符

原因：

- 发的是 HID 键盘报告
- 不是普通文本通知

所以：

- “App 接收区没看到字符”不一定代表发键失败
- 更可靠的验证方式是电脑文本框输入

## 10. 建议的下一步

当前推荐的后续开发顺序：

1. 在任意输入源里调用 `BLE_HID_TriggerKeyTap`
2. 用一个固定按键先打通“真实触发 -> BLE 发键”
3. 再把真实按键扫描结果接到 BLE 层
4. 后续再扩展到 USB / RF / 模式切换

## 11. 给后续 AI 的一句话总结

当前 `CH585M` 的 BLE HID 已经完成“可广播、可连接、可由软件手动触发单次按键点按”的基础能力。

后续如果要继续开发，请优先围绕下面两个接口工作：

- `BLE_HID_TriggerKeyTap(uint8_t keycode)`
- `BLE_HID_SendKeyboard(const uint8_t *report8)`

前者适合大多数上层业务直接调用，后者适合已经有完整 HID 报告的上层逻辑。
