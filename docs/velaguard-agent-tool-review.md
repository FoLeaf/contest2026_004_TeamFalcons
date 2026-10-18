# Agent 工具执行、请求轮次与权限离线复核

复核对象：`AI-01`、`AI-02`、`AI-03` 三项维护清单条目的实现与测试覆盖。
复核日期：2026-10-18。
基线提交：`ff140dd`（fix(velaguard): RS485 DIR 释放挪进 TX 完成中断，D-04 关闭）。
初版工作区：独立 worktree，分支 `codex/agent-fault-review`，从上述提交创建，未带入其他会话的未提交改动。

初版为测试与审查任务，未修改产品实现，未修改公共依赖实现，未访问串口、开发板、真实 LLM 或 MQTT 服务，未运行共享固件构建。后续维护补修已在公共 `packages/ai_agent` 功能分支提交，并集成到 `codex/maintenance-closeout`，具体提交见 4.F-02、4.F-03、4.F-05 与 9.5。

---

## 1. 结论摘要

| 编号 | 级别 | 状态 | 结论 |
|---|---|---|---|
| F-01 | 中 | 已修复，公共提交 `c2ed337` | `write_file` 原先会跟随路径最后一段的悬空软链接写入。写入改为 `O_NOFOLLOW` 后，末段软链接不会在策略检查与打开之间被跟随，悬空链接指向沙箱外或受保护路径的复现均通过。 |
| F-02 | 低 | 已修复，公共提交 `8d16058` | 产品命令闸门的元数据白名单只匹配命令名，初版确认 `date -s` 经 `run_shell` 可达。`2a20a58` 的首次收紧仍接受 format 后的尾随参数；`bcec30a` 加入纯参数红测，`8d16058` 改为完整匹配只读参数序列并通过，不执行改时命令。 |
| F-03 | 低 | 已修复，公共提交 `2a20a58` | `tool_registry_execute` 原先只在工具返回 OK 时记一次调用，被拒绝或失败的工具调用不计入限流。现在 `tool_guard_check` 在放行敏感工具时原子预留额度，失败调用也计数，并发突发放行测试通过。 |
| F-04 | 低 | 已修复，集成分支 | 最后回复的读取改为在锁内复制到调用方缓冲区，不再暴露共享缓冲区指针；诊断读取获得一致的 NUL 结尾快照。 |
| F-05 | 低 | 已修复，公共提交 `7dc0a88` | 原先代理获得 ai_agent 的默认内建工具集合。现在 `CONFIG_VG_HMI` 下只允许 `read_file`、`write_file`、`get_current_time`、`run_shell`、`vg_point_read`、`vg_run_report`。白名单外工具不进入注册 JSON，执行入口也拒绝；运行时配置只能进一步禁用，不能扩大白名单。 |

AI-01、AI-02、AI-03 三项的核心目标经复核成立：执行预算与回收、轮次关联与迟到回复隔离、文件与凭证路径保护在主机上都已由新增用例证明。F-01 由公共提交 `c2ed337` 修复，F-02 由 `2a20a58`、`bcec30a`、`8d16058` 完成修复与回归，F-03 由 `2a20a58` 修复，F-04 已在集成分支修复并加入回归；F-05 由公共提交 `7dc0a88` 收窄为产品硬白名单。

新增测试全部通过，未发现已有测试的回归；集成分支的主机测试数量与最终结果以 G-05 的实际输出为准，不沿用初版 worktree 的旧数量。

---

## 2. 复核范围与方法

### 2.1 阅读的源码

| 文件 | 关注点 |
|---|---|
| `packages/ai_agent/src/tools/tool_shell_runner.c` | 执行截止时间、非阻塞管道、有界回收、NuttX 与主机分支 |
| `packages/ai_agent/src/tools/tool_shell.c` | 产品命令闸门、元数据白名单、`cat`/`ls` 内建、参数上限 |
| `packages/ai_agent/src/tools/tool_files.c` | 路径解析、读写保护、软链接处理 |
| `packages/ai_agent/src/sdk/velaclaw_client_local.c` | 请求标识、busy 判定、迟到与重复回复 |
| `packages/ai_agent/src/tools/tool_registry.c` | 工具执行入口、限流调用点、审计落盘 |
| `packages/ai_agent/src/tools/tool_guard.c` | 限流窗口、脱敏、注入模式 |
| `app/velaguard/vg_agent_round.c` / `.h` | 轮次状态机、请求序号、owner、回收 |
| `app/velaguard/vg_agent_tools.c` | 自定义 C 工具的参数检查、输出上限、调用限流 |
| `app/velaguard/vg_agent_query.c` | 限流规则实现、点位匹配 |
| `nuttx/sched/task/task_setup.c`、`sched/task/task_spawn.c` | `task_spawn` 的 argv 组装 |
| `apps/nshlib/nsh_timcmds.c`、`nsh_command.c` | NSH `date` 命令是否可设置时钟 |
| `scripts/configs/velaguard-lvgl.defconfig` | 产品实际开关：`CONFIG_VG_HMI`、`CONFIG_SYSTEM_POPEN`、`CONFIG_SCHED_CHILD_STATUS` |

### 2.2 实际运行的命令与结果

在 worktree 内运行既有测试，确认基线：

```
make -C app/velaguard/host_tests test
```

结果：28 个 C 目标与 2 个 Python 检查全部通过，与维护清单记录的基线一致。与本次复核直接相关的既有目标：

```
test_shell_runner: OK (timeout, closed pipe, overflow, recovery, reap)
test_local_client: OK (busy, stale, duplicate, failed submit, recovery)
test_file_policy: OK (symlink, missing target, config, firmware facts, allowed report)
test_shell_output: OK (exact canaries, metadata bounds, product allowlist)
test_agent_round: OK (timeout, late callback, result ownership, recovery)
test_agent_tools: OK
test_agent_query: OK
```

新增测试运行：

```
make -C app/velaguard/host_tests/agent_review test
make -C app/velaguard/host_tests/agent_review repro
```

结果见第 5 节与第 6 节。

### 2.3 构建隔离

新增测试放在 `app/velaguard/host_tests/agent_review/`，自带 Makefile，未改动 `host_tests/Makefile` 或其他会话在用的构建文件。编译产物全部落在该目录内。公共源码树只读使用，运行前后对 `nuttx`、`apps`、`packages` 做文件时间检查，未发现新写入：

```
find nuttx apps packages -newermt '-20 minutes' -type f
```

输出为空。`packages/ai_agent/src` 下存在的 `<源文件>.<绝对路径>.o` 是共享固件构建留下的历史产物，时间戳早于本次复核，与新增测试无关。

---

## 3. 逐项结论

### 3.1 AI-01 执行预算与回收

实现要点与复核结论：

- 单调时钟 `now_ms()`（`tool_shell_runner.c:27`）与循环顶部的截止检查（`:128`）配合，超时判定不依赖管道状态。
- 管道读端设为非阻塞（`:122`），循环同时处理读、EOF 与 `waitpid(WNOHANG)`（`:127` 至 `:154`），因此子进程持续输出不会阻塞截止判定。
- 输出超限只置标志并继续排空（`:140`），命令不会因为缓冲区满而卡住。
- 停止路径先杀进程组（主机）或任务（NuttX），再在 100 ms 内做有界回收（`:159` 至 `:173`），返回码保持 `-ETIMEDOUT`。

新增覆盖（`test_runner_budget.c`）与既有 `test_shell_runner.c` 的分工：既有用例覆盖正常输出、静默挂住、关闭 stdout、有界溢出、失败后恢复和直接子进程回收。新增用例覆盖的是它证明不了的部分：持续输出下的截止判定、截止时间的实际误差、命令留在后台的后代进程是否被回收、连续超时是否泄漏描述符或僵尸进程、被信号杀死的子进程是否记为失败、以及 1/2/3/5/64/257 字节缓冲区的精确 canary。

结果：

```
test_runner_budget: OK (endless output, deadline accuracy,
descendant reclaim, fd/zombie leaks, signalled child, cap canaries)
```

其中后代进程回收一项具备判别力：单独杀死直接子进程时该用例会失败（已用等效 shell 命令实测确认，仅杀直接子进程时后台写入者仍会落盘）。

### 3.2 AI-02 请求轮次隔离

实现要点与复核结论：

- SDK 侧以 `pending_id`（`app_id:serial`）关联回复，`tap_callback` 在锁内比对 `chat_id`（`velaclaw_client_local.c:57`），并丢弃 `is_progress` 消息（`:53`）。
- busy 判定按已用时间与超时比较（`:152`），超时后的新请求可以进入。
- 板端 `vg_agent_round.c` 另用 `g_request_serial` 作为回调凭据（`:55`），并要求状态处于 QUEUED 或 RUNNING 才接受回复（`:56`）。
- 状态迁移全部在 `g_lock` 内完成，`round_submit` 在调用客户端之前释放锁（`:181`），因此同步派发的回调不会与提交路径争同一把锁。
- `g_submitting` 防止 `vg_agent_round_tick` 重入提交（`:211`）。

新增覆盖（`test_round_isolation.c`）针对既有 `test_agent_round.c` 未覆盖的四类情况：

1. 总线始终不可用时，队列看门狗在 `VG_AGENT_ROUND_QUEUE_TIMEOUT_MS` 后释放请求，期间每次 tick 重试打开客户端。
2. 回复在状态仍为 QUEUED 时到达（同步派发的客户端）会被保留，不会被丢弃。
3. 提交失败（客户端返回 busy）后，该轮的凭据既不能把失败轮标记为已回复，也不能污染下一轮。
4. `vg_agent_round_reclaim` 的语义：只在宽限期用尽后释放他人的已完成轮次，永不释放调用者自己的结果，永不释放运行中的轮次，且释放不改变代次。

结果：

```
test_round_isolation: OK (queue watchdog, stale/duplicate replies,
early reply, submit failure isolation, reclaim)
```

判别力验证：对 `vg_agent_round.c` 的副本注入三处变异（去掉 QUEUED 接受、去掉 owner 判断、放大队列看门狗阈值），三处都被用例捕获。产品源码未被修改。

### 3.3 AI-03 文件与凭证保护

实现要点与复核结论：

- `validate_path` 同时做逻辑前缀检查与解析后检查（`tool_files.c:126`），并拒绝任何含 `..` 的路径（`:129`）。
- `resolve_target` 在整条路径不存在时回退到解析最深的已存在祖先（`:105`），用于识别逃逸的父目录软链接。F-01 的末段悬空软链接由写入打开时的 `O_NOFOLLOW` 拒绝。
- 读保护覆盖 `AGENT_CONFIG_DIR` 与 `/data/velaguard/provision`（`:143`），写保护覆盖配置目录、`/data/velaguard` 下 `reports` 以外的全部路径，以及固件运行时报告（`:164`）。
- 凭证的两处存放位置都在保护范围内：`VG_PROVISION_FILE` 为 `/data/velaguard/provision/llm_secrets.v1`（`vg_provision.h:26`），LLM 端点与密钥的镜像副本在 `/data/agent/config/config.json`（`vg_provision.h:57`、`agent_config.h:96`）。
- 审计日志位于 `/data/velaguard/logs/agent_tools.log`（`tool_registry.c:66`），不在 `reports` 之下，因此代理无法经 `write_file` 伪造或覆盖自己的调用审计。

新增覆盖（`test_file_policy_extra.c`）在既有 `test_file_policy.c` 之外补充：凭证 blob 的直读与经软链接读取、代理配置经软链接读取、`edit_file` 与 `write_file` 的策略一致性、指向已存在受保护文件的软链接、逃逸的父目录软链接、含 `..` 的读写路径、嵌套报告目录的创建、以及 `read_file`/`write_file`/`list_dir` 在 0/1/2/17/64 字节缓冲区下的精确 canary。

结果：

```
test_file_policy_extra: OK (credential/config read guard, edit_file parity,
symlink targets, .. refusal, small-buffer canaries)
```

### 3.4 复核清单第 1 至 9 项

| 项 | 结论 |
|---|---|
| 1 挂住、关闭 stdout、持续输出、输出超限、异常退出 | 主机已覆盖，见 3.1。NuttX 侧终止行为见第 7 节。 |
| 2 超时后能否完成下一次请求 | 已覆盖：连续 30 次超时后紧接的正常请求成功；轮次侧由看门狗与 `clear` 恢复。 |
| 3 busy、迟到回调、重复回调、中间 progress、最终结果串轮 | 已覆盖：SDK 侧由既有 `test_local_client` 覆盖 busy/迟到/重复/提交失败；板端侧由新增用例覆盖跨轮迟到、重复、同步早到回复与代次计数。 |
| 4 超时回收是否覆盖实际产品调用链 | 部分覆盖。产品中真正经 `tool_shell_run` 的命令是 `date`、`heap_info`、`net_status`、`router_status`（其余元数据命令由 `tool_shell.c` 的 C 内建直接处理，不派生子进程）。这些命令是否会派生子进程或长时间阻塞，只能在板端确认。 |
| 5 自定义 C 工具的参数检查 | 已覆盖。`vg_agent_tools.c` 自行检查类型、长度与 JSON 合法性（`parse_query`，`:334`），既有 `test_agent_tools.c` 的 `test_query_argument_guards` 已断言非字符串、超长与非法 JSON；`vg_agent_query.c` 的限流规则由 `test_agent_query.c` 的 `test_rate_limit*` 覆盖。本次未重复编写。 |
| 6 代理能否触及点表、配置生效、寄存器写入、清告警、输出控制 | 命令侧已由新增闸门表覆盖：`vgpoint` 全部子命令、`vgdiscover apply`、`vgmodbus`、`vgcfg commit` 均被拒绝，路径前缀（`/bin/vgpoint`）与词前缀（`dumpsters`、`dum`）变体也被拒绝。文件侧由写保护覆盖。代理没有可写寄存器、清告警或控制数字量输出的工具。`date -s` 的残余入口已由 F-02 的公共修复关闭。 |
| 7 文件保护对软链接、路径别名、不存在目标与父目录的处理 | 已存在软链接、逃逸的父目录、含 `..` 的路径、不存在目标的父目录均处理正确；末段悬空软链接已由 F-01 的 `O_NOFOLLOW` 修复覆盖。 |
| 8 合法报告产物写入是否仍可用 | 可用。日报、嵌套报告目录、代理自身记忆文件均写入成功，且内容确实落盘。 |
| 9 凭证与配置保护是否在 C 实现与工具注册层生效 | C 实现层已生效并有测试证据。工具注册层方面，`tool_guard.c` 提供输入长度上限、限流与脱敏；F-03 的限流记账缺口已修复并由并发预留测试覆盖。F-05 的产品硬白名单同时约束注册和执行，配置只能收窄。 |

---

## 4. 缺陷与复现

### F-01 `write_file` 跟随末段悬空软链接写入沙箱外（已修复）

**原位置**：`packages/ai_agent/src/tools/tool_files.c`，`resolve_target`、`validate_path`、`is_write_protected`、`tool_write_file_execute`。

**原机理**：策略检查调用 `resolve_target`。整条路径不存在时，它用 `realpath` 解析最深的已存在祖先，然后把剩余部分原样拼回：

```
realpath("data/velaguard/reports/a.md")  失败（悬空）
realpath("data/velaguard/reports")       成功
resolved = <绝对路径>/data/velaguard/reports/a.md
```

最后一段从未被解析，因此策略看到的是一个位于唯一可写子树内的路径。随后 `tool_write_file_execute` 对逻辑路径执行 `fopen(path, "w")`，该调用会跟随链接并创建目标文件。

**修复**：公共提交 `c2ed337` 把写入改为：

```
open(path, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0644)
```

成功后再用 `fdopen()` 取得 `FILE *`；`fdopen()` 失败时关闭描述符并返回错误。`O_NOFOLLOW` 让末段为软链接时直接拒绝打开，悬空链接也不会被跟随创建目标。父目录软链接仍由 `realpath` 策略检查负责。公共回归 `test_file_policy.c` 新增了末段悬空链接指向沙箱外和受保护路径两种断言。

**修复前复现**：

```
make -C app/velaguard/host_tests/agent_review repro
```

输出（节选）：

```
control: an existing symlink to an existing protected file
  write through an existing link is refused                  PASS
  and the protected file still exists                        PASS
case A: dangling link, target outside the sandbox
  write through the dangling link is refused                 FAIL
  no file was created outside the sandbox                    FAIL
case B: dangling link, target at a protected path
  write through the dangling link is refused                 FAIL
  no file was created at the protected path                  FAIL
repro_symlink_write_escape: 4 check(s) FAILED - defect present
```

原始失败输出保存在 `docs/maintenance-evidence/2026-10-18/f01-symlink-write-pre-fix.log`，退出码为 1。修复后同一程序 6 项检查全部通过，且纳入 `agent_review` 的默认 `test` 目标；完整输出保存在 `f01-symlink-write-post-fix.log`。集成后的完整主机测试也通过，输出保存在 `f01-host-tests.log`。

**原影响范围（实测）**：

- 只影响 `write_file`。`edit_file` 不受影响，因为它先以读方式打开文件，悬空链接读不到内容。
- 已存在的软链接指向已存在文件时处理正确，因为此时 `realpath` 能解析整条路径。
- 逃逸的父目录软链接处理正确，这正是 AI-03 修复覆盖的情形。
- 读取不受影响：已存在的链接会被解析并识别为受保护，悬空链接根本打不开。
- 目标所在的目录必须已存在，因为 `ensure_parent_dirs` 只沿逻辑路径创建目录。

**证据边界**：板端 `/data` 是 FAT 卷，不支持软链接，因此该缺陷在当前板上原本就不可达，修复也无法在板端用真实软链接验证。以上结论只证明主机隔离目录中的原缺陷可复现且已关闭，不把主机回归当作板端验收。

### F-02 元数据白名单只匹配命令名，`date -s` 可改系统时钟（已修复）

**原位置**：`packages/ai_agent/src/tools/tool_shell.c` 的 `CONFIG_VG_HMI` 元数据白名单。初版只对首个词做 `strcmp(name, metadata[k])`，没有检查 `date` 的选项。

**原缺陷证据**：`date` 在元数据白名单内。NSH 的 `cmd_date` 支持 `-s` 设置系统时钟（`apps/nshlib/nsh_timcmds.c` 的 `date_settime`）。产品配置未关闭该命令：`scripts/configs/velaguard-lvgl.defconfig` 中无 `CONFIG_NSH_DISABLE_DATE`，`CONFIG_SYSTEM_POPEN=y` 且未设 `CONFIG_SYSTEM_POPEN_SHPATH`，因此运行器走 `nsh_system` 分支，命令交给 NSH 解析执行。

**首次修复的缺口**：公共提交 `2a20a58` 在 C 闸门内增加 `date_args_read_only()`，但剩余参数以 `+` 开头时直接返回允许，因此还会接受 `date +%s -s ...`、`date -u +%s -s ...` 等带尾随参数的字符串。NuttX `cmd_date()` 依次调用 `getopt(argc, argv, "s:u")`、解析首个 `+FORMAT`，随后用 `argc > 0` 拒绝多余操作数；静态路径看起来会拒绝这些尾随参数，但 C 闸门不能依赖后端恰好拒绝。

**完整修复**：`bcec30a` 先只增加纯参数负向断言。该测试在修复前以 `SIGABRT` 退出，首个失败是 `date +%s -s 'Oct 18 20:00:00 2026'` 未被拒绝。`8d16058` 把参数判断改为完整序列匹配：

- 允许无参数、单独 `-u`，或 `-u` 后跟一个 `+FORMAT`。
- 允许 `+FORMAT` 单独出现，FORMAT 必须非空。
- 允许各参数之间和末尾有多余空白。
- 拒绝 `-s`、重复 `-u`、空 `+`、format 后的任何选项或操作数，以及用制表符隐藏的尾随参数。

**验证**：公共树 `test_shell_output.c` 与 VelaGuard 的 `agent_review/test_shell_gate.c` 都直接调用 `is_blocked()` 检查允许和拒绝两侧，不执行任何改时命令。修复前的原始输出保存在 `docs/maintenance-evidence/2026-10-18/f02-date-args-pre-fix.log`，退出码为 134；修复后同一用例通过，输出保存在 `f02-date-args-post-fix.log`，退出码为 0。正向项继续覆盖 `date`、`date -u`、`date +%s` 和 `date -u +%s`。

**证据边界**：本项证明的是 C 闸门的接受范围已经收紧。没有实际执行 `date +%s -s ...`，因此不宣称该形态已经在板端成功修改系统时钟；新增的 C 拒绝发生在任何后端参数解析之前。

**残留加固项**：`tool_guard.c` 的限流窗口仍以 `time(NULL)` 为时基，系统级校时仍可能使窗口提前或延后过期。代理经 `date -s` 改动时基的路径已经关闭；把限流时基改为单调时钟属于独立的低优先级加固，不影响本次修复成立。

### F-03 敏感工具的限流只在成功时记账（已修复）

**原位置**：`packages/ai_agent/src/tools/tool_registry.c` 的工具执行分支。初版在 `execute()` 返回 OK 后才调用 `tool_guard_record_call()`，`tool_guard_check()` 只检查额度而不记账。

**原缺陷**：`run_shell`、`write_file`、`edit_file` 只要持续失败（被命令闸门拒绝、命令超时、工具自身返回错误），就不会占用限流额度。被拒绝的调用恰恰是最需要计入的那一类。

**修复**：公共提交 `2a20a58` 把敏感工具的额度检查改为 `check_and_reserve_rate()`，在 `tool_guard_check()` 返回放行前原子清理过期记录、检查上限并预留一个额度。`tool_registry_execute()` 不再在成功后重复记账。被放行后无论执行成功、失败还是超时，都只占用一次额度。

**验证**：`packages/ai_agent/tests/test_tool_guard_rate.c` 先连续放行 10 次并确认第 11 次被拒绝，再推进窗口确认额度恢复，最后用 24 个并发线程确认只放行 10 次。该测试通过 `agent_review` 的 `test_tool_guard_rate` 目标执行。

**边界**：工具在被 guard 判定为禁用、输入超长或参数 null 时不会执行，也不会预留额度。这三种拒绝不进入工具执行路径，不构成本缺陷中的失败调用绕过。

### F-04 最后一条回复的读取未加锁（已修复）

**原位置与缺陷**：`vg_agent_round_last_reply()` 直接返回 `g_reply`，未持有 `g_lock`；写入方 `round_reply` 与 `vg_agent_round_queue_owned` 都在锁内。调用方（`vgagent.c` 的 NSH 回显与日志）可能读到撕裂的文本，或在新轮次入队后读到已被清空的串。该缺陷只影响诊断读取，不影响轮次状态机与结果归属。

**修复**：接口改为 `int vg_agent_round_copy_last_reply(char *buffer, size_t size)`。实现持 `g_lock`，用 `snprintf` 把回复复制到调用方缓冲区后解锁；空指针或零长度缓冲区返回 `-EINVAL`，其余情况返回 `0`，结果始终以 NUL 结尾。回复容量常量 `VG_AGENT_ROUND_REPLY_MAX` 移到公共头文件，供调用方声明缓冲区。`vgagent.c` 的 `print_status()` 只获取一次快照，判断非空和打印都使用同一份文本，避免两次读取跨轮次。

**验证**：`test_round_isolation.c` 改用复制接口读取回复，新增空指针、零长度和 4 字节截断断言，确认截断结果仍以 NUL 结尾，`make -C app/velaguard/host_tests/agent_review clean test` 全部通过。完整主机测试 `make -C app/velaguard/host_tests test` 也全部通过。原始输出、提交号、相关文件哈希和退出码分别保存在 `docs/maintenance-evidence/2026-10-18/f04-agent-round-snapshot.log` 与 `f04-host-tests.log`。

**证据边界**：以上是主机并发契约与参数边界测试，不证明板端出现过该竞态，也不把接口修改当作 G 阶段真机验收。板端版本仍需随最终固件统一构建、烧录和验收。

### F-05 内建工具集合未按产品需求收窄（已修复）

**原位置与缺陷**：`packages/ai_agent/src/tools/tool_registry.c` 的 `tool_registry_init()` 原先几乎无条件注册 ai_agent 的全部内建工具。代理可获得 `edit_file`、`list_dir`、`cron_add`、`fetch_url`、`web_search`、`news_search`、`get_weather`、`analyze_image`、`feishu_*`、`music_*`、`vibrate` 等与 VelaGuard Skills 无关的工具。`tool_guard_is_enabled()` 在 `tool_enabled_<name>` 缺失时默认启用，不能形成产品级工具上限。

**修复**：公共提交 `7dc0a88` 在 `CONFIG_VG_HMI` 下建立硬白名单，只允许：

```
read_file
write_file
get_current_time
run_shell
vg_point_read
vg_run_report
```

约束在两层同时生效：

- `tool_guard_is_enabled()` 先检查硬白名单，再读取 `tool_enabled_<name>`。运行时配置可把白名单内工具设为 `0`，但不能把白名单外工具设为 `1`。
- `register_tool()` 跳过白名单外的内建工具，因此这些工具不会出现在提供给模型的 tools JSON 中。
- `tool_registry_execute()` 在任何内建或 provider 执行之前调用 `tool_guard_check()`，白名单外名称返回 `GUARD_DENY_DISABLED`。因此即使名称来自模型、旧配置或自定义 provider，也不会执行。

`write_file` 仍受文件路径策略约束，只能写入代理数据目录和报告子树；`run_shell` 仍受 `tool_shell.c` 的产品只读命令闸门约束。两者保留是因为现有 Skills、报告和只读诊断需要，不能据此绕过 V5 的点表、寄存器、告警和数字量输出限制。

**验证**：公共用例 `tests/test_tool_guard_vg_scope.c` 检查六个允许项、多个白名单外拒绝项、配置只能收窄，以及配置尝试重新开放 `web_search` 时仍被拒绝。`tests/test_tool_registry_vg_scope.c` 检查注册集合恰好为这六项，`edit_file`、`list_dir`、`cron_add`、`fetch_url`、`vibrate` 等不会注册。

在修复前快照 `c2ed337` 上，两组用例都以 `SIGABRT` 退出，退出码为 134，输出保存在 `docs/maintenance-evidence/2026-10-18/f05-vg-tool-scope-pre-fix.log`。公共提交 `7dc0a88` 后，`make -C app/velaguard/host_tests/agent_review clean test` 通过，输出保存在 `f05-vg-tool-scope-post-fix.log`；集成后的完整 `make -C app/velaguard/host_tests clean test` 退出码为 0，输出保存在 `f05-host-tests.log`。

**证据边界**：`vgagent tools` 只打印以 `vg_` 开头的产品自建工具，没有列出完整内建集合的板端只读接口。F-05 的确定性证据是 C 注册与执行路径加隔离主机回归；最终固件必须确认编译时启用 `CONFIG_VG_HMI`，但不新增只为展示工具清单的诊断接口。

---

## 5. 已核对且未发现问题的项

这些结论同样有源码或测试证据，记录下来以免后续重复排查。

| 项 | 证据 |
|---|---|
| `task_spawn` 的参数传递没有错位 | `tool_shell_runner.c:62` 传入 `args + 1`（跳过命令名）是正确的：NuttX 的 `nxtask_setup_stackargs` 会把任务名写入 `stackargv[0]`，再把调用方数组复制到 `stackargv[1..]`（`nuttx/sched/task/task_setup.c:538` 起）。参数与 `argc` 均符合应用预期。 |
| 运行器对配置的编译期前置检查与产品一致 | 运行器要求 `CONFIG_SYSTEM_POPEN` 时必须有 `CONFIG_SCHED_CHILD_STATUS`（`tool_shell_runner.c:5`）。产品 defconfig 两项均已开启。 |
| 产品命令闸门未过度封堵 | `test_shell_gate.c` 的允许行覆盖 `vgstats dump`、`vgcfg dump`、`vgruntime dump`、`vgnet status`、`free`、`ps`、`df`、`uptime`、`uname`、`date`、`heap_info`、`net_status`、`router_status`，以及 `vgstats  dump` 这类多余空格的写法。 |
| 子命令闸门不因词前缀或路径前缀失效 | `vgstats dumpsters`、`vgstats dum`、`/bin/vgpoint get` 均被拒绝。 |
| 代理无法伪造自己的调用审计 | 审计日志在 `/data/velaguard/logs/`，位于写保护范围内。 |
| `vg_agent_tools` 的参数检查与调用限流规则已被既有测试覆盖 | `test_agent_tools.c` 的 `test_query_argument_guards`、`test_output_is_bounded`；`test_agent_query.c` 的 `test_rate_limit`、`test_rate_limit_clock_wrap`、`test_rate_limit_guards`。 |
| FAT 大小写不敏感不构成绕过 | `logical_path_allowed` 与 `path_under_resolved_root` 都做大小写敏感比较。受保护路径的大小写变体（如 `/data/velaguard/CONFIG/...`）会被判为受保护并拒绝，`reports` 的大小写变体会被判为不可写并拒绝。两个方向都偏向拒绝，属于安全的一侧。此项由源码推得，本次未在设备上验证；板端可用只读路径请求观察，见 G-03 第 5 步。 |
| `list_dir` 只遍历代理数据目录 | `tool_files.c:478` 以 `AGENT_DATA_DIR` 为根。代理无法枚举 `/data/velaguard`，符合最小权限；这是有意行为，不是缺陷。 |

---

## 6. 新增测试清单

目录：`app/velaguard/host_tests/agent_review/`。

| 文件 | 覆盖 |
|---|---|
| `Makefile` | 自带构建规则，不并入 `host_tests/Makefile`；`test` 目标包含 F-01、F-05 回归，`repro` 目标可单独运行 F-01 回归 |
| `test_runner_budget.c` | 持续输出下的截止判定、截止误差、后台后代回收、描述符与僵尸泄漏、被信号杀死记为失败、小缓冲区 canary、参数守卫 |
| `test_round_isolation.c` | 队列看门狗、跨轮迟到回复、重复回复、同步早到回复、提交失败隔离、`reclaim` 语义、代次计数、回复快照的参数与截断边界、参数守卫 |
| `test_file_policy_extra.c` | 凭证与配置的读取保护（含软链接路径）、`edit_file` 策略一致、软链接目标、`..` 拒绝、嵌套报告写入、小缓冲区 canary、非法参数 |
| `test_shell_gate.c` | 产品命令闸门的完整双向表：拒绝项、允许项、子命令边界、shell 元字符、参数守卫 |
| `test_tool_guard_rate.c` | 公共树用例：敏感工具的失败调用计入额度、窗口恢复、24 线程并发只放行 10 次 |
| `test_tool_guard_vg_scope.c` | 公共树用例：VelaGuard 硬白名单、白名单外执行拒绝、运行时配置只能收窄 |
| `test_tool_registry_vg_scope.c` | 公共树用例：白名单外内建工具不进入注册集合 |
| `repro_symlink_write_escape.c` | F-01 的最小回归：末段悬空软链接指向沙箱外和受保护路径均不得写入 |

测试设计约束：全部使用虚构数据与临时目录；不使用固定 sleep 作为同步手段，等待均有明确上限（最长单次约 1.2 秒）；不发起真实无限挂起任务。

---

## 7. 主机测试不能证明的 NuttX 行为

以下结论只能在设备上取得，主机用例的通过不构成证据。带只读观察方式的项可以在阶段 G 取得证据，其余项只能保留主机证据。

| 项 | 内容 | 板端可否取得证据 |
|---|---|---|
| 7.1 | 超时后的后代进程回收。主机分支用 `kill(-pid, SIGKILL)` 杀进程组（`tool_shell_runner.c:161`），NuttX 分支用 `kill(pid, SIGKILL)` 只杀任务本身（`:163`）。若某条命令自行派生了任务，该任务不会随超时被回收。产品侧当前经 `tool_shell_run` 的命令是 `date`、`heap_info`、`net_status`、`router_status`。 | 可，方式见 G-01 的回收容差检查（基线对比）。 |
| 7.2 | `task_spawn` 与文件动作在板端的实际行为。主机用 `posix_spawn` 与 `/bin/sh`；板端走 `task_spawn` 加 `posix_spawn_file_actions`。需要确认重定向后的管道读端在应用退出时确实产生 EOF，且返回的 pid 可被 `waitpid` 回收。 | 可间接观察：G-01 中若出现 `command failed rc=... (10s limit)`，说明 EOF 或回收之一未按预期完成；正常返回则说明两者成立。 |
| 7.3 | `waitpid` 与 `CONFIG_SCHED_CHILD_STATUS` 的交互。有界回收循环依赖 `ECHILD` 判定（`:169`）。 | 可，同 7.2。 |
| 7.4 | `usleep(1000)` 轮询的代价。EOF 之后循环以 1 ms 步进轮询（`:134`），在按 tick 调度的 NuttX 上实际步进取决于 `CONFIG_USEC_PER_TICK`。 | 可，通过 G-01 中文件工作线程 tick 计数是否停滞判断。 |
| 7.5 | 10 秒预算在真实工具线程上的可行性。工具线程栈为 16 KB，运行器在栈上有 256 字节分块缓冲。 | 只能观察，不能确定性注入，理由见 G-01 的已知限制。 |
| 7.6 | 轮次预算与真实 LLM 时延的关系。`VG_AGENT_ROUND_TIMEOUT_MS` 为 300 秒，`vg_agent_round.c` 与 SDK 使用同一常量。 | 可，G-02 记录各阶段 `vgagent status` 的时间点。 |
| 7.7 | 代理实际尝试 `date -s` 的发生率。F-02 已在 C 闸门修复，无需通过观察发生率证明可达性或修复效果。 | 不需要观察，也不得实际执行改时间的命令。 |
| 7.8 | `/data` 是否无法承载软链接。 | 可从挂载表读取文件系统类型作为环境记录，见 G-03 第 4 步。F-01 已由主机回归关闭，不再要求板端构造软链接。 |
| 7.9 | FAT 大小写不敏感下的路径判定（第 5 节的推论）。 | 可，用只读路径请求观察拒绝结果，见 G-03 第 5 步。 |
| 7.10 | 完整内建工具集合在板端是否与硬白名单一致（F-05）。 | 不可完整核对：`vgagent tools` 只打印 `"vg_` 开头的子集（`vgagent.c` 的 `print_tools`），没有打印完整工具清单的只读接口。确定性的注册和执行约束由 `7dc0a88` 的代码与主机用例证明；板端只记录现有自定义工具输出，不虚构完整清单。 |

---

## 8. 阶段 G 真机验收步骤

### 8.0 本计划的约束

以下步骤遵守四条约束：

1. **策略逻辑用主机隔离目录与虚构数据验证**，板端只做只读检查。板端不向真实凭证发起读取，不向真实 `points.json` 发起写入，不要求代理清除告警或控制数字量输出。
2. **没有安全预检接口的项目列为未验证**，以主机证据为准，不为了拿到板端证据去触碰真实密钥或真实点表。
3. **不执行任何修改系统时间的命令**，主机与板端都不执行。
4. **验证拒绝行为时只使用只读的被禁命令**（例如 `vgpoint get`）。最坏情况是闸门失效时代理读到它本来就能通过 `vg_point_read` 读到的数据，不改变任何状态。禁止用 `reboot`、`rm`、`dd`、`mount` 之类命令做拒绝验证。

前置条件：烧录包含本轮修复的固件；COM3 NSH 可用；模拟从站可用；保留 `docs/maintenance-evidence/2026-10-18/` 下的证据目录用于存放新证据。

### G-01 执行预算与回收容差（对应 AI-01）

两个概念分开判定，不合并成一条：

- **执行预算**：`tool_shell_run` 的 10 秒上限（`tool_shell_runner.c:128`、`tool_shell.c:432`）。判据是运行器是否在预算内返回。
- **回收容差**：停止路径先发信号，再在 100 ms 内做有界回收（`tool_shell_runner.c:166` 至 `:171`）。判据是经过一段明显大于该容差的宽限期后，任务集合是否回到基线。信号发出后任务短暂存在属于设计内行为。

步骤：

1. 在 NSH 中执行 `ps`，保存基线快照。基线本身包含控制台自己的 nsh 任务，这是正常状态。
2. 触发一次告警，使代理执行告警解释轮次。记录串口日志中 `round submitted` 与 `round finished` 的时间差。
3. 通过正常交互渠道要求代理运行一条只读的元数据命令（`free`、`ps`、`net_status` 之一），以经过 `run_shell`。若该轮次代理没有调用 `run_shell`，记录为本次未覆盖运行器路径，不作为失败。
4. 活动结束后等待不少于 5 秒（远大于 100 ms 回收容差），再次执行 `ps`，与第 1 步的基线逐项比较。
5. 在 NSH 中连续两次读取 `vgruntime dump`，比较 tick 计数。

通过条件：

- 执行预算：日志中没有任何 `run_shell` 调用超过 10 秒；出现失败时，日志行是运行器的 `command failed rc=-110 (10s limit)` 形态；该轮次在轮次预算内结束。
- 回收容差：宽限期后的 `ps` 任务集合与基线集合一致，即没有**新增**的 `agent_shell` 残留任务。判据是相对基线的增量，不是任务集合为空。
- 文件工作线程的 tick 计数在整段过程中持续增长，无停滞。

已知限制：产品允许的命令集中没有会长时间阻塞的只读命令，`vgagent tool` 也无法传入 `run_shell` 所需的 JSON（NSH 词法分析会吃掉引号，见 `vgagent.c` 中 `run_tool_cmd` 的说明），因此**无法在板端确定性地构造一次运行器超时**。执行预算的确定性证据来自主机用例 `test_runner_budget`，板端只提供未挂起与回收正常的观察证据。若需要板端确定性注入，需要先决定是否引入构建期测试钩子；本计划不提出新增危险工具，也不为测试向代理开放工具。

### G-02 轮次隔离与恢复（对应 AI-02）

步骤：

1. 触发告警解释轮次，在 `vgagent status` 显示 `state=running` 时，从 NSH 执行 `vgagent ask 读取 ups_load 当前值`（问题本身是只读查询，且代理的工具闸门另有限制）。预期 `queue failed: -EBUSY`（`vg_agent_round_queue_owned` 在非 IDLE 时返回忙），随后 `vgagent status` 仍显示原轮次状态未变。
2. 待该轮次结束后，再执行一次 `vgagent ask`，确认其完成。比较日报产物在两次操作前后的内容，确认人工轮次没有覆盖日报流程的产物。
3. 在轮次进行中切断网络（关闭 ESP8266 或拔除网线），确认轮次在预算内进入 `state=error`，随后一次正常请求可以完成；同时确认本地确定性流程（Modbus 采集、帧统计、规则告警、LED 与屏幕）在此期间继续运行。
4. 代次观察：`vgagent status` 输出 `round: state=... gen=%u owner=...`（`vgagent.c:185` 至 `:188`）。在以下四个时点各记录一次：轮次入队前、`state=running` 时、`state=done` 后、`state=error` 后。

通过条件：

- 轮次进行中的人工请求被拒绝为忙，且不改变进行中的轮次状态。
- 人工轮次的结果不被日报流程读取，日报产物内容正确。
- 断网后轮次在预算内进入错误状态，随后一次正常请求成功；本地确定性流程未中断。
- 代次在 `state=done` 后加一，在 `state=error` 后不变。

代次字段的观察方法与区分：板端接口是 `vgagent status` 打印的 `gen=`，对应 `vg_agent_round_generation()`（`vg_agent_round.h:91`），只统计收到回复的轮次。注意不要与点表代次混淆：`vg_point_table.c` 另有 `g_live_gen`，经 `vg_live_points_copy_versioned` 对外（`vg_discover.h:145`），两者是不同计数器，观察时以命令输出所在的字段名为准。

### G-03 文件与凭证保护（对应 AI-03）

板端只做只读检查：

1. 用只读方式确认 `/data/velaguard/provision/llm_secrets.v1` 存在，并记录文件大小。不读取、不导出文件内容。
2. 在会话开始与结束各执行一次 `vgpoint list`，比较已确认点表的条数与内容。
3. 触发日报流程，确认 `/data/velaguard/reports/` 下出现新产物，内容为板上真实统计。
4. 从挂载表读取 `/data` 的文件系统类型，记录该环境信息。F-01 的软链接回归只使用主机隔离目录，不在板端创建软链接。
5. 用只读路径请求观察策略对大小写变体的处理，作为第 5 节推论的补充证据。

通过条件：

- 凭证 blob 存在，且未被任何步骤读取内容。
- 已确认点表的条数与内容在整个过程中不变。
- 日报产物正常落盘，内容来自板上统计。
- `/data` 的文件系统类型被记录。
- 大小写变体请求的结果被记录。

**列为未验证的项目**（没有安全预检接口，不在本计划内执行）：

| 项目 | 未验证的理由 | 现有证据 |
|---|---|---|
| 代理读取凭证 blob 或代理配置的请求是否被板端拒绝 | 唯一的板端验证方式是把代理指向真实密钥，若拒绝逻辑失效即造成泄漏 | `test_file_policy_extra` 在主机隔离目录下用虚构数据覆盖直读与经软链接读取 |
| 代理写入已确认点表的请求是否被板端拒绝 | 唯一的板端验证方式是让代理写真实点表，若拒绝逻辑失效即损坏采集表 | 同上，覆盖 `write_file` 与 `edit_file` 两条路径 |
| 凭证 blob 的加密性 | 主机无法验证：`vg_provision_crypto.c` 的非 NuttX 分支对 seal 与 open 直接返回 `-ENOSYS`（`:55` 起），而 `test_provision_creds` 正是用桩替代了这两个函数，它覆盖的是凭证就绪与存储持久性的判定谓词，不覆盖加密本身。板端验证需要读取 blob 内容，与本计划的约束冲突 | 代码层证据：`vg_provision_seal` 只把 `vg_provision_crypto_seal` 的输出写入文件（`vg_provision.c:414`），目标分支用 mbedtls，密钥由芯片 UID 派生（`vg_provision_crypto.c:27` 至 `:52`） |

若团队后续仍需要板端证据，可选方案是在受保护目录内放一个不含敏感内容的诱饵文件，再让代理读取该诱饵。该方案需要人工向板写入，本计划未采用。

### G-04 工具注册与权限（对应复核清单第 5、6、9 项）

1. 在 NSH 中执行 `vgagent tools`（只读，`vgagent.c:273`）。注意该命令只打印 `"vg_` 开头的工具，即 VelaGuard 自己注册的那部分（`vgagent.c` 的 `print_tools`），它不能列出内建工具集合。逐字记录该输出，确认其中只有只读工具，没有点表编辑、配置生效、寄存器写入、清告警或输出控制类工具。
2. 完整内建工具集合无法用只读接口在板端列出（见第 7 节 7.10）。F-05 的硬白名单在 `tool_guard.c` 和 `tool_registry.c` 的 C 路径实施，由 `test_tool_guard_vg_scope` 与 `test_tool_registry_vg_scope` 固定；命令闸门的允许与拒绝两侧由 `test_shell_gate` 固定。本步骤不读取真实代理配置，也不把 `vgagent tools` 的输出扩写成不存在的能力。
3. 为产生一条被拒绝的审计记录，通过正常交互渠道要求代理执行一条只读的被禁命令（`vgpoint get`）。随后检查 `/data/velaguard/logs/agent_tools.log`。注意 `rc` 列的含义（`tool_registry.c:70` 至 `:74`）：`0` 为成功，`-2` 为工具守卫层拒绝（例如限流），`-3` 为未知工具，其余为工具自身的返回值。命令闸门拒绝发生在 `tool_run_shell_execute` 内部，因此该条记录应为工具自身的返回值 `-1`，而不是 `-2`。确认该条记录存在，且记录中没有完整密钥或口令。这一步只使用只读被禁命令，理由见 8.0 第 4 条。
4. 在短时间内连续要求代理读取点位，超过 `VG_TOOL_RATE_MAX_CALLS`（`vg_agent_tools.c:397`，20 次 / 60 秒）后确认出现限流提示，且提示以正常结果返回而不是被当作未知工具。
5. 针对 F-03：`run_shell` 一类的敏感工具上限是 `AGENT_TOOL_RATE_LIMIT_MAX_CALLS`（`agent_config.h:415`，10 次 / 60 秒）。确定性证据由主机用例 `test_tool_guard_rate` 提供。板端只在真实模型确实选择 `run_shell` 时记录调用次数与限流结果；不能用提示词诱导代替确定性验证，也不能为了测试向代理开放工具。
6. 不执行任何修改点表、清除告警或改变数字量输出的请求。代理是否具备这些能力由第 1 步的清单与第 2 步的代码确认，不通过实际发起请求验证。

通过条件：

- `vgagent tools` 的输出被逐字记录，其中的 VelaGuard 工具只有只读工具。
- F-05 按第 7 节 7.10 记录：完整工具清单没有板端只读接口，以硬白名单代码和主机回归为确定性证据。
- 审计日志含被拒绝记录，`rc` 为工具自身返回值，且记录中没有密钥正文。
- 读点工具在超过 20 次 / 60 秒后返回限流提示。
- F-03 以主机并发测试为确定性证据；若板端真实调用路径产生观察，只作为补充记录。

### G-05 回归

1. 在集成后的提交上重跑完整主机测试，目标数量以当前 `host_tests/Makefile` 为准，结果应为全部通过。
2. 重跑 `app/velaguard/host_tests/agent_review` 的 `test` 目标，结果应为全部通过。
3. 运行 `repro` 目标并记录结果。

通过条件：第 1、2、3 步全部通过。`repro` 由修复前的失败转为通过，作为 F-01 的回归证据。

---

## 9. 范围、完成状态与未做事项

### 9.1 本次复核已完成

- 阅读 AI-01、AI-02、AI-03 的实现与既有测试，确认其核心目标成立。
- 新增五组主机用例与一个复现程序，放在独立目录，未改动初版审查时的共享构建文件。
- 复核清单第 1 至 9 项逐项给出结论与证据。
- 发现 F-01 至 F-05，其中 F-01 有可执行复现。
- 公共提交 `c2ed337` 用 `O_NOFOLLOW` 修复 F-01，并在公共 `test_file_policy.c` 与 VelaGuard 专项回归中覆盖两种悬空软链接写入。
- 公共提交 `2a20a58` 修复 F-03 的原子限流记账；F-02 经 `bcec30a` 红测和 `8d16058` 完整参数修复，并把对应回归接入集成后的专项测试。
- F-04 在集成分支把最后回复接口改为锁内复制，并加入空指针、零长度和截断回归。
- 公共提交 `7dc0a88` 在 `CONFIG_VG_HMI` 下加入 F-05 硬白名单，注册层隐藏白名单外工具，执行层再次拒绝，并用两组公共回归证明配置只能收窄。
- 给出阶段 G 真机验收步骤、通过条件与未验证项目清单。

### 9.2 不属于本次复核、需要独立任务的事项

- **完整工具清单的板端只读接口**。当前没有安全接口列出全部注册工具；若后续需要直接目视核对，可作为独立诊断能力设计，不把新增接口作为 F-05 修复的前置条件。
- **板端验收本身**。本次未执行任何板端步骤，第 8 节是待执行的计划。

### 9.3 未做事项

- 初版审查未修改产品实现与公共依赖实现；F-01、F-02、F-03 的后续修复见 9.5。
- 未访问串口、开发板、真实 LLM 或 MQTT 服务；未复位、未烧录；未运行共享固件构建。
- 初版审查未修改 `../nuttx`、`../apps`、`../packages` 的实际 checkout；后续只在 `../packages/ai_agent` 的 VelaGuard 功能分支提交了 F-01、F-02、F-03，未修改 vendor 映射。
- 未修改共享维护清单 `docs/maintenance-todo.md`，未修改其他会话在 `docs/maintenance-evidence/2026-10-18/` 下的证据文件。
- 未读取或输出真实凭证，未改写任何历史原始日志。
- 未执行任何修改系统时间的命令。
- 未自行 push、合并或执行板端验收。

### 9.4 本版修订记录

本版先按以下五条意见修订第 8 节，再同步后续公共修复的实际状态：

| 意见 | 修订 |
|---|---|
| 1 移除向真实凭证读取、向真实点表写入、要求清告警或控制输出的步骤 | G-03 改为只读检查加未验证项目清单；G-04 第 6 步明确不发起这类请求，改由工具清单与代码确认；策略逻辑的证据移到主机隔离目录用例。同时修正一处错误：上一版把凭证 blob 的加密性列为已由 `test_provision_creds` 确认，实际该用例用桩替代了 seal 与 open，加密性改为未验证项，只保留代码层证据 |
| 2 `date -s` 可达性依据 C 闸门判断，不得实际执行改时间命令 | 第 7 节 7.7 改为发生率观察并声明与可达性无关；4.F-02 的可达性依据为闸门代码加 NSH 源码加 defconfig；8.0 第 3 条禁止执行改时间命令 |
| 3 `ps` 比较基线与新增残留任务，区分执行预算与回收容差 | G-01 改为基线快照加宽限期后对比，判据为相对基线的增量；执行预算与回收容差分成两条独立判据 |
| 4 generation 需说明真实观察方法 | G-02 第 4 步给出接口 `vgagent status` 的 `gen=` 字段与四个观察时点，并说明与点表代次的区别 |
| 5 区分本次审查完成与未来缺陷修复 | 新增 9.1 与 9.2；G-05 通过条件不再要求 F-01 已修复 |
| 6 公共补修完成后同步结论 | F-03 标记为 `2a20a58` 已修复；F-02 补记首次收紧的尾随参数缺口，并以 `bcec30a` 红测和 `8d16058` 完整匹配收口；F-05 改为代码层风险加运行时未验证；G-03、G-04、G-05 与测试清单按当前证据边界更新 |
| 7 F-04 快照接口补修 | 标记 F-04 已修复，记录锁内复制、公共容量常量、单次快照打印和专项测试边界，并补入原始证据路径 |
| 8 F-01 写入路径补修 | 标记 F-01 已修复，记录 `O_NOFOLLOW`、公共与专项回归、修前失败和修后通过证据，并更新 G-05 的预期结果 |
| 9 F-05 工具面补修 | 标记 F-05 已由 `7dc0a88` 修复，记录 VelaGuard 硬白名单、注册层与执行层双重约束、配置只能收窄、修前失败和修后通过证据，并更新 G-04、7.10 与测试清单 |

### 9.5 后续修复

公共 checkout `/home/hello19y/openvela/packages/ai_agent` 的 VelaGuard 功能分支 `velaguard/llm-tls-send-retry` 包含 F-01、F-02、F-03、F-05 的相关提交：

- `c2ed337`：`tool_write_file_execute()` 使用 `O_NOFOLLOW` 打开写入目标，公共 `test_file_policy.c` 增加末段悬空软链接指向沙箱外和受保护路径的回归。
- `2a20a58`：`src/tools/tool_shell.c` 首次增加只读参数闸门；`src/tools/tool_guard.c` 改为检查时原子预留额度，`src/tools/tool_registry.c` 删除成功后的重复记账；增加 F-03 的窗口上限、恢复和 24 线程并发回归。
- `bcec30a`：只增加 `date` 尾随 setter、尾随操作数、重复选项和空 format 的纯参数负向断言，保留修复前失败证据。
- `8d16058`：按完整参数序列收紧 `date_args_read_only()`，保留正常只读形式，拒绝 format 后附加的选项或操作数，并补齐制表符、尾随空白等正负用例。
- `7dc0a88`：在 `CONFIG_VG_HMI` 下把工具集合收窄为六个产品工具；白名单外工具不注册且执行层拒绝，运行时配置只能进一步禁用；增加 guard 与 registry 两组回归。

F-04 的锁内快照修复提交在集成分支 `codex/maintenance-closeout`，不修改公共 checkout。本集成 worktree 的 `agent_review/Makefile` 调用公共树中的 `test_tool_guard_rate.c`，VelaGuard 的 `test_shell_gate.c` 继续覆盖产品命令表。两类测试都只验证 C 逻辑，不执行改时命令，也不访问真实凭证或开发板。
