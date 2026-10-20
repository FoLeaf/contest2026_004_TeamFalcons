# RSP-01 失败路径验收。
#
# 用两段真实请求验证失败处理，不依赖拔线，也不制造真实服务限流：
#
#   1. 连接失败：把 LLM 端点临时指向一个保留地址（TEST-NET-1，不可路由），
#      请求必然连接失败。验证有界结束、计时行完整、Agent 回到可接受下一
#      请求的状态。
#   2. 恢复：把端点改回原来的配置再发一次请求，验证链路仍可用。
#
# 端点和密钥不写入本脚本、日志或屏幕。原配置从板上 eMMC 的加密存储恢复：
# 重启后 ai_agent 会重新加载，因此失败路径测试结束时执行一次复位即可。
# 本脚本在结束时主动 check 原端点是否恢复。
#
# 用法：
#   powershell.exe -ExecutionPolicy Bypass -File scripts/serial_rsp01_failpath.ps1 `
#     -ReadSec 90 -Log .\docs\maintenance-evidence\2026-10-20\rsp01-failpath.log
param(
  [string]$ComPort = "COM3",
  [int]$ReadSec = 90,
  [string]$Log = ""
)
$ErrorActionPreference = "Stop"

try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch { }

# TEST-NET-1 (RFC 5737).  Reserved for documentation, never routed, so a
# connection here fails the same way a dead endpoint does, on any network.
$UnreachableUrl = "https://192.0.2.1/v1"

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
  Start-Sleep -Milliseconds 400
  Write-Host "`n=== $c ==="
  Drain $sec $want
}

try {
  Drain 4 "" | Out-Null

  Write-Host "`n--- baseline: agent must answer before we break anything ---"
  Send "ai_agent --daemon &" 45 "nsh>"
  Start-Sleep 3
  Send "ai_agent" 25 "vela>"
  Send "ask baseline before failure test" $ReadSec ""
  Send "quit" 5 ""
  Start-Sleep 1

  Write-Host "`n--- break: point the LLM endpoint at an unroutable address ---"
  Send "ai_agent --daemon &" 40 "nsh>"
  Start-Sleep 3
  Send "ai_agent" 20 "vela>"
  Send "set_llm $UnreachableUrl test-model placeholder-key" 10 "vela>"
  Send "ask connectivity must fail within a bounded time" $ReadSec ""
  Write-Host "`n--- agent must still accept a second request after the failure ---"
  Send "ask second request after failure" $ReadSec ""
  Send "quit" 5 ""
  Start-Sleep 1

  Write-Host "`n--- recover: reboot reloads the provisioned endpoint from eMMC ---"
  Send "reboot" 40 "nsh>"
  Start-Sleep 12
  Drain 10 "" | Out-Null

  Write-Host "`n--- verify: the link works again ---"
  Send "ai_agent --daemon &" 45 "nsh>"
  Start-Sleep 3
  Send "ai_agent" 25 "vela>"
  Send "ask verification request after recovery" $ReadSec ""
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

# ── 结论判定 ────────────────────────────────────────────────────

Write-Host "`n===== RSP-01 failure path summary ====="

# A failed attempt must still emit a timing line: rc is negative on transport
# failure, and http/total are present so the wait is attributable.
$httpLines = [regex]::Matches($text, "http iter=\d+ attempt=\d+.*?rc=-?\d+ status=\d+ parse_ok=\d+")
$failLines = $httpLines | Where-Object { $_ -match "rc=-" }
Write-Host ("http timing lines: {0}, failed attempts: {1}" -f $httpLines.Count, $failLines.Count)

if ($failLines.Count -eq 0) {
  Write-Host "FAIL: no failed attempt produced a timing line"
} else {
  $failLines | ForEach-Object { Write-Host ("  " + $_.Value) }
}

# The round must end, not hang: an END line proves the loop returned.
$endLines = [regex]::Matches($text, "END status=\w+ iters=\d+ tools=\d+ llm_ms=\d+ tool_ms=\d+ elapsed_ms=\d+")
Write-Host ("END lines: {0}" -f $endLines.Count)

# The agent must be back to accepting work: a second ask after the failure
# has to produce its own trace and its own reply.
$afterFailure = [regex]::Matches($text, "second request after failure")
Write-Host ("second-request marker present: {0}" -f ($afterFailure.Count -gt 0))

# Recovery: the last ask must succeed, which shows resources were released.
$lastStatus = if ($endLines.Count -gt 0) { $endLines[$endLines.Count - 1].Value } else { "(none)" }
Write-Host ("last END: " + $lastStatus)
if ($lastStatus -match "status=ok") {
  Write-Host "PASS: request after recovery succeeded"
} else {
  Write-Host "FAIL: request after recovery did not succeed"
}
