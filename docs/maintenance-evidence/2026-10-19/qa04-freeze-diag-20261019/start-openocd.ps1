$ErrorActionPreference = 'Stop'
$exe = 'D:\Develop\xpack-openocd-0.12.0-7\bin\openocd.exe'
$scripts = 'D:\Develop\xpack-openocd-0.12.0-7\openocd\scripts'
$cfg = '\\wsl.localhost\Debian\home\hello19y\openvela\.vg-maintenance-closeout\scripts\gdb\stlink-h7.cfg'
$log = '\\wsl.localhost\Debian\home\hello19y\openvela\.vg-maintenance-closeout\.debug\freeze-diag-20261019\openocd.log'
$cmd = "`"$exe`" -s `"$scripts`" -f `"$cfg`" -c `"init`" -c `"reset run`" -l `"$log`""
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $cmd }
Start-Sleep -Seconds 4
$p = Get-CimInstance Win32_Process -Filter "ProcessId=$($r.ProcessId)"
[pscustomobject]@{ pid=$r.ProcessId; rc=$r.ReturnValue; alive=($null -ne $p); cmd=$cmd } | ConvertTo-Json -Compress
