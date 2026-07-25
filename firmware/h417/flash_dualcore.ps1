param([switch]$DryRun)

$ErrorActionPreference = "Stop"

$projectDir = $PSScriptRoot
$flashHelper = Join-Path $projectDir `
    "..\..\skills\wch-mrs-automation\scripts\wch-auto.ps1"
$v3fImage = Join-Path $projectDir "build\V3F\h417_V3F.hex"
$v5fImage = Join-Path $projectDir `
    "build\V5F\rtthread_ch32h417_V5F.hex"

foreach($path in @($flashHelper, $v3fImage, $v5fImage))
{
    if(-not (Test-Path -LiteralPath $path))
    {
        throw "Required production file was not found: $path"
    }
}

$arguments = @(
    "-NoLogo",
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $flashHelper,
    "-Action", "flash",
    "-ProjectDir", $projectDir,
    "-Chip", "CH32H417",
    "-Core", "both",
    "-ImagePathV3F", $v3fImage,
    "-ImagePathV5F", $v5fImage
)
if($DryRun)
{
    $arguments += "-DryRun"
}

& powershell @arguments
if($LASTEXITCODE -ne 0)
{
    exit $LASTEXITCODE
}
