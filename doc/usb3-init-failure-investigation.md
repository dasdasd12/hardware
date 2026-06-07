# CH32H417 USB3.0 (USBSS) 初始化失败调查报告

**日期**: 2026-05-18  
**调查人**: AI Assistant  
**相关模块**: CherryUSB, USBSS (USB3.0 SuperSpeed) Device, CH32H417 V5F/V3F Dual-Core  

---

## 1. 问题现象

V5F 运行时，`hid_cdc_composite_init()` 返回 `-1`，串口输出：

```
USB device init failed; keep RT-Thread running.
```

---

## 2. 失败点精确定位

CherryUSB 的 `usbd_initialize()` -> `usb_dc_init()` 链路中，**唯一返回 -1 的位置**是 USBSS PLL 锁定超时。

文件：`rtthread_port/rt-thread/components/drivers/usb/cherryusb/port/ch32h417/usb_dc_ch32h417.c`

```c
int usb_dc_init(uint8_t busid)
{
    uint32_t timeout = USBSS_PLLRDY_TIMEOUT;  // 1000000U

    /* ... */

    /* Enable USBSS PLL */
    RCC->CTLR |= (uint32_t)RCC_USBSS_PLLON;
    while ((RCC->CTLR & (uint32_t)RCC_USBSS_PLLRDY) != (uint32_t)RCC_USBSS_PLLRDY) {
        if (timeout-- == 0U) {
            RCC->CTLR &= ~(uint32_t)RCC_USBSS_PLLON;
            return -1;  // <-- 失败点
        }
    }

    /* Enable USBSS clocks */
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBSS, ENABLE);
    RCC_PIPECmd(ENABLE);
    RCC_UTMIcmd(ENABLE);
    RCC_USBSS_PLLCmd(ENABLE);

    /* ... */
    return 0;
}
```

**结论**: `usb_dc_init()` 在 V5F 上执行时，USBSS PLL（`RCC_USBSS_PLLON`）使能后，`RCC_USBSS_PLLRDY` 在约 100 万次轮询内始终未置位，导致超时返回 `-1`。

---

## 3. 已排除的因素

### 3.1 时钟配置
- 当前使用 `SYSCLK_400M_CoreCLK_V5F_400M_V3F_100M_HSE`
- WCH 官方 USBSS 例程（`CH372Device/V3F/User/system_ch32h417.c`）使用**完全相同的 `SystemInit()` 和 `SetSYSCLK_400M...()`**
- USBSS PLL 参考时钟通过 `PLLCFGR2` 配置为 25MHz HSE，与官方一致

### 3.2 USBHS PLL 依赖
- `SYSCLK_400M` 配置**不开启** USBHS PLL（`RCC_USBHS_PLLON`）
- 但官方 USBSS 例程同样使用 `SYSCLK_400M`，且**不开启** USBHS PLL，USBSS 仍能正常工作
- **结论**: USBSS PLL 锁定**不依赖** USBHS PLL

### 3.3 PHY 配置
- CherryUSB 的 `usb_ss_cfg_mod()` 与官方 `USBSS_CFG_MOD()` 写入的寄存器地址和数值完全一致：

| 地址 | CherryUSB | 官方 WCH |
|------|-----------|----------|
| `0x400341f8` (CFG_CR) | `0x7c12`, `0x79AA`, `0x4430`, `0x0010` | 相同 |
| `0x5003C018` | `0xB0054000` | 相同 |

### 3.4 PLL 使能序列
- CherryUSB: `RCC->CTLR |= RCC_USBSS_PLLON; while (!(RCC->CTLR & RCC_USBSS_PLLRDY));`
- 官方 `USBSS_PLL_Init()`: **完全相同的逻辑**
- 区别仅在于 CherryUSB 加了 1M 次轮询的超时保护，官方是死等

### 3.5 RCC 寄存器访问
- V5F 已成功通过 `RCC_HB1PeriphClockCmd` / `RCC_HB2PeriphClockCmd` 操作 RCC 寄存器（USART8、GPIO 时钟均正常）
- `RCC_USBSS_PLLON`（bit 22）和 `RCC_USBSS_PLLRDY`（bit 23）定义正确

---

## 4. 关键发现：官方例程的初始化位置

对比 WCH EVT 官方例程后，发现一个**架构级差异**：

| 项目 | 官方 EVT 例程 | 当前项目 |
|------|--------------|---------|
| USBSS 初始化调用核心 | **V3F** (`main.c` -> `Hardware()` -> `USBSS_Device_Init`) | **V5F** (`main.c` -> `hid_cdc_composite_init`) |
| V3F 状态 | V3F 先进入 STOP 模式，被 V5F 通过 HSEM 唤醒后，**在 V3F 上执行 USBSS 初始化** | V3F 执行 `NVIC_WakeUp_V5F()` 后立即进入 `__WFI()`，不再唤醒 |
| V5F 官方 USBSS 例程 | 官方 `CH372Device/V5F/User/main.c` 实际运行的是 **USBHS** (`USBHSD`)，**不是 USBSS** | 当前代码在 V5F 上直接初始化 USBSS |

**核心发现**: WCH 官方所有 USBSS Device 例程（CH372Device、UVC 等）均在 **V3F** 上完成 `USBSS_PLL_Init()` 和 `USBSS_Device_Init()`。

---

## 5. 根因假设

### 假设 1（主因）：V3F 休眠导致 USBSS 电源/时钟域未激活

CH32H417 的 USBSS 控制器及 PHY 可能绑定在 V3F 的电源域或 AHB 总线域上。当 V3F 进入 `__WFI()` 后：
- CPU 时钟停止
- V3F 关联的外设总线时钟可能被门控
- USBSS PLL 虽由 RCC 控制，但其模拟/PHY 部分可能依赖 V3F 域的供电状态

**证据**:
- 官方双核例程中，V3F 被 V5F 通过 HSEM 唤醒后才调用 `Hardware()` 初始化 USBSS
- 官方 V5F 例程不涉及 USBSS，仅使用 USBHS

### 假设 2（次因）：缺少 V3F 与 V5F 的 HSEM 同步

官方双核例程使用 HSEM（硬件信号量）进行 V3F/V5F 同步：
```c
// V3F 官方代码
HSEM_ITConfig(HSEM_ID0, ENABLE);
PWR_EnterSTOPMode(PWR_Regulator_ON, PWR_STOPEntry_WFE);
HSEM_ClearFlag(HSEM_ID0);
Hardware();  // 初始化 USBSS
```

当前项目没有这套握手机制，V3F 直接进入 WFI 不再响应。

---

## 6. 验证步骤

### 步骤 1：加打印确认失败点

在 `usb_dc_ch32h417.c:usb_dc_init()` 中增加日志：

```c
rt_kprintf("[USBSS] Enabling USBSS PLL...\n");
RCC->CTLR |= (uint32_t)RCC_USBSS_PLLON;
while ((RCC->CTLR & (uint32_t)RCC_USBSS_PLLRDY) != (uint32_t)RCC_USBSS_PLLRDY) {
    if (timeout-- == 0U) {
        rt_kprintf("[USBSS] ERROR: PLL lock timeout!\n");
        RCC->CTLR &= ~(uint32_t)RCC_USBSS_PLLON;
        return -1;
    }
}
rt_kprintf("[USBSS] PLL locked successfully.\n");
```

### 步骤 2：临时让 V3F 保持活跃

修改 `rtthread_port/v3f_wakeup/main.c`：

```c
// 原代码
// while (1) { __WFI(); }

// 临时测试：忙等保持 V3F 活跃
while (1) { __NOP(); }
```

若 USBSS 初始化因此成功，可确认 **V3F 休眠是根因**。

### 步骤 3：将 USBSS 底层初始化搬到 V3F（可选）

在 V3F 的 `main.c` 中调用 `USBSS_RCC_Init(ENABLE)` 和 `USBSS_Device_Init(ENABLE)`，V5F 仅负责 CherryUSB 协议栈。

---

## 7. 修复方案

### 方案 A：保持 V3F 活跃（短期/调试）

移除 V3F 的 `__WFI()`，改为忙等或事件循环，确保 V3F 电源域不关闭。

**缺点**: 增加功耗，不符合低功耗设计。

### 方案 B：HSEM 同步 + V3F 初始化（推荐）

1. **V3F**: 初始化 HSEM 中断，进入 STOP/WFE 等待
2. **V5F**: 启动完成后，通过 HSEM 通知 V3F 唤醒
3. **V3F**: 唤醒后执行 USBSS 底层初始化（PLL + PHY + LINK）
4. **V5F**: 继续执行 CherryUSB 协议栈（`usbd_initialize` 后续逻辑）

这与 WCH 官方双核例程的架构完全一致。

### 方案 C：修改 V3F 不进入 WFI，仅做延时等待（折中）

在 V3F 启动 V5F 后，延时一段时间（如 1 秒）再进入 WFI，给 V5F 完成 USBSS 初始化的窗口。

**缺点**: 依赖时序，不够稳健。

---

## 8. 附带问题：NVIC 优先级未设置

与 USART8 类似，USBSS 中断使能时未配置优先级：

```c
// usb_dc_ch32h417.c:154-155
NVIC_EnableIRQ(USBSS_IRQn);
NVIC_EnableIRQ(USBSS_LINK_IRQn);
```

建议修复：

```c
NVIC_SetPriority(USBSS_IRQn, 1);
NVIC_SetPriority(USBSS_LINK_IRQn, 1);
NVIC_EnableIRQ(USBSS_IRQn);
NVIC_EnableIRQ(USBSS_LINK_IRQn);
```

---

## 9. 文件关联

| 文件 | 作用 |
|------|------|
| `rtthread_port/rt-thread/components/drivers/usb/cherryusb/port/ch32h417/usb_dc_ch32h417.c` | CherryUSB CH32H417 底层驱动，PLL 超时位置 |
| `rtthread_port/rt-thread/components/drivers/usb/cherryusb/demo/hid_cdc_composite.c` | HID+CDC 复合设备描述符与入口 |
| `rtthread_port/applications/main.c` | V5F 应用主函数，调用 `hid_cdc_composite_init()` |
| `rtthread_port/v3f_wakeup/main.c` | V3F 唤醒程序，执行后进入 `__WFI()` |
| `rtthread_port/v3f_wakeup/system_ch32h417.c` | V3F 时钟配置（400M HSE） |
| `rtthread_port/bsp/system_ch32h417.c` | V5F 时钟配置与 `SystemAndCoreClockUpdate()` |
| `C:/program1/hardware/WCH/CH32H417/CH32H417EVT/EVT/EXAM/USBSS/DEVICE/CH372Device/` | WCH 官方 USBSS 参考例程 |

---

## 10. 结论

USB3.0 初始化失败**不是**时钟树、PHY 配置或 PLL 使能序列的问题，而是**架构级问题**：

> **V5F 在 V3F 处于 `__WFI()` 休眠状态时尝试初始化 USBSS，导致 USBSS PLL 无法锁定。**

WCH 官方所有 USBSS 例程均将初始化放在 V3F 上执行。建议：
1. 先通过**步骤 2**（让 V3F 忙等）快速验证假设
2. 验证通过后，采用**方案 B**（HSEM 同步 + V3F 初始化）做长久修复
