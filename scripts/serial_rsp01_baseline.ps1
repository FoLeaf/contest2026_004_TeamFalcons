# RSP-01 分段计时板端基线采集。
#
# 走真实链路：NSH 启动 ai_agent 守护进程，进入 vela> 后发一条只读自然语言
# 请求，抓回整轮串口输出。脚本只负责发问与存盘；计时由固件打
# "http iter=..." 与 "tool_iter=..." 两行，脚本再从日志里把它们提取成表。
#
# 生成中文问题必须走本文件内的常量。PowerShell 5.1 读无 BOM 的 .ps1 会按
# ANSI 解析，命令行传中文也会在 WSL 到 Windows 的转换中损坏。本文件带
# UTF-8 BOM，串口也按 UTF-8 收发。
#
# 用法：
#   powershell.exe -ExecutionPolicy Bypass -File scripts/serial_rsp01_baseline.ps1 `
#     -Preset point -Repeat 3 -ReadSec 120 -OutDir .\docs\maintenance-evidence\2026-10-20
#
# 产物：
#   <OutDir>\rsp01-<preset>-<n>-<yyyyMMdd-HHmmss>.log   原始串口记录
#   <OutDir>\rsp01-<preset>-summary.txt                 分段提取结果
param(
  [string]$ComPort = "COM3",
  [ValidateSet("hello", "point", "report", "runreport", "alarm")]
  [string]$Preset = "point",
  [ValidateRange(1, 20)]
  [int]$Repeat = 1,
  [int]$ReadSec = 120,
  [string]$OutDir = ".\rsp01-evidence"
)
$ErrorActionPreference = "Stop"

$questions = @{
  hello     = "你好，请用一句话介绍你自己"
  point     = "告诉我UPS负载的值"
  report    = "按 operations_report Skill 生成今日运营日报"
  runreport = "给我截止目前的运行报告"
  alarm     = "按 alarm_interpretation Skill 解释当前告警"
}

$Question = $questions[$Preset]
if (-not $Question) { throw "未知 preset: $Preset" }

if (-not (Test-Path -LiteralPath $OutDir)) {
  New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"

foreach ($n in 1..$Repeat) {
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
    Drain 5 "" | Out-Null
    Send "ai_agent --daemon &" 60 "nsh>"
    Start-Sleep 5
    Send "ai_agent" 30 "vela>"

    # 同一轮里连发 Repeat 次会让上下文累积，ReAct 轮数无法比较，所以每轮
    # 重新启动守护进程不可行（冷启动成本会混入）。这里在一次会话里只发
    # 一问，Repeat 由外层循环重新建连实现。
    Write-Host "`n=== ask ($Preset) #$n ==="
    Send "ask $Question" $ReadSec ""

    $port.DiscardInBuffer()
    $port.Write("quit`r")
    Start-Sleep -Seconds 1
    Drain 3 "" | Out-Null
  }
  finally {
    # 异常时也关串口，否则下次循环打不开同一个 COM 口。
    if ($port.IsOpen) { $port.Close() }
  }

  $logPath = Join-Path $OutDir ("rsp01-" + $Preset + "-" + $n + "-" + $stamp + ".log")
  ($captured -join "") | Set-Content -LiteralPath $logPath -Encoding UTF8
  Write-Host "`n[saved] $logPath"
}

# ── 提取 ────────────────────────────────────────────────────────
#
# 每个 http 行按字段切分，输出一张表。缺失字段写 na，不补零：未测阶段
# 与零耗时不是一回事。

$rows = New-Object System.Collections.Generic.List[string]
$rows.Add("file`ttotal_ms`thttp_ms`tdns_tcp`ttls`tsetup`tsend`tttfb`tbody`tpool`tbuild`tparse`treused`tattempt`tstatus")

Get-ChildItem -LiteralPath $OutDir -Filter ("rsp01-" + $Preset + "-*.log") |
  Sort-Object Name |
  ForEach-Object {
    $text = Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8
    foreach ($m in [regex]::Matches($text, "http iter=(\d+) attempt=(\d+) reused=(\d+) proxy=(\d+) dns_tcp=(\S+) setup=(\S+) tls=(\S+) send=(\S+) ttfb=(\S+) body=(\S+) pool=(\S+) build=(\S+) parse=(\S+) http=(\S+) backoff=(\S+) total=(\S+) req=(\d+) rc=(-?\d+) status=(\d+) parse_ok=(\d+)")) {
      $rows.Add(($_.Name, $m.Groups[17].Value, $m.Groups[14].Value,
        $m.Groups[5].Value, $m.Groups[7].Value, $m.Groups[6].Value,
        $m.Groups[8].Value, $m.Groups[9].Value, $m.Groups[10].Value,
        $m.Groups[11].Value, $m.Groups[12].Value, $m.Groups[13].Value,
        $m.Groups[3].Value, $m.Groups[2].Value, $m.Groups[19].Value) -join "`t")
    }
  }

$summaryPath = Join-Path $OutDir ("rsp01-" + $Preset + "-summary.txt")
$rows | Set-Content -LiteralPath $summaryPath -Encoding UTF8
Write-Host "`n[summary] $summaryPath"
$rows | ForEach-Object { Write-Host $_ }
