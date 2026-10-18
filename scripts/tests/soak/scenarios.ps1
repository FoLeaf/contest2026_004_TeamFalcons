# 离线长测场景表。每个场景只描述"板端怎么表现"和"期望得到什么判定"。
# 板端行为字段见 soak_fake.ps1 的 New-SoakScenario 默认值。
function Get-SoakScenarios {
  $common = @{ Hours = 0.02; IntervalSec = 1; MinimumOnline = 14; CommandTimeoutSec = 30; StoreRounds = 50 }
  return [ordered]@{

    'happy-path' = @{
      Note  = '全部检查通过：分片读取、提示符后异步日志、点表不变、在线 14、堆自洽。'
      Soak  = $common
      Board = @{}
      Expect = @{ Verdict = 'PASS'; ExitCode = 0; MinCycles = 2; NoFail = $true }
    }

    'prompt-then-async-one-read' = @{
      Note  = '整个响应在一次读取里到达，提示符后面紧跟异步日志。旧脚本按"文本以提示符结尾"判断会超时。'
      Soak  = $common
      Board = @{ ChunkSize = 8192; AsyncAfterPrompt = $true }
      Expect = @{ Verdict = 'PASS'; ExitCode = 0; MinCycles = 2 }
    }

    'ansi-erase-suffix' = @{
      Note  = '每行末尾都带 ESC[K（板端提示符实际形态）。解析前不剥转义会让点表行无法识别。'
      Soak  = $common
      Board = @{ EscapeSuffix = $true; ChunkSize = 64 }
      Expect = @{ Verdict = 'PASS'; ExitCode = 0; MinCycles = 2 }
    }

    'no-async-after-prompt' = @{
      Note  = '不交错异步日志的对照场景。'
      Soak  = $common
      Board = @{ ChunkSize = 4096; AsyncAfterPrompt = $false }
      Expect = @{ Verdict = 'PASS'; ExitCode = 0; MinCycles = 2 }
    }

    'start-failure' = @{
      Note  = '板端不在 NSH（同步命令没有提示符），启动阶段就失败。'
      Soak  = @{ Hours = 0.02; IntervalSec = 1; MinimumOnline = 14; CommandTimeoutSec = 10; StoreRounds = 50 }
      Board = @{ StallCommands = @('^$') }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'Command timeout'; NoPass = $true }
    }

    'command-timeout' = @{
      Note  = '运行中 vgagent status 挂住不回，命令超时。'
      Soak  = @{ Hours = 0.02; IntervalSec = 1; MinimumOnline = 14; CommandTimeoutSec = 10; StoreRounds = 50 }
      Board = @{ StallCommands = @('^vgagent status$'); StallFromCycle = 2 }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'Command timeout'; NoPass = $true }
    }

    'link-drop' = @{
      Note  = 'USB 串口中途断开：写命令时抛"端口被关闭"。'
      Soak  = $common
      Board = @{ DropLinkAtCycle = 2 }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'Serial link lost'; NoPass = $true }
    }

    'firmware-fault' = @{
      Note  = '板端 HardFault 转储。'
      Soak  = $common
      Board = @{ FaultAtCycle = 2 }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'Firmware failure detected'; NoPass = $true }
    }

    'panic-split-command' = @{
      Note  = 'PANIC 在命令响应的多次读取之间被拆开；只看单个 chunk 会漏掉。'
      Soak  = $common
      Board = @{
        FaultAtCycle = 2
        FaultMode = 'panic'
        SplitNeedle = 'PANIC'
        SplitAt = 2
      }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'Firmware failure detected'; NoPass = $true }
    }

    'fault-split-clear' = @{
      Note  = 'HARDFAULT 在命令下发前排空缓冲的多次读取之间被拆开。'
      Soak  = $common
      Board = @{
        FaultInjectionAtCycle = 2
        FaultInjectionContext = '^before '
        FaultInjectionText = 'HARDFAULT: split across clear reads'
        SplitNeedle = 'HARDFAULT'
        SplitAt = 4
      }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'Firmware failure detected'; NoPass = $true }
    }

    'fault-split-wait' = @{
      Note  = 'HARDFAULT 在轮次等待的多次读取之间被拆开。'
      Soak  = @{ Hours = 0.02; IntervalSec = 10; MinimumOnline = 14; CommandTimeoutSec = 30; StoreRounds = 50 }
      Board = @{
        FaultInjectionAtCycle = 1
        FaultInjectionContext = '^interval wait$'
        FaultInjectionText = 'HARDFAULT: split across interval reads'
        SplitNeedle = 'HARDFAULT'
        SplitAt = 4
      }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'Firmware failure detected'; NoPass = $true }
    }

    'unexpected-reboot' = @{
      Note  = '命令输出里出现启动横幅。'
      Soak  = $common
      Board = @{ RebootAtCycle = 2 }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'boot banner'; NoPass = $true }
    }

    'reboot-split-command' = @{
      Note  = '启动横幅在命令响应的多次读取之间被拆开；只看单个 chunk 会漏掉。'
      Soak  = $common
      Board = @{
        RebootAtCycle = 2
        RebootBannerText = 'NuttShell (NSH)'
        SplitNeedle = 'NuttShell (NSH)'
        SplitAt = 6
      }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'boot banner'; NoPass = $true }
    }

    'reboot-split-clear' = @{
      Note  = '启动横幅在命令下发前排空缓冲的多次读取之间被拆开。'
      Soak  = $common
      Board = @{
        FaultInjectionAtCycle = 2
        FaultInjectionContext = '^before '
        FaultInjectionText = 'NuttShell (NSH): reboot split across clear reads'
        SplitNeedle = 'NuttShell (NSH)'
        SplitAt = 6
      }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'boot banner'; NoPass = $true }
    }

    'reboot-split-wait' = @{
      Note  = '启动横幅在轮次等待的多次读取之间被拆开。'
      Soak  = @{ Hours = 0.02; IntervalSec = 10; MinimumOnline = 14; CommandTimeoutSec = 30; StoreRounds = 50 }
      Board = @{
        FaultInjectionAtCycle = 1
        FaultInjectionContext = '^interval wait$'
        FaultInjectionText = 'NuttShell (NSH): reboot split across interval reads'
        SplitNeedle = 'NuttShell (NSH)'
        SplitAt = 6
      }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'boot banner'; NoPass = $true }
    }

    'uptime-regression' = @{
      Note  = 'uptime_s 回退，说明板端重启过。'
      Soak  = $common
      Board = @{ UptimeRegressionAtCycle = 2 }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'uptime decreased'; NoPass = $true }
    }

    'table-change' = @{
      Note  = '已确认点表在长测中被改写。'
      Soak  = $common
      Board = @{ TableChangeAtCycle = 2 }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'Committed table changed'; NoPass = $true }
    }

    'online-short' = @{
      Note  = '在线点数低于 MinimumOnline。'
      Soak  = $common
      Board = @{ OnlineAfterCycle = @{ 2 = 13 } }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'Only 13 of 14 points online'; NoPass = $true }
    }

    'heap-inconsistent' = @{
      Note  = '堆统计自相矛盾（used + free 与 total 不符）。'
      Soak  = $common
      Board = @{ HeapFaultAtCycle = 2 }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'Heap accounting is inconsistent'; NoPass = $true }
    }

    'credentials-missing' = @{
      Note  = 'LLM 凭证未就绪。'
      Soak  = $common
      Board = @{ Credentials = 'missing'; ProvisionFile = 'absent' }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'credentials are not provisioned'; NoPass = $true }
    }

    'stopped-before-duration' = @{
      Note  = '运行到一半失败，未满时长，不得写出 PASS。'
      Soak  = @{ Hours = 1; IntervalSec = 1; MinimumOnline = 14; CommandTimeoutSec = 30; StoreRounds = 50 }
      Board = @{ HeapFaultAtCycle = 3 }
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; NoPass = $true }
    }

    'simulated-interrupt' = @{
      Note  = '模拟用户中断：只写 ABORT 与 RUN_END status=INCOMPLETE。'
      Soak  = @{ Hours = 1; IntervalSec = 1; MinimumOnline = 14; CommandTimeoutSec = 30; StoreRounds = 50 }
      Board = @{ StopAtCycle = 2 }
      Expect = @{ Verdict = 'INCOMPLETE'; ExitCode = 2; NoPass = $true }
    }

    'port-unavailable' = @{
      Note  = '不注入假串口：走真实启动路径，串口不存在时必须在打开端口前失败。'
      Soak  = @{ Hours = 0.01; IntervalSec = 1; MinimumOnline = 14; CommandTimeoutSec = 10; StoreRounds = 50 }
      Board = @{}
      NoTestRuntime = $true
      ComPort = 'COM_NOT_A_REAL_PORT'
      Expect = @{ Verdict = 'FAIL'; ExitCode = 1; ReasonContains = 'unavailable'; NoPass = $true }
    }

    'slow-run' = @{
      Note  = '不用于判定断言：把虚拟时钟挂上少量真实等待，供"进程被强制结束"的用例中途杀死。'
      Soak  = @{ Hours = 1; IntervalSec = 1; MinimumOnline = 14; CommandTimeoutSec = 30; StoreRounds = 50 }
      Board = @{ RealSleepMs = 5 }
      Expect = @{ Verdict = 'ANY' }
    }

    'online-min-default' = @{
      Note  = 'MinimumOnline=0 且全部离线仍然 PASS，同时留下 WARN。这是默认值的实际效果。'
      Soak  = @{ Hours = 0.02; IntervalSec = 1; MinimumOnline = 0; CommandTimeoutSec = 30; StoreRounds = 50 }
      Board = @{ OnlineAfterCycle = @{ 0 = 0 } }
      Expect = @{ Verdict = 'PASS'; ExitCode = 0; MinCycles = 2; WarnContains = 'online_min=0' }
    }
  }
}
