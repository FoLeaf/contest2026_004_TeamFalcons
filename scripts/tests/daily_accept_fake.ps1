# Fake NSH transport for stage1_daily_auto_report_accept.ps1.
#
# The test runs the production script with -TestRuntime.  No COM port is
# opened and no network service is contacted.  Each command returns only the
# response that a board would print for that command.

class DailyAcceptVirtualClock {
  [datetime]$Current = [datetime]'2026-10-19T14:00:00+08:00'
  [double]$ElapsedSeconds = 0

  [datetime] Now() { return $this.Current }
  [void] Advance([double]$seconds) {
    $this.ElapsedSeconds += $seconds
    $this.Current = $this.Current.AddSeconds($seconds)
  }
  [void] Sleep([int]$milliseconds) { $this.Advance($milliseconds / 1000.0) }
}

class DailyAcceptFakeBoard {
  [bool]$HasReport = $true
  [bool]$HasHold = $false
  [bool]$StaleHold = $false
  [bool]$CpFails = $false
  [bool]$CmpFails = $false
  [bool]$RmFails = $false
  [bool]$EmitDailyRound = $true
  [bool]$ReportBodyPresent = $true
  [bool]$WcBytesWrong = $false
  [bool]$RestoreCpFails = $false
  [bool]$DeferAfterRm = $false
  [string]$DeferredRound = ''
  [bool]$RoundReportVisible = $false
  [int]$Commands = 0
  [int]$WriteCommands = 0

  [string] Prompt() { return "nsh> $([char]27)[K" }

  [string[]] Respond([string]$command) {
    $this.Commands++
    $lines = @($command)
    $diagnostic = $null
    switch -regex ($command) {
      '^ls -l /data/velaguard/reports/daily-2026-10-19\.hold\.md$' {
        if ($this.HasHold) {
          $lines += ' -rw-rw-rw-   0         546 /data/velaguard/reports/daily-2026-10-19.hold.md'
        } else {
          $lines += 'nsh: ls: stat failed: 2'
        }
      }
      '^ls -l /data/velaguard/reports/daily-2026-10-19\.md$' {
        if ($this.HasReport) {
          $lines += ' -rw-rw-rw-   0         546 /data/velaguard/reports/daily-2026-10-19.md'
        } else {
          $lines += 'nsh: ls: stat failed: 2'
        }
      }
      '^cp /data/velaguard/reports/daily-2026-10-19\.md /data/velaguard/reports/daily-2026-10-19\.hold\.md$' {
        if (!$this.CpFails -and $this.HasReport) { $this.HasHold = $true }
        else { $lines += 'nsh: cp: open_wrfd failed: 28'; $diagnostic = 'cp' }
      }
      '^cmp /data/velaguard/reports/daily-2026-10-19\.md /data/velaguard/reports/daily-2026-10-19\.hold\.md$' {
        if ($this.CmpFails) {
          $lines += 'files differ: byte 12'; $diagnostic = 'cmp'
        }
      }
      '^rm /data/velaguard/reports/daily-2026-10-19\.md$' {
        if (!$this.RmFails -and $this.HasReport) { $this.HasReport = $false }
        else { $lines += 'nsh: rm: unlink failed: 2'; $diagnostic = 'rm' }
      }
      '^cp /data/velaguard/reports/daily-2026-10-19\.hold\.md /data/velaguard/reports/daily-2026-10-19\.md$' {
        if ($this.RestoreCpFails) {
          $lines += 'nsh: cp: open_wrfd failed: 28'; $diagnostic = 'cp'
        } elseif ($this.HasHold) { $this.HasReport = $true }
        else { $lines += 'nsh: cp: open_rdfd failed: 2'; $diagnostic = 'cp' }
      }
      '^rm /data/velaguard/reports/daily-2026-10-19\.hold\.md$' {
        if (!$this.RmFails -and $this.HasHold -and $this.HasReport) {
          $this.HasHold = $false
        }
        else { $lines += 'nsh: rm: unlink failed: 2'; $diagnostic = 'rm' }
      }
      '^cat /data/velaguard/reports/daily-2026-10-19\.md$' {
        if ($this.RoundReportVisible -and $this.ReportBodyPresent) {
          $lines += 'AI-DAILY v1',
            'date=2026-10-19',
            'source=agent',
            '---',
            '通信质量',
            '总线成功 100%。',
            '点位在线',
            '全部点位在线。',
            '异常时间线',
            '暂无异常。'
        } else {
          $lines += 'nsh: cat: open failed: 2'; $diagnostic = 'cat'
        }
      }
      '^wc -c /data/velaguard/reports/daily-2026-10-19\.md$' {
        if ($this.RoundReportVisible -and $this.ReportBodyPresent) {
          if ($this.WcBytesWrong) {
            $lines += '205 /data/velaguard/reports/daily-2026-10-19.md'
          } else {
            $lines += '145 /data/velaguard/reports/daily-2026-10-19.md'
          }
        } else {
          $lines += 'nsh: wc: open failed: 2'; $diagnostic = 'wc'
        }
      }
      default {}
    }
    $lines += $diagnostic
    $lines += $this.Prompt()
    return @($lines | Where-Object { $null -ne $_ })
  }
}

class DailyAcceptFakePort {
  [System.Collections.Generic.Queue[string]]$Pending = [System.Collections.Generic.Queue[string]]::new()
  [int]$BytesToRead = 0
  [bool]$IsOpen = $false
  [object]$Board = $null
  [object]$Clock = $null
  [string]$Encoding = ''
  [int]$ReadTimeout = 0
  [string]$NewLine = ''
  [bool]$DtrEnable = $false
  [int]$EmptyReads = 0
  [int]$PostRmEmptyReads = 0
  [bool]$RmCompleted = $false

  [void] Open() { $this.IsOpen = $true }
  [void] Close() { $this.IsOpen = $false }
  [void] Dispose() { $this.IsOpen = $false }

  [void] Write([string]$text) {
    if (!$this.IsOpen) { throw 'fake serial port is closed' }
    $command = $text.TrimEnd([char]13, [char]10)
    $this.Board.WriteCommands++
    $response = $this.Board.Respond($command)
    if ($command -eq 'cp /data/velaguard/reports/daily-2026-10-19.md /data/velaguard/reports/daily-2026-10-19.hold.md') {
      # The first fake run supplies the scheduler's requested report after rm.
    }
    if ($command -eq 'rm /data/velaguard/reports/daily-2026-10-19.md' -and
        $this.Board.EmitDailyRound) {
      $round = @(
        'vghmi: daily report requested',
        '[vgagent] round submitted: 124 bytes',
        '[trace:0123456789abcdef] BEGIN chat=velaguard:13 chan=local_client',
        '[agent] Tool call: write_file args={"path":"/data/velaguard/reports/daily-2026-10-19.md","content":"AI-DAILY v1\n"}',
        '[tools] Executing tool: write_file',
        '[tool_files] write_file: /data/velaguard/reports/daily-2026-10-19.md (546 bytes)',
        '[trace:0123456789abcdef] END status=ok iters=4 tools=3 llm_ms=2000 elapsed=3s backend=0'
      )
      if ($this.Board.DeferAfterRm) {
        # Stage the board request after rm has returned to a prompt.  This
        # reproduces the race where the scheduler emits its own prompt and
        # request between two host commands.
        $this.Board.DeferredRound = ($round -join "`r`n") + "`r`n"
      } else {
        $response = @($response[0]) + $round + @($response[-1])
        $this.Board.RoundReportVisible = $true
      }
      $this.RmCompleted = $true
    } elseif ($command -eq 'ls -l /data/velaguard/reports/daily-2026-10-19.md' -and
              $this.Board.DeferAfterRm -and
              $this.Board.DeferredRound) {
      # A real board can print asynchronous scheduler output even when the
      # user-issued command is still waiting for bytes.  Deliver it as part of
      # the ls response, immediately after the intentional missing-file
      # diagnostic.  The production code scans from before ls so it must not
      # lose this request.
      $missingLine = $response | Where-Object { $_ -match '^nsh: ls:' } |
        Select-Object -First 1
      $prompt = $response[-1]
      $roundLines = @($this.Board.DeferredRound -split "\r?\n" |
        Where-Object { $_ })
      $response = @($response[0]) + @($missingLine) +
        $roundLines +
        @($prompt)
      $this.Board.DeferredRound = ''
      $this.Board.RoundReportVisible = $true
    }
    $this.Pending.Enqueue(($response -join "`r`n") + "`r`n")
    $this.BytesToRead = $this.Pending.Count
    # The production loop may consult BytesToRead immediately after Write and
    # then Sleep.  Advance the virtual clock here so no test depends on real
    # wall time and its own timeout path always progresses.
    $this.Clock.Advance(1)
  }

  [string] ReadExisting() {
    if ($this.Pending.Count -eq 0) {
      # The scheduler emits its request after the host has completed rm and
      # moved on to ls.  Delaying one more empty read lets Wait-ForPatternFrom
      # see that it did not preexist at the post-rm offset.
      $this.EmptyReads++
      if ($this.RmCompleted) { $this.PostRmEmptyReads++ }
      if ($this.Board.DeferredRound -and $this.PostRmEmptyReads -ge 3) {
        $this.Pending.Enqueue($this.Board.DeferredRound)
        $this.Board.DeferredRound = ''
        $this.BytesToRead = $this.Pending.Count
      } else {
        return ''
      }
    }
    # Let Send-Command observe the completion prompt before the following
    # command is written.  The virtual clock also advances so the loop's
    # deadline is reachable without real-time sleeps.
    $text = $this.Pending.Dequeue()
    $this.EmptyReads = 0
    $this.BytesToRead = $this.Pending.Count
    return $text
  }
}

function New-DailyAcceptFakeRuntime([hashtable]$options) {
  $board = [DailyAcceptFakeBoard]::new()
  foreach ($key in $options.Keys) { $board.$key = $options[$key] }
  $port = [DailyAcceptFakePort]::new()
  $port.Board = $board
  $port.Clock = [DailyAcceptVirtualClock]::new()
  return @{
    Transport = $port
    Clock = $port.Clock
    Board = $board
  }
}
