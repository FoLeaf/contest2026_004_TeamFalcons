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
  [switch]$KeepCapture
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($Out)) {
  $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
  $Out = Join-Path $repo ".debug/stage1-daily-auto-report-$stamp.log"
}
$Out = [System.IO.Path]::GetFullPath($Out)
$date = (Get-Date).ToString("yyyy-MM-dd")
$report = "/data/velaguard/reports/daily-$date.md"
$hold = "/data/velaguard/reports/daily-$date" + "_acceptance.hold.md"
$holdName = Split-Path -Leaf $hold
$log = New-Object System.IO.StreamWriter(
  $Out, $false, [System.Text.UTF8Encoding]::new($false)
)
$port = New-Object System.IO.Ports.SerialPort $ComPort, $Baud
$port.Encoding = [System.Text.Encoding]::UTF8
$port.ReadTimeout = 500
$port.NewLine = "`n"
$port.DtrEnable = $true

function Emit([string]$text) {
  Write-Host -NoNewline $text
  $log.Write($text)
  $log.Flush()
}

function Drain {
  while ($port.IsOpen -and $port.BytesToRead -gt 0) {
    Emit $port.ReadExisting()
  }
}

function Send-Command([string]$command, [int]$seconds = 8) {
  Emit "`nNSH> $command`n"
  $port.Write("$command`r")
  $deadline = (Get-Date).AddSeconds($seconds)
  while ((Get-Date) -lt $deadline) {
    Drain
    Start-Sleep -Milliseconds 80
  }
  Drain
}

function Wait-ForLine([string]$pattern, [int]$seconds) {
  $deadline = (Get-Date).AddSeconds($seconds)
  while ((Get-Date) -lt $deadline) {
    Drain
    Start-Sleep -Milliseconds 100
  }
  Drain
  $captured = [System.IO.File]::ReadAllText($Out, [System.Text.Encoding]::UTF8)
  return [regex]::IsMatch($captured, $pattern, "Multiline")
}

function Wait-ForAppendedPattern([string]$pattern, [int]$seconds) {
  $offset = (Get-Item -LiteralPath $Out).Length
  $deadline = (Get-Date).AddSeconds($seconds)
  while ((Get-Date) -lt $deadline) {
    Drain
    Start-Sleep -Milliseconds 100
    if ((Get-Item -LiteralPath $Out).Length -gt $offset) {
      $stream = [System.IO.File]::OpenRead($Out)
      try {
        [void]$stream.Seek($offset, [System.IO.SeekOrigin]::Begin)
        $reader = New-Object System.IO.StreamReader(
          $stream, [System.Text.Encoding]::UTF8, $true, 4096, $true
        )
        $tail = $reader.ReadToEnd()
      }
      finally {
        if ($null -ne $reader) { $reader.Dispose() }
        $stream.Dispose()
      }
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

$restoreNeeded = $false
try {
  $port.Open()
  Start-Sleep -Milliseconds 800
  Drain

  # Hold today's file before deleting the live copy.  The board's cp is used,
  # so no host process reads or rewrites eMMC content.
  Send-Command "ls -l $report" 5
  $listing0 = [System.IO.File]::ReadAllText(
    $Out, [System.Text.Encoding]::UTF8
  )
  if (-not [regex]::IsMatch($listing0, " -rw-.*$/data/velaguard/reports/daily-$date\.md")) {
    throw "today's report is not visible before hold: $report"
  }
  Send-Command "cp $report $hold" 5
  Send-Command "ls -l $hold" 5
  $listing1 = [System.IO.File]::ReadAllText(
    $Out, [System.Text.Encoding]::UTF8
  )
  if (-not [regex]::IsMatch($listing1, " -rw-.*$hold")) {
    throw "backup was not created: $hold"
  }
  Write-Host "HOLD OK hold=$holdName"
  $log.WriteLine("HOLD OK hold=$holdName")
  $log.Flush()
  $restoreNeeded = $true

  Send-Command "rm $report" 5
  Send-Command "ls -l $report" 5
  $listing2 = [System.IO.File]::ReadAllText(
    $Out, [System.Text.Encoding]::UTF8
  )
  if ($listing2 -match "/data/velaguard/reports/daily-$date\.md") {
    throw "live report still exists after rm: $report"
  }
  Write-Host "REMOVED OK path=$report"
  $log.WriteLine("REMOVED OK path=$report")
  $log.Flush()

  # Avoid explicit agent commands from here until the request appears.  The
  # whole stream is retained, including interleaved network and slave lines.
  if (-not (Wait-ForAppendedPattern "^vghmi: daily report requested$" $WaitSec)) {
    throw "board did not request a daily report within ${WaitSec}s"
  }

  # The round can take several minutes.  Wait for this day's write intent
  # before treating a trace end as the daily flow's result.
  $writePattern = "^\[agent\] Tool call: write_file args=.*" +
    [regex]::Escape($report)
  if (-not (Wait-ForAppendedPattern $writePattern $WaitSec)) {
    throw "agent did not request today's exact report path within ${WaitSec}s"
  }
  if (-not (Wait-ForAppendedPattern "^\[trace:[0-9a-f]{16}\] END status=" $WaitSec)) {
    throw "agent trace did not end within ${WaitSec}s"
  }

  Send-Command "cat $report" 10
  Send-Command "wc -c $report" 5
  $whole = [System.IO.File]::ReadAllText(
    $Out, [System.Text.Encoding]::UTF8
  )
  if ($whole -notmatch "AI-DAILY v1" -or
      $whole -notmatch "source=agent" -or
      $whole -notmatch "date=$date") {
    throw "generated daily report is not visible on the board"
  }
  Write-Host "VERIFY OK path=$report"
  $log.WriteLine("VERIFY OK path=$report")
  $log.Flush()

  # Restore from the board-side backup, compare bytes, then remove the hold.
  Send-Command "cp $hold $report" 5
  Send-Command "cmp $report $hold" 5
  $restoreText = [System.IO.File]::ReadAllText(
    $Out, [System.Text.Encoding]::UTF8
  )
  if ($restoreText -match "cmp: .*differ|nsh: cmp: .*failed") {
    throw "board-side cmp reported a restore mismatch"
  }
  Write-Host "RESTORE OK path=$report hold=$holdName cmp=identical"
  $log.WriteLine("RESTORE OK path=$report hold=$holdName cmp=identical")
  $log.Flush()
  Send-Command "rm $hold" 5
  Write-Host "CLEANUP OK hold=$holdName"
  $log.WriteLine("CLEANUP OK hold=$holdName")
  $log.Flush()

  if (-not (Invoke-Classifier)) {
    throw "offline classifier rejected the capture: $Out"
  }
  Write-Output "DAILY_AUTO_REPORT PASS capture=$Out"
}
finally {
  # Never leave a failed destructive test without its original file.  This
  # runs after any exception as well, using the board-side hold only.
  if ($restoreNeeded -and $port.IsOpen) {
    try {
      Send-Command "cp $hold $report" 5
      Send-Command "cmp $report $hold" 5
      Send-Command "rm $hold" 5
      Write-Host "RESTORED original report and removed hold"
    } catch {
      Write-Warning "automatic restore failed; hold remains at $hold"
    }
  }
  if ($port.IsOpen) { $port.Close() }
  $log.Close()
  Write-Host "capture saved at $Out"
}
