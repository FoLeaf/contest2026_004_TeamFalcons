# Flash net firmware, provision LLM secrets, then run agent accept (single COM3 session).
param(
  [string]$ComPort = "COM3",
  [string]$ContestRoot = ""
)

$ErrorActionPreference = "Stop"
if ($ContestRoot -eq "") { $ContestRoot = Split-Path -Parent $PSScriptRoot }

$flashPs1 = Join-Path $ContestRoot "scripts\flash.ps1"
$provPs1  = Join-Path $ContestRoot "scripts\provision-llm-from-secrets.ps1"
$acceptPs1 = Join-Path $ContestRoot "scripts\stage1_agent_accept.ps1"

Write-Host "[1/4] Flash net firmware..."
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $flashPs1
if ($LASTEXITCODE -ne 0) { throw "flash failed exit=$LASTEXITCODE" }

Write-Host "[2/4] Wait for board reboot..."
Start-Sleep -Seconds 8

Write-Host "[3/4] Provision LLM from secrets/ (if present)..."
$keyFile = Join-Path $ContestRoot "secrets\agent_llm.key"
if (Test-Path $keyFile) {
  & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $provPs1 -ComPort $ComPort -ContestRoot $ContestRoot
  if ($LASTEXITCODE -ne 0) { throw "provision failed exit=$LASTEXITCODE" }
  Start-Sleep -Seconds 5
} else {
  Write-Host "[3/4] SKIP provision (no secrets/agent_llm.key)"
}

Write-Host "[4/4] Agent accept..."
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $acceptPs1 -ComPort $ComPort -Log (Join-Path $ContestRoot ".debug\stage1_agent_accept.log")
exit $LASTEXITCODE
