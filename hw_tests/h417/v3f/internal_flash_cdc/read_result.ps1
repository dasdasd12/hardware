param(
    [Parameter(Mandatory = $true)]
    [string]$Port,

    [ValidateRange(1, 60)]
    [int]$TimeoutSeconds = 45
)

$serial = [System.IO.Ports.SerialPort]::new(
    $Port,
    115200,
    [System.IO.Ports.Parity]::None,
    8,
    [System.IO.Ports.StopBits]::One
)
$serial.DtrEnable = $true
$serial.ReadTimeout = 200

$received = [System.Text.StringBuilder]::new()
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)

try {
    $serial.Open()

    while ([DateTime]::UtcNow -lt $deadline) {
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

                if ($received.ToString().Contains("RESULT PASS")) {
                    exit 0
                }
                if ($received.ToString().Contains("RESULT DEGRADED")) {
                    exit 3
                }
                if ($received.ToString().Contains("RESULT FAIL")) {
                    exit 2
                }
            }
        }

        Start-Sleep -Milliseconds 20
    }
}
finally {
    if ($serial.IsOpen) {
        $serial.Close()
    }
    $serial.Dispose()
}

Write-Error "Timed out waiting for the H417 Flash test result on $Port."
exit 1
