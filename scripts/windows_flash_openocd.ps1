[CmdletBinding()]
param(
  [string]$CubeCli = "",
  [string]$ExternalLoader = "",
  [string]$OutDir = "",
  [switch]$NoBuild,
  [switch]$DebugBuild,
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
if ($ValidateOnly) { $parameters["ValidateOnly"] = $true }

& (Join-Path $PSScriptRoot "windows_flash_cube.ps1") @parameters
exit $LASTEXITCODE
