param(
    [string]$Port = "COM5",
    [int]$Baud = 115200
)

$serial = New-Object System.IO.Ports.SerialPort $Port, $Baud, "None", 8, "One"
$serial.ReadTimeout = 1000

try {
    $serial.Open()
    Write-Host "reading $Port at $Baud; press Ctrl+C to stop"
    while ($true) {
        try {
            $line = $serial.ReadLine()
            if ($line) {
                Write-Host $line.TrimEnd()
            }
        }
        catch [System.TimeoutException] {
        }
    }
}
finally {
    if ($serial.IsOpen) {
        $serial.Close()
    }
}
