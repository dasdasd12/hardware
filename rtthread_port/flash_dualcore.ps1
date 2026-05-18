$openocd = "C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\OpenOCD\OpenOCD\bin\openocd.exe"
$ocdBin  = "C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\OpenOCD\OpenOCD\bin"
$gdb     = "C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC12\bin\riscv-wch-elf-gdb.exe"
$cfg     = "wch-dual-core.cfg"

$v5fElf = "build\v5f\rtthread_ch32h417_v5f.elf"
$v3fElf = "v3f_wakeup\build\v3f_wakeup.elf"

Write-Host "=== 1. Starting OpenOCD Server ==="
$ocdProc = Start-Process -FilePath $openocd -ArgumentList "-s", $ocdBin, "-f", $cfg -NoNewWindow -PassThru -RedirectStandardOutput "openocd_out.txt" -RedirectStandardError "openocd_err.txt"
Start-Sleep -Seconds 4

$ocdErr = Get-Content "openocd_err.txt" -Raw
Write-Host "OpenOCD stderr:"
Write-Host $ocdErr

if ($ocdErr -match "failed to connect") {
    Write-Host "ERROR: OpenOCD could not connect to the target."
    Write-Host "Please ensure the two-wire debug interface is enabled (use WCHISPTool with BOOT0=VCC)."
    if (!$ocdProc.HasExited) { Stop-Process -Id $ocdProc.Id -Force }
    exit 1
}

Write-Host "=== 2. Flash V5F RT-Thread (port 3334) ==="
& $gdb -batch `
  -ex "set remotetimeout 30" `
  -ex "target remote localhost:3334" `
  -ex "monitor reset halt" `
  -ex "load" `
  -ex "info registers pc" `
  $v5fElf

Write-Host "=== 3. Flash V3F Wake-up (port 3333) ==="
& $gdb -batch `
  -ex "set remotetimeout 30" `
  -ex "target remote localhost:3333" `
  -ex "monitor reset halt" `
  -ex "load" `
  -ex 'set $pc = main' `
  -ex "detach" `
  -ex "quit" `
  $v3fElf

Write-Host "=== 4. Stopping OpenOCD ==="
if (!$ocdProc.HasExited) {
    Stop-Process -Id $ocdProc.Id -Force
}

Write-Host "Done. V3F should now be running and has woken V5F."
Write-Host "Connect serial terminal to PB4 (USART8) at 115200-8-N-1 to see RT-Thread boot."
