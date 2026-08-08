param(
    [Parameter(Mandatory = $true)]
    [string]$Port,

    [string]$VideoPath = "",

    [ValidateRange(1, 120)]
    [int]$FrameCount = 90,

    [ValidateRange(1, 60)]
    [int]$FrameRate = 30,

    [ValidateRange(1, 120)]
    [int]$Gop = 30,

    [string]$PackedPath = "",

    [string]$PythonPath = "C:\Users\DDDD\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe",

    [string]$FfmpegPath = "C:\Program Files\AnsysEM\AnsysEM20.2\Win64\tp\ffmpeg\20071211\winx64\ffmpeg.exe",

    [ValidateRange(60, 1200)]
    [int]$TimeoutSeconds = 600,

    [ValidateRange(64, 4096)]
    [int]$CdcWriteChunkBytes = 1024,

    [ValidateRange(0, 20)]
    [int]$CdcWriteGapMs = 0,

    [switch]$ReusePacked,

    [switch]$NoRotate180,

    [switch]$ChunkedAbsolute,

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
if (-not (Test-Path -LiteralPath $VideoPath -PathType Leaf)) {
    throw "Video not found: $VideoPath"
}
if (-not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) {
    throw "Python not found: $PythonPath"
}
if (-not (Test-Path -LiteralPath $FfmpegPath -PathType Leaf)) {
    throw "ffmpeg not found: $FfmpegPath"
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\..\.."))
$packedDirectory = Join-Path $repoRoot ".tmp\sdram_video"
[System.IO.Directory]::CreateDirectory($packedDirectory) | Out-Null
if ([string]::IsNullOrWhiteSpace($PackedPath)) {
    $PackedPath = Join-Path $packedDirectory `
        ("miku_h4v1_{0}f_{1}fps.h4v" -f $FrameCount, $FrameRate)
}
else {
    $PackedPath = [System.IO.Path]::GetFullPath($PackedPath)
}
$uploadPath = "$PackedPath.upload.bin"
$metadataPath = "$PackedPath.json"
$packer = Join-Path $PSScriptRoot "pack_sdram_video_lz4.py"

if (-not ($ReusePacked -and
          (Test-Path -LiteralPath $PackedPath -PathType Leaf) -and
          (Test-Path -LiteralPath $uploadPath -PathType Leaf) -and
          (Test-Path -LiteralPath $metadataPath -PathType Leaf))) {
    $dependencyPath = Join-Path $repoRoot ".tmp\h417_codec_bench_deps"
    $oldPythonPath = $env:PYTHONPATH
    try {
        $env:PYTHONPATH = if ([string]::IsNullOrWhiteSpace($oldPythonPath)) {
            $dependencyPath
        }
        else {
            "$dependencyPath;$oldPythonPath"
        }
        $packerArgs = @(
            $packer,
            "--input", $VideoPath,
            "--output", $PackedPath,
            "--upload-output", $uploadPath,
            "--json-output", $metadataPath,
            "--transfer-alignment", 32768,
            "--frames", $FrameCount,
            "--fps", $FrameRate,
            "--gop", $Gop,
            "--ffmpeg", $FfmpegPath
        )
        if ($NoRotate180) {
            $packerArgs += "--no-rotate-180"
        }
        if ($ChunkedAbsolute) {
            $packerArgs += "--chunked-absolute"
        }
        if ($ChunkedAbsolute) {
            Write-Host "Preparing H4V1 LZ4 chunked-absolute video..."
        }
        else {
            Write-Host "Preparing H4V1 LZ4 keyframe/XOR-delta video..."
        }
        & $PythonPath @packerArgs
        if ($LASTEXITCODE -ne 0) {
            throw "H4V1 preparation failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        $env:PYTHONPATH = $oldPythonPath
    }
}

$metadata = Get-Content -LiteralPath $metadataPath -Raw -Encoding UTF8 |
    ConvertFrom-Json
if ([long]$metadata.transfer_bytes -gt 30L * 1024L * 1024L) {
    throw "Compressed upload exceeds the 30 MiB SDRAM container region"
}
if ([long]$metadata.transfer_bytes -ne
    (Get-Item -LiteralPath $uploadPath).Length) {
    throw "Upload image size does not match its metadata"
}

Write-Host ("H4V1 path={0}" -f $PackedPath)
Write-Host ("H4V1 frames={0} fps={1} duration={2:N3}s container={3:N3}MiB transfer={4:N3}MiB crc={5} ratio={6:N3}:1" -f `
    $metadata.frames, $metadata.fps, $metadata.duration_seconds, `
    ([double]$metadata.file_bytes / 1MB), `
    ([double]$metadata.transfer_bytes / 1MB), `
    $metadata.transfer_crc32, $metadata.compression_ratio)

if ($PrepareOnly) {
    exit 0
}

function Wait-H4V1Pattern {
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

$windowBytes = 32 * 1024
Write-Host ("CDC credit chunk={0} bytes gap={1}ms window={2} bytes stop_and_wait=1" -f `
    $CdcWriteChunkBytes, $CdcWriteGapMs, $windowBytes)
$serial = [System.IO.Ports.SerialPort]::new(
    $Port, 115200, [System.IO.Ports.Parity]::None, 8,
    [System.IO.Ports.StopBits]::One
)
$serial.DtrEnable = $true
$serial.RtsEnable = $true
$serial.ReadTimeout = 200
$serial.WriteTimeout = 5000
$serial.WriteBufferSize = 65536

try {
    $serial.Open()
    $serial.DiscardInBuffer()
    $serial.DiscardOutBuffer()
    Start-Sleep -Milliseconds 250

    $waiting = $null
    for ($attempt = 0; $attempt -lt 5 -and $null -eq $waiting; $attempt++) {
        $serial.Write("STATUS`r`n")
        $waiting = Wait-H4V1Pattern -Serial $serial `
            -Pattern "H4V1 WAIT" -TimeoutMilliseconds 2000
    }
    if ($null -eq $waiting) {
        throw "The MCU did not enter the H4V1 command state on $Port"
    }

    $command = "H4V1 {0} {1}`r`n" -f `
        $metadata.transfer_bytes, $metadata.transfer_crc32
    $serial.Write($command)
    $ready = Wait-H4V1Pattern -Serial $serial `
        -Pattern "VIDEO READY format=H4V1.*" -TimeoutMilliseconds 10000
    if ($null -eq $ready) {
        throw "The MCU did not accept the H4V1 metadata"
    }

    $buffer = [byte[]]::new($windowBytes)
    $stream = [System.IO.File]::OpenRead($uploadPath)
    try {
        [long]$sent = 0
        while ($sent -lt [long]$metadata.transfer_bytes) {
            $want = [int][Math]::Min(
                [long]$windowBytes,
                [long]$metadata.transfer_bytes - $sent
            )
            $count = 0
            while ($count -lt $want) {
                $read = $stream.Read($buffer, $count, $want - $count)
                if ($read -le 0) {
                    throw "Upload image ended at $sent bytes"
                }
                $count += $read
            }
            $written = 0
            while ($written -lt $count) {
                $writeCount = [int][Math]::Min(
                    $CdcWriteChunkBytes,
                    $count - $written
                )
                try {
                    $serial.Write($buffer, $written, $writeCount)
                }
                catch {
                    Write-Warning ("CDC write stopped at {0}/{1} bytes; draining MCU diagnostics..." -f `
                        ($sent + $written), $metadata.transfer_bytes)
                    [void](Wait-H4V1Pattern -Serial $serial `
                        -Pattern "VIDEO RX WAIT|VIDEO FAIL|WATCHDOG" `
                        -TimeoutMilliseconds 6000)
                    throw
                }
                $written += $writeCount
                if ($CdcWriteGapMs -gt 0) {
                    Start-Sleep -Milliseconds $CdcWriteGapMs
                }
            }
            $sent += $count
            $ackPattern = "VIDEO ACK bytes={0}/{1}" -f `
                $sent, $metadata.transfer_bytes
            $ack = Wait-H4V1Pattern -Serial $serial `
                -Pattern ([regex]::Escape($ackPattern)) `
                -TimeoutMilliseconds 15000
            if ($null -eq $ack) {
                throw "No MCU ACK after $sent/$($metadata.transfer_bytes) bytes"
            }
            if (($sent % 1MB) -eq 0 -or
                $sent -eq [long]$metadata.transfer_bytes) {
                Write-Host ("CDC upload {0:N3}/{1:N3} MiB" -f `
                    ($sent / 1MB),
                    ([long]$metadata.transfer_bytes / 1MB))
            }
        }
    }
    finally {
        $stream.Dispose()
    }

    $result = Wait-H4V1Pattern -Serial $serial `
        -Pattern "RESULT (PASS|FAIL)" `
        -TimeoutMilliseconds ($TimeoutSeconds * 1000)
    if ($null -eq $result) {
        throw "Timed out waiting for H4V1 SDRAM test result"
    }
    if ($result.Groups[1].Value -eq "FAIL") {
        exit 2
    }
    Write-Host "H4V1 SDRAM test PASS."
    exit 0
}
finally {
    try {
        if ($serial.IsOpen) {
            $serial.Close()
        }
    }
    catch {
    }
    try {
        $serial.Dispose()
    }
    catch {
    }
}
