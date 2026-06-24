$openocd = "C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\OpenOCD\OpenOCD\bin\openocd.exe"
$ocdBin  = "C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\OpenOCD\OpenOCD\bin"
$cfg     = "wch-dual-core.cfg"

$v5fElf = "build\v5f\rtthread_ch32h417_v5f.elf"
$v3fElf = "v3f_wakeup\build\v3f_wakeup.elf"

Set-Location $PSScriptRoot

function Quote-CmdArg([string]$arg) {
    return '"' + ($arg -replace '"', '\"') + '"'
}

if (!(Test-Path $v5fElf)) {
    Write-Host "ERROR: V5F ELF not found: $v5fElf"
    exit 1
}

if (!(Test-Path $v3fElf)) {
    Write-Host "ERROR: V3F ELF not found: $v3fElf"
    exit 1
}

$v5fElfArg = $v5fElf.Replace("\", "/")
$v3fElfArg = $v3fElf.Replace("\", "/")
$stdoutLog = "openocd_dual_flash.out.log"
$stderrLog = "openocd_dual_flash.err.log"

Write-Host "=== Flash V5F, flash V3F, then run V3F boot core ==="
$openocdArgs = @(
    "-s", $ocdBin,
    "-f", $cfg,
    "-c", "page_erase",
    "-c", "init",
    "-c", "targets wch_riscv.cpu.1",
    "-c", "halt",
    "-c", "program `"$v5fElfArg`" verify",
    "-c", "targets wch_riscv.cpu.0",
    "-c", "halt",
    "-c", "program `"$v3fElfArg`" verify",
    "-c", "reset run",
    "-c", "shutdown"
)

$cmdLine = (Quote-CmdArg $openocd) + " " +
    (($openocdArgs | ForEach-Object { Quote-CmdArg $_ }) -join " ") +
    " 1> " + (Quote-CmdArg $stdoutLog) +
    " 2> " + (Quote-CmdArg $stderrLog)

& "C:\Windows\System32\cmd.exe" /d /c $cmdLine
$openocdExitCode = $LASTEXITCODE

$ocdOut = Get-Content $stdoutLog -Raw
$ocdErr = Get-Content $stderrLog -Raw
if ($ocdOut) { Write-Host $ocdOut }
if ($ocdErr) { Write-Host $ocdErr }

if ($openocdExitCode -ne 0) {
    Write-Host "ERROR: OpenOCD flash failed. See $stdoutLog and $stderrLog"
    exit $openocdExitCode
}

Write-Host "Done. V3F should now be running and has woken V5F."
Write-Host "Connect serial terminal to PB4 (USART8) at 115200-8-N-1 to see RT-Thread boot."
