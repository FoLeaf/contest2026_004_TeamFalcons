$ErrorActionPreference = 'Stop'
$script = '\\wsl.localhost\Debian\home\hello19y\openvela\.vg-maintenance-closeout\scripts\maintenance_soak.ps1'
$runid = 'qa04-freezediag-20261019T1953'
$log = '\\wsl.localhost\Debian\home\hello19y\openvela\.vg-maintenance-closeout\.debug\freeze-diag-20261019\host-soak.log'
$cmd = "`"$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe`" -NoProfile -ExecutionPolicy Bypass -File `"$script`" -ComPort COM3 -Hours 8 -IntervalSec 900 -StoreRounds 50 -MinimumOnline 14 -CommandTimeoutSec 240 -RunId $runid -LogPath `"$log`""
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $cmd }
Start-Sleep -Seconds 3
$p = Get-CimInstance Win32_Process -Filter "ProcessId=$($r.ProcessId)"
[pscustomobject]@{ returned_pid=$r.ProcessId; return_value=$r.ReturnValue; alive=($null -ne $p); started=$p.CreationDate } | ConvertTo-Json -Compress
