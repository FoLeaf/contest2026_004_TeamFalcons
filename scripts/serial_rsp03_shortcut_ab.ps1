# RSP-03 part 2: paired measurement of the single-point reply shortcut.
#
# Same firmware runs both arms; "point_shortcut off" restores the previous
# two-request behaviour.  Alternating within one session keeps network drift
# away from the comparison, which matters because the server's first-byte
# delay swings between roughly 2 s and 8 s.
#
# Each sample records the whole round (elapsed_ms) and how many LLM requests
# it took, which is the effect under test.
#
# Usage:
#   powershell.exe -ExecutionPolicy Bypass -File scripts/serial_rsp03_shortcut_ab.ps1 `
#     -Pairs 10 -ReadSec 90 -Log .\docs\maintenance-evidence\2026-10-20\rsp03-shortcut-ab.log
param(
  [string]$ComPort = "COM3",
  [int]$Pairs = 10,
  [int]$ReadSec = 90,
  [string]$Log = ""
)
$ErrorActionPreference = "Stop"
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch { }

$port = New-Object System.IO.Ports.SerialPort $ComPort, 115200
$port.Encoding = [System.Text.Encoding]::UTF8
$port.ReadTimeout = 8000
$port.DtrEnable = $true
$port.RtsEnable = $true
$port.Open()
Start-Sleep 1

$captured = New-Object System.Collections.Generic.List[string]

function Drain([int]$sec, [string]$want) {
  $dl = (Get-Date).AddSeconds($sec)
  while ((Get-Date) -lt $dl) {
    if ($port.BytesToRead -gt 0) {
      $chunk = $port.ReadExisting()
      Write-Host -NoNewline $chunk
      $captured.Add($chunk)
      if ($want -ne "" -and $chunk -match $want) { break }
    }
    Start-Sleep -Milliseconds 100
  }
}

function Send([string]$c, [int]$sec, [string]$want) {
  $port.DiscardInBuffer()
  $port.Write("$c`r")
  Start-Sleep -Milliseconds 350
  Write-Host "`n=== $c ==="
  Drain $sec $want
}

try {
  Drain 4 "" | Out-Null
  Send "ai_agent --daemon &" 45 "nsh>"
  Start-Sleep 3
  Send "ai_agent" 25 "vela>"

  for ($i = 1; $i -le $Pairs; $i++) {
    # Alternate which arm runs first so ordering cannot favour one.
    if ($i % 2 -eq 1) { $order = @("on", "off") } else { $order = @("off", "on") }

    foreach ($arm in $order) {
      Send "point_shortcut $arm" 6 "vela>" | Out-Null
      Write-Host "`n--- pair $i arm=$arm ---"
      Send "ask 告诉我UPS负载的值" $ReadSec ""
    }
  }

  Send "quit" 5 ""
  Start-Sleep 1
}
finally {
  if ($port.IsOpen) { $port.Close() }
}

$text = ($captured -join "")
if ($Log -ne "") {
  $text | Set-Content -LiteralPath $Log -Encoding UTF8
  Write-Host "`n[saved] $Log"
}

# ── 提取 ────────────────────────────────────────────────────────
#
# A round's arm is the last "point shortcut: X" line before its BEGIN, and
# its cost is the END line that follows.  Requests per round come from
# counting http lines between BEGIN and END.

Write-Host "`n===== RSP-03 shortcut A/B ====="

$rounds = @()
$currentArm = "on"
$inRound = $false
$round = $null

foreach ($line in ($text -split "`r?`n")) {
  if ($line -match "point shortcut: (on|off)") {
    $currentArm = $Matches[1]
  }
  if ($line -match "BEGIN chat=") {
    $inRound = $true
    $round = @{ arm = $currentArm; http = 0; end = $null }
    continue
  }
  if ($inRound -and $line -match "http iter=") { $round.http++ }
  if ($inRound -and $line -match "END status=(\w+) .*elapsed_ms=(\d+)") {
    $round.end = [int]$Matches[2]
    $round.status = $Matches[1]
    $rounds += [pscustomobject]$round
    $inRound = $false
  }
}

# Tool-side rounds (board-initiated advice) also emit traces; only rounds that
# actually called the point tool twice or once belong to this test.  Keep
# every round and report both, then the per-arm summary.

$rounds | ForEach-Object {
  Write-Host ("  arm={0,-3} http={1} status={2,-7} elapsed={3}ms" -f
    $_.arm, $_.http, $_.status, $_.end)
}

foreach ($arm in @("on", "off")) {
  $sel = $rounds | Where-Object { $_.arm -eq $arm }
  if ($sel.Count -eq 0) { continue }
  $vals = $sel | ForEach-Object { $_.end } | Sort-Object
  $med = $vals[[int]([math]::Floor($vals.Count / 2))]
  $http = ($sel | ForEach-Object { $_.http } | Measure-Object -Average).Average
  Write-Host ("{0,-3} n={1} median={2}ms max={3}ms http_per_round={4:n1}" -f
    $arm, $sel.Count, $med, ($vals | Measure-Object -Maximum).Maximum, $http)
}
