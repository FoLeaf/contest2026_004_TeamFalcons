# Diagnostic Modbus RTU slave for the existing 14-point room scene.
# Only replies to board FC03/FC04 reads; never initiates bus traffic.
#
# 控制文件（默认 ../.debug/maintenance-slave-mode.txt）按行解释，可以一次写多行。
# 文件内容或写入时间变化时才重新执行一遍，所以同一批命令重新写入会再次生效。
#
#   normal            恢复基线读数、清掉覆盖值与静默（默认行为不变）
#   warn              等价于 normal 后再把 1 号从站寄存器 0 置 300（30.0 C）
#   critical          等价于 normal 后再把 1 号从站寄存器 0 置 360（36.0 C）
#   offline           抑制全部从站的应答
#   offline1          只抑制 1 号从站（兼容旧脚本）
#   offline <列表>     只抑制列出的从站，例如 offline 5 或 offline 2,3,5
#   value <从站> <寄存器> <原始值>   覆盖一个寄存器的 16 位原始值
#   clear <从站> <寄存器>           去掉该寄存器的覆盖值
#   drop <从站> <寄存器> <次数>      丢掉接下来若干次对该寄存器的应答后自动恢复
#   reset             回到基线：清覆盖值、清静默、清 drop 计数
#   note <文本>        只写一行日志，用来标记工况切换时刻
#   stop              退出
#
# 原始值是 16 位寄存器值，不是显示值：显示值 = 原始值 * 点表 scale。
# 带符号点按 int16 解释，写 65436 等同于 -100。
#
# 用法示例：
#   powershell -File scripts/maintenance_rtu_slave.ps1 -ComPort COM4
#   powershell -File scripts/maintenance_rtu_slave.ps1 -SelfTest        # 离线自检，不开串口
param(
  [string]$ComPort = 'COM4',
  [string]$ControlPath = '',
  [string]$LogPath = '',
  [switch]$EnableRts,
  [switch]$EnableDtr,
  [ValidateRange(1,168)][int]$Hours = 24,
  # Default to prompt replies so tests can expose receive timing defects.
  # Nonzero delays are only for explicit timing/timeout experiments.
  [ValidateRange(0,1000)][int]$ReplyDelayMs = 0,
  # 每次请求写一行 REQ 日志，供板验时和板端事件时间对照。
  [switch]$RequestLog,
  # 只跑离线自检：验证 CRC、帧解析、控制语法、静默、drop 与工况数据，不开串口。
  [switch]$SelfTest,
  [string]$FixturePath = ''
)
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.Encoding]::UTF8
if (!$ControlPath) { $ControlPath = Join-Path $PSScriptRoot '../.debug/maintenance-slave-mode.txt' }
if (!$LogPath) { $LogPath = Join-Path $PSScriptRoot '../.debug/maintenance-slave.log' }
if (!$FixturePath) { $FixturePath = Join-Path $PSScriptRoot 'alarm_scenarios/scenarios.json' }
$ControlPath = [IO.Path]::GetFullPath($ControlPath)
$LogPath = [IO.Path]::GetFullPath($LogPath)

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Ports;
using System.Diagnostics;
using System.Text;
using System.Threading;

public class VelaSlaveState
{
  public string Mode = "normal";
  public bool SilentAll;
  public HashSet<int> Silent = new HashSet<int>();
  public Dictionary<int,int> Overrides = new Dictionary<int,int>();
  public Dictionary<int,int> Drops = new Dictionary<int,int>();
}

public static class VelaMaintenanceSlave
{
  // 基线读数：与 14 点房间场景点表一致，原始值而非显示值。
  static readonly Dictionary<int,int> Base = new Dictionary<int,int> {
    { (1<<16)|0, 240 },  { (1<<16)|1, 500 },
    { (2<<16)|2, 0 },
    { (3<<16)|0, 10 },
    { (4<<16)|0, 250 },  { (4<<16)|1, 240 },  { (4<<16)|2, 1 },
    { (5<<16)|0, 400 },  { (5<<16)|1, 80 },   { (5<<16)|2, 0 },
    { (6<<16)|0, 2300 }, { (6<<16)|1, 300 },  { (6<<16)|2, 1234 },
    { (7<<16)|0, 0 },
  };

  static int Key(int slave, int reg) { return (slave << 16) | (reg & 0xFFFF); }

  public static ushort Crc(byte[] a, int n)
  {
    ushort c = 0xffff;
    for (int i = 0; i < n; i++)
    {
      c ^= a[i];
      for (int j = 0; j < 8; j++)
        c = (ushort)(((c & 1) != 0) ? (c >> 1) ^ 0xa001 : c >> 1);
    }
    return c;
  }

  public static VelaSlaveState NewState() { return new VelaSlaveState(); }

  public static int Value(VelaSlaveState st, int slave, int reg)
  {
    int v;
    if (st.Overrides.TryGetValue(Key(slave, reg), out v)) return v;
    if (slave == 1 && reg == 0)
    {
      if (st.Mode == "critical") return 360;
      if (st.Mode == "warn") return 300;
    }
    if (Base.TryGetValue(Key(slave, reg), out v)) return v;
    return -1;
  }

  public static bool IsSilent(VelaSlaveState st, int slave)
  {
    return st.SilentAll || st.Silent.Contains(slave);
  }

  // 消耗一次 drop 计数；返回 true 表示这次请求应被丢弃。
  public static bool TakeDrop(VelaSlaveState st, int slave, int reg)
  {
    int k = Key(slave, reg);
    int left;
    if (!st.Drops.TryGetValue(k, out left) || left <= 0) return false;
    if (left == 1) st.Drops.Remove(k); else st.Drops[k] = left - 1;
    return true;
  }

  public static int DropLeft(VelaSlaveState st, int slave, int reg)
  {
    int left;
    return st.Drops.TryGetValue(Key(slave, reg), out left) ? left : 0;
  }

  static void ParseSlaveList(string text, HashSet<int> into)
  {
    into.Clear();
    foreach (string part in text.Split(','))
    {
      string t = part.Trim();
      if (t.Length == 0) continue;
      int a;
      if (!int.TryParse(t, NumberStyles.Integer, CultureInfo.InvariantCulture, out a)) continue;
      if (a >= 1 && a <= 247) into.Add(a);
    }
  }

  // 执行一行控制命令，返回日志用的效果说明；无法识别返回 null。
  public static string ApplyControl(VelaSlaveState st, string raw)
  {
    string line = (raw == null) ? "" : raw.Trim();
    // 编辑器可能给控制文件加 UTF-8 BOM，第一行会带上 \uFEFF
    if (line.Length > 0 && line[0] == '\uFEFF') line = line.Substring(1).Trim();
    if (line.Length == 0) return "";
    string[] parts = line.Split(new char[] { ' ', '\t' }, 2,
                                StringSplitOptions.RemoveEmptyEntries);
    string verb = parts[0].ToLowerInvariant();
    string rest = (parts.Length > 1) ? parts[1].Trim() : "";

    if (verb == "stop") return "stop";

    if (verb == "normal" || verb == "warn" || verb == "critical")
    {
      st.Mode = verb;
      st.Overrides.Clear();
      st.SilentAll = false;
      st.Silent.Clear();
      return "mode=" + verb;
    }

    if (verb == "reset")
    {
      st.Mode = "normal";
      st.Overrides.Clear();
      st.SilentAll = false;
      st.Silent.Clear();
      st.Drops.Clear();
      return "reset";
    }

    if (verb == "offline" || verb == "offline1")
    {
      if (verb == "offline1")
      {
        st.SilentAll = false;
        st.Silent.Clear();
        st.Silent.Add(1);
        return "silent=1";
      }
      if (rest.Length == 0 || rest.Equals("all", StringComparison.OrdinalIgnoreCase))
      {
        st.SilentAll = true;
        st.Silent.Clear();
        return "silent=all";
      }
      st.SilentAll = false;
      ParseSlaveList(rest, st.Silent);
      var list = new List<string>();
      foreach (int a in st.Silent) list.Add(a.ToString(CultureInfo.InvariantCulture));
      list.Sort();
      return "silent=" + string.Join(",", list.ToArray());
    }

    if (verb == "value" || verb == "drop")
    {
      string[] f = rest.Split(new char[] { ' ', '\t' },
                              StringSplitOptions.RemoveEmptyEntries);
      if (f.Length != 3) return null;
      int slave, reg, num;
      if (!int.TryParse(f[0], NumberStyles.Integer, CultureInfo.InvariantCulture, out slave)) return null;
      if (!int.TryParse(f[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out reg)) return null;
      if (!int.TryParse(f[2], NumberStyles.Integer, CultureInfo.InvariantCulture, out num)) return null;
      if (slave < 1 || slave > 247 || reg < 0 || reg > 65535) return null;
      if (verb == "value")
      {
        if (num < -32768 || num > 65535) return null;
        st.Overrides[Key(slave, reg)] = num & 0xFFFF;
        return string.Format(CultureInfo.InvariantCulture,
                             "value slave={0} reg={1} raw={2}", slave, reg, num & 0xFFFF);
      }
      if (num < 0 || num > 1000000) return null;
      st.Drops[Key(slave, reg)] = num;
      return string.Format(CultureInfo.InvariantCulture,
                           "drop slave={0} reg={1} n={2}", slave, reg, num);
    }

    if (verb == "clear")
    {
      string[] f = rest.Split(new char[] { ' ', '\t' },
                              StringSplitOptions.RemoveEmptyEntries);
      if (f.Length != 2) return null;
      int slave, reg;
      if (!int.TryParse(f[0], NumberStyles.Integer, CultureInfo.InvariantCulture, out slave)) return null;
      if (!int.TryParse(f[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out reg)) return null;
      st.Overrides.Remove(Key(slave, reg));
      st.Drops.Remove(Key(slave, reg));
      return string.Format(CultureInfo.InvariantCulture,
                           "clear slave={0} reg={1}", slave, reg);
    }

    if (verb == "note") return "note " + rest;

    return null;
  }

  // 8 字节请求帧解析（FC03/FC04，含 CRC 校验）。
  public static bool TryParseRequest(byte[] buf, int len, out int slave,
                                     out int fc, out int reg, out int qty)
  {
    slave = fc = reg = qty = 0;
    if (buf == null || len < 8) return false;
    ushort crc = Crc(buf, 6);
    if (buf[6] != (byte)crc || buf[7] != (byte)(crc >> 8)) return false;
    slave = buf[0];
    fc = buf[1];
    reg = (buf[2] << 8) | buf[3];
    qty = (buf[4] << 8) | buf[5];
    return true;
  }

  // 按当前状态构造应答；error 非 0 时是 Modbus 异常应答。
  public static byte[] BuildResponse(VelaSlaveState st, int slave, int fc,
                                     int reg, int qty, out int error)
  {
    error = 0;
    if (fc != 3 && fc != 4) error = 1;
    else if (qty < 1 || qty > 125) error = 3;
    if (error == 0)
    {
      for (int i = 0; i < qty; i++)
      {
        if (Value(st, slave, reg + i) < 0) { error = 2; break; }
      }
    }

    byte[] response = (error != 0) ? new byte[] { (byte)slave, (byte)(fc | 0x80), (byte)error, 0, 0 }
                                   : new byte[5 + qty * 2];
    if (error == 0)
    {
      response[0] = (byte)slave;
      response[1] = (byte)fc;
      response[2] = (byte)(qty * 2);
      for (int i = 0; i < qty; i++)
      {
        int v = Value(st, slave, reg + i);
        response[3 + i * 2] = (byte)(v >> 8);
        response[4 + i * 2] = (byte)v;
      }
    }
    ushort crc = Crc(response, response.Length - 2);
    response[response.Length - 2] = (byte)crc;
    response[response.Length - 1] = (byte)(crc >> 8);
    return response;
  }

  public static string DescribeSilent(VelaSlaveState st)
  {
    if (st.SilentAll) return "all";
    if (st.Silent.Count == 0) return "-";
    var list = new List<string>();
    foreach (int a in st.Silent) list.Add(a.ToString(CultureInfo.InvariantCulture));
    list.Sort();
    return string.Join(",", list.ToArray());
  }

  public static void Run(string port, string control, string log, int hours,
                         bool rts, bool dtr, int replyDelayMs, bool requestLog)
  {
    using (var w = new StreamWriter(log, true, new System.Text.UTF8Encoding(false)))
    using (var p = new SerialPort(port, 9600, Parity.None, 8, StopBits.One))
    {
      w.AutoFlush = true; p.ReadTimeout = 100; p.WriteTimeout = 1000;
      p.RtsEnable = rts; p.DtrEnable = dtr; p.Open();
      w.WriteLine("START {0:o} port={1} mode=slave-only baud=9600 rts={2} dtr={3} reply_delay_ms={4} request_log={5}",
                  DateTime.Now, port, rts, dtr, replyDelayMs, requestLog);

      var timer = Stopwatch.StartNew();
      var rx = new List<byte>();
      var st = NewState();
      string lastControl = null;
      DateTime lastWrite = DateTime.MinValue;
      long requests = 0, replies = 0, ignored = 0, bad = 0, dropped = 0;
      int rawBudget = 256;
      long next = 0, refresh = 0;

      try
      {
        while (timer.Elapsed.TotalHours < hours)
        {
          if (timer.ElapsedMilliseconds >= refresh)
          {
            try
            {
              string body = File.ReadAllText(control);
              DateTime written = File.GetLastWriteTime(control);
              if (body != lastControl || written != lastWrite)
              {
                lastControl = body; lastWrite = written;
                string modeBefore = st.Mode;
                bool stop = false;
                foreach (string raw in body.Replace("\r\n", "\n").Split('\n'))
                {
                  string effect = ApplyControl(st, raw);
                  if (effect == null)
                  {
                    w.WriteLine("CONTROL_ERR {0:o} line=\"{1}\"", DateTime.Now, raw.Trim());
                    continue;
                  }
                  if (effect.Length == 0) continue;
                  w.WriteLine("CONTROL {0:o} written={1:o} line=\"{2}\" effect={3}",
                              DateTime.Now, written, raw.Trim(), effect);
                  if (effect == "stop") { stop = true; break; }
                }
                if (stop) break;
                if (st.Mode != modeBefore) w.WriteLine("MODE {0:o} {1}", DateTime.Now, st.Mode);
              }
            }
            catch (IOException) { }
            refresh = timer.ElapsedMilliseconds + 250;
          }

          int available = p.BytesToRead;
          if (available > 0)
          {
            byte[] chunk = new byte[available];
            int n = p.Read(chunk, 0, chunk.Length);
            if (rawBudget > 0)
            {
              int sample = Math.Min(rawBudget, n);
              w.WriteLine("RX {0:o} {1}", DateTime.Now, BitConverter.ToString(chunk, 0, sample));
              rawBudget -= sample;
            }
            for (int i = 0; i < n; i++) rx.Add(chunk[i]);
          }

          while (rx.Count >= 8)
          {
            byte[] q = rx.GetRange(0, 8).ToArray();
            int s, fc, reg, qty;
            if (!TryParseRequest(q, 8, out s, out fc, out reg, out qty))
            {
              rx.RemoveAt(0); bad++; continue;
            }
            rx.RemoveRange(0, 8); requests++;

            if (s < 1 || s > 247 || IsSilent(st, s))
            {
              ignored++;
              if (requestLog)
                w.WriteLine("REQ {0:o} slave={1} fc={2} reg={3} qty={4} action=suppress",
                            DateTime.Now, s, fc, reg, qty);
              continue;
            }

            if (TakeDrop(st, s, reg))
            {
              dropped++;
              if (requestLog)
                w.WriteLine("REQ {0:o} slave={1} fc={2} reg={3} qty={4} action=drop remaining={5}",
                            DateTime.Now, s, fc, reg, qty, DropLeft(st, s, reg));
              continue;
            }

            int error;
            byte[] response = BuildResponse(st, s, fc, reg, qty, out error);
            if (replyDelayMs > 0) { Thread.Sleep(replyDelayMs); }
            p.Write(response, 0, response.Length); replies++;
            if (requestLog)
            {
              w.WriteLine("REQ {0:o} slave={1} fc={2} reg={3} qty={4} action=reply error={5} value={6}",
                          DateTime.Now, s, fc, reg, qty, error,
                          error == 0 ? Value(st, s, reg).ToString(CultureInfo.InvariantCulture) : "-");
            }
          }

          if (timer.ElapsedMilliseconds >= next)
          {
            w.WriteLine("STATUS {0:o} elapsed_s={1:F1} mode={2} requests={3} replies={4} suppressed={5} crc_scan={6} dropped={7} silent={8} overrides={9} drops={10}",
                        DateTime.Now, timer.Elapsed.TotalSeconds, st.Mode, requests, replies,
                        ignored, bad, dropped, DescribeSilent(st), st.Overrides.Count, st.Drops.Count);
            next = timer.ElapsedMilliseconds + 10000;
          }

          Thread.Sleep(2);
        }
      }
      finally
      {
        w.WriteLine("STOP {0:o} requests={1} replies={2} suppressed={3} crc_scan={4} dropped={5}",
                    DateTime.Now, requests, replies, ignored, bad, dropped);
      }
    }
  }
}
'@

function Invoke-SlaveSelfTest {
  param([string]$Fixture)
  $script:SelfTestFail = 0
  function Check([bool]$cond, [string]$what) {
    if ($cond) { Write-Host ("PASS {0}" -f $what) }
    else { Write-Host ("FAIL {0}" -f $what); $script:SelfTestFail++ }
  }

  # 1. CRC 向量（由独立的 Python 实现算出，不是本脚本自己推的）
  $vectors = @(
    @{ name = 'req s1 fc3 reg0 qty1'; bytes = @(0x01,0x03,0x00,0x00,0x00,0x01); crc = 0x0A84 },
    @{ name = 'req s1 fc3 reg1 qty1'; bytes = @(0x01,0x03,0x00,0x01,0x00,0x01); crc = 0xCAD5 },
    @{ name = 'req s2 fc3 reg2 qty1'; bytes = @(0x02,0x03,0x00,0x02,0x00,0x01); crc = 0xF925 },
    @{ name = 'req s5 fc3 reg1 qty1'; bytes = @(0x05,0x03,0x00,0x01,0x00,0x01); crc = 0x4ED4 },
    @{ name = 'req s7 fc3 reg0 qty1'; bytes = @(0x07,0x03,0x00,0x00,0x00,0x01); crc = 0x6C84 },
    @{ name = 'req s1 fc4 reg0 qty1'; bytes = @(0x01,0x04,0x00,0x00,0x00,0x01); crc = 0xCA31 },
    @{ name = 'resp s1 temp=240';     bytes = @(0x01,0x03,0x02,0x00,0xF0);       crc = 0x00B8 },
    @{ name = 'resp s1 temp=280';     bytes = @(0x01,0x03,0x02,0x01,0x18);       crc = 0xDEB9 },
    @{ name = 'resp s7 door=1';       bytes = @(0x07,0x03,0x02,0x00,0x01);       crc = 0x84F1 }
  )
  foreach ($v in $vectors) {
    $b = [byte[]]$v.bytes
    $got = [VelaMaintenanceSlave]::Crc($b, $b.Length)
    Check ($got -eq $v.crc) ("CRC {0}: 0x{1:X4} 期望 0x{2:X4}" -f $v.name, $got, $v.crc)
  }

  # 2. 请求帧解析与地址匹配（CRC 错的帧必须被拒）
  $st = [VelaMaintenanceSlave]::NewState()
  foreach ($v in $vectors) {
    if ($v.name -notlike 'req *') { continue }
    $b = [byte[]]($v.bytes + @(($v.crc -band 0xFF), (($v.crc -shr 8) -band 0xFF)))
    $s = 0; $fc = 0; $reg = 0; $qty = 0
    $ok = [VelaMaintenanceSlave]::TryParseRequest($b, 8, [ref]$s, [ref]$fc, [ref]$reg, [ref]$qty)
    Check ($ok -and $s -eq $v.bytes[0] -and $fc -eq $v.bytes[1] -and
           $reg -eq (($v.bytes[2] -shl 8) -bor $v.bytes[3]) -and
           $qty -eq (($v.bytes[4] -shl 8) -bor $v.bytes[5])) ("解析 {0}" -f $v.name)
    $broken = [byte[]]$b.Clone()
    $broken[7] = ($broken[7] -bxor 0xFF)
    $s2 = 0; $fc2 = 0; $reg2 = 0; $qty2 = 0
    Check (-not [VelaMaintenanceSlave]::TryParseRequest($broken, 8, [ref]$s2, [ref]$fc2, [ref]$reg2, [ref]$qty2)) ("CRC 错帧被拒 {0}" -f $v.name)
  }

  # 3. 旧三档模式与 reset 的兼容行为
  $st = [VelaMaintenanceSlave]::NewState()
  [void][VelaMaintenanceSlave]::ApplyControl($st, 'normal')
  Check (([VelaMaintenanceSlave]::Value($st, 1, 0) -eq 240)) 'normal 模式温度 240'
  [void][VelaMaintenanceSlave]::ApplyControl($st, 'warn')
  Check (([VelaMaintenanceSlave]::Value($st, 1, 0) -eq 300)) 'warn 模式温度 300'
  [void][VelaMaintenanceSlave]::ApplyControl($st, 'critical')
  Check (([VelaMaintenanceSlave]::Value($st, 1, 0) -eq 360)) 'critical 模式温度 360'
  Check (([VelaMaintenanceSlave]::Value($st, 1, 1) -eq 500)) 'warn/critical 不影响其他寄存器'
  [void][VelaMaintenanceSlave]::ApplyControl($st, 'value 1 0 280')
  Check (([VelaMaintenanceSlave]::Value($st, 1, 0) -eq 280)) '覆盖值优先于模式'
  [void][VelaMaintenanceSlave]::ApplyControl($st, 'normal')
  Check (([VelaMaintenanceSlave]::Value($st, 1, 0) -eq 240)) 'normal 清掉覆盖值'
  [void][VelaMaintenanceSlave]::ApplyControl($st, 'value 4 0 65436')
  Check (([VelaMaintenanceSlave]::Value($st, 4, 0) -eq 65436)) '有符号点可写负值等价的原始值'
  [void][VelaMaintenanceSlave]::ApplyControl($st, 'reset')
  Check (([VelaMaintenanceSlave]::Value($st, 4, 0) -eq 250)) 'reset 清掉覆盖值'
  [void][VelaMaintenanceSlave]::ApplyControl($st, ([char]0xFEFF + 'value 1 0 281'))
  Check (([VelaMaintenanceSlave]::Value($st, 1, 0) -eq 281)) '控制文件带 BOM 时首行仍可识别'
  [void][VelaMaintenanceSlave]::ApplyControl($st, 'reset')

  # 4. 静默：选择性、全静默与恢复
  $st = [VelaMaintenanceSlave]::NewState()
  [void][VelaMaintenanceSlave]::ApplyControl($st, 'offline 5')
  Check (([VelaMaintenanceSlave]::IsSilent($st, 5)) -and -not ([VelaMaintenanceSlave]::IsSilent($st, 1)) -and
         -not ([VelaMaintenanceSlave]::IsSilent($st, 7))) 'offline 5 只静默 5 号从站'
  [void][VelaMaintenanceSlave]::ApplyControl($st, 'offline 2,3')
  Check (([VelaMaintenanceSlave]::IsSilent($st, 2)) -and ([VelaMaintenanceSlave]::IsSilent($st, 3)) -and
         -not ([VelaMaintenanceSlave]::IsSilent($st, 5))) 'offline 2,3 覆盖列表'
  [void][VelaMaintenanceSlave]::ApplyControl($st, 'offline1')
  Check (([VelaMaintenanceSlave]::IsSilent($st, 1)) -and -not ([VelaMaintenanceSlave]::IsSilent($st, 2))) 'offline1 只静默 1 号从站'
  [void][VelaMaintenanceSlave]::ApplyControl($st, 'offline')
  Check (([VelaMaintenanceSlave]::IsSilent($st, 1)) -and ([VelaMaintenanceSlave]::IsSilent($st, 7))) 'offline 静默全部'
  [void][VelaMaintenanceSlave]::ApplyControl($st, 'normal')
  Check (-not ([VelaMaintenanceSlave]::IsSilent($st, 1))) 'normal 取消静默'

  # 5. drop 计数：只丢指定寄存器、次数用尽后恢复
  $st = [VelaMaintenanceSlave]::NewState()
  [void][VelaMaintenanceSlave]::ApplyControl($st, 'drop 7 0 2')
  Check ([VelaMaintenanceSlave]::TakeDrop($st, 7, 0)) 'drop 第 1 次命中'
  Check ([VelaMaintenanceSlave]::TakeDrop($st, 7, 0)) 'drop 第 2 次命中'
  Check (-not [VelaMaintenanceSlave]::TakeDrop($st, 7, 0)) 'drop 用尽后不再丢'
  Check (-not [VelaMaintenanceSlave]::TakeDrop($st, 7, 1)) 'drop 不影响同从站其他寄存器'
  Check (-not [VelaMaintenanceSlave]::TakeDrop($st, 5, 0)) 'drop 不影响其他从站'
  [void][VelaMaintenanceSlave]::ApplyControl($st, 'drop 5 0 4')
  Check (([VelaMaintenanceSlave]::DropLeft($st, 5, 0) -eq 4)) 'drop 剩余次数可读'

  # 6. 非法控制行必须被拒且不改状态
  $st = [VelaMaintenanceSlave]::NewState()
  foreach ($bad in @('value 1 0', 'value 1 0 abc', 'value 0 0 1', 'value 1 0 99999',
                     'drop 1 0', 'clear 1', 'offline 999', 'gt 1 0 1')) {
    $r = [VelaMaintenanceSlave]::ApplyControl($st, $bad)
    if ($bad -eq 'offline 999') {
      Check ($r -ne $null -and -not ([VelaMaintenanceSlave]::IsSilent($st, 999))) ("非法行按空列表处理: {0}" -f $bad)
    } else {
      Check ($r -eq $null) ("非法控制行被拒: {0}" -f $bad)
    }
  }
  Check (([VelaMaintenanceSlave]::Value($st, 1, 0) -eq 240)) '非法控制行不改读数'

  # 7. 应答帧与点表原始值一致（数据来自生成的工况文件）
  if (!(Test-Path -LiteralPath $Fixture)) {
    Check $false ("工况文件存在: {0}" -f $Fixture)
  } else {
    # scenarios.json 是无 BOM 的 UTF-8，Windows PowerShell 5.1 默认按 ANSI 读会读坏
    $doc = Get-Content -Raw -Encoding UTF8 -LiteralPath $Fixture | ConvertFrom-Json
    $points = @{}
    foreach ($p in $doc.points) { $points[$p.id] = $p }
    $steps = 0
    foreach ($case in $doc.cases) {
      foreach ($step in $case.steps) {
        if ($step.control) {
          $st = [VelaMaintenanceSlave]::NewState()
          foreach ($line in ($step.control -split "`n")) {
            $effect = [VelaMaintenanceSlave]::ApplyControl($st, $line)
            if ($null -eq $effect) { Check $false ("控制行可解析 {0}: {1}" -f $case.id, $line) }
          }
          foreach ($e in $step.expects) {
            $p = $points[$e.point]
            $got = [VelaMaintenanceSlave]::Value($st, [int]$p.addr, [int]$p.reg)
            Check ($got -eq [int]$e.raw) ("{0}/{1}: {2} 读数 {3} 期望 {4}" -f $case.id, $step.label, $e.point, $got, $e.raw)
            $steps++
          }
        }
      }
    }
    Check ($steps -gt 0) ("工况文件里的原始值都核对过（{0} 项）" -f $steps)

    # 应答帧把原始值按大端写回，用点表基线抽查一条
    $st = [VelaMaintenanceSlave]::NewState()
    $err = 0
    $resp = [VelaMaintenanceSlave]::BuildResponse($st, 1, 3, 0, 1, [ref]$err)
    Check (($err -eq 0) -and ($resp.Length -eq 7) -and ($resp[3] -eq 0x00) -and ($resp[4] -eq 0xF0)) '温度基线应答帧为 0x00F0'
    $resp2 = [VelaMaintenanceSlave]::BuildResponse($st, 9, 3, 0, 1, [ref]$err)
    Check ($err -eq 2) '未映射寄存器回非法数据地址'
    $resp3 = [VelaMaintenanceSlave]::BuildResponse($st, 1, 6, 0, 1, [ref]$err)
    Check ($err -eq 1) '非 FC03/FC04 回非法功能码'
  }

  if ($script:SelfTestFail -eq 0) { Write-Host "SELF-TEST: ok" }
  else { Write-Host ("SELF-TEST: {0} fail(s)" -f $script:SelfTestFail) }
  return $script:SelfTestFail
}

if ($SelfTest) {
  $rc = Invoke-SlaveSelfTest -Fixture $FixturePath
  exit ([int]($rc -ne 0))
}

[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($LogPath)) | Out-Null
# Modes: normal, warn (temp=30 C), critical (temp=36 C), offline
# (all slave responses suppressed), offline1 (only slave 1 suppressed), stop.
# 另见文件头的控制文件语法：offline <列表>、value、clear、drop、reset、note。
if (!(Test-Path -LiteralPath $ControlPath)) { [IO.File]::WriteAllText($ControlPath,'normal') }
[VelaMaintenanceSlave]::Run($ComPort,$ControlPath,$LogPath,$Hours,$EnableRts.IsPresent,$EnableDtr.IsPresent,$ReplyDelayMs,$RequestLog.IsPresent)
