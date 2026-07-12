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

$arguments = @()
if (-not [string]::IsNullOrWhiteSpace($CubeCli)) { $arguments += @("-CubeCli", $CubeCli) }
if (-not [string]::IsNullOrWhiteSpace($ExternalLoader)) { $arguments += @("-ExternalLoader", $ExternalLoader) }
if (-not [string]::IsNullOrWhiteSpace($OutDir)) { $arguments += @("-OutDir", $OutDir) }
if ($NoBuild) { $arguments += "-NoBuild" }
if ($DebugBuild) { $arguments += "-DebugBuild" }
if ($ValidateOnly) { $arguments += "-ValidateOnly" }

& (Join-Path $PSScriptRoot "windows_flash_cube.ps1") @arguments
exit $LASTEXITCODE
