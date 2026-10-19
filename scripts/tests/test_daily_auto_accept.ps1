# Offline safety tests for stage1_daily_auto_report_accept.ps1.
#
# These tests execute the production script against a fake NSH transport.
# They do not open COM3, contact a broker, reset the board, or use the network.
[CmdletBinding()]
param(
  [string]$OutRoot = ''
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'daily_accept_fake.ps1')

$script = [IO.Path]::GetFullPath(
  (Join-Path $PSScriptRoot '../stage1_daily_auto_report_accept.ps1')
)
if (!$OutRoot) {
  $OutRoot = [IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot '../../.debug/daily-accept-tests')
  )
}
$runRoot = Join-Path $OutRoot (Get-Date -Format 'yyyyMMdd-HHmmss')
[IO.Directory]::CreateDirectory($runRoot) | Out-Null

$script:Passed = 0
$script:Failed = 0
$script:Failures = @()

function Assert-That([string]$name, [bool]$ok, [string]$detail) {
  if ($ok) {
    $script:Passed++
    Write-Host ("  ok   {0}" -f $name)
  } else {
    $script:Failed++
    $script:Failures += ("{0}: {1}" -f $name, $detail)
    Write-Host ("  FAIL {0}: {1}" -f $name, $detail) -ForegroundColor Red
  }
}

function Invoke-Case([string]$name, [hashtable]$boardOptions, [bool]$expectPass) {
  $dir = Join-Path $runRoot $name
  [IO.Directory]::CreateDirectory($dir) | Out-Null
  $capture = Join-Path $dir 'capture.log'
  $runtime = New-DailyAcceptFakeRuntime $boardOptions
  $output = ''
  $errorText = ''
  $ok = $true
  try {
    $warnings = @()
    $output = & $script -TestRuntime $runtime -Out $capture -WaitSec 20 `
      -ErrorAction Stop -WarningVariable warnings 6>&1 | Out-String
  } catch {
    $ok = $false
    $errorText = $_.Exception.Message
  }
  $passed = $ok -and ($output -match '(?m)^DAILY_AUTO_REPORT PASS')
  Assert-That "$name/verdict" ($passed -eq $expectPass) (
    "expected pass=$expectPass, actual pass=$passed, error=$errorText output=$output"
  )
  Assert-That "$name/capture-exists" (Test-Path -LiteralPath $capture) 'no capture was written'
  if (Test-Path -LiteralPath $capture) {
    $text = [IO.File]::ReadAllText($capture)
    Assert-That "$name/capture-has-prompt" ($text -match '(?m)^nsh>') 'capture has no NSH response'
  }
  return @{
    Output = $output
    Warnings = ($warnings -join "`n")
    Error = $errorText
    Capture = $capture
    Board = $runtime.Board
  }
}

$invalidRuntime = @{ Transport = $null; Clock = $null }
$invalidError = ''
try {
  $invalidCapture = Join-Path $runRoot 'invalid-runtime/capture.log'
  [IO.Directory]::CreateDirectory((Split-Path -Parent $invalidCapture)) | Out-Null
  & $script -TestRuntime $invalidRuntime -Out $invalidCapture -WaitSec 20 `
    -ErrorAction Stop 6>&1 | Out-Null
} catch {
  $invalidError = $_.Exception.Message
} finally {
  # Remove the stream writer created after validation, if there is one.
  if (Get-Variable log -Scope Script -ErrorAction SilentlyContinue) {
    $script:log.Dispose()
  }
}
Assert-That 'invalid-runtime/rejected' (
  $invalidError -match 'must contain Transport and Clock objects'
) "invalid runtime was accepted: $invalidError"
Assert-That 'invalid-runtime/no-open' (
  !(Test-Path -LiteralPath $invalidCapture) -or
  [IO.File]::ReadAllText($invalidCapture).Length -eq 0
) 'the invalid runtime opened or wrote to the transport'

$normal = Invoke-Case 'happy-path' @{ DeferAfterRm = $true } $true
Assert-That 'happy-path/restored' $normal.Board.HasReport 'the live report was not restored'
Assert-That 'happy-path/hold-removed' (!$normal.Board.HasHold) 'the hold was not removed'

$stale = Invoke-Case 'stale-hold-refused' @{ HasHold = $true; StaleHold = $true } $false
Assert-That 'stale-hold/live-preserved' $stale.Board.HasReport 'the live report was removed before stale-hold rejection'
Assert-That 'stale-hold/stale-preserved' $stale.Board.HasHold 'the stale hold was unexpectedly removed'
Assert-That 'stale-hold/no-removal-command' (
  $stale.Board.Commands -lt 10 -and
  -not ([IO.File]::ReadAllText($stale.Capture) -match '(?m)^DAILY_ACCEPT> rm ')
) 'the script issued rm after refusing the stale hold'

$copyFailure = Invoke-Case 'copy-failure-refused' @{ CpFails = $true } $false
Assert-That 'copy-failure/live-preserved' $copyFailure.Board.HasReport 'the live report was removed after failed cp'
Assert-That 'copy-failure/hold-absent' (!$copyFailure.Board.HasHold) 'a failed cp created a hold'
Assert-That 'copy-failure/no-removal-command' (
  -not ([IO.File]::ReadAllText($copyFailure.Capture) -match '(?m)^DAILY_ACCEPT> rm ')
) 'the script issued rm after failed cp'

$compareFailure = Invoke-Case 'compare-failure-refused' @{ CmpFails = $true } $false
Assert-That 'compare-failure/live-preserved' $compareFailure.Board.HasReport 'the live report was removed after failed cmp'
Assert-That 'compare-failure/hold-preserved' $compareFailure.Board.HasHold 'the unverified hold was removed'
Assert-That 'compare-failure/no-removal-command' (
  -not ([IO.File]::ReadAllText($compareFailure.Capture) -match '(?m)^DAILY_ACCEPT> rm ')
) 'the script issued rm after failed cmp'

$removeFailure = Invoke-Case 'remove-failure-recovered' @{ RmFails = $true } $false
Assert-That 'remove-failure/live-preserved' $removeFailure.Board.HasReport 'the live report was lost after failed rm'

$noRequest = Invoke-Case 'no-daily-request-recovered' @{ EmitDailyRound = $false } $false
Assert-That 'no-request/live-restored' $noRequest.Board.HasReport 'the held report was not restored after timeout'
Assert-That 'no-request/hold-removed' (!$noRequest.Board.HasHold) 'the hold remained after successful recovery'

$badBody = Invoke-Case 'missing-report-body-recovered' @{ ReportBodyPresent = $false } $false
Assert-That 'bad-body/live-restored' $badBody.Board.HasReport 'the held report was not restored after an unreadable report'
Assert-That 'bad-body/hold-removed' (!$badBody.Board.HasHold) 'the hold remained after successful recovery'

$wcMismatch = Invoke-Case 'wc-byte-mismatch-recovered' @{
  WcBytesWrong = $true
} $false
Assert-That 'wc-mismatch/live-restored' $wcMismatch.Board.HasReport 'the held report was not restored after a byte-count mismatch'
Assert-That 'wc-mismatch/hold-removed' (!$wcMismatch.Board.HasHold) 'the hold remained after a byte-count mismatch'
Assert-That 'wc-mismatch/reported' (
  $wcMismatch.Output + $wcMismatch.Warnings + $wcMismatch.Error -match
    'board-side wc and cat byte counts differ'
) 'the byte-count mismatch was not reported'

$restoreFailure = Invoke-Case 'restore-copy-failure-keeps-hold' @{
  RestoreCpFails = $true
} $false
Assert-That 'restore-failure/hold-kept' $restoreFailure.Board.HasHold 'the hold was deleted despite failed recovery cmp'
Assert-That 'restore-failure/warning' (
  $restoreFailure.Output + $restoreFailure.Warnings +
    $restoreFailure.Error -match 'automatic restore failed; hold remains'
) 'restore failure was not reported'

Write-Host ''
Write-Host ("artifacts: {0}" -f $runRoot)
Write-Host ("passed={0} failed={1}" -f $script:Passed, $script:Failed)
if ($script:Failed -gt 0) {
  foreach ($failure in $script:Failures) {
    Write-Host ("  - {0}" -f $failure)
  }
  exit 1
}
exit 0
