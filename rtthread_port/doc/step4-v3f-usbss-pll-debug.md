# Step 4 调试指南 — V3F 端 USBSS PLL 启动与共享标志

本文档对应 plan `v5f-usb-velvet-toast.md` 的 Step 4：把 USBSS 时钟树 / PLL 的初始化从 V5F 挪到 V3F，并通过共享 SRAM 标志告知 V5F“USBSS 已就绪，可以访问 USBSSD 寄存器”。这一步是在写大量 V5F 端 CherryUSB 移植代码（Step 3）之前的架构性验证。

代码改动位置：`v3f_wakeup/main.c`。V5F 侧未改。

---

## 1. 本次改动到底干了什么

`v3f_wakeup/main.c` 在 V5F 唤醒、SCTLR 设置完成之后，新增一段 USBSS 时钟初始化：

```c
V3F_BootTrace(V3F_STAGE_USBSS_PLL_BEGIN);   // stage = 6
if (V3F_USBSS_PLL_Init()) {
    *V3F_USBSS_FLAG_ADDR = V3F_USBSS_READY; // 0xABCD1234 写入 0x20178020
    V3F_BootTrace(V3F_STAGE_USBSS_PLL_DONE); // stage = 7
} else {
    *V3F_USBSS_FLAG_ADDR = V3F_USBSS_FAILED; // 0xDEADBEEF
    V3F_BootTrace(V3F_STAGE_USBSS_PLL_TIMEOUT); // stage = 8
}
```

`V3F_USBSS_PLL_Init()` 镜像官方 EVT 例程 `EVT/EXAM/USBSS/DEVICE/CH372Device/Common/ch32h417_usbss_device.c::USBSS_RCC_Init(ENABLE)`：

1. `RCC->CTLR |= RCC_USBSS_PLLON`（bit22 = 1）
2. 有限次轮询 `RCC_USBSS_PLLRDY`（bit23），上限约 2M 次循环 ≈ 50 ms @ V3F 100 MHz
3. PLL 锁定后再依次开启 `HBPeriph_USBSS` / `PIPE` / `UTMI` / `RCC_USBSS_PLLCmd`

PLL 参考时钟（25 MHz HSE → RCC_USBSSPLL_REFSEL = 0x20）由 `SystemInit` 通过
`RCC->PLLCFGR2 |= 0x00080020` 在更早阶段已经选好，不需要重复。

### 关键内存地址（共享 SRAM 0x20178000 区域）

| 地址 | 含义 | 期望值 |
|------|------|--------|
| `0x20178000` | V3F boot magic | `0x56334642` ("V3FB") |
| `0x20178004` | V3F 当前 stage | `7` (`USBSS_PLL_DONE`) |
| `0x20178008` | `NVIC->WAKEIP[1]` 拷贝 | 非零 |
| `0x2017800C` | `NVIC->SCTLR` 拷贝 | bit4 应为 1 |
| `0x20178010` | V3F idle 计数器 | 进入 idle 后递增 |
| `0x20178020` | **USBSS ready 标志** | `0xABCD1234` |

V5F 侧（Step 3 待写）会轮询 `0x20178020`，等到 `0xABCD1234` 才碰 USBSSD 寄存器。

---

## 2. 调试方法（按优先级）

烧录前先 `make` 两边都构建干净：

```powershell
cd C:\program1\Program\AI_keyb_wch\hardware\rtthread_port\v3f_wakeup; make
cd C:\program1\Program\AI_keyb_wch\hardware\rtthread_port;             make
```

烧录使用 wch-mrs-automation skill：

```powershell
.\scripts\wch-auto.ps1 -Action flash -ProjectDir <abs path> -Core both
```

若 `WCH-Link failed to connect`：先 `-Action reset-link`，再加 `-RestartLink` 或 `-RecoverMode` 重试。本次 PS skill 自动尝试 OpenOCD 回退，但 V3F、V5F 两边的 hex 都必须存在；新写的 `*V3F_USBSS_FLAG_ADDR = 0` 已加入，V3F bin/hex 会被重新生成。

### 方法 A：UART 输出（**首选**，不需要 OpenOCD）

V5F 侧 board.c 中的 `bsp_early_putc` 走的是 USART8 / PB4，连到 USB-TTL 接到 PC 的 COM5。波特率 115200 8N1。

**只要能看到 `[V5F] early-uart up`**，说明 V3F 至少跑到 `NVIC_WakeUp_V5F`（V3F_STAGE_V5F_WAKE_DONE，stage 4）—— 因为 V5F 没被唤醒就不会有 UART 输出。

UART 输出本身不能区分 V3F 在 `USBSS_PLL_DONE` 还是 `USBSS_PLL_TIMEOUT`，但能区分 V3F 是否“至少跑过了 V5F 唤醒”。要进一步细分，需要方法 B。

期待看到的串口序列（来自 V5F 现有代码）：

```
[V5F] early-uart up
 \ | /
- RT -     Thread Operating System
 / | \     5.x.x build ...
... msh banner ...
heartbeat (按当前 main.c 周期打印)
```

如果 USB 在 Step 3/4/5 完成后枚举成功，CDC 端会另外出现一个串口设备；HID 键盘则不需要串口。

### 方法 B：OpenOCD `mdw` 读 V3F 跟踪块（**用于细分 V3F 失败点**）

通过 wch-mrs-automation skill 启动 OpenOCD daemon（不进入 GDB，直接发命令）：

```powershell
.\scripts\wch-auto.ps1 -Action debug-check -ProjectDir <abs path> -Core v3f
```

`debug-check` 会保留 OpenOCD daemon 在 3333（V3F）/ 3334（V5F）。然后用 telnet（或 OpenOCD `monitor`）执行：

```
mdw 0x20178000 9
```

输出应类似（每个字 4 字节，小端）：

```
0x20178000: 56334642 00000007 00000XXX 00000010 NNNNNNNN 00000000 00000000 00000000 ABCD1234
```

字段对应：

```
[0] = 0x56334642   V3F_BOOT_MAGIC，证明 V3F 至少进了 main
[1] = stage        当前 stage
[2] = WAKEIP[1]    V5F 是否被唤醒（非零 = 被唤醒）
[3] = SCTLR        位 4 应为 1
[4] = idle ticks   进入 idle 循环后才会递增
[8] = USBSS flag   关注的标志位
```

### 方法 C：交互式 GDB（确实需要单步、看寄存器时才用）

```powershell
.\scripts\wch-auto.ps1 -Action debug -ProjectDir <abs path> -Core v3f
```

在 GDB 内：

```
load                  # 仅当 hex 没烧好时
break v3f_wakeup/main.c:V3F_USBSS_PLL_Init
continue
# PLL 启动那一行单步：
next
# 查看 RCC->CTLR
monitor mdw 0x40021000 1
```

> 注意：CH32H417 V3F 的 GDB 端口是 3333。RT-Thread 跑在 V5F 上，V5F 的 GDB 端口是 3334。

---

## 3. 不同观察值 → 假设 → 下一步

按串口输出 + `mdw 0x20178000 9` 综合判断。

### 情况 1：UART 有 `[V5F] early-uart up`，`stage == 7`，`flag == 0xABCD1234`

**含义**：V3F 成功锁定 USBSS PLL。架构假设（USBSS PLL 必须由 V3F 启动）得到确认。

**下一步**：进入 Step 3，安全编写 `usb_dc_ch32h417_usbss.c`，让 V5F 侧轮询 `0x20178020` 后操作 USBSSD。

### 情况 2：UART 有 `[V5F] early-uart up`，`stage == 8`，`flag == 0xDEADBEEF`

**含义**：V3F 跑到了 `V3F_USBSS_PLL_BEGIN` 但 PLL 在 50 ms 内未锁定。

**可能原因（按概率）**：

1. **PLL 参考时钟未配置/配错** —— `RCC->PLLCFGR2` 的 `RCC_USBSSPLL_REFSEL` bits 应为 `0x20`（25 MHz）。`SystemInit` 第 113 行 `RCC->PLLCFGR2 |= 0x00080020` 应已设置。读 `mdw 0x40021014 1`（PLLCFGR2）验证。
2. **HSE 没起来** —— PLL 参考是 HSE 派生的；若 HSE 失败，USBSS PLL 永远锁不上。可读 `RCC->CTLR` 的 `HSERDY` bit 17。`mdw 0x40021000 1`。
3. **V5F 唤醒后立刻把 USBSS PLL 关了** —— 不应该，但 V5F 端如果误调 `RCC_USBSS_PLLCmd(DISABLE)` / `USBSS_RCC_Init(DISABLE)`，会破坏 V3F 这边。当前 V5F 没有这种代码，但要警惕。
4. **VDD12 不稳** —— 官方 example 的 `// PWR_VDD12ExternPower();` 注释提示该域电流大；外部 1.2V 若不稳定，USBSS PLL 锁定失败。属于硬件层面。

**下一步**：

- 先读 `mdw 0x40021000 1`、`mdw 0x40021014 1`，确认 HSE 起来且 REFSEL=0x20。
- 若都正常，按 plan 后备方案：回到完整官方 HSEM + STOP 模式（`PWR_EnterSTOPMode(PWR_Regulator_ON, PWR_STOPEntry_WFE)` + `HSEM_FastTake/Release`）。

### 情况 3：UART 有 `[V5F] early-uart up`，`stage == 6`，`flag == 0`

**含义**：V3F 进入了 `V3F_USBSS_PLL_BEGIN`，但 `V3F_USBSS_PLL_Init` 内某条指令死循环或异常 —— `while(...PLLRDY)` 当年是无限 while，现在改成有限循环就不该卡住，但其后 `RCC_HBPeriphClockCmd` 之类可能触发问题。

**可能原因**：

1. 某个 `RCC_*Cmd` 函数链到了未链入的 SPL 实现（v3f_wakeup `Makefile` 是否包含 `ch32h417_rcc.c`？已确认包含）。
2. PLL 已锁但 `RCC_HBPeriphClockCmd(RCC_HBPeriph_USBSS, ENABLE)` 触发了 hard fault —— 这种情况下 stage 不会前进。

**下一步**：方法 C 单步进入 `V3F_USBSS_PLL_Init`，定位是哪条 RCC 写挂掉。

### 情况 4：UART 有 `[V5F] early-uart up`，`stage <= 5`，`flag == 0`

**含义**：V3F 在到达 USBSS PLL init 之前就停了。但 V5F 已经能输出 UART，说明 V3F 至少跑过了 stage 4 (`V5F_WAKE_DONE`)。

**可能原因**：

- `stage == 4`：V3F 卡在 `NVIC->SCTLR |= 1<<4`（极不可能） 或 `BootTrace(STAGE_SCTLR_DEBUG_DONE)` 之后到 `USBSS_PLL_BEGIN` 之间。当前两者紧挨着，几乎没有可疑代码。
- `stage == 5` (`ADC_DONE`)：只有定义了 `V3F_ENABLE_ADC_INIT` 时才出现。默认未启用，可忽略。

**下一步**：方法 C / `mdw` 多读几次，确认 stage 是否在变化（如果在 `IDLE_LOOP` 之前的某个值上不动，且 idle counter 不变，确实是死住了）。

### 情况 5：UART **完全没输出**，包括 `[V5F] early-uart up`

**含义**：V3F 没能跑到 `NVIC_WakeUp_V5F`，或 V5F 唤醒失败。Step 1 是为了让这种情况“可见”—— 现在又出现就说明此次改动引入了回归。

**可能原因**：

1. **V3F 在新代码段触发 hard fault，没有 trap handler 回退** —— V3F 启动文件应该有默认 trap，但若 RCC 寄存器访问异常，可能锁死整个总线。
2. **V3F 没烧成功** —— `flash` 步骤未完成或被回退覆盖。

**下一步**：

- 方法 B 读 `mdw 0x20178000 5`：如果 magic 不是 `0x56334642`，V3F 根本没进 main，问题在烧录或启动。
- 如果 magic 正确但 stage 卡在 `<= 4`，说明 V3F 在 V5F 唤醒前就死了 —— 这不应该是 USBSS 改动的责任，要排查 SPL 链接、ROM 是否被脏数据污染等。

### 情况 6：`flag == 0xABCD1234` 但 V5F 在 Step 3 之后 USB 仍未枚举

不在 Step 4 范围内，但顺便记录：那就证明 “V3F 端 PLL 锁住 ≠ V5F 端 USBSSD 寄存器可用”。这是 Step 3 的事，需要按官方 `Hardware()` 流程在 V5F 写完 `USBSSD->LINK_CFG / LINK_CTRL / USB_CONTROL` 等。

---

## 4. 失败兜底：升级到完整官方 HSEM + STOP 路径

如果情况 2 的快速修复（参考时钟 / HSE 检查）都做完仍然锁不住 PLL，按官方 `EVT/EXAM/USBSS/DEVICE/CH372Device/V3F/User/main.c:59-66` 把 V3F 改成：

```c
NVIC_WakeUp_V5F(V5F_START_ADDR);
HSEM_ITConfig(HSEM_ID0, ENABLE);
NVIC->SCTLR |= 1<<4;
RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE);
PWR_EnterSTOPMode(PWR_Regulator_ON, PWR_STOPEntry_WFE); // 等 V5F HSEM_Release
HSEM_ClearFlag(HSEM_ID0);
// 然后才做 USBSS init
```

这需要：

- 加入 `ch32h417_pwr.c` / HSEM 相关 SPL（pwr 已在 Makefile 中）。
- V5F 端在合适时刻 `HSEM_FastTake(0); HSEM_ReleaseOneSem(0,0);`。
- 在 V3F 写 `HSEM_IRQHandler`（或检查 WFE 唤醒源）。

> 之前的研究 `doc/usb3-init-failure-investigation.md` 怀疑过 STOP 模式会让 USBSS 不稳定。如果走这条路径，先观察是否真的有副作用；如果有，把 STOP 改成简单的 spin-wait 同步即可保留 HSEM 协议的 timing 含义而不进低功耗。

---

## 5. 安全回滚

如果想暂时禁用这次 V3F 改动，单文件可恢复：在 `v3f_wakeup/main.c` 把以下区块整段注释或用宏包起来：

```c
V3F_BootTrace(V3F_STAGE_USBSS_PLL_BEGIN);
if (V3F_USBSS_PLL_Init()) { ... } else { ... }
```

`*V3F_USBSS_FLAG_ADDR = 0;` 那一行保留，让 V5F 侧的轮询会“一直等不到 ready”，从而**不会**误触 USBSSD 寄存器。V5F 现有代码（含 UART heartbeat）不依赖该标志，所以基础 RT-Thread 仍然能跑。

---

## 6. 快速参考

烧录失败时的常用 PS 命令：

```powershell
# 软复位 WCH-Link
.\scripts\wch-auto.ps1 -Action reset-link

# 单核构建
.\scripts\wch-auto.ps1 -Action build -ProjectDir <abs> -Core v3f
.\scripts\wch-auto.ps1 -Action build -ProjectDir <abs> -Core v5f

# 单核烧录（避免 dual-core OpenOCD 二次 init bug）
.\scripts\wch-auto.ps1 -Action flash -ProjectDir <abs> -Core v3f
.\scripts\wch-auto.ps1 -Action flash -ProjectDir <abs> -Core v5f

# Dry run 看命令但不烧
.\scripts\wch-auto.ps1 -Action flash -ProjectDir <abs> -Core both -DryRun
```

最小验证流程（板子接上 + COM5 接好后）：

1. `flash -Core both`
2. 复位板子
3. 看 COM5 是否有 `[V5F] early-uart up`
4. `debug-check -Core v3f` → telnet localhost 4444 → `mdw 0x20178000 9`
5. 读 stage 与 flag，对照本文第 3 节情况表
