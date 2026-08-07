param(
    [Parameter(Mandatory = $true)]
    [string]$Port,

    [ValidateRange(10, 600)]
    [int]$TimeoutSeconds = 180
)

function New-SdramSerialPort {
    $instance = [System.IO.Ports.SerialPort]::new(
        $Port,
        115200,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One
    )
    $instance.DtrEnable = $true
    $instance.ReadTimeout = 200
    $instance.WriteTimeout = 1000
    return $instance
}

function Close-SdramSerialPort {
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
        # A watchdog reset can remove the device before Close() completes.
    }
    $Instance.Dispose()
}

$received = [System.Text.StringBuilder]::new()
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$nextStart = [DateTime]::MinValue
$testStarted = $false
$continuousMode = $false
$serial = $null
$reconnects = 0

try {
    while ($continuousMode -or [DateTime]::UtcNow -lt $deadline) {
        if ($null -eq $serial -or -not $serial.IsOpen) {
            try {
                $serial = New-SdramSerialPort
                $serial.Open()
                $serial.DiscardInBuffer()
                $serial.DiscardOutBuffer()
                if ($reconnects -gt 0) {
                    Write-Host "`nCDC RECONNECTED port=$Port count=$reconnects"
                }
                $nextStart = [DateTime]::MinValue
                $testStarted = $false
            }
            catch {
                Close-SdramSerialPort -Instance $serial
                $serial = $null
                Start-Sleep -Milliseconds 500
                continue
            }
        }

        try {
            if (-not $testStarted -and [DateTime]::UtcNow -ge $nextStart) {
                $serial.Write("start`r`n")
                $nextStart = [DateTime]::UtcNow.AddSeconds(1)
            }

            $available = $serial.BytesToRead
            if ($available -gt 0) {
                $bytes = [byte[]]::new($available)
                $count = $serial.Read($bytes, 0, $bytes.Length)
                if ($count -gt 0) {
                    $text = [System.Text.Encoding]::ASCII.GetString(
                        $bytes,
                        0,
                        $count
                    )
                    [void]$received.Append($text)
                    Write-Host -NoNewline $text

                    $allText = $received.ToString()
                    if ($allText.Contains("H417 SDRAM CDC TEST") -or
                        $allText.Contains("H417 SDRAM LTDC TEST")) {
                        $testStarted = $true
                    }
                    if ($allText.Contains("CONTINUOUS START")) {
                        $continuousMode = $true
                    }
                    if ($allText.Contains("RESULT PASS")) {
                        exit 0
                    }
                    if ($allText.Contains("RESULT FAIL")) {
                        exit 2
                    }
                    if ($received.Length -gt 4096) {
                        $tailLength = [Math]::Min(512, $allText.Length)
                        $tail = $allText.Substring($allText.Length - $tailLength)
                        [void]$received.Clear()
                        [void]$received.Append($tail)
                    }
                }
            }
        }
        catch {
            Close-SdramSerialPort -Instance $serial
            $serial = $null
            $reconnects++
            $continuousMode = $false
            Start-Sleep -Milliseconds 500
            continue
        }

        Start-Sleep -Milliseconds 20
    }
}
finally {
    Close-SdramSerialPort -Instance $serial
}

Write-Error "Timed out waiting for the H417 SDRAM result on $Port."
exit 1
