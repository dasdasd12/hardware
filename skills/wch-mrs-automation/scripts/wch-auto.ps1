#requires -Version 5.1
<#
.SYNOPSIS
    CH32H417-dedicated MounRiver Studio command-line build, flash, and debug helper.

.EXAMPLE
    .\wch-auto.ps1 -Action detect -ProjectDir C:\work\project
    .\wch-auto.ps1 -Action init -ProjectDir C:\program1\Program\H417lib\HSEM_CoreSync
    .\wch-auto.ps1 -Action build -ProjectDir C:\program1\Program\H417lib\HSEM_CoreSync -Core both
    .\wch-auto.ps1 -Action flash -ProjectDir C:\program1\Program\H417lib\HSEM_CoreSync -Core both
    .\wch-auto.ps1 -Action debug-check -ProjectDir C:\program1\Program\H417lib\HSEM_CoreSync -Core v3f
#>
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("detect", "init", "build", "flash", "debug", "debug-check", "reset-link", "loop")]
    [string]$Action,

    [string]$ProjectDir = (Get-Location).Path,

    [string]$ElfPath = "",
    [string]$ElfPathV3F = "",
    [string]$ElfPathV5F = "",
    [string]$ImagePath = "",
    [string]$ImagePathV3F = "",
    [string]$ImagePathV5F = "",

    [ValidateSet("auto", "CH32H417")]
    [string]$Chip = "auto",

    [ValidateSet("auto", "v3f", "v5f", "both")]
    [string]$Core = "auto",

    [string]$MRSPath = "C:\MounRiver\MounRiver_Studio2",
    [string]$EVTRoot = "C:\program1\hardware\WCH\CH32H417\CH32H417EVT\EVT\EXAM",
    [int]$GdbPort = 0,

    [ValidateSet("auto", "mrs", "openocd")]
    [string]$FlashTool = "auto",

    [ValidateSet("one-wire", "two-wire")]
    [string]$DebugInterface = "one-wire",

    [ValidateSet("High", "Middle", "Low")]
    [string]$ClkSpeed = "High",

    [string]$FlashAddress = "0x08000000",

    [switch]$SkipBuild,
    [switch]$SkipFlash,
    [switch]$VisibleOpenOCD,
    [switch]$KeepCodeProtect,
    [switch]$EraseAll,
    [switch]$ClearCodeFlash,
    [switch]$DisablePowerOut,
    [switch]$EnableSdiPrintf,
    [switch]$NoVerify,
    [switch]$NoReset,
    [switch]$LoadBeforeDebug,
    [switch]$RecoverMode,
    [switch]$RestartLink,
    [switch]$StartOpenOCD,
    [switch]$AllowOpenOCDFlash,
    [switch]$AllowTargetReset,
    [switch]$AllowLinkReset,
    [switch]$KillStaleDebuggers,
    [switch]$StopOpenOCDAfterDebug,
    [switch]$AllowCodexHardwareAccess,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$script:RunningInCodex = [bool]($env:CODEX_THREAD_ID -or ($env:CODEX_INTERNAL_ORIGINATOR_OVERRIDE -like "codex*"))

function Write-Info { param([string]$Message) Write-Host "[INFO]  $Message" -ForegroundColor Cyan }
function Write-Ok   { param([string]$Message) Write-Host "[OK]    $Message" -ForegroundColor Green }
function Write-Warn { param([string]$Message) Write-Host "[WARN]  $Message" -ForegroundColor Yellow }
function Write-Err  { param([string]$Message) Write-Host "[ERR]   $Message" -ForegroundColor Red }
function Write-Step { param([string]$Message) Write-Host "`n========== $Message ==========" -ForegroundColor Cyan }

function Assert-CodexHardwareAccess {
    param([string]$Operation)
    if ($script:RunningInCodex -and (-not $AllowCodexHardwareAccess)) {
        throw "Codex hardware guard: refusing to run '$Operation'. Allowed by default in Codex: detect, build, MRS DLL flash, flash -DryRun, and GDB attach to an already-running OpenOCD daemon using halt only. Blocked: reset-link, RecoverMode, RestartLink, OpenOCD flash/fallback, starting/stopping OpenOCD, and reset halt. Pass -AllowCodexHardwareAccess only after explicitly accepting that risk."
    }
}

function Assert-LinkResetAllowed {
    param([string]$Operation)
    if (-not $AllowLinkReset) {
        throw "$Operation requires -AllowLinkReset. WCH-Link software reset is not a safe default on this setup."
    }
    if ($script:RunningInCodex -and (-not $AllowCodexHardwareAccess)) {
        Assert-CodexHardwareAccess $Operation
    }
}

function Assert-OpenOCDFlashAllowed {
    param([string]$Operation)
    if (-not $AllowOpenOCDFlash) {
        throw "$Operation requires -AllowOpenOCDFlash. OpenOCD program/fallback exits the daemon and can trigger the WCH-Link second-init bug; prefer MRS DLL flash."
    }
    if ($script:RunningInCodex -and (-not $AllowCodexHardwareAccess)) {
        Assert-CodexHardwareAccess $Operation
    }
}

function Assert-OpenOCDStartAllowed {
    param([string]$Operation)
    if (-not $StartOpenOCD) {
        throw "$Operation requires an already-running OpenOCD daemon. Start OpenOCD once outside this script and keep it open, or pass -StartOpenOCD explicitly."
    }
    if ($script:RunningInCodex -and (-not $AllowCodexHardwareAccess)) {
        Assert-CodexHardwareAccess $Operation
    }
}

function Assert-TargetResetAllowed {
    param([string]$Operation)
    if (-not $AllowTargetReset) {
        throw "$Operation requires -AllowTargetReset. The safe default is halt-only; reset halt can stop V3F before it wakes V5F."
    }
    if ($script:RunningInCodex -and (-not $AllowCodexHardwareAccess)) {
        Assert-CodexHardwareAccess $Operation
    }
}

function Convert-ToForwardPath {
    param([string]$Path)
    return ($Path -replace "\\", "/")
}

function Get-ShortPath {
    param([string]$Path)
    if (-not $Path) { return "" }
    if (-not (Test-Path -LiteralPath $Path)) { return $Path }
    try {
        $fso = New-Object -ComObject Scripting.FileSystemObject
        $item = $fso.GetFolder($Path)
        return $item.ShortPath
    } catch {
        return $Path
    }
}

function Resolve-ExistingPath {
    param([string]$Path)
    if (Test-Path -LiteralPath $Path) {
        return (Resolve-Path -LiteralPath $Path).Path
    }
    return [System.IO.Path]::GetFullPath($Path)
}

function Get-UniqueExistingDirs {
    param([string[]]$Dirs)
    $result = New-Object System.Collections.Generic.List[string]
    foreach ($dir in $Dirs) {
        if (-not $dir) { continue }
        if ((Test-Path -LiteralPath $dir) -and (-not $result.Contains($dir))) {
            [void]$result.Add($dir)
        }
    }
    return $result.ToArray()
}

function Find-MRSToolchain {
    param([string]$BasePath)

    $roots = @(
        $BasePath,
        "C:\MounRiver\MounRiver_Studio2",
        "C:\MounRiver\MounRiver_Studio",
        "C:\Program Files\MounRiver_Studio2",
        "C:\Program Files (x86)\MounRiver_Studio2"
    )

    foreach ($root in $roots) {
        if (-not $root) { continue }

        $openocd = Join-Path $root "resources\app\resources\win32\components\WCH\OpenOCD\OpenOCD\bin\openocd.exe"
        $openocdBin = Split-Path $openocd -Parent
        $make = Join-Path $root "resources\app\resources\win32\others\Build_Tools\Make\bin\make.exe"
        $communicationLib = Join-Path $root "resources\app\resources\win32\components\WCH\Others\CommunicationLib\default"
        $firmwareLink = Join-Path $root "resources\app\resources\win32\components\WCH\Others\Firmware_Link\default"

        $gcc12Bin = Join-Path $root "resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC12\bin"
        $gcc15Bin = Join-Path $root "resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC15\bin"
        $gcc12 = Join-Path $gcc12Bin "riscv-wch-elf-gcc.exe"
        $gdb12 = Join-Path $gcc12Bin "riscv-wch-elf-gdb.exe"
        $gcc15 = Join-Path $gcc15Bin "riscv32-wch-elf-gcc.exe"
        $gdb15 = Join-Path $gcc15Bin "riscv32-wch-elf-gdb.exe"

        if (-not (Test-Path -LiteralPath $openocd)) { continue }

        $gcc = $null
        $gdb = $null
        $prefix = $null
        $primaryBin = $null

        # WCH EVT projects in this workspace use GCC12 names in generated makefiles.
        if (Test-Path -LiteralPath $gcc12) {
            $gcc = $gcc12
            $gdb = $gdb12
            $prefix = "riscv-wch-elf-"
            $primaryBin = $gcc12Bin
        } elseif (Test-Path -LiteralPath $gcc15) {
            $gcc = $gcc15
            $gdb = $gdb15
            $prefix = "riscv32-wch-elf-"
            $primaryBin = $gcc15Bin
        }

        $pathDirs = Get-UniqueExistingDirs @(
            (Split-Path $make -Parent),
            $gcc12Bin,
            $gcc15Bin,
            $primaryBin,
            $openocdBin
        )

        return @{
            MRSPath = $root
            OpenOCD = $openocd
            OpenOCDBin = $openocdBin
            CommunicationLib = $communicationLib
            FirmwareLink = $firmwareLink
            Make = if (Test-Path -LiteralPath $make) { $make } else { "make" }
            GCC = $gcc
            GDB = $gdb
            Prefix = $prefix
            GCCBin = $primaryBin
            PathDirs = $pathDirs
            WCHLinkUpdateTool = Join-Path $root "resources\app\resources\win32\components\WCH\Others\WCHLinkEJtagUpdTool\default\WCHLinkEJtagUpdTool.exe"
        }
    }

    return $null
}

function Add-ToolchainPath {
    param([hashtable]$Toolchain)
    if (-not $Toolchain) { return }
    $prefix = ($Toolchain.PathDirs | Where-Object { $_ }) -join ";"
    if ($prefix) {
        $env:PATH = "$prefix;$env:PATH"
    }
}

function Get-WCHLinkDevices {
    try {
        return Get-PnpDevice -ErrorAction Stop | Where-Object {
            $_.InstanceId -match "VID_1A86.*PID_801[012]" -or
            $_.FriendlyName -like "*WCH-Link*" -or
            $_.FriendlyName -like "*CMSIS-DAP*"
        }
    } catch {
        return @()
    }
}

function Get-WCHLinkMode {
    $devices = @(Get-WCHLinkDevices)
    if ($devices.Count -eq 0) { return "none" }
    if ($devices | Where-Object { $_.FriendlyName -like "*CMSIS-DAP*" }) { return "cmsis-dap-or-riscv" }
    if ($devices | Where-Object { $_.FriendlyName -like "*WCH-Link*" }) { return "wch-link" }
    return "unknown"
}

function Kill-StaleWCHProcesses {
    $staleNames = @("openocd", "gdb", "riscv-wch-elf-gdb", "riscv32-wch-elf-gdb")
    foreach ($procName in $staleNames) {
        $procs = Get-Process -Name $procName -ErrorAction SilentlyContinue
        foreach ($proc in $procs) {
            Write-Warn "Terminating stale process: $($proc.ProcessName) (PID $($proc.Id))"
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        }
    }
    Start-Sleep -Milliseconds 800
}

function Test-TcpPort {
    param([string]$HostName = "localhost", [int]$Port, [int]$TimeoutMs = 1000)
    try {
        $client = New-Object System.Net.Sockets.TcpClient
        $client.Connect($HostName, $Port)
        $client.Close()
        return $true
    } catch {
        return $false
    }
}

function Find-RunningOpenOCD {
    $procs = Get-Process -Name "openocd" -ErrorAction SilentlyContinue
    if ($procs) {
        return $procs | Select-Object -First 1
    }
    return $null
}

function Reset-WCHLinkUSB {
    <#
    .SYNOPSIS
        Software-reset WCH-Link by disabling/enabling its USB composite device.
        This simulates a physical re-plug without touching the hardware.
    #>
    Write-Step "WCH-Link software reset"
    $composite = Get-PnpDevice -Class USB | Where-Object {
        $_.InstanceId -match '^USB\\VID_1A86&PID_801[012]\\' -and
        $_.FriendlyName -like "*Composite*"
    } | Where-Object { $_.Status -eq 'OK' } | Select-Object -First 1

    if (-not $composite) {
        Write-Warn "No active WCH-Link USB composite device found. Trying any WCH-Link..."
        $composite = Get-PnpDevice | Where-Object {
            $_.InstanceId -match '^USB\\VID_1A86&PID_801[012]\\' -and
            $_.FriendlyName -notlike "*MI_*"
        } | Select-Object -First 1
    }

    if (-not $composite) {
        throw "WCH-Link USB device not found. Check cable and target power."
    }

    Write-Info "Found $($composite.FriendlyName) at $($composite.InstanceId)"
    if ($DryRun) {
        Write-Warn "Dry run: skipping USB disable/enable."
        return
    }

    try {
        Write-Info "Disabling device..."
        Disable-PnpDevice -InstanceId $composite.InstanceId -Confirm:$false -ErrorAction Stop
        Start-Sleep -Seconds 2
        Write-Info "Re-enabling device..."
        Enable-PnpDevice -InstanceId $composite.InstanceId -Confirm:$false -ErrorAction Stop
        Start-Sleep -Seconds 3
        Write-Ok "WCH-Link software reset complete. Wait a few seconds for driver re-initialization."
    } catch {
        throw "Failed to reset WCH-Link via PnP: $_"
    }
}

function Infer-Chip {
    param([string]$Dir, [string]$RequestedChip)
    if ($RequestedChip -ne "auto") { return $RequestedChip }
    return "CH32H417"
}

function Get-H417ProjectRoot {
    param([string]$Dir)
    $full = Resolve-ExistingPath $Dir
    $leaf = Split-Path $full -Leaf
    if (($leaf -ieq "V3F" -or $leaf -ieq "V5F") -and (Test-Path -LiteralPath (Join-Path (Split-Path $full -Parent) "Common"))) {
        return (Split-Path $full -Parent)
    }
    return $full
}

function Test-H417DualLayout {
    param([string]$Dir)
    $root = Get-H417ProjectRoot $Dir
    return (
        (Test-Path -LiteralPath (Join-Path $root "V3F\User")) -and
        (Test-Path -LiteralPath (Join-Path $root "V5F\User"))
    )
}

function Get-H417CoreInfo {
    param([ValidateSet("v3f", "v5f")] [string]$CoreName)
    if ($CoreName -eq "v3f") {
        return @{
            Name = "v3f"
            Upper = "V3F"
            Target = "wch_riscv.cpu.0"
            GdbPort = 3333
            Startup = "startup_ch32h417_v3f.S"
            Linker = "Link_v3f.ld"
            FlashOrigin = "0x00000000"
        }
    }
    return @{
        Name = "v5f"
        Upper = "V5F"
        Target = "wch_riscv.cpu.1"
        GdbPort = 3334
        Startup = "startup_ch32h417_v5f.S"
        Linker = "Link_v5f.ld"
        FlashOrigin = "0x00010000"
    }
}

function Resolve-Core {
    param([string]$Dir, [string]$RequestedCore, [string]$CurrentChip, [string]$Elf)
    if ($CurrentChip -ne "CH32H417") { return "single" }
    if ($RequestedCore -ne "auto") { return $RequestedCore }
    if ($Elf -match "V5F|v5f") { return "v5f" }
    if ($Elf -match "V3F|v3f") { return "v3f" }

    $leaf = Split-Path (Resolve-ExistingPath $Dir) -Leaf
    if ($leaf -ieq "V3F") { return "v3f" }
    if ($leaf -ieq "V5F") { return "v5f" }
    if (Test-H417DualLayout $Dir) { return "both" }
    return "v3f"
}

function Test-MRSProjectStructure {
    param([string]$Dir)
    if (Test-Path -LiteralPath (Join-Path $Dir "Makefile")) { return $true }
    if (Test-H417DualLayout $Dir) { return $true }
    $projectFiles = @(
        (Get-ChildItem -Path $Dir -Filter "*.wvproj" -ErrorAction SilentlyContinue),
        (Get-ChildItem -Path $Dir -Filter "*.wvsln" -ErrorAction SilentlyContinue),
        (Get-ChildItem -Path $Dir -Filter ".cproject" -ErrorAction SilentlyContinue),
        (Get-ChildItem -Path $Dir -Filter ".project" -ErrorAction SilentlyContinue)
    )
    return (($projectFiles | Where-Object { $_ }).Count -gt 0)
}

function Find-ElfFile {
    param([string]$Dir, [string]$CoreName = "single")
    $root = Get-H417ProjectRoot $Dir
    $patterns = New-Object System.Collections.Generic.List[string]

    if ($CoreName -eq "v3f") {
        [void]$patterns.Add((Join-Path $root "build\v3f\*.elf"))
        [void]$patterns.Add((Join-Path $root "V3F\obj\*V3F*.elf"))
        [void]$patterns.Add((Join-Path $root "V3F\obj\*.elf"))
        [void]$patterns.Add((Join-Path $root "*v3f*\build\*.elf"))
        [void]$patterns.Add((Join-Path $root "*V3F*\build\*.elf"))
    } elseif ($CoreName -eq "v5f") {
        [void]$patterns.Add((Join-Path $root "build\v5f\*.elf"))
        [void]$patterns.Add((Join-Path $root "V5F\obj\*V5F*.elf"))
        [void]$patterns.Add((Join-Path $root "V5F\obj\*.elf"))
        [void]$patterns.Add((Join-Path $root "*v5f*\build\*.elf"))
        [void]$patterns.Add((Join-Path $root "*V5F*\build\*.elf"))
    }

    [void]$patterns.Add((Join-Path $Dir "build\*.elf"))
    [void]$patterns.Add((Join-Path $Dir "obj\*.elf"))
    [void]$patterns.Add((Join-Path $Dir "*.elf"))
    [void]$patterns.Add((Join-Path $Dir "Debug\*.elf"))
    [void]$patterns.Add((Join-Path $Dir "Release\*.elf"))

    foreach ($pattern in $patterns) {
        $files = @(Get-ChildItem -Path $pattern -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)
        if ($files.Count -gt 0) { return $files[0].FullName }
    }
    return $null
}

function Find-ImageFile {
    param([string]$Dir, [string]$CoreName = "single")
    $root = Get-H417ProjectRoot $Dir
    $patterns = New-Object System.Collections.Generic.List[string]
    $extensions = @("*.hex", "*.bin")

    foreach ($ext in $extensions) {
        if ($CoreName -eq "v3f") {
            [void]$patterns.Add((Join-Path $root "build\v3f\$ext"))
            [void]$patterns.Add((Join-Path $root "V3F\obj\*V3F*$($ext.Substring(1))"))
            [void]$patterns.Add((Join-Path $root "V3F\obj\$ext"))
            [void]$patterns.Add((Join-Path $root "*v3f*\build\$ext"))
            [void]$patterns.Add((Join-Path $root "*V3F*\build\$ext"))
        } elseif ($CoreName -eq "v5f") {
            [void]$patterns.Add((Join-Path $root "build\v5f\$ext"))
            [void]$patterns.Add((Join-Path $root "V5F\obj\*V5F*$($ext.Substring(1))"))
            [void]$patterns.Add((Join-Path $root "V5F\obj\$ext"))
            [void]$patterns.Add((Join-Path $root "*v5f*\build\$ext"))
            [void]$patterns.Add((Join-Path $root "*V5F*\build\$ext"))
        }

        [void]$patterns.Add((Join-Path $Dir "build\$ext"))
        [void]$patterns.Add((Join-Path $Dir "obj\$ext"))
        [void]$patterns.Add((Join-Path $Dir $ext))
        [void]$patterns.Add((Join-Path $Dir "Debug\$ext"))
        [void]$patterns.Add((Join-Path $Dir "Release\$ext"))
    }

    foreach ($pattern in $patterns) {
        $files = @(Get-ChildItem -Path $pattern -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)
        if ($files.Count -gt 0) { return $files[0].FullName }
    }
    return $null
}

function Resolve-FlashImage {
    param([string]$Dir, [string]$CoreName = "single", [string]$ExplicitImage = "", [string]$ExplicitElf = "")

    if ($ExplicitImage) {
        return Resolve-ExistingPath $ExplicitImage
    }

    if ($ExplicitElf) {
        $candidate = Resolve-ExistingPath $ExplicitElf
        $ext = [System.IO.Path]::GetExtension($candidate).ToLowerInvariant()
        if ($ext -eq ".hex" -or $ext -eq ".bin") { return $candidate }

        $hex = [System.IO.Path]::ChangeExtension($candidate, ".hex")
        if (Test-Path -LiteralPath $hex) { return (Resolve-Path -LiteralPath $hex).Path }

        $bin = [System.IO.Path]::ChangeExtension($candidate, ".bin")
        if (Test-Path -LiteralPath $bin) { return (Resolve-Path -LiteralPath $bin).Path }

        throw "MRS DLL flash needs a .hex or .bin file. No sibling .hex/.bin found for $candidate"
    }

    return Find-ImageFile -Dir $Dir -CoreName $CoreName
}

function Find-H417ElfPair {
    param([string]$Dir)
    $v3 = if ($ElfPathV3F) { Resolve-ExistingPath $ElfPathV3F } else { Find-ElfFile -Dir $Dir -CoreName "v3f" }
    $v5 = if ($ElfPathV5F) { Resolve-ExistingPath $ElfPathV5F } else { Find-ElfFile -Dir $Dir -CoreName "v5f" }
    return @{ v3f = $v3; v5f = $v5 }
}

function Find-H417ImagePair {
    param([string]$Dir)
    $v3 = Resolve-FlashImage -Dir $Dir -CoreName "v3f" -ExplicitImage $ImagePathV3F -ExplicitElf $ElfPathV3F
    $v5 = Resolve-FlashImage -Dir $Dir -CoreName "v5f" -ExplicitImage $ImagePathV5F -ExplicitElf $ElfPathV5F
    return @{ v3f = $v3; v5f = $v5 }
}

function New-H417DualMakefile {
    param([string]$Root, [string]$EvtRoot)
    $makefile = Join-Path $Root "Makefile"
    if (Test-Path -LiteralPath $makefile) {
        Write-Warn "Makefile already exists, leaving it unchanged: $makefile"
        return
    }

    $scriptDir = Split-Path -Parent $PSCommandPath
    $template = Join-Path $scriptDir "Makefile.h417-dual.template"
    if (-not (Test-Path -LiteralPath $template)) {
        throw "Missing template: $template"
    }
    if (-not (Test-Path -LiteralPath $EvtRoot)) {
        throw "EVT root not found: $EvtRoot"
    }

    $projectName = Split-Path $Root -Leaf
    $content = Get-Content -LiteralPath $template -Raw -Encoding UTF8
    $content = $content.Replace("{{PROJECT_NAME}}", $projectName)
    $content = $content.Replace("{{EVT_ROOT}}", (Convert-ToForwardPath (Resolve-ExistingPath $EvtRoot)))
    Set-Content -LiteralPath $makefile -Value $content -Encoding UTF8
    Write-Ok "Generated CH32H417 dual-core Makefile: $makefile"
}

function New-GenericMakefile {
    param([string]$Dir, [string]$TargetChip)
    $makefile = Join-Path $Dir "Makefile"
    if (Test-Path -LiteralPath $makefile) {
        Write-Warn "Makefile already exists, leaving it unchanged: $makefile"
        return
    }

    $srcDirs = @()
    foreach ($name in @("src", "Src", "Core", "Startup", "Lib", "User", "Common")) {
        if (Test-Path -LiteralPath (Join-Path $Dir $name)) { $srcDirs += $name }
    }
    if ($srcDirs.Count -eq 0) { $srcDirs = @(".") }

    $ldItem = Get-ChildItem -Path $Dir -Filter "*.ld" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    $ldScript = if ($ldItem) { Convert-ToForwardPath (Resolve-Path -LiteralPath $ldItem.FullName -Relative) } else { "Link.ld" }

    $scriptDir = Split-Path -Parent $PSCommandPath
    $template = Join-Path $scriptDir "Makefile.template"
    $content = Get-Content -LiteralPath $template -Raw -Encoding UTF8
    $content = $content.Replace("{{CHIP}}", $TargetChip)
    $content = $content.Replace("{{TIMESTAMP}}", (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
    $content = $content.Replace("{{SRC_DIRS}}", ($srcDirs -join " "))
    $content = $content.Replace("{{LDSCRIPT}}", $ldScript)
    Set-Content -LiteralPath $makefile -Value $content -Encoding UTF8
    Write-Ok "Generated Makefile: $makefile"
}

function New-VSCodeConfig {
    param([string]$Root, [string]$TargetChip, [hashtable]$Toolchain)
    $vscodeDir = Join-Path $Root ".vscode"
    if (-not (Test-Path -LiteralPath $vscodeDir)) {
        New-Item -ItemType Directory -Path $vscodeDir | Out-Null
    }

    $scriptDir = Split-Path -Parent $PSCommandPath
    $scriptPath = Convert-ToForwardPath (Resolve-ExistingPath $PSCommandPath)
    $tasksTemplate = Join-Path $scriptDir "tasks.json.template"
    $launchTemplate = Join-Path $scriptDir "launch.json.template"

    if (Test-Path -LiteralPath $tasksTemplate) {
        $tasks = Get-Content -LiteralPath $tasksTemplate -Raw -Encoding UTF8
        $tasks = $tasks.Replace("{{CHIP}}", $TargetChip)
        $tasks = $tasks.Replace("{{SCRIPT}}", $scriptPath)
        Set-Content -LiteralPath (Join-Path $vscodeDir "tasks.json") -Value $tasks -Encoding UTF8
        Write-Ok "Generated .vscode/tasks.json"
    }

    if (Test-Path -LiteralPath $launchTemplate) {
        $launch = Get-Content -LiteralPath $launchTemplate -Raw -Encoding UTF8
        $cfgFile = if ($TargetChip -eq "CH32H417") { "wch-dual-core.cfg" } else { "wch-riscv.cfg" }
        $gdbExe = if ($Toolchain.GDB) { Split-Path $Toolchain.GDB -Leaf } else { "riscv-wch-elf-gdb.exe" }
        $launch = $launch.Replace("{{CHIP}}", $TargetChip)
        $launch = $launch.Replace("{{GDBEXE}}", $gdbExe)
        $launch = $launch.Replace("{{CFGFILE}}", $cfgFile)
        Set-Content -LiteralPath (Join-Path $vscodeDir "launch.json") -Value $launch -Encoding UTF8
        Write-Ok "Generated .vscode/launch.json"
    }
}

function Invoke-Detect {
    param([hashtable]$Toolchain, [string]$TargetChip)
    Write-Step "WCH environment"
    if (-not $Toolchain) {
        Write-Err "MounRiver Studio 2 toolchain was not found."
        return
    }

    Write-Ok "MRS2:     $($Toolchain.MRSPath)"
    Write-Ok "OpenOCD:  $($Toolchain.OpenOCD)"
    if (Test-Path -LiteralPath $Toolchain.CommunicationLib) {
        Write-Ok "MRS DLL:  $($Toolchain.CommunicationLib)"
    }
    Write-Ok "Make:     $($Toolchain.Make)"
    Write-Ok "GCC:      $($Toolchain.GCC)"
    Write-Ok "GDB:      $($Toolchain.GDB)"
    Write-Info "WCH-Link mode hint: $(Get-WCHLinkMode)"

    if ($TargetChip -eq "CH32H417") {
        Write-Step "CH32H417 mapping"
        Write-Host "V3F: target wch_riscv.cpu.0, GDB 3333, flash origin 0x00000000" -ForegroundColor White
        Write-Host "V5F: target wch_riscv.cpu.1, GDB 3334, flash origin 0x00010000" -ForegroundColor White
        Write-Host "Dual projects flash V5F first, then V3F, then resume." -ForegroundColor White
    }

    $root = if ($TargetChip -eq "CH32H417") { Get-H417ProjectRoot $ProjectDir } else { Resolve-ExistingPath $ProjectDir }
    if (Test-Path -LiteralPath (Join-Path $root "Makefile")) {
        Write-Ok "Project Makefile: $(Join-Path $root "Makefile")"
    } elseif (Test-MRSProjectStructure $root) {
        Write-Warn "No root Makefile found, but an MRS/EVT structure was detected."
        Write-Info "Run init to generate command-line build files."
    } else {
        Write-Warn "No Makefile or known MRS structure detected."
    }

    foreach ($coreName in @("v3f", "v5f")) {
        if ($TargetChip -eq "CH32H417") {
            $elf = Find-ElfFile -Dir $root -CoreName $coreName
            if ($elf) { Write-Ok "$($coreName.ToUpper()) ELF: $elf" }
        }
    }
}

function Invoke-Init {
    param([hashtable]$Toolchain, [string]$TargetChip)
    Write-Step "Project init"
    $root = if ($TargetChip -eq "CH32H417") { Get-H417ProjectRoot $ProjectDir } else { Resolve-ExistingPath $ProjectDir }

    if ($TargetChip -eq "CH32H417" -and (Test-H417DualLayout $root)) {
        New-H417DualMakefile -Root $root -EvtRoot $EVTRoot
    } else {
        New-GenericMakefile -Dir $root -TargetChip $TargetChip
    }
    New-VSCodeConfig -Root $root -TargetChip $TargetChip -Toolchain $Toolchain
}

function Invoke-MakeAt {
    param([hashtable]$Toolchain, [string]$Dir, [string[]]$MakeArgs)
    Push-Location $Dir
    try {
        Add-ToolchainPath $Toolchain
        Write-Info "make command: $($Toolchain.Make) $($MakeArgs -join ' ')"
        & $Toolchain.Make @MakeArgs
        if ($LASTEXITCODE -ne 0) { throw "make failed in $Dir with exit code $LASTEXITCODE" }
    } finally {
        Pop-Location
    }
}

function Invoke-Build {
    param([hashtable]$Toolchain, [string]$TargetChip, [string]$RequestedCore)
    Write-Step "Build"
    if (-not $Toolchain) { throw "MRS toolchain not found." }

    $root = if ($TargetChip -eq "CH32H417") { Get-H417ProjectRoot $ProjectDir } else { Resolve-ExistingPath $ProjectDir }
    $resolvedCore = Resolve-Core -Dir $root -RequestedCore $RequestedCore -CurrentChip $TargetChip -Elf $ElfPath

    if (-not (Test-Path -LiteralPath (Join-Path $root "Makefile"))) {
        if (Test-MRSProjectStructure $root) {
            Invoke-Init -Toolchain $Toolchain -TargetChip $TargetChip
        } else {
            throw "No Makefile and no supported MRS structure found in $root"
        }
    }

    if (Test-Path -LiteralPath (Join-Path $root "Makefile")) {
        $prefixFull = if ($Toolchain.GCCBin) { (Convert-ToForwardPath (Get-ShortPath $Toolchain.GCCBin)) + "/" + $Toolchain.Prefix } else { $Toolchain.Prefix }

        # Non-standard dual-core layout: build V3F and V5F in separate directories
        if ($resolvedCore -eq "both" -and -not (Test-H417DualLayout $root)) {
            $v3fDirs = @("v3f_wakeup", "V3F", "v3f")
            $v3fDir = $null
            foreach ($d in $v3fDirs) {
                $candidate = Join-Path $root $d
                if (Test-Path -LiteralPath (Join-Path $candidate "Makefile")) { $v3fDir = $candidate; break }
            }
            if (-not $v3fDir) {
                throw "Both-core build requested but no V3F Makefile found in $($v3fDirs -join ', ') under $root"
            }
            Write-Info "Building V5F in $root"
            Invoke-MakeAt -Toolchain $Toolchain -Dir $root -MakeArgs @("-j", "CHIP=$TargetChip", "CORE=v5f", "PREFIX=$prefixFull")
            Write-Info "Building V3F in $v3fDir"
            Invoke-MakeAt -Toolchain $Toolchain -Dir $v3fDir -MakeArgs @("-j", "CHIP=$TargetChip", "CORE=v3f", "PREFIX=$prefixFull")
            Write-Ok "Dual-core build finished"
            return
        }

        $coreArg = if ($resolvedCore -eq "single") { "CORE=single" } else { "CORE=$resolvedCore" }
        Write-Info "make -j $coreArg PREFIX=$prefixFull"
        Invoke-MakeAt -Toolchain $Toolchain -Dir $root -MakeArgs @("-j", "CHIP=$TargetChip", $coreArg, "PREFIX=$prefixFull")
        Write-Ok "Build finished"
        return
    }

    throw "Build setup failed for $root"
}

function Invoke-OpenOCD {
    param([hashtable]$Toolchain, [string[]]$OpenOCDArgs)
    Write-Info "openocd $($OpenOCDArgs -join ' ')"
    if ($DryRun) {
        Write-Warn "Dry run: OpenOCD was not executed."
        return
    }
    & $Toolchain.OpenOCD @OpenOCDArgs
    if ($LASTEXITCODE -ne 0) { throw "OpenOCD failed with exit code $LASTEXITCODE" }
}

function Get-MRSChipInfo {
    param([string]$TargetChip)
    switch ($TargetChip) {
        "CH32H417" {
            return @{
                ChipID = 198
                FlashAddress = if ($FlashAddress) { $FlashAddress } else { "0x08000000" }
            }
        }
        default {
            throw "MRS DLL flash path is not configured for $TargetChip. Use -FlashTool openocd."
        }
    }
}

function Get-MRSFlashOperationType {
    param([bool]$Reset, [bool]$EraseBefore)
    $operation = 0
    if ($DisablePowerOut) { $operation = $operation -bor 0x80 }
    if ($ClearCodeFlash) { $operation = $operation -bor 0x40 }
    if (-not $KeepCodeProtect) { $operation = $operation -bor 0x20 }
    if ($EnableSdiPrintf) { $operation = $operation -bor 0x10 }
    if ($EraseBefore -and $EraseAll) { $operation = $operation -bor 0x08 }
    $operation = $operation -bor 0x04
    if (-not $NoVerify) { $operation = $operation -bor 0x02 }
    if ($Reset -and (-not $NoReset)) { $operation = $operation -bor 0x01 }
    return $operation
}

function Invoke-MRSLink {
    param(
        [hashtable]$Toolchain,
        [string]$HelperAction,
        [int]$ChipID,
        [int]$OperationType = 0,
        [string]$DataFilePath = "",
        [string]$Address = "0x08000000"
    )

    if (-not (Test-Path -LiteralPath $Toolchain.CommunicationLib)) {
        throw "MRS CommunicationLib not found: $($Toolchain.CommunicationLib)"
    }

    $scriptDir = Split-Path -Parent $PSCommandPath
    $helper = Join-Path $scriptDir "mrs-link.ps1"
    if (-not (Test-Path -LiteralPath $helper)) {
        throw "Missing MRS DLL helper: $helper"
    }

    $ps32 = Join-Path $env:WINDIR "SysWOW64\WindowsPowerShell\v1.0\powershell.exe"
    $psExe = if (Test-Path -LiteralPath $ps32) { $ps32 } else { "powershell.exe" }
    $args = @(
        "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass",
        "-File", $helper,
        "-Action", $HelperAction,
        "-ChipID", $ChipID,
        "-DebugInterface", $DebugInterface,
        "-ClkSpeed", $ClkSpeed,
        "-OperationType", $OperationType,
        "-FlashAddress", $Address,
        "-CommunicationLibPath", $Toolchain.CommunicationLib,
        "-FirmwareLinkPath", $Toolchain.FirmwareLink
    )
    if ($DataFilePath) { $args += @("-DataFilePath", $DataFilePath) }

    Write-Info "MRS helper: $HelperAction chip=$ChipID interface=$DebugInterface clk=$ClkSpeed"
    if ($DataFilePath) { Write-Info "MRS target: $DataFilePath" }
    if ($DryRun) {
        Write-Warn "Dry run: MRS helper was not executed."
        Write-Info "$psExe $($args -join ' ')"
        return
    }

    & $psExe @args
    if ($LASTEXITCODE -ne 0) { throw "MRS helper failed with exit code $LASTEXITCODE" }
}

function Invoke-MRSSetTarget {
    param([hashtable]$Toolchain, [string]$TargetChip)
    if ($TargetChip -ne "CH32H417") { return }
    $chipInfo = Get-MRSChipInfo -TargetChip $TargetChip
    Invoke-MRSLink -Toolchain $Toolchain -HelperAction "set-line-mode" -ChipID $chipInfo.ChipID -Address $chipInfo.FlashAddress
}

function Invoke-MRSReset {
    param([hashtable]$Toolchain, [string]$TargetChip)
    if ($TargetChip -ne "CH32H417") { return }
    $chipInfo = Get-MRSChipInfo -TargetChip $TargetChip
    Invoke-MRSLink -Toolchain $Toolchain -HelperAction "reset" -ChipID $chipInfo.ChipID -Address $chipInfo.FlashAddress
}

function Invoke-MRSFlashImage {
    param([hashtable]$Toolchain, [string]$TargetChip, [string]$Image, [bool]$ResetAfter, [bool]$EraseBefore = $true, [string]$Address = "")
    $chipInfo = Get-MRSChipInfo -TargetChip $TargetChip
    if (-not $Address) { $Address = $chipInfo.FlashAddress }
    $operation = Get-MRSFlashOperationType -Reset $ResetAfter -EraseBefore $EraseBefore
    Write-Info ("MRS operation: 0x{0:X2} (disable-protect={1}, erase-all={2}, program, verify={3}, reset={4})" -f $operation, (-not $KeepCodeProtect), ($EraseBefore -and $EraseAll), (-not $NoVerify), ($ResetAfter -and (-not $NoReset)))
    Invoke-MRSLink -Toolchain $Toolchain -HelperAction "flash" -ChipID $chipInfo.ChipID -OperationType $operation -DataFilePath $Image -Address $Address
}

function Invoke-Flash {
    param([hashtable]$Toolchain, [string]$TargetChip, [string]$RequestedCore)
    Write-Step "Flash"
    if (-not $Toolchain) { throw "MRS toolchain not found." }

    $root = if ($TargetChip -eq "CH32H417") { Get-H417ProjectRoot $ProjectDir } else { Resolve-ExistingPath $ProjectDir }
    $resolvedCore = Resolve-Core -Dir $root -RequestedCore $RequestedCore -CurrentChip $TargetChip -Elf $ElfPath
    $resolvedFlashTool = if ($FlashTool -eq "auto") { "mrs" } else { $FlashTool }

    if (($resolvedFlashTool -ne "mrs") -and (-not $DryRun)) {
        Assert-OpenOCDFlashAllowed "OpenOCD flash"
    }

    if ($KillStaleDebuggers) {
        Kill-StaleWCHProcesses
    } else {
        Write-Info "Preserving existing OpenOCD/GDB processes."
    }

    if ($resolvedFlashTool -eq "mrs") {
        if ($TargetChip -ne "CH32H417") {
            throw "MRS DLL flash is currently enabled for CH32H417 only. Use -FlashTool openocd for $TargetChip."
        }

        if ($resolvedCore -eq "both") {
            $pair = Find-H417ImagePair -Dir $root
            if (-not $pair.v3f -or -not $pair.v5f) {
                throw "Both-core MRS flash needs V3F and V5F .hex/.bin files. Build first or pass -ImagePathV3F and -ImagePathV5F."
            }
            Write-Info "V3F image: $($pair.v3f)"
            Write-Info "V5F image: $($pair.v5f)"

            $mrsOk = $false
            try {
                Invoke-MRSFlashImage -Toolchain $Toolchain -TargetChip $TargetChip -Image $pair.v3f -ResetAfter:$false -EraseBefore:$true -Address "0x08000000"
                Write-Info "Waiting 1s for target to settle after V3F flash..."
                Start-Sleep -Seconds 1
                Invoke-MRSFlashImage -Toolchain $Toolchain -TargetChip $TargetChip -Image $pair.v5f -ResetAfter:$true -EraseBefore:$false -Address "0x08010000"
                $mrsOk = $true
            } catch {
                Write-Warn "MRS DLL flash failed: $_"
                if ($RecoverMode -and $AllowOpenOCDFlash) {
                    Write-Warn "RecoverMode + AllowOpenOCDFlash enabled: attempting OpenOCD fallback..."
                } else {
                    if ($RecoverMode) {
                        Write-Warn "RecoverMode no longer falls back to OpenOCD by default. Pass -AllowOpenOCDFlash only if you intentionally accept the second-init risk."
                    }
                    throw
                }
            }

            if ($mrsOk) {
                Write-Ok "Dual-core MRS flash finished"
                return
            }

            # RecoverMode fallback via OpenOCD
            Assert-OpenOCDFlashAllowed "OpenOCD fallback flash"
            if ($KillStaleDebuggers) { Kill-StaleWCHProcesses }
            Invoke-MRSSetTarget -Toolchain $Toolchain -TargetChip $TargetChip
            if (-not $KeepCodeProtect) {
                Write-Warn "OpenOCD fallback does not use the MRS DLL Code-Protect unlock operation."
            }
            $baseArgs = @("-s", $Toolchain.OpenOCDBin, "-f", "wch-dual-core.cfg", "-c", "init")
            $v3 = Convert-ToForwardPath $pair.v3f
            $v5 = Convert-ToForwardPath $pair.v5f
            $args = $baseArgs + @(
                "-c", "targets wch_riscv.cpu.1",
                "-c", "halt",
                "-c", "program `"$v5`" verify",
                "-c", "resume",
                "-c", "targets wch_riscv.cpu.0",
                "-c", "halt",
                "-c", "program `"$v3`" verify",
                "-c", "resume",
                "-c", "exit"
            )
            Invoke-OpenOCD -Toolchain $Toolchain -OpenOCDArgs $args
            Write-Ok "Dual-core OpenOCD fallback flash finished"
            return
        }

        $image = Resolve-FlashImage -Dir $root -CoreName $resolvedCore -ExplicitImage $ImagePath -ExplicitElf $ElfPath
        if (-not $image) { throw "Flash image not found for $resolvedCore. Build first or pass -ImagePath." }
        if ($resolvedCore -eq "v5f") {
            Write-Warn "V5F-only flash will not normally run after reset unless a V3F image wakes V5F."
        }
        Write-Info "$($resolvedCore.ToUpper()) image: $image"

        $mrsOk = $false
        try {
            Invoke-MRSFlashImage -Toolchain $Toolchain -TargetChip $TargetChip -Image $image -ResetAfter:$true
            $mrsOk = $true
        } catch {
            Write-Warn "MRS DLL flash failed: $_"
            if ($RecoverMode -and $AllowOpenOCDFlash) {
                Write-Warn "RecoverMode + AllowOpenOCDFlash enabled: attempting OpenOCD fallback..."
            } else {
                if ($RecoverMode) {
                    Write-Warn "RecoverMode no longer falls back to OpenOCD by default. Pass -AllowOpenOCDFlash only if you intentionally accept the second-init risk."
                }
                throw
            }
        }

        if ($mrsOk) {
            Write-Ok "MRS flash finished"
            return
        }

        # RecoverMode fallback via OpenOCD
        Assert-OpenOCDFlashAllowed "OpenOCD fallback flash"
        if ($KillStaleDebuggers) { Kill-StaleWCHProcesses }
        Invoke-MRSSetTarget -Toolchain $Toolchain -TargetChip $TargetChip
        if (-not $KeepCodeProtect) {
            Write-Warn "OpenOCD fallback does not use the MRS DLL Code-Protect unlock operation."
        }
        $info = Get-H417CoreInfo $resolvedCore
        $elf = if ($ElfPath) { Resolve-ExistingPath $ElfPath } else { Find-ElfFile -Dir $root -CoreName $resolvedCore }
        if (-not $elf) { throw "ELF not found for $resolvedCore. Build first or pass -ElfPath." }
        $elfForward = Convert-ToForwardPath $elf
        $programCmd = if ($resolvedCore -eq "v3f") { "program `"$elfForward`" verify reset" } else { "program `"$elfForward`" verify" }
        $args = @("-s", $Toolchain.OpenOCDBin, "-f", "wch-dual-core.cfg", "-c", "init") + @(
            "-c", "targets $($info.Target)",
            "-c", "halt",
            "-c", $programCmd,
            "-c", "exit"
        )
        Write-Info "OpenOCD fallback for $resolvedCore"
        Invoke-OpenOCD -Toolchain $Toolchain -OpenOCDArgs $args
        Write-Ok "OpenOCD fallback flash finished"
        return
    }

    if ($TargetChip -eq "CH32H417") {
        Invoke-MRSSetTarget -Toolchain $Toolchain -TargetChip $TargetChip
        if (-not $KeepCodeProtect) {
            Write-Warn "OpenOCD flash fallback does not use the MRS DLL Code-Protect unlock operation. Prefer -FlashTool mrs for CH32H417."
        }
        $baseArgs = @("-s", $Toolchain.OpenOCDBin, "-f", "wch-dual-core.cfg", "-c", "init")

        if ($resolvedCore -eq "both") {
            $pair = Find-H417ElfPair -Dir $root
            if (-not $pair.v3f -or -not $pair.v5f) {
                throw "Both-core flash needs V3F and V5F ELF files. Build first or pass -ElfPathV3F and -ElfPathV5F."
            }

            $v3 = Convert-ToForwardPath $pair.v3f
            $v5 = Convert-ToForwardPath $pair.v5f
            # Flash V5F first, keep it halted, then flash V3F and resume.
            # V3F will call NVIC_WakeUp_V5F to wake V5F.
            # If V5F is resumed before V3F wakes it, NVIC_WakeUp_V5F may
            # interrupt V5F mid-initialization and leave it in an inconsistent state.
            $args = $baseArgs + @(
                "-c", "targets wch_riscv.cpu.1",
                "-c", "halt",
                "-c", "program `"$v5`" verify",
                "-c", "targets wch_riscv.cpu.0",
                "-c", "halt",
                "-c", "program `"$v3`" verify",
                "-c", "resume",
                "-c", "exit"
            )
            Write-Info "V3F ELF: $($pair.v3f)"
            Write-Info "V5F ELF: $($pair.v5f)"
            Invoke-OpenOCD -Toolchain $Toolchain -OpenOCDArgs $args
            Write-Ok "Dual-core flash finished"
            return
        }

        $info = Get-H417CoreInfo $resolvedCore
        $elf = if ($ElfPath) { Resolve-ExistingPath $ElfPath } else { Find-ElfFile -Dir $root -CoreName $resolvedCore }
        if (-not $elf) { throw "ELF not found for $resolvedCore. Build first or pass -ElfPath." }
        $elfForward = Convert-ToForwardPath $elf
        $programCmd = if ($resolvedCore -eq "v3f") { "program `"$elfForward`" verify reset" } else { "program `"$elfForward`" verify" }
        $args = $baseArgs + @(
            "-c", "targets $($info.Target)",
            "-c", "halt",
            "-c", $programCmd,
            "-c", "exit"
        )
        Write-Info "$($info.Upper) ELF: $elf"
        if ($resolvedCore -eq "v5f") {
            Write-Warn "V5F-only flash will not normally run after reset unless a V3F image wakes V5F."
        }
        Invoke-OpenOCD -Toolchain $Toolchain -OpenOCDArgs $args
        Write-Ok "Flash finished"
        return
    }

    $elfSingle = if ($ElfPath) { Resolve-ExistingPath $ElfPath } else { Find-ElfFile -Dir $root -CoreName "single" }
    if (-not $elfSingle) { throw "ELF not found. Build first or pass -ElfPath." }
    $singleForward = Convert-ToForwardPath $elfSingle
    Invoke-OpenOCD -Toolchain $Toolchain -OpenOCDArgs @(
        "-s", $Toolchain.OpenOCDBin,
        "-f", "wch-riscv.cfg",
        "-c", "init",
        "-c", "halt",
        "-c", "program `"$singleForward`" verify reset",
        "-c", "exit"
    )
    Write-Ok "Flash finished"
}

function Invoke-Debug {
    param([hashtable]$Toolchain, [string]$TargetChip, [string]$RequestedCore, [bool]$CheckOnly)
    Write-Step $(if ($CheckOnly) { "Debug check" } else { "Interactive debug" })
    if (-not $Toolchain) { throw "MRS toolchain not found." }
    if (-not $Toolchain.GDB) { throw "GDB not found in the MRS toolchain." }

    $root = if ($TargetChip -eq "CH32H417") { Get-H417ProjectRoot $ProjectDir } else { Resolve-ExistingPath $ProjectDir }
    $resolvedCore = Resolve-Core -Dir $root -RequestedCore $RequestedCore -CurrentChip $TargetChip -Elf $ElfPath
    if ($resolvedCore -eq "both") { throw "Debug one core at a time: use -Core v3f or -Core v5f." }

    $cfgFile = if ($TargetChip -eq "CH32H417") { "wch-dual-core.cfg" } else { "wch-riscv.cfg" }
    $port = 3333
    $elfCore = "single"
    if ($TargetChip -eq "CH32H417") {
        $info = Get-H417CoreInfo $resolvedCore
        $port = $info.GdbPort
        $elfCore = $resolvedCore
    }
    if ($GdbPort -gt 0) { $port = $GdbPort }

    $elf = if ($ElfPath) { Resolve-ExistingPath $ElfPath } else { Find-ElfFile -Dir $root -CoreName $elfCore }
    if (-not $elf) { throw "ELF not found. Build first or pass -ElfPath." }

    # CRITICAL: OpenOCD 0.11.0+dev-snapshot for WCH cannot be started a second time
    # after any previous OpenOCD process exits. Only re-plugging WCH-Link restores it.
    # Therefore, NEVER start a new OpenOCD if one is already running. Reuse the daemon.
    $ocdProc = Find-RunningOpenOCD
    $weStartedOCD = $false
    $codexSafeMode = $script:RunningInCodex -and (-not $AllowCodexHardwareAccess)

    if ($ocdProc) {
        Write-Ok "Reusing existing OpenOCD daemon (PID $($ocdProc.Id))"
    } else {
        if ($DryRun) {
            Write-Warn "Dry run: OpenOCD/GDB were not executed."
            Write-Info "OpenOCD would be required on $cfgFile"
            Write-Info "GDB port: $port"
            Write-Info "ELF: $elf"
            return
        }
        Assert-OpenOCDStartAllowed "starting OpenOCD"
        if ($TargetChip -eq "CH32H417") {
            Invoke-MRSSetTarget -Toolchain $Toolchain -TargetChip $TargetChip
            if ($AllowTargetReset) {
                Write-Warn "AllowTargetReset enabled: MRS reset before OpenOCD start."
                Invoke-MRSReset -Toolchain $Toolchain -TargetChip $TargetChip
            }
        }

        $windowStyle = if ($VisibleOpenOCD) { "Normal" } else { "Hidden" }
        $ocdArgs = @("-s", $Toolchain.OpenOCDBin, "-f", $cfgFile)
        if ($TargetChip -eq "CH32H417") {
            $info = Get-H417CoreInfo $resolvedCore
            # Arguments containing spaces must be explicitly quoted for Start-Process.
            $ocdArgs += @("-c", "init", "-c", "`"targets $($info.Target)`"")
            if ($AllowTargetReset) {
                $ocdArgs += @("-c", "`"reset halt`"")
            } else {
                $ocdArgs += @("-c", "`"halt`"")
            }
        }
        Write-Info "Starting OpenOCD on $cfgFile"
        if ($DryRun) {
            Write-Warn "Dry run: OpenOCD/GDB were not executed."
            Write-Info "OpenOCD args: $($ocdArgs -join ' ')"
            Write-Info "GDB port: $port"
            Write-Info "ELF: $elf"
            return
        }
        $ocdProc = Start-Process -FilePath $Toolchain.OpenOCD -ArgumentList $ocdArgs -PassThru -WindowStyle $windowStyle
        $weStartedOCD = $true
        Start-Sleep -Seconds 2

        # Verify OpenOCD actually came up and is listening.
        $portReady = $false
        for ($i = 0; $i -lt 6; $i++) {
            if (Test-TcpPort -Port $port -TimeoutMs 500) { $portReady = $true; break }
            Start-Sleep -Milliseconds 800
            if ($ocdProc.HasExited) { break }
        }

        if (-not $portReady) {
            Write-Warn "OpenOCD did not open GDB port $port (WCH-Link second-init bug?)."
            if ($RecoverMode -or $RestartLink) {
                Write-Warn "Automatic recovery is disabled. Do not reset-link or restart OpenOCD from this script."
            }
            throw "OpenOCD failed to start cleanly. Leave recovery to MRS/external tools, then start one daemon and keep it open."
        }
    }

    try {
        Add-ToolchainPath $Toolchain
        if ($codexSafeMode -and (-not (Test-TcpPort -Port $port -TimeoutMs 1000))) {
            throw "Codex hardware guard: OpenOCD is running but GDB port $port is not reachable. Do not restart it from Codex; check the daemon window."
        }

        if ($CheckOnly) {
            $haltCommand = if ($AllowTargetReset) { "monitor reset halt" } else { "monitor halt" }
            if ($AllowTargetReset -and $TargetChip -eq "CH32H417" -and $resolvedCore -eq "v5f") {
                Write-Warn "V5F is normally woken by V3F; using halt instead of reset halt."
                $haltCommand = "monitor halt"
            }
            $gdbArgs = @(
                "-batch",
                $elf,
                "-ex", "set mem inaccessible-by-default off",
                "-ex", "set architecture riscv:rv32",
                "-ex", "set remotetimeout 30",
                "-ex", "set disassembler-options xw",
                "-ex", "target remote localhost:$port",
                "-ex", $haltCommand,
                "-ex", "info registers pc sp gp",
                "-ex", "detach",
                "-ex", "quit"
            )
        } else {
            $haltCommand = if ($AllowTargetReset) { "monitor reset halt" } else { "monitor halt" }
            if ($AllowTargetReset -and $TargetChip -eq "CH32H417" -and $resolvedCore -eq "v5f") {
                Write-Warn "V5F is normally woken by V3F; using halt instead of reset halt."
                $haltCommand = "monitor halt"
            }
            $gdbArgs = @(
                $elf,
                "-ex", "set mem inaccessible-by-default off",
                "-ex", "set architecture riscv:rv32",
                "-ex", "set remotetimeout unlimited",
                "-ex", "set disassembler-options xw",
                "-ex", "target remote localhost:$port",
                "-ex", $haltCommand
            )
            if ($LoadBeforeDebug) {
                Assert-OpenOCDFlashAllowed "GDB load"
                Write-Warn "GDB load bypasses the MRS DLL Code-Protect unlock path. Use flash first unless this is intentional."
                $gdbArgs += @("-ex", "load")
            }
            $gdbArgs += @("-ex", "break main")
        }

        Write-Info "GDB port: $port"
        if ($DryRun) {
            Write-Warn "Dry run: GDB was not executed."
            Write-Info "GDB args: $($gdbArgs -join ' ')"
            return
        }
        & $Toolchain.GDB @gdbArgs
        if ($LASTEXITCODE -ne 0) { throw "GDB failed with exit code $LASTEXITCODE" }
    } finally {
        # Do NOT kill OpenOCD after debug-check. Keeping the daemon alive
        # avoids the WCH-Link second-init bug. Only kill it for interactive
        # debug if the user explicitly wants to, or if we didn't start it.
        if ($weStartedOCD -and $ocdProc -and -not $ocdProc.HasExited -and (-not $StopOpenOCDAfterDebug)) {
            Write-Info "Leaving OpenOCD daemon running for future sessions."
            # Intentionally NOT killing the process.
        } elseif ($ocdProc -and -not $ocdProc.HasExited -and $weStartedOCD -and $StopOpenOCDAfterDebug) {
            if ($script:RunningInCodex -and (-not $AllowCodexHardwareAccess)) {
                Assert-CodexHardwareAccess "stopping OpenOCD"
            }
            Write-Warn "Stopping OpenOCD. The next OpenOCD start may fail until WCH-Link is re-plugged or software-reset."
            Stop-Process -Id $ocdProc.Id -Force -ErrorAction SilentlyContinue
        }
    }
}

$ProjectDir = Resolve-ExistingPath $ProjectDir
$Chip = Infer-Chip -Dir $ProjectDir -RequestedChip $Chip
$toolchain = Find-MRSToolchain -BasePath $MRSPath

switch ($Action) {
    "detect" {
        Invoke-Detect -Toolchain $toolchain -TargetChip $Chip
    }
    "init" {
        Invoke-Init -Toolchain $toolchain -TargetChip $Chip
    }
    "build" {
        Invoke-Build -Toolchain $toolchain -TargetChip $Chip -RequestedCore $Core
    }
    "flash" {
        if ($RestartLink) { Assert-LinkResetAllowed "flash -RestartLink" }
        if (($FlashTool -eq "openocd") -and (-not $DryRun)) { Assert-OpenOCDFlashAllowed "OpenOCD flash" }
        if ($RestartLink) { Reset-WCHLinkUSB }
        Invoke-Flash -Toolchain $toolchain -TargetChip $Chip -RequestedCore $Core
    }
    "debug" {
        if ($RestartLink) { Assert-LinkResetAllowed "debug -RestartLink" }
        if ($RestartLink) { Reset-WCHLinkUSB }
        Invoke-Debug -Toolchain $toolchain -TargetChip $Chip -RequestedCore $Core -CheckOnly:$false
    }
    "debug-check" {
        if ($RestartLink) { Assert-LinkResetAllowed "debug-check -RestartLink" }
        if ($RestartLink) { Reset-WCHLinkUSB }
        Invoke-Debug -Toolchain $toolchain -TargetChip $Chip -RequestedCore $Core -CheckOnly:$true
    }
    "reset-link" {
        Assert-LinkResetAllowed "reset-link"
        Reset-WCHLinkUSB
    }
    "loop" {
        if ($RestartLink) { Assert-LinkResetAllowed "loop -RestartLink" }
        if ($RestartLink) { Reset-WCHLinkUSB }
        if (-not $SkipBuild) {
            Invoke-Build -Toolchain $toolchain -TargetChip $Chip -RequestedCore $Core
        }
        if (-not $SkipFlash) {
            Invoke-Flash -Toolchain $toolchain -TargetChip $Chip -RequestedCore $Core
        }
        Write-Host ""
        Write-Info "For noninteractive debug validation:"
        Write-Host "  .\wch-auto.ps1 -Action debug-check -ProjectDir `"$ProjectDir`" -Chip $Chip -Core v3f" -ForegroundColor White
        if ($Chip -eq "CH32H417") {
            Write-Host "  .\wch-auto.ps1 -Action debug-check -ProjectDir `"$ProjectDir`" -Chip $Chip -Core v5f" -ForegroundColor White
        }
    }
}
