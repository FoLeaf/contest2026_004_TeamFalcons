# 24 小时长测运行说明（scripts/maintenance_soak.ps1）

更新日期：2026-10-18。对象是 `scripts/maintenance_soak.ps1`，用于在真实开发板上跑串口长测。
本文说明判定规则、正式运行的命令模板、启动前条件，以及脚本无法证明的事项。
离线测试见 `scripts/tests/soak/`。

## 1. 覆盖范围

脚本实际执行的检查，写在每次运行的 `CHECKS` 行里：

| 检查 | 判据 |
|---|---|
| `store-probe` | 每轮 `vgstoretest <rounds>` 返回 `vgstoretest: OK` |
| `committed-table-stable` | 每轮 `vgpoint list` 的已确认点表与开场基线逐行一致 |
| `credentials-provisioned` | `vgagent status` 的 `llm: credentials=ready` |
| `uptime-monotonic` | `vgruntime dump` 的 `uptime_s` 不回落 |
| `online-min` | 在线点数不少于 `-MinimumOnline` |
| `heap-consistency` | `/proc/meminfo` 的 `used + free == total` 且最大空闲块不超过总空闲 |

同一行里的 `not_checked=` 列出本轮没有验证的内容：告警与阈值、MQTT 上报、LLM 实际调用结果、日报落盘、语音、屏幕显示。
`credentials=ready` 只表示凭证已下发，不代表 LLM 调用成功；在线点数只表示采集在跑，不代表告警或上报正常。

## 2. 判定规则

每次运行写一对标记：

```
RUN_BEGIN run=<id> ...
...
RUN_END run=<id> status=PASS|FAIL|INCOMPLETE
```

- **通过**：同一 `run=<id>` 同时存在 `PASS ... status=PASS` 行和 `RUN_END ... status=PASS` 行。
- **失败**：存在 `FAIL ... status=FAIL` 行和对应的 `RUN_END status=FAIL`。
- **结果不完整**：只有 `RUN_BEGIN` 没有 `RUN_END`（终端被关、进程被杀、主机掉电），或者写了 `ABORT ... status=INCOMPLETE` 加 `RUN_END status=INCOMPLETE`（用户中断）。

日志是追加写的。**缺少 `RUN_END` 的运行一律视为结果不完整，不得继承同一文件里更早的 `PASS`。**
判断某次运行是否通过时，先按 `run=` 找到对应的 `RUN_BEGIN`，再找同 id 的 `RUN_END`。

退出码：`0` = PASS，`1` = FAIL，`2` = 结果不完整。中断时 PowerShell 会停掉管道，退出码由 `finally` 给出，日志记录始终可靠。

## 3. 日志行

| 行 | 内容 |
|---|---|
| `RUN_BEGIN` | 运行标识、脚本版本、开始时间、主机名、日志路径 |
| `START` | 串口、时长、间隔、存储轮数、在线下限、命令超时、范围 |
| `CHECKS` | 本轮实际检查项与未检查项 |
| `WARN` | 参数提醒（例如 `online_min=0`） |
| `TEST_MODE` | 仅离线测试出现，表示这一份日志不是板端证据 |
| `FIRMWARE` | `uname -a` 的固件身份 |
| `BASELINE` / `BASELINE_ROW` | 开场已确认点表 |
| `CYCLE` | 每轮耗时、`uptime_s`、在线数、堆用量 |
| `TABLE_AFTER` / `TABLE_AFTER_ROW` | 点表变化时的现场表 |
| `COMMAND` / `COMMAND_MS` | 每条下发命令与耗时 |
| `PASS` / `FAIL` / `ABORT` / `RUN_END` | 结论 |

`COMMAND_MS` 可用于复核命令耗时。归档的旧固件长测里 `vgstoretest 50` 平均 78.5 秒、最大 85.7 秒，
默认 120 秒超时余量偏小，正式运行建议放宽（见下）。

## 4. 正式 24 小时长测

### 4.1 启动前条件

1. 板子已烧入待测固件并启动到 NSH，串口在 **COM3**。
2. 模拟从站在 **COM7** 运行，`-ReplyDelayMs 0`，且该工具的主动采集会话处于关闭状态；
   同一 RS485 总线上不得存在第二个 Modbus 主站。
3. 已确认点表为预期的 14 个点位：`vgpoint list` 显示 `table=committed n=14`。
4. LLM 凭证已下发：`vgagent status` 显示 `llm: credentials=ready`。
5. COM3 没有被其他进程占用（历史上有 `scomm` 自动抢占的情况）。
6. 承载长测的终端在整个 24 小时内保持打开。2026-10-17 那轮旧固件长测就是终端会话关闭导致 COM3 失效，
   在第 3 小时 13 分失败归档。更稳的做法是在 Windows 上用独立窗口或计划任务托管，不要复用交互窗口。
7. 磁盘留出足够空间：日志包含全部命令输出和后台日志。归档的旧固件长测是 57 轮 0.58 MB，
   本轮脚本还会记录等待间隔内到达的后台日志，24 小时按 100 MB 以上预留更保险。

### 4.2 命令模板

从仓库根目录运行。`-LogPath` 用独立文件，避免多次运行混在一个日志里。

在 Windows PowerShell 里：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\maintenance_soak.ps1 `
  -ComPort COM3 -Hours 24 -IntervalSec 60 -StoreRounds 50 `
  -MinimumOnline 14 -CommandTimeoutSec 240 `
  -LogPath .debug\soak-20261018-fw75cbc8.log
```

在本机 WSL 里，用绝对路径的 `powershell.exe` 加 UNC 路径调用同一个脚本：

```bash
/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -ExecutionPolicy Bypass -File \
  '\\wsl.localhost\Debian\home\hello19y\openvela\contest2026_004_TeamFalcons\scripts\maintenance_soak.ps1' \
  -ComPort COM3 -Hours 24 -IntervalSec 60 -StoreRounds 50 -MinimumOnline 14 -CommandTimeoutSec 240 \
  -LogPath '\\wsl.localhost\Debian\home\hello19y\openvela\contest2026_004_TeamFalcons\.debug\soak-20261018-fw75cbc8.log'
```

从站单独一个窗口：

```bash
/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -ExecutionPolicy Bypass -File \
  '\\wsl.localhost\Debian\home\hello19y\openvela\contest2026_004_TeamFalcons\scripts\maintenance_rtu_slave.ps1' \
  -ComPort COM7 -ReplyDelayMs 0 -Hours 30 -LogPath '\\wsl.localhost\Debian\home\hello19y\openvela\contest2026_004_TeamFalcons\.debug\soak-20261018-slave.log'
```

### 4.3 参数说明

| 参数 | 正式运行取值 | 说明 |
|---|---|---|
| `-ComPort` | `COM3` | 板端 NSH 串口 |
| `-Hours` | `24` | 要求时长；PASS 行的 `elapsed_s` 不会小于 `required_s` |
| `-IntervalSec` | `60` | 轮次间隔 |
| `-StoreRounds` | `50` | 与历史长测一致，单次约 80 到 90 秒 |
| `-MinimumOnline` | `14` | **默认值是 0，会把整条总线全部离线也判为通过**，正式运行必须显式给出 |
| `-CommandTimeoutSec` | `240` | 默认 120 秒对 `vgstoretest 50` 余量偏小 |
| `-LogPath` | 每次运行独立 | 默认值 `.debug/maintenance-soak.log` 会被多次运行追加 |
| `-RunId` | 留空 | 默认由时间戳加随机后缀生成 |

`-MinimumOnline 0` 时脚本会写一条 `WARN`，提醒这个取值不检测全部离线。这只是提醒，不改变判定。

## 5. 结束后怎么读结论

1. 在日志里找最后一次运行的 `RUN_BEGIN`，记下 `run=`。
2. 找同 `run=` 的 `RUN_END`。没有就是结果不完整。
3. `RUN_END status=PASS` 时核对 `PASS` 行的 `cycles`、`elapsed_s`、`required_s`、`wall_elapsed_s` 与 `online_min`。
4. `FAIL` 或 `ABORT` 时看 `reason=`，并按 `TABLE_AFTER` / 崩溃转储等上下文定位。
5. 把 `RUN_BEGIN` 到 `RUN_END` 的结论行摘录进 `docs/maintenance-evidence/<日期>/`，完整日志留在 `.debug/`（该目录不入库）。

## 6. 脚本无法证明的事项

这些需要人工配套观察，脚本不会声称覆盖：

- **从站侧计数**：`crc_scan`、`suppressed`、未回复请求数来自从站日志，长测期间要另行核对。
  归档的旧长测里 `crc_scan=7` 全部集中在开场 10 秒，属于串口刚打开时的残余字节。
- **尾部杂字节**：没有从站日志接口就无法区分是总线残留还是真实线路错误。
- **屏幕**：告警页、离线显示、LVGL 是否卡死需要目视。
- **AI 与上报**：LLM 实际调用结果、日报落盘、MQTT 上报内容都不在范围内。
- **告警与阈值**：`-MinimumOnline` 只统计在线点数，不检查告警是否触发。
- **复位原因**：`uptime_s` 回落能发现重启，`boot_total` 只作记录。脚本能发现重启，
  但无法说明重启原因；历史 QA-02 的两个旧重启仍无复位原因寄存器或供电波形证据。
- **恢复能力**：脚本遇到异常就停止，不自动复位、不自动恢复。故障恢复属于其他验收项。

## 7. 离线测试

`scripts/tests/soak/` 下的用例全部使用假串口和虚拟时钟，不打开真实串口、不连 broker、不碰开发板，
也不实际等待 24 小时。每个用例在独立子进程里运行，日志写到 `.debug/soak-tests/<时间戳>/<用例>/`。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\tests\soak\test_soak.ps1
```

覆盖的用例：正常完成、未满时长、启动失败、命令超时、串口断开、固件崩溃输出、异常重启、
uptime 回退、点表变化、在线不足、堆统计异常、凭证缺失、分片与交错输出、提示符后跟异步日志、
ANSI 转义、模拟中断、进程被强制结束、追加到旧日志后不得继承早先的 PASS、串口不存在时不打开端口。

用例用 `-TestRuntime` 注入假串口。该参数标记为 `DontShow`，正式长测不要传。
测试模式下脚本跳过串口枚举检查；测试还固定传 `-ComPort COM_TEST`，即使注入失效也会因串口不存在而失败，
不会误开真实串口。`port-unavailable` 用例反过来走真实启动路径，用不存在的串口名验证脚本在打开端口前就失败。

判定规则本身也有回归用例：`prompt-detection-regression` 用一段"提示符后紧跟异步日志"的样本，
确认旧的按结尾判断写法匹配不上、新写法匹配得上；`ansi-erase-suffix` 让假板端在每行末尾带 `ESC[K`，
去掉转义剥离后该用例会以 `Incomplete committed-table console output.` 失败。

已知限制：假串口在已解码的 .NET 字符串上分片，无法复现 UTF-8 多字节字符被拆到两次读取之间的情形，
该路径只能在真机上观察。

## 8. 本次加固修复的问题

| 问题 | 影响 | 处理 |
|---|---|---|
| 完成判据是整段文本以提示符结尾 | 提示符之后到达的异步日志会让命令误判超时 | 改为期望内容出现之后再回到提示符 |
| 命令之间不排空残留缓冲 | 残留提示符可能被当成下一条命令的完成信号 | 每条命令下发前排空，并扫描崩溃特征 |
| 等待间隔内不读串口 | 间隔内崩溃要到下一轮命令才发现，驱动缓冲可能已溢出 | 等待期间按秒轮询读取并扫描 |
| 点表与运行快照按正则计数，行被截断时静默通过 | 截断的行会造成点表误判或在线数误判 | 只接受字段完整的行，数量与已确认表不符就重读 |
| 点表比对只重读一次 | 单次控制台损坏会被当成真实变化 | 比对不一致时再做一次确认读 |
| `uname -a`、凭证状态的结果被丢弃 | 日志里没有固件身份，无法把结论绑定到具体镜像 | 新增 `FIRMWARE` 行；凭证状态单独判定并给出明确原因 |
| 启动失败（串口不存在）不写日志 | 失败不可追溯 | 端口检查移入 try，失败也写 `FAIL` 与 `RUN_END` |
| 追加日志无法区分运行 | 被中断的运行会继承早先的 PASS | 增加 `run=<id>`、`RUN_BEGIN` / `RUN_END` |
| 中断时不写结论 | 中断与"没有输出"无法区分 | `finally` 写 `ABORT` 与 `RUN_END status=INCOMPLETE` |
| 退出码不确定 | 自动化无法区分三种结果 | 退出码放在 `finally`：0 / 1 / 2 |
| 清理阶段一个异常会跳过后续清理 | 文件句柄泄漏 | 关闭、释放各自独立 try |
| 崩溃特征只匹配 5 个字符串 | panic 转储可能被漏掉 | 增加 `dump_assert_info`、`up_dump_register`；另加启动横幅检测 |
| `MinimumOnline` 默认 0 | 全部离线也能 PASS | 保留默认值，写 `WARN` 并在 `PASS` 行显式记录 `online_min` |
