# Stage1 agent-ops 板端验收（COM3，需 VG_AGENT_AUTOSTART=y）。
# autostart 为 --daemon 后台；交互前先 nsh> ai_agent 进入 vela>。
#   powershell.exe -ExecutionPolicy Bypass -File scripts/stage1_agent_ops_accept.ps1

param(
  [string]$ComPort = "COM3",
  [int]$Baud = 115200,
  [string]$MimoToken = $env:MIMO_API_KEY
)

$ErrorActionPreference = "Stop"
$script:pass = 0
$script:fail = 0

function Wait-Prompt {
  param([string]$Buf, [int]$TimeoutSec = 12)
  $deadline = (Get-Date).AddSeconds($TimeoutSec)
  while ((Get-Date) -lt $deadline) {
    if ($Buf -match "vela>" -or $Buf -match "nsh>") { return $true }
    Start-Sleep -Milliseconds 120
  }
  return $false
}

function Send-Serial {
  param(
    [System.IO.Ports.SerialPort]$Port,
    [string]$Cmd,
    [int]$WaitSec = 20,
    [switch]$WantVela
  )
  $Port.DiscardInBuffer()
  $Port.WriteLine($Cmd)
  Start-Sleep -Milliseconds 600
  $buf = ""
  $deadline = (Get-Date).AddSeconds($WaitSec)
  while ((Get-Date) -lt $deadline) {
    if ($Port.BytesToRead -gt 0) { $buf += $Port.ReadExisting() }
    if ($WantVela -and $buf -match "vela>") { break }
    if (-not $WantVela -and (Wait-Prompt $buf 1)) { break }
    Start-Sleep -Milliseconds 120
  }
  $block = "`n=== $Cmd ===`n$buf"
  Write-Output $block
  return @{ Text = $buf; Block = $block }
}

function Assert-Match {
  param([string]$Name, [string]$Hay, [string]$Pattern)
  if ($Hay -match $Pattern) {
    Write-Output "[PASS] $Name"
    $script:pass++
  } else {
    Write-Output "[FAIL] $Name (pattern: $Pattern)"
    $script:fail++
  }
}

$port = New-Object System.IO.Ports.SerialPort
$port.PortName = $ComPort
$port.BaudRate = $Baud
$port.ReadTimeout = 8000
$port.NewLine = "`n"

try {
  $port.Open()
  Start-Sleep -Milliseconds 1500
  while ($port.BytesToRead -gt 0) { [void]$port.ReadExisting(); Start-Sleep -Milliseconds 80 }

  # Wait for autostart (up to 45s after reset)
  $boot = ""
  $deadline = (Get-Date).AddSeconds(45)
  while ((Get-Date) -lt $deadline) {
    if ($port.BytesToRead -gt 0) { $boot += $port.ReadExisting() }
    if ($boot -match "vgagent: ai_agent autostart ok" -and $boot -match "nsh>") { break }
    Start-Sleep -Milliseconds 200
  }
  Assert-Match "autostart ok" $boot "vgagent: ai_agent autostart ok"

  $r = Send-Serial $port "ls /data/agent/skills" 12
  Assert-Match "operations_report skill" $r.Text "operations_report"
  Assert-Match "alarm_interpretation skill" $r.Text "alarm_interpretation"
  Assert-Match "modbus_query skill" $r.Text "modbus_query"

  $r = Send-Serial $port "cat /data/agent/HEARTBEAT.md" 10
  Assert-Match "HEARTBEAT file" $r.Text "HEARTBEAT|heartbeat|告警|日报"

  $r = Send-Serial $port "ai_agent" 25 -WantVela
  Assert-Match "vela after ai_agent attach" $r.Text "vela>"

  if (-not $MimoToken -or $MimoToken.Length -lt 8) {
    Write-Output "[INFO] MIMO_API_KEY unset; using eMMC /data/agent/config/config.json if present"
  }

  $r = Send-Serial $port 'ask 按 operations_report Skill 生成今日运营日报，写入 /data/velaguard/reports/' 120
  Assert-Match "no LLM watchdog timeout" $r.Text "(?!watchdog)(?!请求超时)"
  Assert-Match "agent replied" $r.Text "\[Agent\]|报告|日报|operations|read_file"

  $r = Send-Serial $port "quit" 15
  Start-Sleep -Seconds 1

  $r = Send-Serial $port "ls /data/velaguard/reports" 10
  Assert-Match "reports dir" $r.Text "daily|reports|\.md"

  Write-Output "`n[stage1_agent_ops_accept] pass=$($script:pass) fail=$($script:fail)"
  if ($script:fail -gt 0) { exit 1 }
}
catch {
  Write-Error "stage1_agent_ops_accept failed: $($_.Exception.Message)"
  exit 1
}
finally {
  if ($port.IsOpen) { $port.Close() }
}
