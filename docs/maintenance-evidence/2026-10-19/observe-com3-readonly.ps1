param([string]$Out, [int]$Seconds = 35)
$ErrorActionPreference = 'Stop'
$port = New-Object System.IO.Ports.SerialPort COM3,115200,None,8,One
$port.Encoding = [System.Text.Encoding]::UTF8
$port.ReadTimeout = 250
$port.DtrEnable = $false
$port.RtsEnable = $false
$port.Open()
$writer = New-Object System.IO.StreamWriter($Out, $false, [System.Text.UTF8Encoding]::new($false))
$writer.AutoFlush = $true
Write-Host "read-only COM3 capture for ${Seconds}s; DTR=RTS=false"
$deadline = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $deadline) {
  if ($port.BytesToRead -gt 0) {
    $chunk = $port.ReadExisting()
    [Console]::Write($chunk)
    $writer.Write($chunk)
  }
  Start-Sleep -Milliseconds 100
}
$writer.Close()
$port.Close()
