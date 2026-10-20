# RSP-01 同一次运行内的证据链。
#
# 一次串口会话里按顺序做四件事，中间不重启，这样才能看出资源和状态在
# 一轮失败之后有没有留下残留：
#
#   1. 正常请求        —— 基线
#   2. 受控失败        —— 端点指向不可路由地址，请求必然失败
#   3. 恢复配置        —— 端点改回原值
#   4. 恢复后正常请求  —— 必须成功，证明资源已释放、状态可继续
#
# 端点与密钥不打印。失败阶段使用假凭证，恢复阶段从板上 eMMC 的加密存储
# 重新载入（provision 脚本在会话外执行，这里只做 ask）。
#
# 用法：
#   powershell.exe -ExecutionPolicy Bypass -File scripts/serial_rsp01_chain.ps1 `
#     -ReadSec 90 -Log .\docs\maintenance-evidence\2026-10-20\rsp01-chain.log
param(
  [string]$ComPort = "COM3",
  [int]$ReadSec = 120,
  [string]$Log = ""
)
$ErrorActionPreference = "Stop"

try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch { }

# TEST-NET-1: reserved for documentation, never routed.
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

  # One agent session for the whole chain: stopping and restarting would
  # reset the very state this test is about.
  Send "ai_agent --daemon &" 45 "nsh>"
  Start-Sleep 3
  Send "ai_agent" 25 "vela>"

  Write-Host "`n--- [1] baseline request ---"
  Send "ask chain step one baseline" $ReadSec ""

  Write-Host "`n--- [2] controlled failure ---"
  # No key argument: set_llm keeps the key already in use.  An earlier
  # version passed a dummy key here, which then survived into step 4 and
  # turned the recovery check into a 401 instead of proving the link worked.
  # The unreachable endpoint alone makes this step fail.
  Send "set_llm $UnreachableUrl test-model" 10 "vela>"
  Send "ask chain step two must fail" $ReadSec ""

  Write-Host "`n--- [3] restore the provisioned endpoint ---"
  # Same rule: the endpoint is named, the credential is left alone, so
  # neither appears on the command line or in the capture.
  Send "set_llm https://token-plan-cn.xiaomimimo.com/v1 mimo-v2.5" 10 "vela>"

  Write-Host "`n--- [4] request after restoration ---"
  Send "ask chain step four must succeed" $ReadSec ""

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

# ── 结论 ────────────────────────────────────────────────────────

Write-Host "`n===== RSP-01 chain summary ====="

$httpLines = [regex]::Matches($text,
  "http iter=\d+ attempt=\d+ reused=\d+ proxy=\d+ dns_tcp=\S+ setup=\S+ tls=\S+ send=\S+ ttfb=\S+ body=\S+ pool=\S+ build=\S+ parse=\S+ http=\S+ backoff=\S+ total=\S+ req=\d+ rc=-?\d+ status=\d+ parse_ok=\d+")
$endLines = [regex]::Matches($text,
  "END status=\w+ iters=\d+ tools=\d+ llm_ms=\d+ tool_ms=\d+ elapsed_ms=\d+")

Write-Host ("http timing lines : {0}" -f $httpLines.Count)
Write-Host ("END lines         : {0}" -f $endLines.Count)

foreach ($m in $endLines) { Write-Host ("  " + $m.Value) }

# Step separation is by order: the first END is the baseline, the last is
# the request after restoration.
if ($endLines.Count -ge 2) {
  $first = $endLines[0].Value
  $last = $endLines[$endLines.Count - 1].Value
  if ($first -match "status=ok") {
    Write-Host "PASS: baseline request succeeded"
  } else {
    Write-Host ("FAIL: baseline request did not succeed: " + $first)
  }
  if ($last -match "status=ok") {
    Write-Host "PASS: request after restoration succeeded"
  } else {
    Write-Host ("FAIL: request after restoration did not succeed: " + $last)
  }
} else {
  Write-Host "FAIL: fewer than two completed rounds; chain incomplete"
}

# A failed attempt must still carry a full timing line.
$failed = $httpLines | Where-Object { $_ -match "rc=-" }
Write-Host ("failed attempts with timing: {0}" -f $failed.Count)
foreach ($m in $failed) { Write-Host ("  " + $m.Value) }
