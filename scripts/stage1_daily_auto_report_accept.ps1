#!/usr/bin/env pwsh
# Automatic daily-report acceptance over the ST-LINK VCP.
#
# This script deliberately keeps the serial stream open for the whole run.
# It writes every received byte to a capture under .debug, then hands the
# capture to the offline classifier for an ordered evidence decision.
#
#   powershell.exe -NoProfile -ExecutionPolicy Bypass `
#     -File scripts/stage1_daily_auto_report_accept.ps1

param(
  [string]$ComPort = "COM3",
  [int]$Baud = 115200,
  [int]$WaitSec = 900,
  [string]$Out = "",
  [switch]$KeepCapture,
  # Offline tests inject a fake transport and virtual clock.  Production runs
  # must leave this unset so this script can never open a supplied object.
  [Parameter(DontShow = $true)][object]$TestRuntime = $null
)

$ErrorActionPreference = "Stop"
class DailyAcceptRealClock {
  [datetime] Now() { return (Get-Date) }
  [void] Sleep([int]$milliseconds) { Start-Sleep -Milliseconds $milliseconds }
}

$testMode = $null -ne $TestRuntime
if ($testMode -and (
    -not $TestRuntime.ContainsKey('Transport') -or
    -not $TestRuntime.ContainsKey('Clock') -or
    $null -eq $TestRuntime.Transport -or
    $null -eq $TestRuntime.Clock)) {
  throw "TestRuntime must contain Transport and Clock objects"
}
$clock = if ($testMode) { $TestRuntime.Clock } else { [DailyAcceptRealClock]::new() }
$port = if ($testMode) { $TestRuntime.Transport } else {
  New-Object System.IO.Ports.SerialPort $ComPort, $Baud
}
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($Out)) {
  $stamp = $clock.Now().ToString("yyyyMMdd-HHmmss")
  $Out = Join-Path $repo ".debug/stage1-daily-auto-report-$stamp.log"
}
$Out = [System.IO.Path]::GetFullPath($Out)
$date = $clock.Now().ToString("yyyy-MM-dd")
$report = "/data/velaguard/reports/daily-$date.md"
$hold = "/data/velaguard/reports/daily-$date.hold.md"
$holdName = Split-Path -Leaf $hold
$promptPattern = "(?m)nsh>(?:\s|\x1b\[K)"
$log = New-Object System.IO.StreamWriter(
  $Out, $false, [System.Text.UTF8Encoding]::new($false)
)
$port.Encoding = [System.Text.Encoding]::UTF8
$port.ReadTimeout = 500
$port.NewLine = "`n"
$port.DtrEnable = $false

function Emit([string]$text) {
  if (-not $testMode) { Write-Host -NoNewline $text }
  $log.Write($text)
  $log.Flush()
  if ($testMode) { $log.BaseStream.Flush() }
}

function Drain {
  # Do not discard a stale nsh prompt before Send-Command records the new
  # capture offset.  If the board is already at a prompt, Send-Command can
  # return immediately after the command it just wrote.
  while ($port.IsOpen -and $port.BytesToRead -gt 0) {
    Emit $port.ReadExisting()
  }
}

function Send-Command(
  [string]$command,
  [int]$seconds = 8,
  [string]$observed = ""
) {
  # This marker is deliberately distinct from the board's own "nsh>" echo.
  # The function returns only the newly received text, so an old prompt or a
  # stale error line cannot satisfy a later command.
  Emit "`nDAILY_ACCEPT> $command`n"
  # Settle bytes that were already queued before the marker.  The following
  # offset is then the first byte belonging to this command's response.
  Drain
  $offset = (Get-Item -LiteralPath $Out).Length
  $port.Write("$command`r")
  $started = $clock.Now()
  while (($clock.Now() - $started).TotalSeconds -lt $seconds) {
    Drain
    $captured = Read-CaptureFrom $offset
    if ($captured.Contains($command) -and
        $captured -match $promptPattern -and
        ([string]::IsNullOrEmpty($observed) -or
         $captured -match $observed)) {
      return $captured
    }
    $clock.Sleep(80)
  }
  $captured = Read-CaptureFrom $offset
  throw "NSH command timed out without a prompt after ${seconds}s: $command`n$captured"
}

function Read-CaptureFrom([int64]$offset) {
  $current = (Get-Item -LiteralPath $Out).Length
  if ($current -le $offset) {
    return ""
  }

  $remaining = [int]($current - $offset)
  $buffer = New-Object byte[] $remaining
  $total = 0
  $stream = New-Object System.IO.FileStream(
    $Out,
    [System.IO.FileMode]::Open,
    [System.IO.FileAccess]::Read,
    [System.IO.FileShare]::ReadWrite
  )
  try {
    [void]$stream.Seek($offset, [System.IO.SeekOrigin]::Begin)
    while ($total -lt $remaining) {
      $read = $stream.Read($buffer, $total, $remaining - $total)
      if ($read -le 0) { break }
      $total += $read
    }
  }
  finally {
    $stream.Dispose()
  }
  return [System.Text.Encoding]::UTF8.GetString($buffer, 0, $total)
}

function Wait-ForPatternFrom(
  [string]$pattern,
  [int]$seconds,
  [int64]$offset
) {
  $started = $clock.Now()
  while (($clock.Now() - $started).TotalSeconds -lt $seconds) {
    Drain
    $clock.Sleep(100)
    if ((Get-Item -LiteralPath $Out).Length -gt $offset) {
      $tail = Read-CaptureFrom $offset
      if ([regex]::IsMatch($tail, $pattern, "Multiline")) {
        return $true
      }
    }
  }
  Drain
  return $false
}

function Invoke-Classifier {
  $classifier = Join-Path $PSScriptRoot "classify_daily_report_accept_log.py"
  $wslClassifier = (& wsl.exe -d Debian -- wslpath -a $classifier).Trim()
  $wslOut = (& wsl.exe -d Debian -- wslpath -a $Out).Trim()
  & wsl.exe -d Debian -- python3 $wslClassifier $wslOut --date $date
  return $LASTEXITCODE -eq 0
}

function Assert-New-Listing([string]$capture, [string]$path) {
  if ($capture -match "(?m)^nsh: (?:ls:|cp:|rm:|cmp:)" -and
      $capture -notmatch "(?m)^\s*-rw-.*\s$([regex]::Escape($path))\s*$") {
    throw "NSH command failed before listing the expected file: $path`n$capture"
  }
  if ($capture -notmatch "(?m)^\s*-rw-.*\s$([regex]::Escape($path))\s*$") {
    throw "NSH did not list the expected file: $path`n$capture"
  }
}

function Assert-New-Missing([string]$capture, [string]$path) {
  $escaped = [regex]::Escape($path)
  if ($capture -match "(?m)^\s*-rw-.*\s$escaped\s*$") {
    throw "NSH still listed a file that should be absent: $path`n$capture"
  }
  # A successful NSH command can leave a prompt line ending in ESC[K.  The
  # missing-file check is therefore based on the diagnostic, not on absence
  # of the prompt.
  if ($capture -notmatch "(?m)^nsh: ls: (?:stat|open) failed:") {
    throw "NSH did not report the expected missing file: $path`n$capture"
  }
}

function Assert-New-Cmp([string]$capture, [string]$left, [string]$right) {
  if ($capture -match "(?m)^files differ:" -or
      $capture -match "(?m)^nsh: cmp:" -or
      $capture -match "(?m)^nsh: (?:cp|rm|ls):") {
    throw "board-side cmp did not prove identical files: $left $right`n$capture"
  }
}

function Get-BoardReport([string]$capture) {
  if ($capture -match "(?m)^nsh: cat:") {
    throw "board-side cat failed:`n$capture"
  }
  $lines = @(($capture -replace "`r", "").Split("`n"))
  $start = [Array]::IndexOf($lines, "AI-DAILY v1")
  if ($start -lt 0) {
    throw "board-side cat did not print the report header:`n$capture"
  }
  $body = New-Object System.Collections.Generic.List[string]
  for ($index = $start; $index -lt $lines.Count; $index++) {
    $line = $lines[$index]
    if ($line -match "^(?:nsh|vela)>" -or
        $line -match "^DAILY_ACCEPT>") {
      break
    }
    $body.Add($line)
  }
  return ($body -join "`n").TrimEnd("`n")
}

function Get-ReportByteCount([string]$capture) {
  $matches = [regex]::Matches(
    $capture,
    "(?m)^\s*(\d+)\s+$([regex]::Escape($report))\s*$"
  )
  if ($matches.Count -eq 0) {
    throw "board-side wc did not print the report byte count:`n$capture"
  }
  if ($capture -match "(?m)^nsh: wc:") {
    throw "board-side wc failed:`n$capture"
  }
  return [int64]$matches[$matches.Count - 1].Groups[1].Value
}

function Restore-HeldReport {
  $copy = Send-Command "cp $hold $report" 8
  $compare = Send-Command "cmp $report $hold" 8
  Assert-New-Cmp ($copy + $compare) $report $hold
  $remove = Send-Command "rm $hold" 8
  $listing = Send-Command "ls -l $hold" 8
  Assert-New-Missing ($remove + $listing) $hold
}

function Invoke-DailyAcceptance {
  $script:restoreNeeded = $false
  $port.Open()
  $clock.Sleep(800)
  Drain

  # Refuse a hold left by an earlier run.  Otherwise a failed cp could be
  # mistaken for a fresh backup merely because the old file still exists.
  $stale = Send-Command "ls -l $hold" 8
  Assert-New-Missing $stale $hold

  # Hold today's file before deleting the live copy.  The board's cp is used,
  # so no host process reads or rewrites eMMC content.
  Assert-New-Listing (Send-Command "ls -l $report" 8) $report
  $copy = Send-Command "cp $report $hold" 8
  $listing1 = Send-Command "ls -l $hold" 8
  Assert-New-Listing ($copy + $listing1) $hold
  Write-Host "HOLD OK hold=$holdName"
  $log.WriteLine("HOLD OK hold=$holdName")
  $log.Flush()
  $compare = Send-Command "cmp $report $hold" 8
  Assert-New-Cmp $compare $report $hold
  # Only a backup already proven identical may be used for recovery.
  $script:restoreNeeded = $true

  # Keep this offset across rm and all later waits.  The scheduler may print
  # its request immediately after rm, before our ls check has drained the
  # serial stream.
  $eventOffset = (Get-Item -LiteralPath $Out).Length
  $remove = Send-Command "rm $report" 8
  $listing2 = Send-Command "ls -l $report" 8
  Assert-New-Missing ($remove + $listing2) $report
  Write-Host "REMOVED OK path=$report"
  $log.WriteLine("REMOVED OK path=$report")
  $log.Flush()

  # Avoid explicit agent commands from here until the request appears.  The
  # whole stream is retained, including interleaved network and slave lines.
  if (-not (Wait-ForPatternFrom "vghmi: daily report requested" $WaitSec $eventOffset)) {
    throw "board did not request a daily report within ${WaitSec}s"
  }

  # The round can take several minutes.  Wait for this day's write intent
  # before treating a trace end as the daily flow's result.
  $writePattern = "\[agent\] Tool call: write_file args=.*" +
    [regex]::Escape($report)
  if (-not (Wait-ForPatternFrom $writePattern $WaitSec $eventOffset)) {
    throw "agent did not request today's exact report path within ${WaitSec}s"
  }
  if (-not (Wait-ForPatternFrom "\[trace:[0-9a-f]{16}\] END status=" $WaitSec $eventOffset)) {
    throw "agent trace did not end within ${WaitSec}s"
  }

  $cat = Send-Command "cat $report" 10 "(?m)^AI-DAILY v1\s*$"
  $wc = Send-Command "wc -c $report" 8 "(?m)^\s*\d+\s+$([regex]::Escape($report))\s*$"
  $boardText = Get-BoardReport $cat
  if ($boardText -notmatch "(?m)^source=agent$" -or
      $boardText -notmatch "(?m)^date=$([regex]::Escape($date))$") {
    throw "generated daily report has the wrong source or date"
  }
  # wc counts the file terminator too.  The board report normally ends in one
  # newline; accept exactly zero or one for older files and compare the same
  # bytes that cat printed.
  $actualBytes = [System.Text.Encoding]::UTF8.GetByteCount($boardText)
  $wcBytes = Get-ReportByteCount $wc
  if ($wcBytes -ne $actualBytes -and $wcBytes -ne $actualBytes + 1) {
    throw "board-side wc and cat byte counts differ: wc=$wcBytes cat=$actualBytes (one trailing newline may be omitted by cat)"
  }
  Write-Host "VERIFY OK path=$report bytes=$actualBytes"
  $log.WriteLine("VERIFY OK path=$report bytes=$actualBytes")
  $log.Flush()

  # Restore from the board-side backup, compare bytes, then remove the hold.
  Restore-HeldReport
  Write-Host "RESTORE OK path=$report hold=$holdName cmp=identical"
  $log.WriteLine("RESTORE OK path=$report hold=$holdName cmp=identical")
  $log.Flush()
  Write-Host "CLEANUP OK hold=$holdName"
  $log.WriteLine("CLEANUP OK hold=$holdName")
  $log.Flush()
  $script:restoreNeeded = $false

  if (-not $testMode -and -not (Invoke-Classifier)) {
    throw "offline classifier rejected the capture: $Out"
  }
  Write-Output "DAILY_AUTO_REPORT PASS capture=$Out"
}

$script:restoreNeeded = $false
try {
  Invoke-DailyAcceptance
}
finally {
  # Never leave a failed destructive test without its original file.  This
  # runs after any exception as well, using the board-side hold only.
  if ($script:restoreNeeded -and $port.IsOpen) {
    try {
      Restore-HeldReport
      Write-Host "RESTORED original report and removed hold"
    } catch {
      Write-Warning "automatic restore failed; hold remains at $hold"
    }
  }
  if ($port.IsOpen) { $port.Close() }
  $log.Close()
  Write-Host "capture saved at $Out"
}
