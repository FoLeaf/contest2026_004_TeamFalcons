# 离线长测测试用的假板端与虚拟时钟。
#
# 这里不打开任何串口、不连 broker、不碰开发板。假串口按场景配置生成板端
# 风格的输出（含 ANSI 提示符、后台日志交错、分片），供 maintenance_soak.ps1
# 的 -TestRuntime 使用。
#
# 已知限制：假串口在已解码的 .NET 字符串上分片，因此无法复现 UTF-8 多字节
# 字符被拆到两次读取之间的情形；该路径只能在真机上观察。

class SoakVirtualClock {
  [double]$Seconds = 0
  [datetime]$Start = [datetime]'2026-10-18T20:00:00+08:00'
  [int]$RealSleepMs = 0

  [datetime] Now() { return $this.Start.AddSeconds($this.Seconds) }
  [double] Elapsed() { return $this.Seconds }
  [void] Sleep([int]$ms) {
    if ($this.RealSleepMs -gt 0) { Start-Sleep -Milliseconds $this.RealSleepMs }
    $this.Seconds += ($ms / 1000.0)
  }
}

class SoakFakeBoard {
  [hashtable]$Cfg
  [int]$Cycles = 0
  [int]$Points = 14
  [bool]$LinkDropped = $false
  [bool]$StopRequested = $false

  SoakFakeBoard([hashtable]$cfg) { $this.Cfg = $cfg }

  [int] OnlineCount() {
    $n = $this.Points
    $table = $this.Cfg['OnlineAfterCycle']
    if ($table) {
      $keys = @($table.Keys | Where-Object { $_ -le $this.Cycles } | Sort-Object)
      if ($keys.Count -gt 0) { $n = [int]$table[$keys[-1]] }
    }
    return $n
  }

  [long] UptimeSeconds() {
    $regression = [int]$this.Cfg['UptimeRegressionAtCycle']
    if ($regression -gt 0 -and $this.Cycles -ge $regression) { return 40 }
    return [long]$this.Cfg['UptimeBase'] + ([long]$this.Cfg['UptimeStep'] * $this.Cycles)
  }

  [string[]] TableRows() {
    $base = @(
      'vgpoint: POINT id=temp name={NAME_TEMP} addr=1 fc=3 reg=0 qty=1 dtype=int16 scale=0.1 unit=C cmp=ge warn=28 crit=35 fail_n=3',
      'vgpoint: POINT id=humid name=humid addr=1 fc=3 reg=1 qty=1 dtype=uint16 scale=0.1 unit=%RH cmp=ge warn=70 crit=85 fail_n=3',
      'vgpoint: POINT id=flood name=flood addr=2 fc=3 reg=2 qty=1 dtype=uint16 scale=1 unit=- cmp=eq warn=- crit=1 fail_n=3',
      'vgpoint: POINT id=smoke name=smoke addr=3 fc=3 reg=0 qty=1 dtype=uint16 scale=0.1 unit=% cmp=ge warn=5 crit=10 fail_n=3',
      'vgpoint: POINT id=acu_ret name=acu_ret addr=4 fc=3 reg=0 qty=1 dtype=int16 scale=0.1 unit=C cmp=ge warn=28 crit=32 fail_n=3',
      'vgpoint: POINT id=acu_set name=acu_set addr=4 fc=3 reg=1 qty=1 dtype=int16 scale=0.1 unit=C cmp=- warn=- crit=- fail_n=3',
      'vgpoint: POINT id=acu_run name=acu_run addr=4 fc=3 reg=2 qty=1 dtype=uint16 scale=1 unit=- cmp=- warn=- crit=- fail_n=3',
      'vgpoint: POINT id=ups_load name=ups_load addr=5 fc=3 reg=0 qty=1 dtype=uint16 scale=0.1 unit=% cmp=ge warn=70 crit=90 fail_n=3',
      'vgpoint: POINT id=ups_soc name=ups_soc addr=5 fc=3 reg=1 qty=1 dtype=uint16 scale=1 unit=% cmp=le warn=40 crit=20 fail_n=3',
      'vgpoint: POINT id=ups_bypass name=ups_bypass addr=5 fc=3 reg=2 qty=1 dtype=uint16 scale=1 unit=- cmp=eq warn=- crit=1 fail_n=3',
      'vgpoint: POINT id=volt_a name=volt_a addr=6 fc=3 reg=0 qty=1 dtype=uint16 scale=0.1 unit=V cmp=le warn=200 crit=180 fail_n=3',
      'vgpoint: POINT id=pwr_kw name=pwr_kw addr=6 fc=3 reg=1 qty=1 dtype=uint16 scale=0.01 unit=kW cmp=ge warn=8 crit=10 fail_n=3',
      'vgpoint: POINT id=energy_kwh name=energy_kwh addr=6 fc=3 reg=2 qty=1 dtype=uint16 scale=0.1 unit=kWh cmp=- warn=- crit=- fail_n=3',
      'vgpoint: POINT id=door name=door addr=7 fc=3 reg=0 qty=1 dtype=uint16 scale=1 unit=- cmp=eq warn=- crit=1 fail_n=3'
    )
    # 非 ASCII 名称用字符码拼出，避免测试源文件的编码影响。
    $chinese = [string]([char]0x6E29) + [string]([char]0x5EA6)
    $rows = @($base | ForEach-Object { $_ -replace '\{NAME_TEMP\}', $chinese })
    $changeAt = [int]$this.Cfg['TableChangeAtCycle']
    if ($changeAt -gt 0 -and $this.Cycles -ge $changeAt) {
      $rows[0] = $rows[0] -replace 'crit=35', 'crit=40'
    }
    return $rows
  }

  [string[]] RuntimePointRows() {
    $spec = @(
      'temp 1 24', 'humid 1 50', 'flood 2 0', 'smoke 3 1',
      'acu_ret 4 25', 'acu_set 4 24', 'acu_run 4 1', 'ups_load 5 40',
      'ups_soc 5 80', 'ups_bypass 5 0', 'volt_a 6 230', 'pwr_kw 6 3',
      'energy_kwh 6 123.4', 'door 7 0'
    )
    $online = $this.OnlineCount()
    $rows = @()
    for ($i = 0; $i -lt $spec.Count; $i++) {
      $f = $spec[$i] -split ' '
      $isOnline = if ($i -lt $online) { 1 } else { 0 }
      $value = if ($isOnline -eq 1) { $f[2] } else { '-' }
      $rows += ('vgruntime: point id={0} slave={1} online={2} online_ms={3} offline_s={4} last_ok_ago_s={5} value={6} kind=none' -f `
          $f[0], $f[1], $isOnline, (77000 + $i * 10), (1 - $isOnline), (1 - $isOnline), $value)
    }
    return $rows
  }

  [string[]] HeapRows() {
    $faultAt = [int]$this.Cfg['HeapFaultAtCycle']
    if ($faultAt -gt 0 -and $this.Cycles -ge $faultAt) {
      # used + free 与 total 对不上，模拟堆统计自相矛盾。
      return @('      total       used       free    maxused    maxfree  nused  nfree name',
               '    7014016     318608    6695000     450216    6291440   1011      9 Umem')
    }
    return @('      total       used       free    maxused    maxfree  nused  nfree name',
             '    7014016     318608    6695408     450216    6291440   1011      9 Umem')
  }

  [bool] IsStalled([string]$cmd) {
    $patterns = @($this.Cfg['StallCommands'])
    if ($patterns.Count -eq 0) { return $false }
    $from = [int]$this.Cfg['StallFromCycle']
    if ($from -gt 0 -and $this.Cycles -lt $from) { return $false }
    foreach ($p in $patterns) { if ($cmd -match $p) { return $true } }
    return $false
  }

  [string[]] Respond([string]$cmd) {
    $dropAt = [int]$this.Cfg['DropLinkAtCycle']
    if ($dropAt -gt 0 -and $this.Cycles -ge $dropAt) { $this.LinkDropped = $true }
    $stopAt = [int]$this.Cfg['StopAtCycle']
    if ($stopAt -gt 0 -and $this.Cycles -ge $stopAt) { $this.StopRequested = $true }

    if ($this.IsStalled($cmd)) {
      # 命令挂住：有回显，但既不产生期望输出，也不回提示符。
      return @($cmd)
    }

    $faultAt = [int]$this.Cfg['FaultAtCycle']
    if ($faultAt -gt 0 -and $cmd -match '^vgstoretest ' -and ($this.Cycles + 1) -ge $faultAt) {
      $this.Cycles++
      if ($this.Cfg['FaultMode'] -eq 'panic') {
        # Keep this response free of the other fatal markers so the test
        # specifically proves that PANIC itself is detected across reads.
        return @($cmd, 'PANIC: injected scheduler failure')
      }
      return @($cmd,
        'HARDFAULT: CFSR=00008200 HFSR=40000000 BFAR=0000003b MMFAR=0000003b',
        'HARDFAULT: stacked PC=90095ff8 LR=9007bfc5',
        'dump_assert_info: Assertion failed panic: at file: :0 task: velaguard_app_main process: velaguard_app_main 0x9000caad',
        'up_dump_register: R0: 24004350 R1: 00000000 R2: 900a8396  R3: 24003d38')
    }

    $body = @()
    switch -regex ($cmd) {
      '^$' { }
      '^uname -a$' { $body = @($this.Cfg['Uname']) }
      '^vgstoretest ' {
        $this.Cycles++
        $body = @('vgstoretest: OK rounds={0} bytes=25600 elapsed_ms=91950' -f $this.Cfg['StoreRounds'])
      }
      '^vgpoint list$' {
        $body = @()
        $body += $this.TableRows()
        $body += ('vgpoint: OK cmd=list table=committed n={0}' -f $this.Points)
      }
      '^vgagent status$' {
        $body = @(
          ('round: state=idle gen={0} owner=none' -f $this.Cycles),
          ('llm: credentials={0} provision_file={1}' -f $this.Cfg['Credentials'], $this.Cfg['ProvisionFile'])
        )
      }
      '^vgruntime dump$' {
        $body = @('vgruntime: uptime_s={0} wall={1:yyyy-MM-ddTHH:mm:ss}' -f $this.UptimeSeconds(), $this.Cfg['Wall'])
        $body += 'vgruntime: slave=1 boot_total=94 boot_ok=94 boot_crc=0 boot_timeout=0 boot_echo=0 boot_other=0 boot_lat_min=20 boot_lat_max=200 boot_lat_avg=25 win_total=64 win_ok=64 win_crc=0 win_timeout=0 win_echo=0 win_other=0'
        $body += 'vgruntime: bus boot_total=649 boot_ok=649 boot_crc=0 boot_timeout=0 boot_echo=0 boot_other=0'
        $body += $this.RuntimePointRows()
      }
      '^cat /proc/meminfo$' { $body = $this.HeapRows() }
      default { $body = @('nsh: command not found: {0}' -f $cmd) }
    }

    $out = @($cmd)
    $out += $body
    $rebootAt = [int]$this.Cfg['RebootAtCycle']
    if ($rebootAt -gt 0 -and $this.Cycles -ge $rebootAt -and $cmd -match '^vgstoretest ') {
      $out += @($this.Cfg['RebootBannerText'])
    }
    $out += "nsh> $([char]27)[K"
    if ($this.Cfg['EscapeSuffix']) {
      # 板端在提示符后带 ESC[K。这里把它加到每一行末尾，用来验证解析前确实剥掉了转义。
      $out = @($out | ForEach-Object { "$_$([char]27)[K" })
    }
    if ($this.Cfg['AsyncAfterPrompt']) {
      # 提示符之后继续打异步日志：这是真机上观察到的形态。
      $out += @('[heartbeat] skip LLM (HMI, no ReAct from heartbeat/poke)',
                '[llm] Response: 0 bytes text, 2 tool calls, finish=tool_calls')
    }
    return $out
  }
}

class SoakFakePort {
  [System.Collections.Generic.Queue[string]]$Pending = [System.Collections.Generic.Queue[string]]::new()
  [System.Collections.Generic.List[string]]$Sent = [System.Collections.Generic.List[string]]::new()
  [bool]$IsOpen = $false
  [int]$ChunkSize = 32
  [int]$ReadCalls = 0
  [object]$Board = $null
  [string]$ReadContext = ''
  [bool]$InjectionDone = $false

  [void] Open() { $this.IsOpen = $true }
  [void] Close() { $this.IsOpen = $false }
  [void] Dispose() { $this.IsOpen = $false }

  [void] Write([string]$text) {
    if ($this.Board.StopRequested) { throw [System.Management.Automation.PipelineStoppedException]::new() }
    if (!$this.IsOpen) { throw [System.InvalidOperationException]::new('端口被关闭。') }
    if ($this.Board.LinkDropped) { throw [System.InvalidOperationException]::new('端口被关闭。') }
    [void]$this.Sent.Add($text)
    $cmd = $text.TrimEnd([char]13, [char]10)
    # 整段响应按固定长度切片，允许切片跨行——真实串口读取就是这样分段的。
    $full = (($this.Board.Respond($cmd) | ForEach-Object { "$_`r`n" }) -join '')
    $this.EnqueueChunkedText($full)
  }

  [void] EnqueueChunkedText([string]$text) {
    $needle = [string]$this.Board.Cfg['SplitNeedle']
    $splitAt = [int]$this.Board.Cfg['SplitAt']
    if ($needle -and $splitAt -gt 0 -and $splitAt -lt $needle.Length) {
      $pos = $text.IndexOf($needle, [StringComparison]::Ordinal)
      if ($pos -ge 0) {
        # Force exactly this pattern across two reads, without reducing every
        # command response to tiny chunks and exhausting the command timeout.
        $cut = $pos + $splitAt
        $this.Pending.Enqueue($text.Substring(0, $cut))
        $text = $text.Substring($cut)
      }
    }
    $size = [Math]::Max(1, $this.ChunkSize)
    for ($i = 0; $i -lt $text.Length; $i += $size) {
      $len = [Math]::Min($size, $text.Length - $i)
      $this.Pending.Enqueue($text.Substring($i, $len))
    }
  }

  [string] ReadExisting() {
    $this.ReadCalls++
    if ($this.Board.LinkDropped) { throw [System.InvalidOperationException]::new('端口被关闭。') }
    $cfg = $this.Board.Cfg
    $injectAt = [int]$cfg['FaultInjectionAtCycle']
    if (!$this.InjectionDone -and $injectAt -gt 0 -and
        $this.Board.Cycles -ge $injectAt -and
        $this.ReadContext -match [string]$cfg['FaultInjectionContext'] -and
        $this.Pending.Count -eq 0) {
      $this.InjectionDone = $true
      $text = ([string]$cfg['FaultInjectionText']) + "`r`n"
      $this.EnqueueChunkedText($text)
    }
    if ($this.Pending.Count -eq 0) { return '' }
    return $this.Pending.Dequeue()
  }
}

function New-SoakFakeRuntime {
  param([hashtable]$Scenario)
  $board = [SoakFakeBoard]::new($Scenario)
  $port = [SoakFakePort]::new()
  $port.Board = $board
  if ($Scenario.ContainsKey('ChunkSize')) { $port.ChunkSize = [int]$Scenario['ChunkSize'] }
  $clock = [SoakVirtualClock]::new()
  if ($Scenario.ContainsKey('RealSleepMs')) { $clock.RealSleepMs = [int]$Scenario['RealSleepMs'] }
  return @{
    Transport = $port
    Clock     = $clock
    Board     = $board
  }
}

# 场景默认值。单项场景只需要覆盖关心的字段。
function New-SoakScenario {
  param([hashtable]$Overrides)
  $cfg = @{
    Uname                 = 'NuttX  0.0.0 75cbc8eb7a5 Oct 18 2026 14:37:55 arm stm32h750b-dk'
    Wall                  = [datetime]'2026-10-18T20:00:00'
    Credentials           = 'ready'
    ProvisionFile         = 'present'
    StoreRounds           = 50
    UptimeBase            = 1000
    UptimeStep            = 60
    UptimeRegressionAtCycle = 0
    OnlineAfterCycle      = @{}
    TableChangeAtCycle    = 0
    HeapFaultAtCycle      = 0
    FaultAtCycle          = 0
    RebootAtCycle         = 0
    RebootBannerText      = @('eMMC mounted at /mnt/emmc', 'Symlink /data -> /mnt/emmc/data', 'NuttShell (NSH)')
    DropLinkAtCycle       = 0
    StopAtCycle           = 0
    FaultMode             = 'hardfault'
    FaultInjectionAtCycle = 0
    FaultInjectionContext = ''
    FaultInjectionText    = 'HARDFAULT: injected fault text split across serial reads'
    SplitNeedle           = ''
    SplitAt               = 0
    StallCommands         = @()
    StallFromCycle        = 0
    AsyncAfterPrompt      = $true
    EscapeSuffix          = $false
    ChunkSize             = 32
  }
  foreach ($k in $Overrides.Keys) { $cfg[$k] = $Overrides[$k] }
  return $cfg
}
