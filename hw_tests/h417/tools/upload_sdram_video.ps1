param(
    [Parameter(Mandatory = $true)]
    [string]$Port,

    [string]$VideoPath = "",

    [ValidateSet("ARGB8888", "ARGB1555")]
    [string]$Format = "ARGB1555",

    [ValidateRange(0, 60)]
    [int]$FrameRate = 0,

    [ValidateRange(0, 64)]
    [int]$FrameCount = 0,

    [string]$PackedPath = "",

    [string]$PythonPath = "C:\Users\DDDD\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe",

    [string]$FfmpegPath = "C:\Program Files\AnsysEM\AnsysEM20.2\Win64\tp\ffmpeg\20071211\winx64\ffmpeg.exe",

    [ValidateRange(30, 600)]
    [int]$TimeoutSeconds = 180,

    [ValidateRange(64, 4096)]
    [int]$CdcWriteChunkBytes = 1024,

    [ValidateRange(0, 20)]
    [int]$CdcWriteGapMs = 0,

    [switch]$NoRotate180,

    [switch]$PrepareOnly
)

$ErrorActionPreference = "Stop"
$script:ReceiveText = ""

if ([string]::IsNullOrWhiteSpace($VideoPath)) {
    $videoName = [string]::Concat([char]0x521D, [char]0x97F3, ".mp4")
    $VideoPath = Join-Path `
        "C:\Program Files (x86)\Steam\steamapps\workshop\content\431960\3702521609" `
        $videoName
}
if ($FrameRate -eq 0) {
    $FrameRate = 30
}
if ($FrameCount -eq 0) {
    $FrameCount = if ($Format -eq "ARGB1555") { 32 } else { 16 }
}

function Wait-VideoPattern {
    param(
        [System.IO.Ports.SerialPort]$Serial,
        [string]$Pattern,
        [int]$TimeoutMilliseconds
    )

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMilliseconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        $available = $Serial.BytesToRead
        if ($available -gt 0) {
            $bytes = [byte[]]::new($available)
            $count = $Serial.Read($bytes, 0, $bytes.Length)
            if ($count -gt 0) {
                $text = [System.Text.Encoding]::ASCII.GetString($bytes, 0, $count)
                Write-Host -NoNewline $text
                $script:ReceiveText += $text
                $match = [regex]::Match($script:ReceiveText, $Pattern)
                if ($match.Success) {
                    $script:ReceiveText = $script:ReceiveText.Substring(
                        $match.Index + $match.Length
                    )
                    return $match
                }
                if ($script:ReceiveText.Length -gt 8192) {
                    $script:ReceiveText = $script:ReceiveText.Substring(
                        $script:ReceiveText.Length - 2048
                    )
                }
            }
        }
        Start-Sleep -Milliseconds 5
    }
    return $null
}

if (-not (Test-Path -LiteralPath $VideoPath -PathType Leaf)) {
    throw "Video not found: $VideoPath"
}
if (-not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) {
    throw "Python not found: $PythonPath"
}
if (-not (Test-Path -LiteralPath $FfmpegPath -PathType Leaf)) {
    throw "ffmpeg not found: $FfmpegPath"
}

$prepareScript = Join-Path $PSScriptRoot "prepare_sdram_video.py"
if ([string]::IsNullOrWhiteSpace($PackedPath)) {
    $repoRoot = [System.IO.Path]::GetFullPath(
        (Join-Path $PSScriptRoot "..\..\..\..")
    )
    $packedDirectory = Join-Path $repoRoot ".tmp\sdram_video"
    [System.IO.Directory]::CreateDirectory($packedDirectory) | Out-Null
    $PackedPath = Join-Path $packedDirectory ("miku_{0}_{1}f_{2}fps.bin" -f $Format, $FrameCount, $FrameRate)
}
else {
    $PackedPath = [System.IO.Path]::GetFullPath($PackedPath)
    [System.IO.Directory]::CreateDirectory(
        [System.IO.Path]::GetDirectoryName($PackedPath)
    ) | Out-Null
}

$prepareArgs = @(
    $prepareScript,
    "--input", $VideoPath,
    "--output", $PackedPath,
    "--format", $Format,
    "--frames", $FrameCount,
    "--fps", $FrameRate,
    "--ffmpeg", $FfmpegPath
)
if ($NoRotate180) {
    $prepareArgs += "--no-rotate-180"
}

Write-Host "Preparing $Format video frames..."
$metadataText = & $PythonPath @prepareArgs
if ($LASTEXITCODE -ne 0) {
    throw "Video preparation failed with exit code $LASTEXITCODE"
}
$metadata = ($metadataText | Select-Object -Last 1) | ConvertFrom-Json
Write-Host ("PACKED path={0}" -f $metadata.output)
Write-Host ("PACKED format={0} frames={1} fps={2} bytes={3} crc={4} lanes={5} ignored={6} rot180={7}" -f `
    $metadata.format, $metadata.frames, $metadata.fps, $metadata.total_bytes, `
    $metadata.crc32, $metadata.lane_mask, $metadata.ignored_lane_mask, `
    $metadata.rotate_180)
$windowBytes = 32 * 1024
Write-Host ("CDC credit chunk={0} bytes gap={1}ms window={2} bytes stop_and_wait=1" -f `
    $CdcWriteChunkBytes, $CdcWriteGapMs, $windowBytes)

if ($PrepareOnly) {
    exit 0
}

$serial = [System.IO.Ports.SerialPort]::new(
    $Port,
    115200,
    [System.IO.Ports.Parity]::None,
    8,
    [System.IO.Ports.StopBits]::One
)
$serial.DtrEnable = $true
$serial.RtsEnable = $true
$serial.ReadTimeout = 200
$serial.WriteTimeout = 10000
$serial.WriteBufferSize = 65536

try {
    $serial.Open()
    $serial.DiscardInBuffer()
    $serial.DiscardOutBuffer()
    Start-Sleep -Milliseconds 250

    $waiting = $null
    for ($attempt = 0; $attempt -lt 5 -and $null -eq $waiting; $attempt++) {
        $serial.Write("STATUS`r`n")
        $waiting = Wait-VideoPattern -Serial $serial `
            -Pattern "VIDEO WAIT" -TimeoutMilliseconds 2000
    }
    if ($null -eq $waiting) {
        throw "The MCU did not enter the SDRAM video command state on $Port"
    }

    $command = "VIDEO {0} {1} {2} {3} {4}`r`n" -f `
        $metadata.format, $metadata.frames, $metadata.fps, `
        $metadata.total_bytes, $metadata.crc32
    $serial.Write($command)
    $ready = Wait-VideoPattern -Serial $serial `
        -Pattern "VIDEO READY.*" -TimeoutMilliseconds 10000
    if ($null -eq $ready) {
        throw "The MCU did not accept the video metadata"
    }

    $buffer = [byte[]]::new($windowBytes)
    $stream = [System.IO.File]::OpenRead($PackedPath)
    try {
        [long]$sent = 0
        while ($sent -lt [long]$metadata.total_bytes) {
            $want = [int][Math]::Min(
                [long]$windowBytes,
                [long]$metadata.total_bytes - $sent
            )
            $count = 0
            while ($count -lt $want) {
                $read = $stream.Read($buffer, $count, $want - $count)
                if ($read -le 0) {
                    throw "Packed file ended at $sent bytes"
                }
                $count += $read
            }
            $written = 0
            while ($written -lt $count) {
                $writeCount = [int][Math]::Min(
                    $CdcWriteChunkBytes,
                    $count - $written
                )
                $serial.Write($buffer, $written, $writeCount)
                $written += $writeCount
                if ($CdcWriteGapMs -gt 0) {
                    Start-Sleep -Milliseconds $CdcWriteGapMs
                }
            }
            $sent += $count
            $ackPattern = "VIDEO ACK bytes={0}/{1}" -f `
                $sent, $metadata.total_bytes
            $ack = Wait-VideoPattern -Serial $serial `
                -Pattern ([regex]::Escape($ackPattern)) `
                -TimeoutMilliseconds 15000
            if ($null -eq $ack) {
                throw "No MCU ACK after $sent/$($metadata.total_bytes) bytes"
            }
            if (($sent % (1024 * 1024)) -eq 0 -or `
                $sent -eq [long]$metadata.total_bytes) {
                Write-Host ("CDC upload {0}/{1} MiB" -f `
                    ($sent / (1024 * 1024)), `
                    ([long]$metadata.total_bytes / (1024 * 1024)))
            }
        }
    }
    finally {
        $stream.Dispose()
    }

    $result = Wait-VideoPattern -Serial $serial `
        -Pattern "RESULT (PASS|FAIL)" `
        -TimeoutMilliseconds ($TimeoutSeconds * 1000)
    if ($null -eq $result) {
        throw "Timed out waiting for upload/readback/LTDC result"
    }
    if ($result.Groups[1].Value -eq "FAIL") {
        exit 2
    }
    Write-Host "SDRAM VIDEO TEST PASS; playback continues on the panel."
    exit 0
}
finally {
    try {
        if ($serial.IsOpen) {
            $serial.Close()
        }
    }
    catch {
        # A watchdog reset removes the virtual COM device before Close().
    }
    try {
        $serial.Dispose()
    }
    catch {
        # Preserve the actual transfer error if Windows already removed COM.
    }
}
