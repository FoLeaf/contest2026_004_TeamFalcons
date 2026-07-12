[CmdletBinding()]
param(
  [string]$WslDistro = "",
  [string]$OpenvelaDir = "",
  [string]$BoardConfig = "stm32h750b-dk:lvgl",
  [string]$OutDir = "",
  [switch]$DebugBuild
)

$ErrorActionPreference = "Stop"

function Resolve-Setting {
  param(
    [string]$Value,
    [string]$EnvironmentName,
    [string]$DefaultValue
  )

  if (-not [string]::IsNullOrWhiteSpace($Value)) {
    return $Value
  }

  $environmentValue = [Environment]::GetEnvironmentVariable($EnvironmentName)
  if (-not [string]::IsNullOrWhiteSpace($environmentValue)) {
    return $environmentValue
  }

  return $DefaultValue
}

$WslDistro = Resolve-Setting $WslDistro "OPENVELA_WSL_DISTRO" "Debian"
$OutDir = Resolve-Setting $OutDir "OPENVELA_OUT_DIR" ""

if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
  throw "wsl.exe was not found. Enable WSL before running this task."
}

function Convert-ToWslPath {
  param(
    [Parameter(Mandatory = $true)][string]$Path,
    [Parameter(Mandatory = $true)][string]$Description
  )

  if ($Path -match '^\\\\wsl(?:\.localhost|\$)\\([^\\]+)\\(.*)$') {
    return "/" + ($Matches[2] -replace '\\', '/')
  }

  $candidates = @(($Path -replace '\\', '/'), $Path) | Select-Object -Unique
  foreach ($candidate in $candidates) {
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
      $converted = & wsl.exe -d $WslDistro -- wslpath -u $candidate 2>$null
      $exitCode = $LASTEXITCODE
    } finally {
      $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($exitCode -eq 0 -and -not [string]::IsNullOrWhiteSpace($converted)) {
      return $converted.Trim()
    }
  }

  throw "Failed to convert $Description to a WSL path: $Path"
}

function Invoke-CheckedWslScript {
  param(
    [Parameter(Mandatory = $true)][string]$Content,
    [Parameter(Mandatory = $true)][string]$ScratchDir
  )

  $scriptPath = Join-Path $ScratchDir "openvela_windows_build.sh"
  $normalized = ($Content -replace "`r`n", "`n") -replace "`r", ""
  [System.IO.File]::WriteAllText(
    $scriptPath,
    $normalized,
    [System.Text.UTF8Encoding]::new($false)
  )

  $scriptWsl = Convert-ToWslPath $scriptPath "temporary build script"
  & wsl.exe -d $WslDistro -- bash $scriptWsl
  if ($LASTEXITCODE -ne 0) {
    throw "WSL build failed with exit code $LASTEXITCODE"
  }
}

$repoDir = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutDir)) {
  $OutDir = Join-Path $repoDir ".debug"
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$repoDirWsl = Convert-ToWslPath $repoDir "contest repository"
$outDirWsl = Convert-ToWslPath $OutDir "artifact output directory"

if ([string]::IsNullOrWhiteSpace($OpenvelaDir)) {
  $OpenvelaDir = [Environment]::GetEnvironmentVariable("OPENVELA_ROOT_WSL")
}
if ([string]::IsNullOrWhiteSpace($OpenvelaDir)) {
  $OpenvelaDir = (& wsl.exe -d $WslDistro -- dirname $repoDirWsl).Trim()
  if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($OpenvelaDir)) {
    throw "Failed to derive the openvela root from $repoDirWsl"
  }
}

$debugBuildFlag = if ($DebugBuild) { "1" } else { "0" }
$buildCommand = @"
set -euo pipefail
OPENVELA_ROOT='$OpenvelaDir'
NUTTX_ROOT='${OpenvelaDir}/nuttx'
CONTEST_ROOT='$repoDirWsl'
OUT_ROOT='$outDirWsl'

export PATH="`$OPENVELA_ROOT/prebuilts/tools/python/bin:`$OPENVELA_ROOT/prebuilts/tools/linux/x86_64:`$OPENVELA_ROOT/prebuilts/kconfig-frontends/bin:`$OPENVELA_ROOT/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin:`$OPENVELA_ROOT/prebuilts/build-tools/linux-x86_64/bin:`$PATH"
export PYTHONPATH="`$OPENVELA_ROOT/prebuilts/tools/python/dist-packages/kconfiglib:`$OPENVELA_ROOT/prebuilts/tools/python/dist-packages:`${PYTHONPATH:-}"

bash "`$CONTEST_ROOT/scripts/apply-openvela-qspi-patch.sh" "`$OPENVELA_ROOT"
"`$NUTTX_ROOT/tools/configure.sh" -e '$BoardConfig'

before_config=`$(mktemp)
cp "`$NUTTX_ROOT/.config" "`$before_config"
kconfig-tweak --file "`$NUTTX_ROOT/.config" \
  --enable CONFIG_STM32H750B_DK_QSPI_BOOT \
  --enable CONFIG_LVX_USE_DEMO_CONTEST2026_004_VSCODE_LAB \
  --disable CONFIG_EXAMPLES_LVGLDEMO \
  --disable CONFIG_LV_BUILD_EXAMPLES \
  --disable CONFIG_LV_USE_DEMO_WIDGETS \
  --set-str CONFIG_INIT_ENTRYPOINT 'vscode_lab_main'

if [ '$debugBuildFlag' = '1' ]; then
  kconfig-tweak --file "`$NUTTX_ROOT/.config" \
    --enable CONFIG_DEBUG_SYMBOLS \
    --set-str CONFIG_DEBUG_SYMBOLS_LEVEL '-g3' \
    --enable CONFIG_DEBUG_NOOPT \
    --disable CONFIG_DEBUG_FULLOPT
else
  kconfig-tweak --file "`$NUTTX_ROOT/.config" \
    --disable CONFIG_DEBUG_SYMBOLS \
    --disable CONFIG_DEBUG_NOOPT \
    --enable CONFIG_DEBUG_FULLOPT
fi

make -C "`$NUTTX_ROOT" olddefconfig
if ! cmp -s "`$before_config" "`$NUTTX_ROOT/.config"; then
  make -C "`$NUTTX_ROOT" clean
fi
rm -f "`$before_config"

make -C "`$NUTTX_ROOT" -j`$(nproc)

# The legacy apps Make flow follows the workspace symlink and leaves these
# generated markers beside the team-owned source.  Keep the contest repository
# source-only after every build.
rm -f "`$CONTEST_ROOT/app/hello_app/.built" \
      "`$CONTEST_ROOT/app/hello_app/.depend" \
      "`$CONTEST_ROOT/app/hello_app/Make.dep"

bootstub_dir="`$OUT_ROOT/qspi_boot_stub"
bash "`$CONTEST_ROOT/scripts/qspi_boot_stub/build_bootstub.sh" "`$bootstub_dir"

mkdir -p "`$OUT_ROOT"
cp "`$NUTTX_ROOT/nuttx" "`$OUT_ROOT/nuttx.elf"
cp "`$NUTTX_ROOT/nuttx.hex" "`$OUT_ROOT/nuttx.hex"
cp "`$NUTTX_ROOT/nuttx.bin" "`$OUT_ROOT/nuttx.bin"
cp "`$bootstub_dir/qspi_bootstub.elf" "`$OUT_ROOT/qspi_bootstub.elf"
cp "`$bootstub_dir/qspi_bootstub.hex" "`$OUT_ROOT/qspi_bootstub.hex"
cp "`$bootstub_dir/qspi_bootstub.bin" "`$OUT_ROOT/qspi_bootstub.bin"

"`$OPENVELA_ROOT/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin/arm-none-eabi-size" "`$OUT_ROOT/nuttx.elf" "`$OUT_ROOT/qspi_bootstub.elf"
"@

$buildKind = if ($DebugBuild) { "debug" } else { "release" }
Write-Host "Building $buildKind QSPI-XIP firmware in WSL distro '$WslDistro'."
Write-Host "openvela root: $OpenvelaDir"
Write-Host "artifacts: $OutDir"
Invoke-CheckedWslScript $buildCommand $OutDir
Write-Host "Build completed: $OutDir"
