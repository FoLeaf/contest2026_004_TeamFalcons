# maintenance_soak.ps1 的离线测试。
#
# 全部用例使用假串口和虚拟时钟，不打开真实串口、不连 broker、不碰开发板，
# 也不实际等待 24 小时。运行方式见 docs/maintenance-soak-runbook.md。
#
# 每个用例在独立子进程里跑，日志写到 .debug/soak-tests/<时间戳>/<用例>/。
[CmdletBinding()]
param(
  [string]$OutRoot = '',
  [string]$Filter = ''
)
$ErrorActionPreference = 'Stop'

$here = $PSScriptRoot
$soakScript = [IO.Path]::GetFullPath((Join-Path $here '../../maintenance_soak.ps1'))
$driver = Join-Path $here 'run_soak_case.ps1'
if (!$OutRoot) { $OutRoot = [IO.Path]::GetFullPath((Join-Path $here '../../../.debug/soak-tests')) }
$runRoot = Join-Path $OutRoot (Get-Date -Format 'yyyyMMdd-HHmmss')
[IO.Directory]::CreateDirectory($runRoot) | Out-Null
$psExe = (Get-Process -Id $PID).Path
$workDir = $env:TEMP

. (Join-Path $here 'scenarios.ps1')
$scenarios = Get-SoakScenarios

$script:Passed = 0
$script:Failed = 0
$script:Failures = @()

function Assert-That {
  param([string]$Case, [bool]$Ok, [string]$Detail)
  if ($Ok) {
    $script:Passed++
    Write-Host ("  ok   {0}" -f $Case)
  }
  else {
    $script:Failed++
    $script:Failures += ("{0}: {1}" -f $Case, $Detail)
    Write-Host ("  FAIL {0} -- {1}" -f $Case, $Detail) -ForegroundColor Red
  }
}

function Get-LogLines([string]$Path) {
  if (!(Test-Path $Path)) { return @() }
  return @([IO.File]::ReadAllLines($Path))
}

function Test-LogMatch([string[]]$Lines, [string]$Pattern) {
  return (@($Lines | Where-Object { $_ -match $Pattern }).Count -gt 0)
}

function Start-SoakChild {
  param([string]$Scenario, [string]$OutDir, [string]$LogName, [switch]$NoWait, [hashtable]$ScenarioSpec = $null)
  $stdout = Join-Path $OutDir 'child.stdout.log'
  $stderr = Join-Path $OutDir 'child.stderr.log'
  [IO.Directory]::CreateDirectory($OutDir) | Out-Null
  $argList = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $driver,
    '-Scenario', $Scenario, '-OutDir', $OutDir, '-SoakScript', $soakScript, '-LogName', $LogName)
  if ($ScenarioSpec -and $ScenarioSpec.ContainsKey('NoTestRuntime') -and $ScenarioSpec['NoTestRuntime']) {
    $argList += @('-NoTestRuntime', '-ComPort', $ScenarioSpec['ComPort'])
  }
  $params = @{
    FilePath               = $psExe
    ArgumentList           = $argList
    WorkingDirectory       = $workDir
    NoNewWindow            = $true
    RedirectStandardOutput = $stdout
    RedirectStandardError  = $stderr
    PassThru               = $true
  }
  if (!$NoWait) { $params['Wait'] = $true }
  return Start-Process @params
}

function Wait-ForLogMatch {
  param([string]$Path, [string]$Pattern, [int]$TimeoutSec = 60)
  $deadline = (Get-Date).AddSeconds($TimeoutSec)
  while ((Get-Date) -lt $deadline) {
    if (Test-Path $Path) {
      # 子进程持有写句柄，用共享读打开。
      $fs = $null
      try {
        $fs = [IO.File]::Open($Path, 'Open', 'Read', 'ReadWrite')
        $sr = New-Object IO.StreamReader($fs)
        $content = $sr.ReadToEnd()
        $sr.Dispose()
      }
      catch { $content = '' }
      finally { if ($fs) { $fs.Dispose() } }
      if ($content -match "(?m)$Pattern") { return $true }
    }
    Start-Sleep -Milliseconds 100
  }
  return $false
}

function Invoke-ScenarioCase {
  param([string]$Name, [hashtable]$Scenario)
  Write-Host ("CASE {0}" -f $Name)
  $dir = Join-Path $runRoot $Name
  $proc = Start-SoakChild -Scenario $Name -OutDir $dir -LogName 'soak.log' -ScenarioSpec $Scenario
  $code = $proc.ExitCode
  $logPath = Join-Path $dir 'soak.log'
  $lines = Get-LogLines $logPath
  $runId = "test-$Name"
  $expect = $Scenario.Expect

  Assert-That "$Name/exit-code" ($code -eq $expect.ExitCode) ("expected exit {0}, got {1}" -f $expect.ExitCode, $code)
  Assert-That "$Name/log-exists" (Test-Path $logPath) 'the run produced no log file'
  Assert-That "$Name/run-begin" (Test-LogMatch $lines ("^RUN_BEGIN run={0} " -f [regex]::Escape($runId))) 'no RUN_BEGIN for this run id'

  switch ($expect.Verdict) {
    'PASS' {
      $passLine = @($lines | Where-Object { $_ -match ("^PASS run={0} status=PASS " -f [regex]::Escape($runId)) })
      Assert-That "$Name/pass-line" ($passLine.Count -eq 1) 'expected exactly one PASS line'
      Assert-That "$Name/run-end-pass" (Test-LogMatch $lines ("^RUN_END run={0} status=PASS" -f [regex]::Escape($runId))) 'no RUN_END status=PASS'
      if ($passLine.Count -eq 1) {
        $elapsed = [double]([regex]::Match($passLine[0], 'elapsed_s=([\d,]+)').Groups[1].Value -replace ',', '')
        $required = [double]([regex]::Match($passLine[0], 'required_s=([\d,]+)').Groups[1].Value -replace ',', '')
        Assert-That "$Name/duration" ($elapsed -ge $required) ("PASS elapsed {0}s is shorter than required {1}s" -f $elapsed, $required)
      }
    }
    'FAIL' {
      Assert-That "$Name/fail-line" (Test-LogMatch $lines ("^FAIL run={0} status=FAIL " -f [regex]::Escape($runId))) 'no FAIL line'
      Assert-That "$Name/run-end-fail" (Test-LogMatch $lines ("^RUN_END run={0} status=FAIL" -f [regex]::Escape($runId))) 'no RUN_END status=FAIL'
    }
    'INCOMPLETE' {
      Assert-That "$Name/abort-line" (Test-LogMatch $lines ("^ABORT run={0} status=INCOMPLETE " -f [regex]::Escape($runId))) 'no ABORT line'
      Assert-That "$Name/run-end-incomplete" (Test-LogMatch $lines ("^RUN_END run={0} status=INCOMPLETE" -f [regex]::Escape($runId))) 'no RUN_END status=INCOMPLETE'
    }
  }
  if ($expect.NoPass) {
    Assert-That "$Name/no-pass" (!(Test-LogMatch $lines '^PASS ')) 'a PASS line exists although the run did not pass'
  }
  if ($expect.ContainsKey('ReasonContains')) {
    Assert-That "$Name/reason" (Test-LogMatch $lines ("^FAIL run={0} .*{1}" -f [regex]::Escape($runId), [regex]::Escape($expect.ReasonContains))) ("FAIL reason does not contain '{0}'" -f $expect.ReasonContains)
  }
  if ($expect.ContainsKey('WarnContains')) {
    Assert-That "$Name/warn" (Test-LogMatch $lines ("^WARN run={0} .*{1}" -f [regex]::Escape($runId), [regex]::Escape($expect.WarnContains))) ("no WARN containing '{0}'" -f $expect.WarnContains)
  }
  if ($expect.ContainsKey('MinCycles')) {
    $cycles = @($lines | Where-Object { $_ -match "^CYCLE run=$([regex]::Escape($runId)) " }).Count
    Assert-That "$Name/cycles" ($cycles -ge $expect.MinCycles) ("only {0} cycles, expected at least {1}" -f $cycles, $expect.MinCycles)
  }
  # 每个用例都要求：RUN_END 必须是这个 run 的最后一条结论行。
  $verdicts = @($lines | Where-Object { $_ -match '^(PASS|FAIL|ABORT|RUN_END) run=' })
  if ($verdicts.Count -gt 0) {
    Assert-That "$Name/verdict-order" ($verdicts[-1] -match "^RUN_END run=$([regex]::Escape($runId)) ") 'the last verdict line is not this run RUN_END'
  }
}

# ---------------------------------------------------------------- 场景用例

$names = @($scenarios.Keys)
if ($Filter) { $names = @($names | Where-Object { $_ -match $Filter }) }

foreach ($name in $names) {
  if ($name -eq 'slow-run') { continue }
  Invoke-ScenarioCase -Name $name -Scenario $scenarios[$name]
}

# ------------------------------------------------- 强制结束：结果必须不完整

Write-Host 'CASE killed-run-leaves-no-verdict'
$killDir = Join-Path $runRoot 'killed-run-leaves-no-verdict'
$killLog = Join-Path $killDir 'soak.log'
$killProc = Start-SoakChild -Scenario 'slow-run' -OutDir $killDir -LogName 'soak.log' -NoWait
$sawCycle = Wait-ForLogMatch -Path $killLog -Pattern '^CYCLE ' -TimeoutSec 90
Stop-Process -Id $killProc.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500
$killLines = Get-LogLines $killLog
Assert-That 'killed-run/reached-cycle' $sawCycle 'the run never reached a CYCLE line'
Assert-That 'killed-run/no-pass' (!(Test-LogMatch $killLines '^PASS ')) 'a killed run wrote PASS'
Assert-That 'killed-run/no-run-end' (!(Test-LogMatch $killLines '^RUN_END ')) 'a killed run wrote RUN_END'
Assert-That 'killed-run/has-run-begin' (Test-LogMatch $killLines '^RUN_BEGIN ') 'no RUN_BEGIN in the killed run log'

# --------------------------- 追加到旧日志：不得继承早先那次运行的 PASS

Write-Host 'CASE appended-log-does-not-inherit-old-pass'
$appendDir = Join-Path $runRoot 'appended-log-does-not-inherit-old-pass'
$appendLog = Join-Path $appendDir 'soak.log'
$first = Start-SoakChild -Scenario 'happy-path' -OutDir $appendDir -LogName 'soak.log'
Assert-That 'appended/first-run-passed' (Test-LogMatch (Get-LogLines $appendLog) '^PASS run=test-happy-path status=PASS ') 'the first run in the shared log did not pass'
# 第二次运行追加到同一文件，中途被强制结束。
$second = Start-SoakChild -Scenario 'slow-run' -OutDir $appendDir -LogName 'soak.log' -NoWait
$null = Wait-ForLogMatch -Path $appendLog -Pattern '^CYCLE run=test-slow-run ' -TimeoutSec 90
Stop-Process -Id $second.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500
$appendLines = Get-LogLines $appendLog
Assert-That 'appended/two-run-begins' ((@($appendLines | Where-Object { $_ -match '^RUN_BEGIN ' }).Count) -eq 2) 'the shared log does not hold exactly two RUN_BEGIN lines'
Assert-That 'appended/old-pass-intact' (Test-LogMatch $appendLines '^PASS run=test-happy-path status=PASS ') 'the first run PASS line is missing'
Assert-That 'appended/second-run-incomplete' (!(Test-LogMatch $appendLines '^RUN_END run=test-slow-run ')) 'the killed second run wrote RUN_END'
Assert-That 'appended/last-run-has-no-pass' (!(Test-LogMatch $appendLines '^PASS run=test-slow-run ')) 'the killed second run wrote PASS'

# ------------------------------- 提示符判定：旧写法在真机形态下会误判超时

Write-Host 'CASE prompt-detection-regression'
$sample = "vgruntime dump`r`n" +
  "vgruntime: uptime_s=77 wall=2026-10-18T18:47:30`r`n" +
  "nsh> $([char]27)[K" +
  "vgtime: restore 2026-10-18 16:56:33`r`n" +
  "[heartbeat] skip LLM (HMI, no ReAct from heartbeat/poke)`r`n"
$oldPattern = 'vgruntime: uptime_s=.*nsh>\s*$'
$newPattern = "(?s)(?:vgruntime: uptime_s=).*?nsh>(?:\s|$)"
Assert-That 'prompt/old-pattern-misses' (!($sample -match $oldPattern)) 'the old end-anchored pattern unexpectedly matched'
Assert-That 'prompt/new-pattern-matches' ([bool]($sample -match $newPattern)) 'the new prompt pattern did not match'

# ---------------------------------------------------------------- 结果汇总

Write-Host ''
Write-Host ("artifacts: {0}" -f $runRoot)
Write-Host ("passed={0} failed={1}" -f $script:Passed, $script:Failed)
if ($script:Failed -gt 0) {
  Write-Host 'failures:'
  foreach ($f in $script:Failures) { Write-Host ("  - {0}" -f $f) }
  exit 1
}
exit 0
