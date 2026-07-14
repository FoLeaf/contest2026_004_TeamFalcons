[CmdletBinding()]
param(
  [string]$CubeCli = "",
  [string]$ExternalLoader = "",
  [string]$OutDir = "",
  [switch]$NoBuild,
  [switch]$DebugBuild,
  [ValidateSet("test", "production")]
  [string]$VelaGuardMode = "test",
  [string]$DeviceIdOverride = "vg-test-001",
  [ValidateSet("incremental", "full")]
  [string]$Rebuild = "incremental",
  [switch]$FullClean,
  [switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"
Write-Warning "OpenOCD is debug-only for STM32H750B-DK dual QSPI. Redirecting the flash request to CubeProgrammer."

$parameters = @{}
if (-not [string]::IsNullOrWhiteSpace($CubeCli)) { $parameters["CubeCli"] = $CubeCli }
if (-not [string]::IsNullOrWhiteSpace($ExternalLoader)) { $parameters["ExternalLoader"] = $ExternalLoader }
if (-not [string]::IsNullOrWhiteSpace($OutDir)) { $parameters["OutDir"] = $OutDir }
if ($NoBuild) { $parameters["NoBuild"] = $true }
if ($DebugBuild) { $parameters["DebugBuild"] = $true }
$parameters["VelaGuardMode"] = $VelaGuardMode
$parameters["DeviceIdOverride"] = $DeviceIdOverride
$parameters["Rebuild"] = $Rebuild
if ($FullClean) { $parameters["FullClean"] = $true }
if ($ValidateOnly) { $parameters["ValidateOnly"] = $true }

& (Join-Path $PSScriptRoot "windows_flash_cube.ps1") @parameters
exit $LASTEXITCODE
