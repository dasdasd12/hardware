param(
    [Parameter(Mandatory = $true)]
    [string]$Port,

    [ValidateRange(60, 1200)]
    [int]$TimeoutSeconds = 600,

    [ValidateRange(250, 5000)]
    [int]$StatusIntervalMilliseconds = 1000
)

$ErrorActionPreference = "Stop"

$expectedBanner = "H417 FLASH H4V1 COLD BOOT v001 READONLY PLAY V01"
$expectedCommonTokens = @(
    @("bytes", "30965760"),
    @("crc", "e32a6c99"),
    @("map", "237"),
    @("frames", "165"),
    @("fps", "30")
)

function New-ColdBootSerialPort {
    param([string]$Name)

    $instance = [System.IO.Ports.SerialPort]::new(
        $Name,
        115200,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One
    )
    $instance.DtrEnable = $true
    $instance.RtsEnable = $true
    $instance.ReadTimeout = 200
    $instance.WriteTimeout = 1000
    return $instance
}

function Close-ColdBootSerialPort {
    param([System.IO.Ports.SerialPort]$Instance)

    if ($null -eq $Instance) {
        return
    }
    try {
        if ($Instance.IsOpen) {
            $Instance.Close()
        }
    }
    catch {
        # USB can disappear while V5 is reset or power-cycled.
    }
    try {
        $Instance.Dispose()
    }
    catch {
    }
}

function Get-UniqueColdBootToken {
    param(
        [string]$Line,
        [string]$Name
    )

    $pattern = "(?:^| ){0}=([^ \t]+)(?= |$)" -f [regex]::Escape($Name)
    $matches = [regex]::Matches($Line, $pattern)
    if ($matches.Count -ne 1) {
        throw "Cold-boot $Name token is missing or duplicated: $Line"
    }
    return $matches[0].Groups[1].Value
}

function Test-ColdBootPassContract {
    param(
        [string]$Line,
        [AllowNull()]$ExpectedDescriptor,
        [AllowNull()]$ExpectedCommit
    )

    foreach ($token in $expectedCommonTokens) {
        $actual = Get-UniqueColdBootToken -Line $Line -Name $token[0]
        if ($actual -cne $token[1]) {
            throw ("Cold-boot {0} contract mismatch: expected={1} got={2}" -f `
                $token[0], $token[1], $actual)
        }
    }

    $descriptor = Get-UniqueColdBootToken -Line $Line -Name "descriptor"
    $commit = Get-UniqueColdBootToken -Line $Line -Name "commit"
    if ($descriptor -cnotmatch "^[0-9a-f]{8}$" -or
        $commit -cnotmatch "^[0-9a-f]{8}$") {
        throw "Cold-boot descriptor/commit CRC is not eight lowercase hex digits: $Line"
    }
    if ($null -ne $ExpectedDescriptor -and
        $descriptor -cne $ExpectedDescriptor) {
        throw "Cold-boot descriptor CRC changed between stages"
    }
    if ($null -ne $ExpectedCommit -and $commit -cne $ExpectedCommit) {
        throw "Cold-boot commit CRC changed between stages"
    }

    return [PSCustomObject]@{
        Descriptor = $descriptor
        Commit = $commit
    }
}

$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$nextStatus = [DateTime]::MinValue
$serial = $null
$pending = ""
$reconnects = 0
$statusWrites = 0
$bannerSeen = $false
$preBannerPasses = 0
$stage = 0
$descriptorCrc = $null
$commitCrc = $null
$exitCode = $null
$failure = $null

Write-Host ("H4V1 COLD BOOT MONITOR port={0} timeout_s={1} upload=none status_only=1" -f `
    $Port, $TimeoutSeconds)
Write-Host ("H4V1 COLD BOOT EXPECT banner={0}" -f $expectedBanner)

try {
    while ($null -eq $exitCode -and [DateTime]::UtcNow -lt $deadline) {
        if ($null -eq $serial -or -not $serial.IsOpen) {
            try {
                $serial = New-ColdBootSerialPort -Name $Port
                $serial.Open()
                $serial.DiscardInBuffer()
                $serial.DiscardOutBuffer()
                $pending = ""
                # Every USB enumeration must prove the exact firmware identity
                # again before a replayed stage is trusted.
                $bannerSeen = $false
                $nextStatus = [DateTime]::MinValue
                if ($reconnects -gt 0) {
                    Write-Host ("CDC RECONNECTED port={0} count={1}" -f `
                        $Port, $reconnects)
                }
            }
            catch {
                Close-ColdBootSerialPort -Instance $serial
                $serial = $null
                Start-Sleep -Milliseconds 500
                continue
            }
        }

        try {
            if ([DateTime]::UtcNow -ge $nextStatus) {
                # This is the only host-to-MCU write in the cold-boot monitor.
                $serial.Write("STATUS`r`n")
                $statusWrites++
                $nextStatus = [DateTime]::UtcNow.AddMilliseconds(
                    $StatusIntervalMilliseconds
                )
            }

            $available = $serial.BytesToRead
            if ($available -gt 0) {
                $bytes = [byte[]]::new($available)
                $count = $serial.Read($bytes, 0, $bytes.Length)
                if ($count -gt 0) {
                    $text = [System.Text.Encoding]::ASCII.GetString(
                        $bytes, 0, $count
                    )
                    Write-Host -NoNewline $text
                    $pending += $text

                    $newline = $pending.IndexOf("`n")
                    while ($newline -ge 0 -and $null -eq $exitCode) {
                        $line = $pending.Substring(0, $newline)
                        $pending = $pending.Substring($newline + 1)
                        if ($line.EndsWith("`r")) {
                            $line = $line.Substring(0, $line.Length - 1)
                        }
                        if ($line.Length -eq 0) {
                            $newline = $pending.IndexOf("`n")
                            continue
                        }

                        if ($line -eq $expectedBanner) {
                            $bannerSeen = $true
                            # The exact banner starts a new, fully witnessed
                            # replay transaction.  Do not carry a partial
                            # sequence (or descriptor association) across it.
                            $stage = 0
                            $descriptorCrc = $null
                            $commitCrc = $null
                            $newline = $pending.IndexOf("`n")
                            continue
                        }
                        if ($line.StartsWith("H417 ")) {
                            $failure = "Unexpected firmware banner; no video data was sent: $line"
                            $exitCode = 3
                            break
                        }

                        $failed = [regex]::Match(
                            $line,
                            "^FLASH COLD BOOT ([A-Z0-9_]+) (REJECT|FAIL)(?: |$)"
                        )
                        if ($failed.Success) {
                            $failure = $line
                            $exitCode = 2
                            break
                        }

                        $passed = [regex]::Match(
                            $line,
                            "^FLASH COLD BOOT ([A-Z0-9_]+) PASS(?: |$)"
                        )
                        if ($passed.Success) {
                            if (-not $bannerSeen) {
                                # A late-attached monitor can enter during the
                                # first playback loop and see its original PASS
                                # before the periodic replay reaches the banner.
                                # Never trust it, but keep waiting for a complete
                                # banner-led replay rather than failing early.
                                $preBannerPasses++
                                Write-Host ("H4V1 COLD BOOT PREBANNER PASS ignored stage={0} count={1}" -f `
                                    $passed.Groups[1].Value, $preBannerPasses)
                                $newline = $pending.IndexOf("`n")
                                continue
                            }

                            $name = $passed.Groups[1].Value
                            try {
                                $contract = Test-ColdBootPassContract `
                                    -Line $line `
                                    -ExpectedDescriptor $descriptorCrc `
                                    -ExpectedCommit $commitCrc
                            }
                            catch {
                                $failure = $_.Exception.Message
                                $exitCode = 3
                                break
                            }

                            if ($name -eq "MANIFEST") {
                                if ($stage -eq 0) {
                                    $descriptorCrc = $contract.Descriptor
                                    $commitCrc = $contract.Commit
                                    $stage = 1
                                    Write-Host "H4V1 COLD BOOT MANIFEST GATE PASS."
                                }
                                # A STATUS replay may repeat an already-passed stage.
                            }
                            elseif ($name -eq "LOAD") {
                                if ($stage -lt 1) {
                                    $failure = "Cold-boot LOAD PASS arrived before MANIFEST PASS"
                                    $exitCode = 3
                                    break
                                }
                                if ($stage -eq 1) {
                                    $stage = 2
                                    Write-Host "H4V1 COLD BOOT LOAD GATE PASS."
                                }
                            }
                            elseif ($name -eq "PLAY") {
                                if ($stage -lt 2) {
                                    $failure = "Cold-boot PLAY PASS arrived before LOAD PASS"
                                    $exitCode = 3
                                    break
                                }
                                Write-Host "H4V1 COLD BOOT PLAY GATE PASS."
                                $exitCode = 0
                                break
                            }
                            else {
                                $failure = "Unknown cold-boot PASS stage: $line"
                                $exitCode = 3
                                break
                            }
                        }

                        $newline = $pending.IndexOf("`n")
                    }

                    if ($pending.Length -gt 8192 -and $null -eq $exitCode) {
                        $failure = "Cold-boot monitor received an unterminated line larger than 8192 bytes"
                        $exitCode = 3
                    }
                }
            }
        }
        catch {
            Close-ColdBootSerialPort -Instance $serial
            $serial = $null
            $pending = ""
            $reconnects++
            Start-Sleep -Milliseconds 500
            continue
        }

        Start-Sleep -Milliseconds 10
    }
}
finally {
    Close-ColdBootSerialPort -Instance $serial
}

if ($null -eq $exitCode) {
    Write-Error `
        ("Timed out waiting for cold-boot MANIFEST -> LOAD -> PLAY on {0}; STATUS writes={1}" -f `
            $Port, $statusWrites) `
        -ErrorAction Continue
    exit 1
}
if ($exitCode -ne 0) {
    Write-Error `
        ("H4V1 cold-boot monitor failed: {0}; STATUS writes={1}; upload_bytes=0" -f `
            $failure, $statusWrites) `
        -ErrorAction Continue
    exit $exitCode
}

Write-Host ("H4V1 COLD BOOT PASS descriptor={0} commit={1} STATUS_writes={2} upload_bytes=0" -f `
    $descriptorCrc, $commitCrc, $statusWrites)
if ($preBannerPasses -gt 0) {
    Write-Host ("H4V1 COLD BOOT LATE_ATTACH recovered prebanner_passes={0}" -f `
        $preBannerPasses)
}
exit 0
