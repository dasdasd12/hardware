#requires -Version 5.1

$ErrorActionPreference = "Stop"

$skillRoot = Split-Path -Parent (Split-Path -Parent $PSCommandPath)
$autoPath = Join-Path $skillRoot "scripts\wch-auto.ps1"
$mrsPath = Join-Path $skillRoot "scripts\mrs-link.ps1"
$skillPath = Join-Path $skillRoot "SKILL.md"
$referencePath = Join-Path $skillRoot "references\h417-dual-core.md"
$launchPath = Join-Path $skillRoot "scripts\launch.json.template"

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Get-RawText {
    param([string]$Path)
    return Get-Content -LiteralPath $Path -Raw -Encoding UTF8
}

foreach ($path in @($autoPath, $mrsPath)) {
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors) | Out-Null
    Assert-True ($errors.Count -eq 0) "PowerShell parse errors in $path"
}

$auto = Get-RawText $autoPath
$mrs = Get-RawText $mrsPath
$skill = Get-RawText $skillPath
$reference = Get-RawText $referencePath
$launch = Get-RawText $launchPath

Assert-True ($mrs -notmatch 'if \(\$Action -eq "rehandshake"\)\s*\{\s*exit 0\s*\}') `
    "rehandshake must not exit 0 without validating target state"
Assert-True ($mrs -match '\$linkOk\s*=\s*\(\$linkedChip -eq \$ChipID\)') `
    "rehandshake/dump-link-status must validate linked chip ID"
Assert-True ($mrs -match '\$protectReadable\s*=\s*\(\$protect -eq 3 -or \$protect -eq 4\)') `
    "rehandshake/dump-link-status must classify readable RDP state"

Assert-True ($auto -match 'function Invoke-MRSSetTarget[\s\S]*-HelperAction "set-target"') `
    "Invoke-MRSSetTarget must call helper action set-target"
Assert-True ($auto -notmatch 'function Invoke-MRSSetTarget[\s\S]*-HelperAction "set-line-mode"') `
    "Invoke-MRSSetTarget must not call set-line-mode"

Assert-True ($auto -notmatch 'RecoverMode enabled: attempting OpenOCD fallback') `
    "RecoverMode must not automatically fall back to OpenOCD"
Assert-True ($auto -match 'RecoverMode enabled: running recover, then retrying MRS DLL flash once') `
    "RecoverMode should recover and retry the MRS DLL flash path"
Assert-True ($auto -match 'function Set-FlashFailureLockout') `
    "MRS flash failures must write a lockout sentinel"
Assert-True ($auto -match 'function Assert-NoFlashFailureLockout') `
    "normal flash must refuse to continue after a previous risky failure"
Assert-True ($auto -match 'function Invoke-MRSFlashPreflight[\s\S]*-HelperAction "dump-link-status"') `
    "MRS flash must preflight link/RDP state before programming"
Assert-True ($auto -match 'Clear-FlashFailureLockout[\s\S]*Recovery complete') `
    "recover must clear flash lockout only after successful recovery"
Assert-True ($auto -notmatch 'MRS DLL flash failed; pulsing target NRST and retrying once') `
    "normal flash must not automatically pulse NRST and keep trying after a risky MRS failure"
Assert-True ($mrs -notmatch 'Escalating to op .*clear-code-flash') `
    "normal flash must not auto-escalate to clear-code-flash after ret=104"
Assert-True ($mrs -notmatch 'OR-ing clear-code-flash') `
    "normal flash must not add clear-code-flash unless the caller explicitly requested it"
Assert-True ($auto -notmatch 'if \(\$RecoverMode -or \$RestartLink\)') `
    "debug-check RecoverMode must not retry OpenOCD as if it were a link reset"
Assert-True ($auto -match 'RecoverMode cannot make OpenOCD attach to a running CH32H417') `
    "debug-check RecoverMode should explain the known OpenOCD attach limitation"
Assert-True ($auto -match 'throw "Forced erase failed:') `
    "recover must throw when forced erase fails"
Assert-True ($auto -match 'Recovery verification failed:') `
    "recover must verify link state after forced erase"

Assert-True ($auto -match 'Dual projects flash V3F first, then V5F') `
    "detect output must describe V3F-first official download order"
Assert-True ($skill -notmatch 'V5F first, then V3F') `
    "SKILL.md must not describe V5F-first as the normal download order"
Assert-True ($skill -match 'V3F first') `
    "SKILL.md must document V3F-first official download order"
Assert-True ($reference -match 'flash(?:es)? V3F first') `
    "H417 reference must document V3F-first MRS DLL download order"

$launchJson = $launch | ConvertFrom-Json
$v3Skip = [bool]$launchJson.configurations[0].openOCDCfg.skipDownloadBeforeDebug
$v5Skip = [bool]$launchJson.configurations[1].openOCDCfg.skipDownloadBeforeDebug
Assert-True $v3Skip "V3F launch config should skip download before debug"
Assert-True (-not $v5Skip) "V5F launch config should keep download enabled before debug"

Write-Host "wch-mrs-automation checks passed"
