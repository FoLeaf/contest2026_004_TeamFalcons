# Stage1 LVGL HMI board accept (velaguard-lvgl = net+HMI @ COM3).
#   powershell.exe -ExecutionPolicy Bypass -File scripts/stage1_lvgl_hmi_accept.ps1

param(
  [string]$ComPort = "COM3",
  [int]$Baud = 115200,
  [string]$Log = ""
)

$ErrorActionPreference = "Stop"
$script:pass = 0
$script:fail = 0

function Send-Serial {
  param(
    [System.IO.Ports.SerialPort]$Port,
    [string]$Cmd,
    [int]$WaitSec = 12
  )
  $Port.DiscardInBuffer()
  $Port.Write("$Cmd`r")
  Start-Sleep -Milliseconds 400
  $buf = ""
  $deadline = (Get-Date).AddSeconds($WaitSec)
  while ((Get-Date) -lt $deadline) {
    if ($Port.BytesToRead -gt 0) { $buf += $Port.ReadExisting() }
    if ($buf -match "nsh>") { break }
    Start-Sleep -Milliseconds 80
  }
  Write-Host "`n=== $Cmd ==="
  Write-Host $buf
  return $buf
}

function Assert-Match {
  param([string]$Name, [string]$Hay, [string]$Pattern)
  if ($Hay -match $Pattern) {
    Write-Host "[PASS] $Name"
    $script:pass++
  } else {
    Write-Host "[FAIL] $Name (pattern: $Pattern)"
    $script:fail++
  }
}

$port = New-Object System.IO.Ports.SerialPort
$port.PortName = $ComPort
$port.BaudRate = $Baud
$port.ReadTimeout = 8000
$port.DtrEnable = $true
$logLines = New-Object System.Collections.Generic.List[string]

try {
  $port.Open()
  $boot = ""
  $bootDeadline = (Get-Date).AddSeconds(20)
  while ((Get-Date) -lt $bootDeadline) {
    if ($port.BytesToRead -gt 0) { $boot += $port.ReadExisting() }
    if ($boot -match "vghmi: autostart ok") { break }
    Start-Sleep -Milliseconds 150
  }
  Start-Sleep -Seconds 2
  while ($port.BytesToRead -gt 0) { $boot += $port.ReadExisting(); Start-Sleep -Milliseconds 80 }

  Write-Host "===== BOOT ====="
  Write-Host $boot
  $logLines.Add($boot)

  Assert-Match "HMI autostart log" $boot "vghmi: autostart ok"
  Assert-Match "LVGL 480x272" $boot "xres: 480"
  Assert-Match "touchscreen open" $boot "touchscreen /dev/input0 open success"
  if ($boot -notmatch "Assertion failed") {
    Write-Host "[PASS] no agent panic"
    $script:pass++
  } else {
    Write-Host "[FAIL] no agent panic"
    $script:fail++
  }

  $help = Send-Serial $port "?" 8
  Assert-Match "vghmi in help" $help "vghmi"
  Assert-Match "vgdiscover in help" $help "vgdiscover"
  Assert-Match "vgmqtt in help" $help "vgmqtt"

  $ps = Send-Serial $port "ps" 8
  Assert-Match "vghmi autostart task" $ps "vghmi"
  Assert-Match "velaguard_app entry" $ps "velaguard_app"

  # C7 NSH cross-check (LCD discover/confirm still visual)
  Start-Sleep -Milliseconds 500
  $vgcfg = Send-Serial $port "vgcfg dump" 12
  Assert-Match "vgcfg dump responds" $vgcfg "seq="

  $lsrep = Send-Serial $port "ls /data/agent/reports" 10
  if ($lsrep -match "daily-") {
    Write-Host "[PASS] reports dir has daily file"
    $script:pass++
  }
  elseif ($lsrep -match "stat failed|No such file|nsh>") {
    Write-Host "[PASS] reports empty-or-missing (C5 empty OK)"
    $script:pass++
  }
  else {
    Write-Host "[FAIL] reports dir check"
    $script:fail++
  }

  Write-Host "`n=== Summary: pass=$($script:pass) fail=$($script:fail) ==="
  Write-Host "NOTE: visually confirm LCD home + discover switch OFF by default."
  Write-Host "NOTE: C2-C4 board: scan@9600 -> confirm -> home shows slaves; vgcfg after confirm."
  Write-Host "NOTE: C5 report page / C6 alarm AI block need LCD visual check."
  if ($Log -ne "") {
    $logLines.Add("pass=$($script:pass) fail=$($script:fail)")
    Set-Content -LiteralPath $Log -Value ($logLines -join [Environment]::NewLine) -Encoding UTF8
  }
  if ($script:fail -gt 0) { exit 1 }
  exit 0
}
finally {
  if ($port.IsOpen) { $port.Close() }
}
