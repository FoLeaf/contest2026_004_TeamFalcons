param(
  [string]$ComPort = "COM3",
  [int]$WaitSec = 180,
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
  Start-Sleep -Milliseconds 300
  Write-Host "`n########## $command ##########"
  return (Drain $seconds)
}

try {
  Drain 2 | Out-Null
  $deadline = (Get-Date).AddSeconds($WaitSec)
  $matched = $false

  while ((Get-Date) -lt $deadline) {
    $status = SendRead "vgagent status" 2
    if ($status -match "state=running" -and $status -match "owner=advice") {
      $matched = $true
      SendRead "vgagent ask read ups_load current value" 4 | Out-Null
      SendRead "vgagent status" 3 | Out-Null
      break
    }
    Start-Sleep -Seconds 1
  }

  if (-not $matched) {
    Write-Host "`nBUSY-PROBE: no running advice round observed"
    exit 2
  }

  Write-Host "`nBUSY-PROBE: matched running advice round"
}
finally {
  $port.Close()
  if ($Log -ne "") {
    ($captured -join "") | Set-Content -LiteralPath $Log -Encoding UTF8
    Write-Host "[saved] $Log"
  }
}
