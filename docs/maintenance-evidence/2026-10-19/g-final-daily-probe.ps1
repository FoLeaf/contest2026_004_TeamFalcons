param(
  [string]$ComPort = "COM3",
  [int]$WaitSec = 240,
  [string]$Log = ""
)
$ErrorActionPreference = "Stop"
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch { }
$port = New-Object System.IO.Ports.SerialPort $ComPort, 115200
$port.Encoding = [System.Text.Encoding]::UTF8
$port.ReadTimeout = 5000
$port.DtrEnable = $true
$port.RtsEnable = $true
$port.Open()
$captured = New-Object System.Collections.Generic.List[string]
function Drain([int]$seconds) {
  $text = ""
  $deadline = (Get-Date).AddSeconds($seconds)
  while ((Get-Date) -lt $deadline) {
    if ($port.BytesToRead -gt 0) {
      $chunk = $port.ReadExisting()
      $text += $chunk
      $captured.Add($chunk)
      Write-Host -NoNewline $chunk
    }
    Start-Sleep -Milliseconds 100
  }
  return $text
}
function SendRead([string]$command, [int]$seconds) {
  $port.DiscardInBuffer()
  $port.Write("$command`r")
  Start-Sleep -Milliseconds 250
  Write-Host "`n########## $command ##########"
  return (Drain $seconds)
}
try {
  Drain 2 | Out-Null
  SendRead "ls -l /data/velaguard/reports/daily-2026-10-19.md" 2 | Out-Null
  SendRead "cp /data/velaguard/reports/daily-2026-10-19.md /data/velaguard/reports/daily-2026-10-19.baseline.md" 2 | Out-Null
  SendRead "ls -l /data/velaguard/reports/daily-2026-10-19.baseline.md" 2 | Out-Null
  SendRead "rm /data/velaguard/reports/daily-2026-10-19.md" 2 | Out-Null
  SendRead "ls -l /data/velaguard/reports/daily-2026-10-19.md" 2 | Out-Null
  Write-Host "`nDAILY-PROBE: observing scheduler"
  $deadline = (Get-Date).AddSeconds($WaitSec)
  while ((Get-Date) -lt $deadline) {
    Drain 1 | Out-Null
  }
}
finally {
  $port.Close()
  if ($Log -ne "") {
    ($captured -join "") | Set-Content -LiteralPath $Log -Encoding UTF8
    Write-Host "`n[saved] $Log"
  }
}
