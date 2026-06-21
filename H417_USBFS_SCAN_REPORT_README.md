# H417 USB2.0 FS 扫描上报调试说明

本文记录当前 `codex/h417-spi-analysis` 分支里，H417 通过 USB2.0 FS CDC 给 PC 上报模拟键盘扫描数据的调试方法。

## 当前固件做了什么

- V3F：最小启动核，负责唤醒 V5F。
- V5F：运行 RT-Thread，初始化 CH585M 扫描帧骨架和 USBFS CDC。
- `ch585_spi_scan.c` 当前仍使用 fake source，模拟两颗 CH585M：
  - source0：键 0-63
  - source1：键 64-127
- `usb_cdc_dual.c` 当前启用 USB2.0 FS CDC，并提供主动发送接口。
- `main.c` 每隔 500 ms 通过 USBFS CDC 发一行扫描数据，每行 8 个 raw ADC 值。

## 烧录顺序

烧录时建议只接 WCH-Link，先不要接 USB2.0 FS 数据线。

```powershell
cd F:\嵌赛\hardware\rtthread_port

$env:PATH='F:\MountRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC12\bin;F:\MountRiver\MounRiver_Studio2\resources\app\resources\win32\others\Build_Tools\Make\bin;' + $env:PATH

& 'F:\MountRiver\MounRiver_Studio2\resources\app\resources\win32\others\Build_Tools\Make\bin\make.exe' -C . PREFIX=riscv-wch-elf-
& 'F:\MountRiver\MounRiver_Studio2\resources\app\resources\win32\others\Build_Tools\Make\bin\make.exe' -C .\v3f_wakeup PREFIX=riscv-wch-elf-
```

当前我已经实测过一次：V5F 和 V3F 都烧录并 verify OK。

## PC 端应该看到什么

烧录完成后再插 USB2.0 FS 口，Windows 里会多出一个 H417 CDC 串口。当前实测：

```text
COM4 = WCH-Link 串口
COM5 = CH32H417 USBFS CDC
```

打开 COM5 后，应该能看到类似：

```text
KS f=0 i=000 71 72 73 74 75 76 77 78
KS f=0 i=008 151 152 153 154 155 156 157 158
...
KS f=0 i=064 1199 1200 1201 1202 1203 1204 1205 1206
...
KS f=0 i=120 1399 1400 1401 1402 1403 1404 1405 1406
KS f=1 i=000 351 352 353 354 355 356 357 358
```

字段含义：

- `KS`：keyboard scan
- `f`：H417 上报帧号，一帧覆盖 128 个键
- `i`：本行第一个键的全局下标
- 后面的 8 个数：对应 8 个键的 raw ADC 值

看到 `i=000` 到 `i=120`，再回到 `f+1 i=000`，说明 128 个键的上报链路已经跑通。

## 推荐观察方式

优先使用 PowerShell 脚本，不依赖 Python 包：

```powershell
powershell -ExecutionPolicy Bypass -File F:\嵌赛\hardware\rtthread_port\tools\read_usbfs_scan.ps1 -Port COM5
```

如果安装了 Python 3 和 pyserial，也可以用：

```powershell
py -3 F:\嵌赛\hardware\rtthread_port\tools\read_usbfs_scan.py --port COM5
```

注意这台电脑上 `python` 默认是 Python 2.7，应该用 `py -3`。

## 如果看不到 COM5

按这个顺序排查：

1. 确认插的是 USB2.0 FS 口，不是 USBHS/USBSS 口。
2. 烧录时拔掉 USB2.0 FS，只保留 WCH-Link；烧录完成后再插回 USB2.0 FS。
3. 重新按一下板上的 Reset。
4. Windows 设备管理器里确认是否出现 `CH32H417 USBFS CDC` 或新的 USB 串口。
5. 如果只有 COM4，说明目前只枚举到了 WCH-Link，H417 USBFS 还没起来。

## 下一步

这一步验证的是 H417 -> PC 的 USB2.0 FS 调试通道。确认稳定后，再把 fake source 替换成真实 H417 SPI master 拉取两颗 CH585M 的扫描帧。
