param([int]$Samples = 40)
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$control = Join-Path $PSScriptRoot 'final-g-postreset-control.txt'
$port = New-Object System.IO.Ports.SerialPort 'COM3',115200
$port.Encoding = [System.Text.Encoding]::UTF8
$port.DtrEnable = $true
$port.RtsEnable = $true
function Drain([int]$ms) {
  $result = ''
  $until = [DateTime]::UtcNow.AddMilliseconds($ms)
  while ([DateTime]::UtcNow -lt $until) {
    if ($port.BytesToRead -gt 0) {
      $part = $port.ReadExisting()
      $result += $part
      Write-Host -NoNewline $part
    }
    Start-Sleep -Milliseconds 50
  }
  return $result
}
function Query([string]$cmd) {
  Write-Host "`nHOST_TIME=$([DateTimeOffset]::Now.ToString('o')) COMMAND=$cmd"
  $port.Write("$cmd`r")
  $response = Drain 3000
  if ($response -notmatch 'nsh>') { throw "No NSH prompt for $cmd; preserving board state" }
  return $response
}
try {
  $port.Open()
  Drain 1000 | Out-Null
  Query 'vgruntime dump' | Out-Null
  Query 'vgpoint list' | Out-Null
  Write-Host "`nHOST_TRIGGER=$([DateTimeOffset]::Now.ToString('o')) drop 7 0 32"
  [System.IO.File]::WriteAllText($control,"drop 7 0 32`n",[System.Text.Encoding]::ASCII)
  for ($i=0; $i -lt $Samples; $i++) {
    Write-Host "`nSAMPLE=$i"
    Query 'vgruntime dump' | Out-Null
  }
  Query 'vgagent status' | Out-Null
  Query 'ls /data/velaguard/pending_alarm.txt' | Out-Null
  Write-Host "`nPROBE_COMPLETED=$([DateTimeOffset]::Now.ToString('o'))"
}
finally {
  [System.IO.File]::WriteAllText($control,"normal`n",[System.Text.Encoding]::ASCII)
  $port.Close()
  Write-Host "`nHOST_NORMAL_RESTORED=$([DateTimeOffset]::Now.ToString('o'))"
}
