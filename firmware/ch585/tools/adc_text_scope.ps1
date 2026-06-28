param(
    [Parameter(Mandatory = $true)]
    [string]$Port,
    [int]$Baud = 115200,
    [ValidateSet("right", "left", "raw")]
    [string]$Side = "right",
    [ValidateSet("mapped", "all")]
    [string]$View = "mapped",
    [double]$Refresh = 0.2,
    [double]$BaselineSeconds = 1.0,
    [double]$Duration = 0,
    [int]$Top = 8,
    [switch]$NoClear
)

$RightLabels = @{
    0 = "F12"; 1 = "F11"; 2 = "F10"; 3 = "F9"; 4 = "F8"; 5 = "F7"; 6 = "F6"
    7 = "Backspace"; 8 = "Equal"; 9 = "Minus"
    16 = "0"; 17 = "9"; 18 = "8"; 19 = "7"; 20 = "Backslash"; 21 = "RBracket"; 22 = "LBracket"; 23 = "P"; 24 = "O"; 25 = "I"
    32 = "U"; 33 = "Y"; 34 = "Enter"; 35 = "Quote"; 36 = "Semicolon"; 37 = "L"; 38 = "K"; 39 = "J"; 40 = "H"; 41 = "Shift"
    48 = "Slash"; 49 = "Dot"; 50 = "Comma"; 51 = "M"; 52 = "N"; 53 = "B"; 54 = "Ctrl"; 55 = "Win"; 56 = "Fn"; 57 = "Alt"; 58 = "Space"
}

$RightHallIds = @{
    0 = 1; 1 = 2; 2 = 3; 3 = 4; 4 = 5; 5 = 6; 6 = 7
    7 = 8; 8 = 9; 9 = 10
    16 = 11; 17 = 12; 18 = 13; 19 = 14; 20 = 15; 21 = 16; 22 = 17; 23 = 18; 24 = 19; 25 = 20
    32 = 21; 33 = 22; 34 = 23; 35 = 24; 36 = 25; 37 = 26; 38 = 27; 39 = 28; 40 = 29; 41 = 30
    48 = 31; 49 = 32; 50 = 33; 51 = 34; 52 = 35; 53 = 36; 54 = 37; 55 = 38; 56 = 39; 57 = 40; 58 = 41
}

$LeftLabels = @{
    0 = "F5"; 1 = "F4"; 2 = "F3"; 3 = "F2"; 4 = "F1"; 5 = "Esc"; 6 = "6"; 7 = "5"; 8 = "4"
    16 = "3"; 17 = "2"; 18 = "1"; 19 = "Grave"; 20 = "Y"; 21 = "T"; 22 = "R"; 23 = "E"; 24 = "W"
    32 = "Q"; 33 = "Tab"; 34 = "G"; 35 = "F"; 36 = "D"; 37 = "S"; 38 = "A"; 39 = "Caps"; 40 = "B"
    48 = "V"; 49 = "C"; 50 = "X"; 51 = "Z"; 52 = "Shift"; 53 = "Space"; 54 = "Alt"; 55 = "Win"; 56 = "Ctrl"
}

$LeftHallIds = @{
    0 = 42; 1 = 43; 2 = 44; 3 = 45; 4 = 46; 5 = 47; 6 = 48; 7 = 49; 8 = 50
    16 = 51; 17 = 52; 18 = 53; 19 = 54; 20 = 55; 21 = 56; 22 = 57; 23 = 58; 24 = 59
    32 = 60; 33 = 61; 34 = 62; 35 = 63; 36 = 64; 37 = 65; 38 = 66; 39 = 67; 40 = 68
    48 = 69; 49 = 70; 50 = 71; 51 = 72; 52 = 73; 53 = 74; 54 = 75; 55 = 76; 56 = 77
}

function Get-KeyLabel {
    param([int]$Key, [string]$LineLabel)
    if ($LineLabel -and $LineLabel -ne "-") { return $LineLabel }
    if ($Side -eq "right" -and $RightLabels.ContainsKey($Key)) { return $RightLabels[$Key] }
    if ($Side -eq "left" -and $LeftLabels.ContainsKey($Key)) { return $LeftLabels[$Key] }
    return "-"
}

function Get-HallId {
    param([int]$Key, [int]$LineHall)
    if ($Side -eq "right" -and $RightHallIds.ContainsKey($Key)) { return $RightHallIds[$Key] }
    if ($Side -eq "left" -and $LeftHallIds.ContainsKey($Key)) { return $LeftHallIds[$Key] }
    if ($LineHall -gt 0) { return $LineHall }
    return -1
}

function New-Bar {
    param([int]$Value, [int]$Max = 1023, [int]$Width = 20, [string]$Fill = "#", [string]$Empty = ".")
    if ($Value -lt 0) { $Value = 0 }
    if ($Value -gt $Max) { $Value = $Max }
    $count = [int][math]::Round(($Value * $Width) / [double]$Max)
    return ($Fill * $count) + ($Empty * ($Width - $count))
}

function Test-MappedKey {
    param([int]$Key)
    if ($Side -eq "right") { return $RightLabels.ContainsKey($Key) }
    if ($Side -eq "left") { return $LeftLabels.ContainsKey($Key) }
    return $true
}

function Get-VisibleKeys {
    param([hashtable]$State)

    if ($View -eq "all" -or $Side -eq "raw") {
        return 0..63
    }

    if ($Side -eq "right") {
        return ($RightLabels.Keys | Sort-Object)
    }

    if ($Side -eq "left") {
        return ($LeftLabels.Keys | Sort-Object)
    }

    return ($State.Keys | Sort-Object)
}

function New-DropBar {
    param([int]$Drop, [int]$Scale = 256, [int]$Width = 16)
    if ($Drop -lt 0) { $Drop = 0 }
    if ($Drop -gt $Scale) { $Drop = $Scale }
    $count = [int][math]::Round(($Drop * $Width) / [double]$Scale)
    return ("!" * $count) + ("." * ($Width - $count))
}

function Update-StateFromLine {
    param([hashtable]$State, [string]$Line)

    $pattern = '^AP\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\S+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(0x[0-9a-fA-F]+)\s+(\d+)\s+(\d+)\s+(-?\d+)'
    $m = [regex]::Match($Line.Trim(), $pattern)
    if (-not $m.Success) { return }

    $key = [int]$m.Groups[3].Value
    $raw = [int]$m.Groups[8].Value

    if (-not $State.ContainsKey($key)) {
        $State[$key] = [ordered]@{
            Key = $key
            Min = $raw
            Max = $raw
            Baseline = $null
            BaselineSum = 0
            BaselineCount = 0
        }
    }

    $item = $State[$key]
    $item.Seq = [int]$m.Groups[1].Value
    $item.Hall = Get-HallId -Key $key -LineHall ([int]$m.Groups[2].Value)
    $item.Lane = [int]$m.Groups[4].Value
    $item.Mux = [int]$m.Groups[5].Value
    $item.D = [int]$m.Groups[6].Value
    $item.Label = Get-KeyLabel -Key $key -LineLabel $m.Groups[7].Value
    $item.Raw = $raw
    $item.Filt = [int]$m.Groups[9].Value
    $item.Pos = [int]$m.Groups[10].Value
    $item.Down = [int]$m.Groups[12].Value
    $item.Status = [int]$m.Groups[14].Value
    if ($raw -lt $item.Min) { $item.Min = $raw }
    if ($raw -gt $item.Max) { $item.Max = $raw }
}

function Update-Baseline {
    param([hashtable]$State, [bool]$Collecting)

    foreach ($item in $State.Values) {
        if ($Collecting) {
            $item.BaselineSum += $item.Raw
            $item.BaselineCount += 1
        } elseif ($null -eq $item.Baseline -and $item.BaselineCount -gt 0) {
            $item.Baseline = [int][math]::Round($item.BaselineSum / [double]$item.BaselineCount)
        } elseif ($null -eq $item.Baseline) {
            $item.Baseline = $item.Raw
        }
    }
}

function Get-TopDrops {
    param([hashtable]$State, [int]$Limit)

    $rows = foreach ($item in $State.Values) {
        if ($View -eq "mapped" -and -not (Test-MappedKey -Key $item.Key)) {
            continue
        }
        $base = if ($null -ne $item.Baseline) { $item.Baseline } else { $item.Raw }
        [pscustomobject]@{
            Key = $item.Key
            Hall = $item.Hall
            Lane = $item.Lane
            D = $item.D
            Label = $item.Label
            Raw = $item.Raw
            Base = $base
            Drop = $base - $item.Raw
            Down = $item.Down
        }
    }
    $rows | Sort-Object Drop -Descending | Select-Object -First $Limit
}

function Render-Scope {
    param([hashtable]$State, [double]$Uptime, [bool]$Collecting)

    $baselineText = if ($Collecting) { "collecting" } else { "locked" }
    $lines = New-Object System.Collections.Generic.List[string]
    $visibleKeys = @(Get-VisibleKeys -State $State)
    [void]$lines.Add(("CH585 ADC text scope side={0} view={1} visible={2} samples={3}/64 uptime={4:n1}s baseline={5}" -f $Side, $View, $visibleKeys.Count, $State.Count, $Uptime, $baselineText))
    [void]$lines.Add("raw bar = ADC/1023; norm = raw*100/1023; drop = baseline - raw; Hall press usually makes raw smaller.")
    [void]$lines.Add("")
    [void]$lines.Add("Top drops:")
    foreach ($row in (Get-TopDrops -State $State -Limit $Top)) {
        [void]$lines.Add(("  H{0:00} K{1:00} L{2}D{3:00} {4,-10} raw={5:0000} base={6:0000} drop={7,4} down={8}" -f `
            $row.Hall, $row.Key, ($row.Lane + 1), $row.D, $row.Label.Substring(0, [Math]::Min(10, $row.Label.Length)), $row.Raw, $row.Base, $row.Drop, $row.Down))
    }
    [void]$lines.Add("")

    if ($View -eq "mapped" -and $Side -ne "raw") {
        [void]$lines.Add("Mapped keys:")
        foreach ($key in $visibleKeys) {
            $lane = [int][math]::Floor($key / 16)
            $d = ($key % 16) + 1
            $label = Get-KeyLabel -Key $key -LineLabel "-"

            if (-not $State.ContainsKey($key)) {
                $hall = Get-HallId -Key $key -LineHall 0
                [void]$lines.Add(("  H{0:00} K{1:00} L{2}D{3:00} {4,-10} raw=---- norm=---% [....................]" -f $hall, $key, ($lane + 1), $d, $label.Substring(0, [Math]::Min(10, $label.Length))))
                continue
            }

            $item = $State[$key]
            $hall = Get-HallId -Key $key -LineHall $item.Hall
            $base = if ($null -ne $item.Baseline) { $item.Baseline } else { $item.Raw }
            $drop = $base - $item.Raw
            $norm = [int][math]::Round(($item.Raw * 100.0) / 1023.0)
            $shortLabel = $label.Substring(0, [Math]::Min(10, $label.Length))
            [void]$lines.Add(("  H{0:00} K{1:00} L{2}D{3:00} {4,-10} raw={5:0000} norm={6,3}% [{7}] drop={8,4} [{9}] min={10:0000} max={11:0000} down={12}" -f `
                $hall, $key, ($lane + 1), $d, $shortLabel, $item.Raw, $norm, (New-Bar -Value $item.Raw), $drop, (New-DropBar -Drop $drop), $item.Min, $item.Max, $item.Down))
        }
        return ($lines -join [Environment]::NewLine)
    }

    for ($lane = 0; $lane -lt 4; $lane++) {
        [void]$lines.Add(("Lane {0} / MUX{0}" -f ($lane + 1)))
        for ($d = 1; $d -le 16; $d++) {
            $key = $lane * 16 + ($d - 1)
            if (-not $State.ContainsKey($key)) {
                [void]$lines.Add(("  K{0:00} D{1:00} {2,-10} raw=---- [....................]" -f $key, $d, "-"))
                continue
            }

            $item = $State[$key]
            $base = if ($null -ne $item.Baseline) { $item.Baseline } else { $item.Raw }
            $drop = $base - $item.Raw
            $norm = [int][math]::Round(($item.Raw * 100.0) / 1023.0)
            $label = $item.Label.Substring(0, [Math]::Min(10, $item.Label.Length))
            [void]$lines.Add(("  K{0:00} D{1:00} {2,-10} raw={3:0000} norm={4,3}% [{5}] drop={6,4} [{7}] min={8:0000} max={9:0000} down={10}" -f `
                $key, $d, $label, $item.Raw, $norm, (New-Bar -Value $item.Raw), $drop, (New-DropBar -Drop $drop), $item.Min, $item.Max, $item.Down))
        }
        [void]$lines.Add("")
    }

    return ($lines -join [Environment]::NewLine)
}

$serial = [System.IO.Ports.SerialPort]::new($Port, $Baud, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$serial.ReadTimeout = 50
$serial.DtrEnable = $true
$serial.RtsEnable = $true
$state = @{}
$buffer = ""
$start = Get-Date
$nextRender = Get-Date

try {
    $serial.Open()
    while ($true) {
        $now = Get-Date
        $uptime = ($now - $start).TotalSeconds
        if ($Duration -gt 0 -and $uptime -ge $Duration) { break }

        $chunk = $serial.ReadExisting()
        if ($chunk.Length -gt 0) {
            $buffer += $chunk
            while ($buffer.Contains("`n")) {
                $idx = $buffer.IndexOf("`n")
                $line = $buffer.Substring(0, $idx)
                $buffer = $buffer.Substring($idx + 1)
                Update-StateFromLine -State $state -Line $line
            }
        }

        $collecting = $uptime -le $BaselineSeconds
        Update-Baseline -State $state -Collecting $collecting

        if ($now -ge $nextRender) {
            if (-not $NoClear) { Clear-Host }
            Write-Output (Render-Scope -State $state -Uptime $uptime -Collecting $collecting)
            $nextRender = $now.AddSeconds($Refresh)
        }

        Start-Sleep -Milliseconds 10
    }
} finally {
    if ($serial.IsOpen) { $serial.Close() }
}

Write-Output ""
Write-Output "--- final ---"
Write-Output (Render-Scope -State $state -Uptime ((Get-Date) - $start).TotalSeconds -Collecting $false)
