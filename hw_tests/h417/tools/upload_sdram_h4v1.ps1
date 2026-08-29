param(
    [Parameter(Mandatory = $true)]
    [string]$Port,

    [string]$VideoPath = "",

    [ValidateRange(1, 165)]
    [int]$FrameCount = 165,

    [ValidateRange(1, 60)]
    [int]$FrameRate = 30,

    [ValidateRange(1, 165)]
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

    [switch]$PrepareOnly,

    [switch]$FlashStage1,

    [switch]$FlashStage2,

    [switch]$FlashStage3,

    [switch]$FlashStage4,

    [switch]$FlashStage5,

    [switch]$FlashInstall,

    [switch]$AuthorizeBlocks768To1014
)

$ErrorActionPreference = "Stop"
$script:ReceiveText = ""

if (($FrameCount -gt 90) -and -not $ChunkedAbsolute) {
    throw "The isolated extended-frame firmware accepts only chunked-absolute H4V1; add -ChunkedAbsolute"
}

if ($FlashStage5 -and (($FrameCount -ne 165) -or
    ($FrameRate -ne 30) -or -not $ChunkedAbsolute -or $NoRotate180)) {
    throw "The destructive Flash Stage5 probe requires the qualified 165-frame, 30fps, rotated chunked-absolute image"
}

if ($FlashInstall -and -not $AuthorizeBlocks768To1014) {
    throw "Flash installation is destructive; add -AuthorizeBlocks768To1014 to authorize erasing/programming H4V1 blocks 768..1014"
}
if ($AuthorizeBlocks768To1014 -and -not $FlashInstall) {
    throw "-AuthorizeBlocks768To1014 is valid only together with -FlashInstall"
}
if ($FlashInstall -and
    ($FlashStage1 -or $FlashStage2 -or $FlashStage3 -or
     $FlashStage4 -or $FlashStage5)) {
    throw "-FlashInstall cannot be combined with FlashStage1..FlashStage5"
}
if ($FlashInstall -and (($FrameCount -ne 165) -or
    ($FrameRate -ne 30) -or -not $ChunkedAbsolute -or $NoRotate180)) {
    throw "Flash installation requires the qualified 165-frame, 30fps, rotated chunked-absolute image"
}

if (-not $FlashStage5 -and
    ($FlashStage1 -or $FlashStage2 -or $FlashStage3 -or $FlashStage4) -and
    ($FrameCount -ne 90)) {
    throw "The qualified read-only Flash Stage1-4 firmware requires exactly 90 frames; omit the FlashStage switch for an isolated extended-frame test, or pass -FrameCount 90"
}

if ([string]::IsNullOrWhiteSpace($VideoPath)) {
    $videoName = -join @(
        [char]0x5C0F, [char]0x98CE, [char]0x5807, ".mp4"
    )
    $VideoPath = Join-Path `
        "C:\Program Files (x86)\Steam\steamapps\workshop\content\431960\3512588767" `
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
    $codecTag = if ($ChunkedAbsolute) { "h4v1_chunk16" } else { "h4v1" }
    $PackedPath = Join-Path $packedDirectory `
        ("xiaofengjin_{0}_{1}f_{2}fps.h4v" -f `
            $codecTag, $FrameCount, $FrameRate)
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
if (([int]$metadata.frames -ne $FrameCount) -or
    ([int]$metadata.fps -ne $FrameRate)) {
    throw ("Packed metadata does not match the request: file={0}f/{1}fps request={2}f/{3}fps" -f `
        $metadata.frames, $metadata.fps, $FrameCount, $FrameRate)
}
if ($FrameCount -gt 90) {
    if (($metadata.compression -ne "chunked_absolute_lz4_hc_16k") -or
        ([int]$metadata.gop -ne 1) -or
        ([int]$metadata.delta_frames -ne 0)) {
        throw "The extended-frame image metadata is not chunked-absolute GOP1"
    }
}
if ($FlashStage5) {
    if (($metadata.format -ne "ARGB1555") -or
        ([int]$metadata.width -ne 800) -or
        ([int]$metadata.height -ne 480) -or
        ([int]$metadata.keyframes -ne 165) -or
        ([int]$metadata.chunks_per_frame -ne 47) -or
        ([int]$metadata.chunk_bytes -ne 16384) -or
        (-not [bool]$metadata.rotate_180) -or
        ([long]$metadata.file_bytes -ne 30933600L) -or
        ([long]$metadata.transfer_bytes -ne 30965760L) -or
        ([string]$metadata.container_crc32 -ne "4097f39a") -or
        ([string]$metadata.transfer_crc32 -ne "e32a6c99")) {
        throw "Packed image does not match the hardware-qualified CHUNK165 Flash Stage5 contract"
    }
    $containerSha = (Get-FileHash -LiteralPath $PackedPath -Algorithm SHA256).Hash
    $uploadSha = (Get-FileHash -LiteralPath $uploadPath -Algorithm SHA256).Hash
    if (($containerSha -ne "969E9292EB5F99897C2120F36978BCEA11D47AA6A8BC37C51970D932BBEDDFCE") -or
        ($uploadSha -ne "4B21CFB3DF55BA07444EADA282DA93CDF315E362A10CDCB5922C0C68E793FA65")) {
        throw "Packed CHUNK165 bytes do not match the hardware-qualified Flash Stage5 asset"
    }
}
if ($FlashInstall) {
    if (($metadata.format -ne "ARGB1555") -or
        ([int]$metadata.width -ne 800) -or
        ([int]$metadata.height -ne 480) -or
        ([int]$metadata.keyframes -ne 165) -or
        ([int]$metadata.chunks_per_frame -ne 47) -or
        ([int]$metadata.chunk_bytes -ne 16384) -or
        (-not [bool]$metadata.rotate_180) -or
        ([long]$metadata.file_bytes -ne 30933600L) -or
        ([long]$metadata.transfer_bytes -ne 30965760L) -or
        ([string]$metadata.container_crc32 -ne "4097f39a") -or
        ([string]$metadata.transfer_crc32 -ne "e32a6c99")) {
        throw "Packed image does not match the qualified CHUNK165 Flash install contract"
    }
    $containerSha = (Get-FileHash -LiteralPath $PackedPath -Algorithm SHA256).Hash
    $uploadSha = (Get-FileHash -LiteralPath $uploadPath -Algorithm SHA256).Hash
    if (($containerSha -ne "969E9292EB5F99897C2120F36978BCEA11D47AA6A8BC37C51970D932BBEDDFCE") -or
        ($uploadSha -ne "4B21CFB3DF55BA07444EADA282DA93CDF315E362A10CDCB5922C0C68E793FA65")) {
        throw "Packed CHUNK165 bytes do not match the qualified Flash install asset"
    }
    Write-Host "FLASH INSTALL AUTH accepted blocks=768..1014 manifest=768 scratch=1015 l8=1016..1023"
}
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
    $waitingPattern = if ($FlashInstall) {
        "(?m)^H417 FLASH H4V1 INSTALL v001 CHUNK165 COMMIT V01\r?$[\s\S]*?^H4V1 WAIT[^\r\n]*"
    }
    elseif ($FlashStage5) {
        "H417 FLASH H4V1 WRITE PROBE v001 CHUNK165 STAGE5[\s\S]*?H4V1 WAIT"
    }
    elseif ($FrameCount -eq 165) {
        "H417 SDRAM VIDEO H4V1 ISOLATED v58 CHUNK165 FULL[\s\S]*?H4V1 WAIT"
    }
    elseif ($FrameCount -eq 120) {
        "H417 SDRAM VIDEO H4V1 ISOLATED v57 CHUNK120[\s\S]*?H4V1 WAIT"
    }
    else {
        "H4V1 WAIT"
    }
    for ($attempt = 0; $attempt -lt 5 -and $null -eq $waiting; $attempt++) {
        $serial.Write("STATUS`r`n")
        $waiting = Wait-H4V1Pattern -Serial $serial `
            -Pattern $waitingPattern -TimeoutMilliseconds 2000
    }
    if ($null -eq $waiting) {
        if ($FlashInstall) {
            throw "The MCU is not running h417_v5f_flash_h4v1_165_install; upload was not started"
        }
        if ($FlashStage5) {
            throw "The MCU is not running h417_v5f_flash_h4v1_165_write_probe; upload was not started"
        }
        if ($FrameCount -eq 165) {
            throw "The MCU is not running the isolated CHUNK165 V5 image"
        }
        if ($FrameCount -eq 120) {
            throw "The MCU is not running the isolated CHUNK120 V5 image"
        }
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

    if ($FlashInstall) {
        $installIdentity = Wait-H4V1Pattern -Serial $serial `
            -Pattern "(?m)^(?:FLASH INSTALL HOOK ENTER[^\r\n]*|FLASH INSTALL PLAN (?:PASS|FAIL)[^\r\n]*|H4C LOOP (?:START|n=)[^\r\n]*)" `
            -TimeoutMilliseconds 5000
        if ($null -eq $installIdentity -or
            $installIdentity.Value -ne
            "FLASH INSTALL HOOK ENTER stage6=armed frames=165 reference=chunk165_pass blocks=768..1014 manifest=768 scratch=1015") {
            throw "The running V5 image does not contain the qualified CHUNK165 Flash INSTALL hook"
        }
        Write-Host "H4V1 Flash install hook identity PASS."

        $installDeadline = [DateTime]::UtcNow.AddSeconds(600)
        $installDescriptorCrc = $null
        $installStages = @("PLAN", "INVALIDATE", "PAYLOAD", "VERIFY", "MANIFEST", "COMMIT")
        foreach ($installStage in $installStages) {
            $remainingMilliseconds = [int][Math]::Max(
                1.0,
                ($installDeadline - [DateTime]::UtcNow).TotalMilliseconds
            )
            $installPattern = "(?m)^(?:FLASH INSTALL {0} (?<state>PASS|FAIL)[^\r\n]*|(?<fatal>FLASH INSTALL STAGE6 FAIL[^\r\n]*)|(?<stage1>FLASH STAGE1 FAIL[^\r\n]*))" -f `
                $installStage
            $installResult = Wait-H4V1Pattern -Serial $serial `
                -Pattern $installPattern `
                -TimeoutMilliseconds $remainingMilliseconds
            if ($null -eq $installResult) {
                throw ("Timed out waiting for Flash INSTALL {0}; total install timeout is 600 seconds" -f `
                    $installStage)
            }
            if ($installResult.Groups["fatal"].Success -or
                $installResult.Groups["stage1"].Success -or
                $installResult.Groups["state"].Value -eq "FAIL") {
                exit 8
            }

            if ($installStage -eq "PLAN") {
                $plan = [regex]::Match(
                    $installResult.Value,
                    "^FLASH INSTALL PLAN PASS candidates=(\d+) good=(\d+) bad=(\d+) unreadable=(\d+) selected=(\d+) first=(\d+) last=(\d+)$"
                )
                if (-not $plan.Success) {
                    throw "Flash INSTALL PLAN PASS has an unknown contract"
                }
                $candidates = [int]$plan.Groups[1].Value
                $good = [int]$plan.Groups[2].Value
                $bad = [int]$plan.Groups[3].Value
                $unreadable = [int]$plan.Groups[4].Value
                $selected = [int]$plan.Groups[5].Value
                $first = [int]$plan.Groups[6].Value
                $last = [int]$plan.Groups[7].Value
                if (($candidates -ne 246) -or ($selected -ne 237) -or
                    (($good + $bad) -ne 246) -or ($good -lt 237) -or
                    ($bad -gt 9) -or ($unreadable -gt $bad) -or
                    ($first -lt 769) -or
                    ($last -gt 1014) -or ($first -gt $last)) {
                    throw "Flash INSTALL PLAN PASS is outside the authorized block/capacity contract"
                }
            }
            elseif ($installStage -eq "INVALIDATE") {
                if ($installResult.Value -ne
                    "FLASH INSTALL INVALIDATE PASS manifest=768 committed=0 a0=38 wel=0") {
                    throw "Flash INSTALL did not prove the previous manifest was invalidated"
                }
            }
            elseif ($installStage -eq "PAYLOAD") {
                if ($installResult.Value -ne
                    "FLASH INSTALL PAYLOAD PASS bytes=30965760 pages=15120 blocks=237 source_crc=e32a6c99") {
                    throw "Flash INSTALL PAYLOAD PASS does not cover the complete qualified transfer"
                }
            }
            elseif ($installStage -eq "VERIFY") {
                if ($installResult.Value -notmatch
                    "^FLASH INSTALL VERIFY PASS bytes=30965760 pages=15120 crc=e32a6c99 ecc_worst=[0-8]$") {
                    throw "Flash INSTALL VERIFY PASS does not match the qualified transfer CRC"
                }
            }
            elseif ($installStage -eq "MANIFEST") {
                $manifest = [regex]::Match(
                    $installResult.Value,
                    "^FLASH INSTALL MANIFEST PASS block=768 descriptor=valid crc=([0-9a-f]{8}) map=237$"
                )
                if (-not $manifest.Success) {
                    throw "Flash INSTALL MANIFEST PASS is outside the block768 descriptor contract"
                }
                $installDescriptorCrc = $manifest.Groups[1].Value
            }
            elseif ($installStage -eq "COMMIT") {
                $commit = [regex]::Match(
                    $installResult.Value,
                    "^FLASH INSTALL COMMIT PASS block=768 page=1 crc=([0-9a-f]{8}) descriptor=([0-9a-f]{8}) committed=1 a0=38 b0=10 wel=0$"
                )
                if ((-not $commit.Success) -or
                    ($null -eq $installDescriptorCrc) -or
                    ($commit.Groups[2].Value -ne $installDescriptorCrc)) {
                    throw "Flash INSTALL COMMIT PASS is outside the qualified committed-and-locked contract"
                }
            }
            Write-Host ("H4V1 Flash INSTALL {0} PASS." -f $installStage)
        }
        Write-Host "H4V1 Flash INSTALL PASS; program the cold-boot V5 image and power-cycle the board."
        exit 0
    }

    if ($FlashStage5) {
        $writeProbeIdentity = Wait-H4V1Pattern -Serial $serial `
            -Pattern "(?:FLASH WRITE_PROBE HOOK ENTER[^\r\n]*|FLASH STAGE1 START[^\r\n]*|H4C LOOP (?:START|n=)[^\r\n]*)" `
            -TimeoutMilliseconds 5000
        if ($null -eq $writeProbeIdentity -or
            $writeProbeIdentity.Value -ne
            "FLASH WRITE_PROBE HOOK ENTER stage5=armed frames=165 reference=chunk165_pass block=1015 row=0000fdc0") {
            throw "The running V5 image is not the qualified h417_v5f_flash_h4v1_165_write_probe image"
        }
        Write-Host "H4V1 Flash Stage5 hook identity PASS."
    }

    if ($FlashStage1 -or $FlashStage2 -or $FlashStage3 -or $FlashStage4 -or $FlashStage5) {
        $flashResult = Wait-H4V1Pattern -Serial $serial `
            -Pattern "FLASH STAGE1 (PASS|FAIL)[^\r\n]*" `
            -TimeoutMilliseconds 15000
        if ($null -eq $flashResult) {
            throw "Timed out waiting for the post-PASS Flash Stage1 result"
        }
        if ($flashResult.Groups[1].Value -eq "FAIL") {
            exit 3
        }
        Write-Host "H4V1 Flash Stage1 PASS."
    }
    if ($FlashStage2 -or $FlashStage3 -or $FlashStage4 -or $FlashStage5) {
        $flashReadResult = Wait-H4V1Pattern -Serial $serial `
            -Pattern "FLASH STAGE2 (PASS|FAIL)[^\r\n]*" `
            -TimeoutMilliseconds 15000
        if ($null -eq $flashReadResult) {
            throw "Timed out waiting for the post-PASS Flash Stage2 result"
        }
        if ($flashReadResult.Groups[1].Value -eq "FAIL") {
            exit 4
        }
        Write-Host "H4V1 Flash Stage2 PASS."
    }
    if ($FlashStage3 -or $FlashStage4 -or $FlashStage5) {
        $flashBandwidthResult = Wait-H4V1Pattern -Serial $serial `
            -Pattern "FLASH STAGE3 (PASS|FAIL)[^\r\n]*" `
            -TimeoutMilliseconds 30000
        if ($null -eq $flashBandwidthResult) {
            throw "Timed out waiting for the post-PASS Flash Stage3 result"
        }
        if ($flashBandwidthResult.Groups[1].Value -eq "FAIL") {
            exit 5
        }
        Write-Host "H4V1 Flash Stage3 PASS."
    }
    if ($FlashStage4 -or $FlashStage5) {
        $flashArrayResult = Wait-H4V1Pattern -Serial $serial `
            -Pattern "FLASH STAGE4 (PASS|FAIL)[^\r\n]*" `
            -TimeoutMilliseconds 30000
        if ($null -eq $flashArrayResult) {
            throw "Timed out waiting for the post-PASS Flash Stage4 result"
        }
        if ($flashArrayResult.Groups[1].Value -eq "FAIL") {
            exit 6
        }
        Write-Host "H4V1 Flash Stage4 PASS."
    }
    if ($FlashStage5) {
        $flashWriteProbeResult = Wait-H4V1Pattern -Serial $serial `
            -Pattern "(?:FLASH STAGE5 (?<stage5>PASS|FAIL)[^\r\n]*|(?<loop>H4C LOOP (?:START|n=))[^\r\n]*)" `
            -TimeoutMilliseconds 30000
        if ($null -eq $flashWriteProbeResult) {
            throw "Timed out waiting for the destructive Flash Stage5 result"
        }
        if ($flashWriteProbeResult.Groups["loop"].Success) {
            throw "The running V5 image has no destructive Flash Stage5 hook; flash h417_v5f_flash_h4v1_165_write_probe and reset V5 before retrying"
        }
        if ($flashWriteProbeResult.Groups["stage5"].Value -eq "FAIL") {
            exit 7
        }
        if ($flashWriteProbeResult.Value -notmatch
            "^FLASH STAGE5 PASS block=1015 row=0000fdc0 marker=ff/ff .* blank=1 ") {
            throw "Flash Stage5 returned PASS outside the qualified block1015 clean-erase contract"
        }
        Write-Host "H4V1 Flash Stage5 PASS."
    }
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
