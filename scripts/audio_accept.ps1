# BVO-04 bounded playback exercise through the standard NxPlayer path.
# Human listening and touch confirmation are separate required evidence.
param([int]$NormalRounds=20, [int]$StopRounds=10, [string]$ComPort='COM3')
$ErrorActionPreference='Stop'
if ($NormalRounds -lt 1 -or $NormalRounds -gt 20 -or $StopRounds -lt 0 -or $StopRounds -gt 10) { throw 'rounds out of range' }
try {[Console]::OutputEncoding=[System.Text.Encoding]::UTF8} catch {}
$p=New-Object System.IO.Ports.SerialPort $ComPort,115200
$p.Encoding=[System.Text.Encoding]::UTF8
$p.DtrEnable=$true; $p.RtsEnable=$true
function Drain([int]$ms) {
 $b='';$until=(Get-Date).AddMilliseconds($ms)
 while ((Get-Date) -lt $until) {
  if ($p.BytesToRead -gt 0) {$s=$p.ReadExisting();$b+=$s;[Console]::Write($s)}
  Start-Sleep -Milliseconds 10
 }
 return $b
}
function Send([string]$cmd,[int]$ms) {
 [Console]::WriteLine("`nHOST $((Get-Date).ToString('o')) SEND $cmd")
 $p.Write("$cmd`r")
 $b=Drain $ms
 $deadline=(Get-Date).AddSeconds(20)
 while ($b -notmatch '(nsh>|nxplayer>)' -and (Get-Date) -lt $deadline) {
  $b+=Drain 100
 }
 if ($b -notmatch '(nsh>|nxplayer>)') {throw "command did not return within bounded wait: $cmd"}
 return $b
}
function Snap([bool]$Player) {
 $cmd=if($Player){'!vgaudio sai'}else{'vgaudio sai'}
 $b=Send $cmd 500
 if ($b -notmatch 'dma_done=(\d+) errors=(\d+) timeouts=(\d+) txbytes=(\d+)') {throw 'missing TX counters'}
 $r=@{done=[long]$Matches[1];errors=[long]$Matches[2];timeouts=[long]$Matches[3];bytes=[long]$Matches[4]}
 if ($b -notmatch 'sai_underruns=0 clockerrors=0') {throw 'SAI underrun/clock error recorded'}
 if ($b -notmatch 'saien=0') {throw 'SAI remains active after bounded wait'}
 if ($r.errors -ne 0 -or $r.timeouts -ne 0) {throw 'DMA transfer error/timeout recorded'}
 return $r
}
$p.Open()
try {
 $null=Drain 12000
 $null=Send 'vgaudio txcheck' 800
 $null=Send 'vgruntime dump' 800
 $null=Send 'vgstats dump 1' 700
 $null=Send 'vgnet status' 700
 $null=Send 'vgstoretest 1' 8000
 $null=Send 'free' 700
 $baseline=Snap $false
 $null=Send 'nxplayer' 400
 $null=Send 'volume 15' 150
 $last=$baseline
 for($i=0;$i -lt $NormalRounds;$i++) {
  $file=if($i % 2 -eq 0){'tone'}else{'speech'}
  $null=Send "play /mnt/emmc/bvo04/$file.wav" 1500
  $now=Snap $true
  if($now.bytes-$last.bytes -ne 64000 -or $now.done-$last.done -ne 8) {throw "incomplete normal round $i"}
  [Console]::WriteLine("ACCEPT normal=$i file=$file bytes=64000 dma=8 PASS")
  $last=$now
 }
 for($i=0;$i -lt $StopRounds;$i++) {
  $null=Send 'play /mnt/emmc/bvo04/tone.wav' 200
  $timer=[Diagnostics.Stopwatch]::StartNew()
  $b=Send 'stop' 800
  $now=Snap $true
  if($b -notmatch 'nxplayer>' -or $now.bytes-$last.bytes -le 0 -or $now.bytes-$last.bytes -ge 64000) {throw "stop not demonstrated $i"}
  [Console]::WriteLine("ACCEPT stop=$i completed_bytes=$($now.bytes-$last.bytes) bounded_wait_ms=$($timer.ElapsedMilliseconds) PASS")
  $last=$now
  $null=Send 'play /mnt/emmc/bvo04/speech.wav' 1500
  $now=Snap $true
  if($now.bytes-$last.bytes -ne 64000 -or $now.done-$last.done -ne 8) {throw "incomplete restart $i"}
  [Console]::WriteLine("ACCEPT restart=$i bytes=64000 dma=8 PASS")
  $last=$now
 }
 # Read-only state plus a restricted storage probe while a known file plays.
 $null=Send 'play /mnt/emmc/bvo04/tone.wav' 100
 $null=Send '!vgstoretest 1' 200
 $last=Snap $true
 $null=Send 'play /mnt/emmc/bvo04/tone.wav' 100
 $null=Send '!vgstats dump 1' 200
 $null=Send '!vgnet status' 1200
 $last=Snap $true
 $null=Send 'quit' 600
 $null=Send 'vgruntime dump' 800
 $null=Send 'vgstats dump 1' 700
 $null=Send 'vgnet status' 700
 $null=Send 'vgstoretest 1' 8000
 $null=Send 'vgaudio txcheck' 800
 $null=Send 'free' 700
 $null=Send 'ps' 1000
 [Console]::WriteLine("ACCEPT summary normal=$NormalRounds stop=$StopRounds restart=$StopRounds software_checks=PASS human_listening=PENDING touch=PENDING")
} finally { $p.Close() }
