param(
    [string]$Port = "COM7",
    [int]$BaudRate = 115200,
    [int]$Seconds = 60,
    [string]$LogPath = ""
)

$serial = New-Object System.IO.Ports.SerialPort $Port, $BaudRate, "None", 8, "One"
$serial.ReadTimeout = 500
$lineCount = 0
$byteCount = 0
$firstLine = $null
$lastLine = $null
$buffer = ""

if ($LogPath -ne "") {
    $dir = Split-Path -Parent $LogPath
    if (($dir -ne "") -and -not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir | Out-Null
    }
    Set-Content -LiteralPath $LogPath -Value "" -Encoding ASCII
}

try {
    $serial.Open()
    $deadline = (Get-Date).AddSeconds($Seconds)

    while ((Get-Date) -lt $deadline) {
        $chunk = $serial.ReadExisting()
        if ($chunk.Length -gt 0) {
            $byteCount += $chunk.Length
            $buffer += $chunk

            while ($buffer.Contains("`n")) {
                $idx = $buffer.IndexOf("`n")
                $line = $buffer.Substring(0, $idx).Trim("`r", "`n")
                $buffer = $buffer.Substring($idx + 1)
                if ($line.Length -gt 0) {
                    if ($null -eq $firstLine) {
                        $firstLine = $line
                    }
                    $lastLine = $line
                    $lineCount++
                    if ($LogPath -ne "") {
                        Add-Content -LiteralPath $LogPath -Value $line -Encoding ASCII
                    }
                }
            }
        }
        Start-Sleep -Milliseconds 50
    }

    if ($buffer.Trim().Length -gt 0) {
        $line = $buffer.Trim()
        if ($null -eq $firstLine) {
            $firstLine = $line
        }
        $lastLine = $line
        $lineCount++
        if ($LogPath -ne "") {
            Add-Content -LiteralPath $LogPath -Value $line -Encoding ASCII
        }
    }
}
finally {
    if ($serial.IsOpen) {
        $serial.Close()
    }
}

Write-Output ("port={0} seconds={1} lines={2} bytes={3}" -f $Port, $Seconds, $lineCount, $byteCount)
Write-Output ("first={0}" -f $firstLine)
Write-Output ("last={0}" -f $lastLine)
if ($LogPath -ne "") {
    Write-Output ("log={0}" -f $LogPath)
}
