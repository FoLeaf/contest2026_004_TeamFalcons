# VelaGuard 24 小时串口长测（scope=storage-runtime）。
#
# 判定与追溯规则见 docs/maintenance-soak-runbook.md。要点：
#   * 只有同一 run=<id> 的 PASS 行与紧随其后的 RUN_END status=PASS 同时存在，
#     该轮长测才算通过。
#   * 缺少 RUN_END 的运行（被强制结束、掉电、终端被关）一律视为结果不完整，
#     不继承同一日志文件里早先出现的 PASS。
#   * PASS 只覆盖脚本实际执行的检查，见 CHECKS 行。
[CmdletBinding()]
param(
  [string]$ComPort = 'COM3',
  [ValidateRange(0.01,168)][double]$Hours = 24,
  [ValidateRange(1,10000)][int]$StoreRounds = 50,
  [ValidateRange(1,3600)][int]$IntervalSec = 60,
  [ValidateRange(0,32)][int]$MinimumOnline = 0,
  [ValidateRange(10,3600)][int]$CommandTimeoutSec = 120,
  [string]$LogPath = '',
  [string]$RunId = '',
  # 离线测试专用：注入假串口与虚拟时钟，永不打开真实端口。
  # 见 scripts/tests/soak/。正式长测不要传这个参数。
  [Parameter(DontShow = $true)][object]$TestRuntime = $null
)

class SoakRealClock {
  [System.Diagnostics.Stopwatch]$sw = [System.Diagnostics.Stopwatch]::StartNew()
  [datetime] Now() { return (Get-Date) }
  [double] Elapsed() { return $this.sw.Elapsed.TotalSeconds }
  [void] Sleep([int]$ms) { Start-Sleep -Milliseconds $ms }
}

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.Encoding]::UTF8

$ScriptVersion = 'soak-3'
# 提示符。板端输出是 "nsh> " 加 ESC[K，ANSI 已在 Get-PlainText 中剥离。
$PromptPattern = 'nsh>(?:\s|$)'
# 固件崩溃特征。取自 docs/maintenance-evidence 里的 HardFault / panic 转储。
$FatalPattern = 'HARDFAULT|PANIC|dump_assert_info|up_dump_register|Assertion failed|outside SRAM|Queue full'
$FaultWindowChars = 160
# 启动横幅。这些字符串只在板端重启后的启动阶段出现。
$RebootPattern = 'NuttShell \(NSH\)|eMMC mounted at|Symlink /data ->'
$Checks = 'store-probe,committed-table-stable,credentials-provisioned,uptime-monotonic,online-min,heap-consistency'
$NotChecked = 'alarm,mqtt,llm-call,daily-report,voice,screen'

$testMode = [bool]$TestRuntime
$script:Clock = $null
$port = $null
if ($testMode) {
  $port = $TestRuntime.Transport
  $script:Clock = $TestRuntime.Clock
}
else {
  $script:Clock = [SoakRealClock]::new()
}
$clock = $script:Clock

if (!$LogPath) { $LogPath = Join-Path $PSScriptRoot '../.debug/maintenance-soak.log' }
$LogPath = [IO.Path]::GetFullPath($LogPath)
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($LogPath)) | Out-Null
if (!$RunId) {
  $RunId = '{0}-{1}' -f (Get-Date -Format 'yyyyMMddTHHmmss'), [guid]::NewGuid().ToString('N').Substring(0,6)
}

$script:Writer = $null
$script:Port = $port
$script:LastReply = ''
$script:FaultTail = ''
$script:Verdict = $null
$script:ExitCode = 0

function Record([string]$Text) {
  if ($script:Writer) { $script:Writer.WriteLine($Text) }
  Write-Host $Text
}

function Get-InnerMessage($ErrorRecord) {
  # .NET 串口异常经 PowerShell 包装成 MethodInvocationException，取最内层才是原因。
  $ex = $ErrorRecord.Exception
  while ($ex.InnerException) { $ex = $ex.InnerException }
  return $ex.Message
}

function Get-PlainText([string]$Text) {
  # 先剥 OSC（ESC ] ... BEL/ST），再剥 CSI（ESC [ 参数 中间 终止），最后剥两字符转义。
  $t = [regex]::Replace($Text, "\x1b\][^\x07\x1b]*(?:\x07|\x1b\\)", '')
  $t = [regex]::Replace($t, "\x1b\[[0-9;?<>=]*[ -/]*[@-~]", '')
  $t = [regex]::Replace($t, "\x1b[@-Z\\-_]", '')
  return $t
}

function Get-PlainLines([string]$Text) {
  return @($Text -split '[\r\n]+' | ForEach-Object { $_.Trim() } | Where-Object { $_ })
}

function Assert-NoFirmwareFault([string]$Text, [string]$Context) {
  # Serial reads split anywhere. Keep a bounded plain-text tail so a fault word
  # or boot banner spanning two chunks is still detected in every read path.
  $plain = Get-PlainText $Text
  $window = $script:FaultTail + $plain
  if ($window -match $FatalPattern) {
    Record $Text
    throw "Firmware failure detected in $Context; soak stopped without reset."
  }
  if ($window -match $RebootPattern) {
    Record $Text
    throw "Board boot banner seen in $Context; the board restarted during the soak."
  }
  if ($window.Length -gt $FaultWindowChars) {
    $script:FaultTail = $window.Substring($window.Length - $FaultWindowChars)
  }
  else {
    $script:FaultTail = $window
  }
}

function Read-TransportText([string]$Context) {
  if ($testMode) { $script:Port.ReadContext = $Context }
  try { return $script:Port.ReadExisting() }
  catch [System.Management.Automation.PipelineStoppedException] { throw }
  catch { throw "Serial link lost during ${Context}: $(Get-InnerMessage $_)" }
}

function Write-TransportCommand([string]$Command) {
  try { $script:Port.Write("$Command`r") }
  catch [System.Management.Automation.PipelineStoppedException] { throw }
  catch { throw "Serial link lost while sending '$Command': $(Get-InnerMessage $_)" }
}

function Clear-TransportBuffer([string]$Context) {
  # 排空前一次交互留下的残留提示符和异步日志，避免它们被当成下一条命令的完成信号。
  for ($i = 0; $i -lt 200; $i++) {
    $chunk = Read-TransportText $Context
    if (!$chunk) { return }
    Assert-NoFirmwareFault $chunk $Context
    Record $chunk
  }
}

function Invoke-Nsh {
  param(
    [string]$Command,
    [string]$Expected,
    [switch]$PromptOnly
  )
  Record ("COMMAND {0:o} run={1} {2}" -f $script:Clock.Now(), $script:RunId, $Command)
  Clear-TransportBuffer "before '$Command'"
  Write-TransportCommand $Command
  $text = ''
  $matched = $false
  $started = $script:Clock.Elapsed()
  while (($script:Clock.Elapsed() - $started) -lt $CommandTimeoutSec) {
    $chunk = Read-TransportText "command '$Command'"
    if ($chunk) {
      $text += $chunk
      Assert-NoFirmwareFault $chunk "command '$Command'"
    }
    $plain = Get-PlainText $text
    # 完成条件是"期望内容出现之后又回到了提示符"，不是"整段文本以提示符结尾"：
    # 板端会在提示符之后继续打异步日志，按结尾判断会把成功误判成超时。
    if ($PromptOnly) { $matched = $plain -match $PromptPattern }
    else { $matched = $plain -match "(?s)(?:$Expected).*?$PromptPattern" }
    if ($matched) { break }
    $script:Clock.Sleep(100)
  }
  $elapsed = $script:Clock.Elapsed() - $started
  Record $text
  if (!$matched) {
    throw "Command timeout after ${CommandTimeoutSec}s: $Command"
  }
  Record ("COMMAND_MS {0}" -f [math]::Round($elapsed * 1000))
  $script:LastReply = Get-PlainText $text
}

function Get-CommittedTable {
  for ($attempt = 1; $attempt -le 3; $attempt++) {
    Invoke-Nsh 'vgpoint list' 'vgpoint: OK cmd=list'
    $plain = $script:LastReply
    $m = [regex]::Match($plain, 'vgpoint: OK cmd=list table=committed n=(\d+)')
    if (!$m.Success) { continue }
    $n = [int]$m.Groups[1].Value
    # 只接受字段完整的行；被后台日志截断的行不算数，数量对不上就重读。
    $rows = @(Get-PlainLines $plain | Where-Object { $_ -match '^vgpoint: POINT id=\S+ .*fail_n=\d+$' })
    if ($rows.Count -eq $n) {
      return [pscustomobject]@{ Count = $n; Rows = $rows }
    }
  }
  throw 'Incomplete committed-table console output.'
}

function Get-RuntimeSnapshot([int]$ExpectedPoints) {
  for ($attempt = 1; $attempt -le 3; $attempt++) {
    Invoke-Nsh 'vgruntime dump' 'vgruntime: uptime_s='
    $plain = $script:LastReply
    $m = [regex]::Match($plain, 'vgruntime: uptime_s=(\d+)')
    if (!$m.Success) { continue }
    $points = @(Get-PlainLines $plain | Where-Object { $_ -match '^vgruntime: point id=\S+ .*\skind=\S+$' })
    # 采样行数量必须与已确认点表一致，否则是控制台输出被截断，不是真的掉线。
    if ($points.Count -ne $ExpectedPoints) { continue }
    $online = @($points | Where-Object { $_ -match '(?:^|\s)online=1(?:\s|$)' }).Count
    return [pscustomobject]@{
      UptimeSeconds = [long]$m.Groups[1].Value
      Points        = $points.Count
      Online        = $online
    }
  }
  throw 'Incomplete runtime dump console output.'
}

function Get-HeapStats {
  for ($attempt = 1; $attempt -le 3; $attempt++) {
    Invoke-Nsh 'cat /proc/meminfo' 'Umem'
    $rows = [System.Collections.ArrayList]::new()
    foreach ($line in (Get-PlainLines $script:LastReply)) {
      $m = [regex]::Match($line, '^(\d+)[ \t]+(\d+)[ \t]+(\d+)[ \t]+(\d+)[ \t]+(\d+)[ \t]+\d+[ \t]+\d+[ \t]+Umem$')
      if ($m.Success) { [void]$rows.Add($m) }
    }
    if ($rows.Count -lt 1) { continue }
    foreach ($m in $rows) {
      [long]$total = $m.Groups[1].Value; [long]$used = $m.Groups[2].Value
      [long]$free = $m.Groups[3].Value; [long]$largest = $m.Groups[5].Value
      if ($total -le 0 -or ($used + $free) -ne $total -or $largest -gt $free) {
        throw ("Heap accounting is inconsistent (total={0} used={1} free={2} largest={3}); stopped without reset." -f $total, $used, $free, $largest)
      }
    }
    $first = $rows[0]
    return [pscustomobject]@{
      Total   = [long]$first.Groups[1].Value
      Used    = [long]$first.Groups[2].Value
      Free    = [long]$first.Groups[3].Value
      MaxUsed = [long]$first.Groups[4].Value
    }
  }
  throw 'Incomplete heap console output.'
}

function Wait-Interval([int]$Seconds) {
  # 等待期间继续读串口：崩溃文本会在下一次命令之前被看见，也不会因驱动缓冲溢出而丢失。
  $deadline = $script:Clock.Elapsed() + $Seconds
  while ($script:Clock.Elapsed() -lt $deadline) {
    $chunk = Read-TransportText 'interval wait'
    if ($chunk) {
      Assert-NoFirmwareFault $chunk 'interval wait'
      Record $chunk
    }
    $script:Clock.Sleep(1000)
  }
}

try {
  $script:Writer = New-Object IO.StreamWriter($LogPath, $true, [Text.Encoding]::UTF8)
  $script:Writer.AutoFlush = $true

  Record ("RUN_BEGIN run={0} script_version={1} time={2:o} host={3} user={4} log={5}" -f `
      $RunId, $ScriptVersion, $clock.Now(), $env:COMPUTERNAME, $env:USERNAME, $LogPath)
  Record ("START run={0} time={1:o} port={2} hours={3} interval_s={4} store_rounds={5} online_min={6} cmd_timeout_s={7} scope=storage-runtime" -f `
      $RunId, $clock.Now(), $ComPort, $Hours, $IntervalSec, $StoreRounds, $MinimumOnline, $CommandTimeoutSec)
  Record ("CHECKS run={0} checks={1} not_checked={2}" -f $RunId, $Checks, $NotChecked)
  if ($testMode) {
    Record ("TEST_MODE run={0} fake transport and virtual clock; this log is not board evidence." -f $RunId)
  }
  if ($MinimumOnline -lt 1) {
    Record ("WARN run={0} online_min=0 accepts an all-offline bus; pass -MinimumOnline for the official soak." -f $RunId)
  }

  if (!$testMode) {
    if ([IO.Ports.SerialPort]::GetPortNames() -notcontains $ComPort) {
      throw "Board port $ComPort is unavailable. No reset or recovery was attempted."
    }
    $port = New-Object IO.Ports.SerialPort $ComPort, 115200
    $port.Encoding = [Text.Encoding]::UTF8
    $script:Port = $port
  }
  $port.Open()

  Invoke-Nsh '' '' -PromptOnly
  Invoke-Nsh 'uname -a' 'NuttX'
  $fw = @(Get-PlainLines $script:LastReply | Where-Object { $_ -match 'NuttX' } | Select-Object -First 1)
  if ($fw.Count -lt 1) { throw 'Firmware identity (uname -a) is missing from the console output.' }
  Record ("FIRMWARE run={0} {1}" -f $RunId, $fw[0])

  $table = Get-CommittedTable
  Record ("BASELINE run={0} table=committed n={1}" -f $RunId, $table.Count)
  foreach ($row in $table.Rows) { Record ("BASELINE_ROW run={0} {1}" -f $RunId, $row) }
  if ($table.Count -lt 1) {
    Record ("WARN run={0} the committed table is empty; storage-runtime checks observe nothing." -f $RunId)
  }
  $baseline = $table.Rows -join "`n"
  $pointCount = $table.Count

  $cycles = 0
  [long]$lastUptime = -1
  $startedAt = $clock.Elapsed()
  $startedWall = $clock.Now()
  $requiredSeconds = $Hours * 3600.0

  while ($true) {
    Invoke-Nsh "vgstoretest $StoreRounds" 'vgstoretest: OK'

    $after = Get-CommittedTable
    if (($after.Rows -join "`n") -cne $baseline) {
      # 单次控制台读取可能被后台日志截断；确认读再比对一次才判定点表变化。
      $confirm = Get-CommittedTable
      if (($confirm.Rows -join "`n") -cne $baseline) {
        Record ("TABLE_AFTER run={0} table=committed n={1}" -f $RunId, $confirm.Count)
        foreach ($row in $confirm.Rows) { Record ("TABLE_AFTER_ROW run={0} {1}" -f $RunId, $row) }
        throw 'Committed table changed during soak.'
      }
      Record ("WARN run={0} one committed-table read did not match the baseline; the confirmation read matched." -f $RunId)
      $after = $confirm
    }

    Invoke-Nsh 'vgagent status' 'llm: credentials='
    $creds = [regex]::Match($script:LastReply, 'llm: credentials=(\S+)')
    if (!$creds.Success) { throw 'LLM credential state is missing from vgagent status.' }
    if ($creds.Groups[1].Value -ne 'ready') {
      throw ("LLM credentials are not provisioned (credentials={0}); soak stopped without reset." -f $creds.Groups[1].Value)
    }

    $snap = Get-RuntimeSnapshot $pointCount
    if ($snap.UptimeSeconds -lt $lastUptime) {
      throw ("Board uptime decreased ({0} -> {1}); the board restarted during the soak." -f $lastUptime, $snap.UptimeSeconds)
    }
    $lastUptime = $snap.UptimeSeconds
    if ($snap.Online -lt $MinimumOnline) {
      throw ("Only {0} of {1} points online; need {2}." -f $snap.Online, $snap.Points, $MinimumOnline)
    }

    $heap = Get-HeapStats

    $cycles++
    Record ("CYCLE run={0} n={1} elapsed_s={2:N0} uptime_s={3} online={4}/{5} heap_used={6} heap_maxused={7}" -f `
        $RunId, $cycles, [math]::Round($clock.Elapsed() - $startedAt), $snap.UptimeSeconds, $snap.Online, $snap.Points, $heap.Used, $heap.MaxUsed)

    if (($clock.Elapsed() - $startedAt) -ge $requiredSeconds) { break }
    Wait-Interval $IntervalSec
  }

  # 到这里最后一轮的全部检查已经执行完，运行时长也达到要求。
  $elapsedSeconds = $clock.Elapsed() - $startedAt
  $wallSeconds = ($clock.Now() - $startedWall).TotalSeconds
  Record ("PASS run={0} status=PASS time={1:o} scope=storage-runtime checks={2} not_checked={3} cycles={4} elapsed_s={5:N0} required_s={6:N0} wall_elapsed_s={7:N0} online_min={8} store_rounds={9} interval_s={10} cmd_timeout_s={11} log={12}" -f `
      $RunId, $clock.Now(), $Checks, $NotChecked, $cycles, [math]::Round($elapsedSeconds), [math]::Round($requiredSeconds), [math]::Round($wallSeconds), $MinimumOnline, $StoreRounds, $IntervalSec, $CommandTimeoutSec, $LogPath)
  $script:Verdict = 'PASS'
}
catch {
  # 用户中断（Ctrl+C）在 PowerShell 里是 PipelineStoppedException：catch 不会执行，
  # finally 会执行，所以中断由 finally 里的"还没写结论"分支记录。
  $script:Verdict = 'FAIL'
  $script:ExitCode = 1
  Record ("FAIL run={0} status=FAIL time={1:o} reason={2}" -f $RunId, $clock.Now(), $_.Exception.Message)
}
finally {
  if (!$script:Verdict) {
    $script:Verdict = 'INCOMPLETE'
    $script:ExitCode = 2
    Record ("ABORT run={0} status=INCOMPLETE time={1:o} reason=the run stopped before a verdict was written" -f $RunId, $clock.Now())
  }
  Record ("RUN_END run={0} status={1} time={2:o}" -f $RunId, $script:Verdict, $clock.Now())
  try { if ($port -and $port.IsOpen) { $port.Close() } } catch { }
  try { if ($port) { $port.Dispose() } } catch { }
  try { if ($script:Writer) { $script:Writer.Flush(); $script:Writer.Dispose() } } catch { }
  $script:Writer = $null
  # 退出码放在 finally 里：中断会停掉管道，写在 try 之后的 exit 不会被执行。
  exit $script:ExitCode
}
