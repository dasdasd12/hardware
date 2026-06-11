$port = New-Object System.IO.Ports.SerialPort COM5, 115200, None, 8, One
$port.Open()
$port.DiscardInBuffer()
Start-Sleep -Seconds 2

$buf = ''
$deadline = (Get-Date).AddSeconds(10)
while ((Get-Date) -lt $deadline) {
    $n = $port.BytesToRead
    if ($n -gt 0) {
        $bytes = New-Object byte[] $n
        $port.Read($bytes, 0, $n)
        $buf += [System.Text.Encoding]::ASCII.GetString($bytes)
        $deadline = (Get-Date).AddSeconds(3)
    }
    Start-Sleep -Milliseconds 100
}
$port.Close()

if ($buf.Length -eq 0) {
    Write-Host '[NO OUTPUT] COM5 no data received'
} else {
    Write-Host $buf
}
