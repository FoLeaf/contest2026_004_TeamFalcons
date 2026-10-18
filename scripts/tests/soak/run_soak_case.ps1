# 单个离线长测场景的驱动进程。只注入假串口与虚拟时钟，不打开真实串口。
param(
  [Parameter(Mandatory = $true)][string]$Scenario,
  [Parameter(Mandatory = $true)][string]$OutDir,
  [Parameter(Mandatory = $true)][string]$SoakScript,
  [string]$LogName = 'soak.log',
  # 不注入假串口，走真实的启动路径。配合一个不存在的串口名使用，脚本会在
  # GetPortNames 检查处失败，不会打开任何真实串口。
  [switch]$NoTestRuntime,
  [string]$ComPort = 'COM_TEST'
)
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'soak_fake.ps1')
. (Join-Path $PSScriptRoot 'scenarios.ps1')

$scenarios = Get-SoakScenarios
if (!$scenarios.Contains($Scenario)) {
  [Console]::Error.WriteLine("unknown scenario: $Scenario")
  exit 90
}
$sc = $scenarios[$Scenario]
$cfg = New-SoakScenario $sc.Board
$runtime = New-SoakFakeRuntime $cfg

[IO.Directory]::CreateDirectory($OutDir) | Out-Null
$logPath = Join-Path $OutDir $LogName

$soakArgs = @{
  LogPath           = $logPath
  RunId             = "test-$Scenario"
  ComPort           = $ComPort
  Hours             = $sc.Soak.Hours
  IntervalSec       = $sc.Soak.IntervalSec
  MinimumOnline     = $sc.Soak.MinimumOnline
  CommandTimeoutSec = $sc.Soak.CommandTimeoutSec
  StoreRounds       = $sc.Soak.StoreRounds
}
if (!$NoTestRuntime) { $soakArgs['TestRuntime'] = $runtime }

& $SoakScript @soakArgs
exit $LASTEXITCODE
