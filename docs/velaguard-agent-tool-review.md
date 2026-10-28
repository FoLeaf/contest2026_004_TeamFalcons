# Agent 工具执行、请求轮次与权限离线复核

复核对象：`AI-01`、`AI-02`、`AI-03` 三项维护清单条目的实现与测试覆盖。
复核日期：2026-10-18。
基线提交：`ff140dd`（fix(velaguard): RS485 DIR 释放挪进 TX 完成中断，D-04 关闭）。
工作区：独立 worktree，分支 `codex/agent-fault-review`，从上述提交创建，未带入其他会话的未提交改动。

本次为测试与审查任务，未修改产品实现，未修改公共依赖实现，未访问串口、开发板、真实 LLM 或 MQTT 服务，未运行共享固件构建。

---

## 1. 结论摘要

| 编号 | 级别 | 状态 | 结论 |
|---|---|---|---|
| F-01 | 中 | 未修复，有复现 | `write_file` 会跟随路径最后一段的悬空软链接写入，可在沙箱外或受保护路径上创建文件；策略检查看到的是链接自身路径。当前板端 `/data` 为 FAT，无软链接，暂不可达。 |
| F-02 | 低 | 未修复，源码可证 | 产品命令闸门只匹配命令名，不检查选项。`date` 在元数据白名单内，NSH 的 `date -s` 会设置系统时钟，经 `run_shell` 可达。系统时钟同时是 `tool_guard` 限流窗口的时基，前跳可重置限流额度。 |
| F-03 | 低 | 未修复，源码可证 | `tool_registry_execute` 只在工具返回 OK 时记一次调用，被拒绝或失败的工具调用不计入限流，因此反复失败的敏感工具不受限流约束。 |
| F-04 | 低 | 未修复，契约缺口 | `vg_agent_round_last_reply()` 不加锁返回共享缓冲区，诊断读取可能看到撕裂或已被新轮次清空的文本。仅影响日志与 NSH 回显。 |
| F-05 | 低 | 待板端确认 | 代理实际获得的工具集合是 ai_agent 的默认内建集合，注册表中除相机与 `run_shell` 变体外没有按 VelaGuard 需求收窄；`tool_guard_is_enabled` 在配置项缺失时返回启用。需要在板端确认产品配置是否逐项禁用。 |

AI-01、AI-02、AI-03 三项的核心目标经复核成立：执行预算与回收、轮次关联与迟到回复隔离、文件与凭证路径保护在主机上都已由新增用例证明。上述四项是与它们同一条调用链上的残余缺口，其中只有 F-01 有可执行的复现程序。

新增测试全部通过，未发现已有测试的回归：本 worktree 内 28 个 C 目标与 2 个 Python 检查全部通过。

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
- `resolve_target` 在整条路径不存在时回退到解析最深的已存在祖先（`:105`），这是覆盖父目录软链接的关键，也是 F-01 的来源。
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
| 6 代理能否触及点表、配置生效、寄存器写入、清告警、输出控制 | 命令侧已由新增闸门表覆盖：`vgpoint` 全部子命令、`vgdiscover apply`、`vgmodbus`、`vgcfg commit` 均被拒绝，路径前缀（`/bin/vgpoint`）与词前缀（`dumpsters`、`dum`）变体也被拒绝。文件侧由写保护覆盖。代理没有可写寄存器、清告警或控制数字量输出的工具。残余缺口见 F-02。 |
| 7 文件保护对软链接、路径别名、不存在目标与父目录的处理 | 已存在软链接、逃逸的父目录、含 `..` 的路径、不存在目标的父目录均处理正确；最后一段的悬空软链接处理错误，见 F-01。 |
| 8 合法报告产物写入是否仍可用 | 可用。日报、嵌套报告目录、代理自身记忆文件均写入成功，且内容确实落盘。 |
| 9 凭证与配置保护是否在 C 实现与工具注册层生效 | C 实现层已生效并有测试证据。工具注册层方面，`tool_guard.c` 提供输入长度上限、限流与脱敏，但限流的记账点存在缺口，见 F-03。 |

---

## 4. 缺陷与复现

### F-01 `write_file` 跟随末段悬空软链接写入沙箱外

**位置**：`packages/ai_agent/src/tools/tool_files.c`，`resolve_target`（:105）、`validate_path`（:126）、`is_write_protected`（:164）、`tool_write_file_execute`（:288）。

**机理**：策略检查调用 `resolve_target`。整条路径不存在时，它用 `realpath` 解析最深的已存在祖先，然后把剩余部分原样拼回：

```
realpath("data/velaguard/reports/a.md")  失败（悬空）
realpath("data/velaguard/reports")       成功
resolved = <绝对路径>/data/velaguard/reports/a.md
```

最后一段从未被解析，因此策略看到的是一个位于唯一可写子树内的路径。随后 `tool_write_file_execute` 对逻辑路径执行 `fopen(path, "w")`，该调用会跟随链接并创建目标文件。

**复现**：

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

case A 中沙箱外出现新文件，case B 中受保护路径上出现新文件，两者都是策略本应拒绝的写入。

**影响范围（实测，非推测）**：

- 只影响 `write_file`。`edit_file` 不受影响，因为它先以读方式打开文件，悬空链接读不到内容。
- 已存在的软链接指向已存在文件时处理正确，因为此时 `realpath` 能解析整条路径。
- 逃逸的父目录软链接处理正确，这正是 AI-03 修复覆盖的情形。
- 读取不受影响：已存在的链接会被解析并识别为受保护，悬空链接根本打不开。
- 目标所在的目录必须已存在，因为 `ensure_parent_dirs` 只沿逻辑路径创建目录。

**可达性**：板端 `/data` 是 FAT 卷，不支持软链接，当前不可达。若 `/data` 将来迁移到支持软链接的文件系统，或出现能创建软链接的工具，该缺口即变为可用。写入路径属于 AI-03 的修复范围，建议一并收紧。

**建议修复方向**（本次不实施）：让 `resolve_target` 也解析最后一段，或在 `lstat` 报告目标为软链接时拒绝写入。

### F-02 元数据白名单只匹配命令名，`date -s` 可改系统时钟

**位置**：`packages/ai_agent/src/tools/tool_shell.c:235` 至 `:246`（`CONFIG_VG_HMI` 分支的元数据白名单，只做 `strcmp(name, metadata[k])`）。

**证据**：`date` 在该白名单内（`tool_shell.c:239`）。NSH 的 `cmd_date` 支持 `-s` 设置系统时钟（`apps/nshlib/nsh_timcmds.c:373`，`date_settime`）。产品配置未关闭该命令：`scripts/configs/velaguard-lvgl.defconfig` 中无 `CONFIG_NSH_DISABLE_DATE`，`CONFIG_SYSTEM_POPEN=y` 且未设 `CONFIG_SYSTEM_POPEN_SHPATH`，因此 `tool_shell_runner.c:79` 走 `nsh_system` 分支，命令交给 NSH 解析执行。

**命令闸门的实测行为**：新增 `test_shell_gate.c` 的允许行包含 `date` 与 `date +%s`，证明闸门放行该命令名；同一张表也证明选项会原样通过（`free -h` 等）。为避免改动测试机时钟，`date -s` 未在主机上执行。

**可达性判定依据**：可达性由 C 闸门的匹配方式决定，不取决于 LLM 是否碰巧生成该参数。判据是四条源码事实：闸门只按命令名匹配（`tool_shell.c:238` 至 `:245`），`date` 在该名单内，NSH 的 `date` 支持 `-s`，且产品配置未关闭该命令。四条同时成立即说明该调用路径存在。代理是否真的用到它属于发生率问题，见第 7 节 7.7，与可达性结论无关；验证可达性不需要、也不允许实际执行修改时间的命令。

**连带影响**：`tool_guard.c` 的限流窗口以 `time(NULL)` 为时基（`tool_guard.c:108`、`:127`），`check_rate_limit` 用 `now - rs->timestamps[oldest] > window` 判断过期。系统时钟前跳会使全部记录立即过期，等于重置限流额度；后跳则会使记录长期不过期，在时钟追平之前持续拒绝敏感工具。此连带影响由源码推得，未在设备上验证。

**建议方向**：元数据命令按命令名加选项双重匹配，或从白名单中移除 `date`，或把限流时基改为单调时钟。

### F-03 敏感工具的限流只在成功时记账

**位置**：`packages/ai_agent/src/tools/tool_registry.c:632` 至 `:634`：

```c
int ret = s_tools[i].execute(input_json, output, output_size);
if (ret == OK) {
    tool_guard_record_call(name);
}
```

`tool_guard_check` 只判断是否已达上限，不记账；记账在 `tool_guard_record_call`，而这里只在成功时调用。因此 `run_shell`、`write_file`、`edit_file` 只要持续失败（被闸门拒绝、命令超时、退出码非零），就永远不占用限流额度。被拒绝的调用恰恰是最需要计入的那一类。

**未做离线复现的原因**：`tool_registry.c` 依赖工具模块与配置存储，为其搭建桩的成本高于该缺陷的收益。代码路径可直接从上述四行读出。

**建议方向**：把 `tool_guard_record_call` 移到 `tool_guard_check` 通过之后、执行之前，使尝试本身计数。

### F-04 最后一条回复的读取未加锁

**位置**：`app/velaguard/vg_agent_round.c:328`，`vg_agent_round_last_reply` 直接返回 `g_reply`，未持有 `g_lock`；写入方 `round_reply`（`:63`）与 `vg_agent_round_queue_owned`（`:147`，写入空串）都在锁内。

**影响**：调用方（`vgagent.c:199` 至 `:201`，NSH 回显与日志）可能读到撕裂的文本，或在新轮次入队后读到已被清空的串。属于诊断接口的契约缺口，不影响轮次状态机与结果归属。

**建议方向**：在锁内拷贝到调用方提供的缓冲区，或在头文件中明确标注该接口为尽力而为。

### F-05 内建工具集合未按产品需求收窄，待板端确认

**位置**：`packages/ai_agent/src/tools/tool_registry.c` 的 `tool_registry_init`（:225 起）。

**已核实**：内建工具表的注册大部分无条件执行，只有相机一组受 `CONFIG_AI_AGENT_CAMERA` 约束（:354 至 :367），`run_shell` 的变体受 shell 安全模式约束（:370 至 :393）。表中包含 `read_file`、`write_file`、`edit_file`、`list_dir`、`run_shell`、`cron_add`、`fetch_url`、`web_search`、`news_search`、`get_weather`、`get_current_time`、`analyze_image`、`feishu_*`、`music_*`、`get_battery`、`get_wear_state`、`get_screen_state`、`get_heartrate`、`vibrate` 等。`tool_guard_is_enabled` 在配置项不存在时返回启用（`tool_guard.c`，未配置即启用）。

**未核实**：这些工具在本板上是否真的可用（多数面向其他硬件），以及产品是否通过 `tool_enabled_<name>` 逐项禁用。两者都需要在板端确认。

**若成立的影响**：代理可达的工具面比产品设计描述的更宽。`fetch_url` 与 `web_search` 会带来设计文档未提及的外发网络路径；`cron_add` 允许代理安排延后执行的工具调用（`action` 为工具名，`action_args` 为其参数）。延后调用经同一注册表入口执行，因此仍受同一命令闸门与文件策略约束（此点未在板端验证）。

**对安全边界的影响**：以上工具都不能写 Modbus 寄存器、修改点表、清除告警或控制数字量输出，V5 与 V8 的安全边界不受该观察影响。

**板端确认方式**：见第 8 节 G-04 第 1 步。

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
| FAT 大小写不敏感不构成绕过 | `logical_path_allowed` 与 `path_under_resolved_root` 都做大小写敏感比较。受保护路径的大小写变体（如 `/data/velaguard/CONFIG/...`）会被判为受保护并拒绝，`reports` 的大小写变体会被判为不可写并拒绝。两个方向都偏向拒绝，属于安全的一侧。此项由源码推得，本次未在设备上验证；板端可用只读路径请求观察，见 G-03 第 6 步。 |
| `list_dir` 只遍历代理数据目录 | `tool_files.c:478` 以 `AGENT_DATA_DIR` 为根。代理无法枚举 `/data/velaguard`，符合最小权限；这是有意行为，不是缺陷。 |

---

## 6. 新增测试清单

目录：`app/velaguard/host_tests/agent_review/`。

| 文件 | 覆盖 |
|---|---|
| `Makefile` | 自带构建规则，不并入 `host_tests/Makefile`；`test` 目标为通过的回归集，`repro` 目标演示 F-01 |
| `test_runner_budget.c` | 持续输出下的截止判定、截止误差、后台后代回收、描述符与僵尸泄漏、被信号杀死记为失败、小缓冲区 canary、参数守卫 |
| `test_round_isolation.c` | 队列看门狗、跨轮迟到回复、重复回复、同步早到回复、提交失败隔离、`reclaim` 语义、代次计数、参数守卫 |
| `test_file_policy_extra.c` | 凭证与配置的读取保护（含软链接路径）、`edit_file` 策略一致、软链接目标、`..` 拒绝、嵌套报告写入、小缓冲区 canary、非法参数 |
| `test_shell_gate.c` | 产品命令闸门的完整双向表：拒绝项、允许项、子命令边界、shell 元字符、参数守卫 |
| `repro_symlink_write_escape.c` | F-01 的最小复现，预期失败 |

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
| 7.7 | 代理实际使用 `date -s` 的发生率。此项只影响该行为出现的频率，不影响 F-02 的可达性结论；可达性由 C 闸门判定，见 4.F-02 与 G-04。 | 可观察，但不需要为验证可达性而观察，也不得实际执行改时间的命令。 |
| 7.8 | `/data` 是否无法承载软链接（F-01 的可达性）。 | 部分可：可从挂载表读取文件系统类型后推断，见 G-03 第 4 步；直接验证需要向板写入，本计划不采用。 |
| 7.9 | FAT 大小写不敏感下的路径判定（第 5 节的推论）。 | 可，用只读路径请求观察拒绝结果，见 G-03 第 5 步。 |
| 7.10 | 内建工具集合在板端是否被逐项禁用（F-05）。 | 不可：`vgagent tools` 只打印 `"vg_` 开头的子集（`vgagent.c` 的 `print_tools`），没有打印完整工具清单的只读接口。F-05 因此保留代码层证据。 |

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
4. 从挂载表读取 `/data` 的文件系统类型，据此推断 F-01 是否可达。若挂载表不能确定类型，记录为未能判定。
5. 用只读路径请求观察策略对大小写变体的处理，作为第 5 节推论的补充证据。

通过条件：

- 凭证 blob 存在，且未被任何步骤读取内容。
- 已确认点表的条数与内容在整个过程中不变。
- 日报产物正常落盘，内容来自板上统计。
- `/data` 的文件系统类型被记录，并据此给出 F-01 可达性判定或未判定。
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
2. 内建工具集合无法用只读接口在板端列出（见第 7 节 7.10）。F-05 因此保留代码层证据：`tool_registry.c` 的内建表、`vg_agent_tools.c` 注册的两个只读工具，以及命令闸门 `tool_shell.c` 的 `s_vg_readonly` 表。第 6 节新增的 `test_shell_gate` 用例已把闸门的允许与拒绝两侧固定在主机上。
3. 为产生一条被拒绝的审计记录，通过正常交互渠道要求代理执行一条只读的被禁命令（`vgpoint get`）。随后检查 `/data/velaguard/logs/agent_tools.log`。注意 `rc` 列的含义（`tool_registry.c:70` 至 `:74`）：`0` 为成功，`-2` 为工具守卫层拒绝（例如限流），`-3` 为未知工具，其余为工具自身的返回值。命令闸门拒绝发生在 `tool_run_shell_execute` 内部，因此该条记录应为工具自身的返回值 `-1`，而不是 `-2`。确认该条记录存在，且记录中没有完整密钥或口令。这一步只使用只读被禁命令，理由见 8.0 第 4 条。
4. 在短时间内连续要求代理读取点位，超过 `VG_TOOL_RATE_MAX_CALLS`（`vg_agent_tools.c:397`，20 次 / 60 秒）后确认出现限流提示，且提示以正常结果返回而不是被当作未知工具。
5. 针对 F-03：`run_shell` 一类的敏感工具上限是 `AGENT_TOOL_RATE_LIMIT_MAX_CALLS`（`agent_config.h:415`，10 次 / 60 秒）。在限流窗口内连续发起只读的被禁命令调用，观察它们是否占用限流额度。若始终不被限流，F-03 在板端得到确认。
6. 不执行任何修改点表、清除告警或改变数字量输出的请求。代理是否具备这些能力由第 1 步的清单与第 2 步的代码确认，不通过实际发起请求验证。

通过条件：

- `vgagent tools` 的输出被逐字记录，其中的 VelaGuard 工具只有只读工具。
- F-05 按第 7 节 7.10 记录为无板端只读接口，保留代码层证据。
- 审计日志含被拒绝记录，`rc` 为工具自身返回值，且记录中没有密钥正文。
- 读点工具在超过 20 次 / 60 秒后返回限流提示。
- F-03 的观察结果被记录。

### G-05 回归

1. 在复核所用提交上重跑完整主机测试（28 个 C 目标与 2 个 Python 检查），结果应为全部通过。
2. 重跑 `app/velaguard/host_tests/agent_review` 的 `test` 目标，结果应为全部通过。
3. 运行 `repro` 目标并记录结果。

通过条件：第 1、2 步全部通过。第 3 步在本任务范围内的预期结果是复现程序失败，即缺陷仍然存在；消除该失败属于 F-01 的修复任务，不属于本次复核的完成条件。

---

## 9. 范围、完成状态与未做事项

### 9.1 本次复核已完成

- 阅读 AI-01、AI-02、AI-03 的实现与既有测试，确认其核心目标成立。
- 新增四组主机用例与一个复现程序，放在独立目录，未改动共享构建文件。
- 复核清单第 1 至 9 项逐项给出结论与证据。
- 发现 F-01 至 F-05，其中 F-01 有可执行复现。
- 给出阶段 G 真机验收步骤、通过条件与未验证项目清单。

### 9.2 不属于本次复核、需要独立任务的事项

- **F-01 至 F-05 的修复**。本次只提供复现与修复建议，不修改产品实现与公共依赖实现。每一项的修复需要独立的任务与提交。
- **板端验收本身**。本次未执行任何板端步骤，第 8 节是待执行的计划。
- **F-01 复现程序的失败**。该失败是缺陷仍然存在的证据，其消除属于 F-01 的修复任务。

### 9.3 未做事项

- 未修改产品实现与公共依赖实现。
- 未访问串口、开发板、真实 LLM 或 MQTT 服务；未复位、未烧录；未运行共享固件构建。
- 未修改 `../nuttx`、`../apps`、`../packages` 的实际 checkout，未修改 vendor 映射。
- 未修改共享维护清单 `docs/maintenance-todo.md`，未修改其他会话在 `docs/maintenance-evidence/2026-10-18/` 下的证据文件。
- 未读取或输出真实凭证，未改写任何历史原始日志。
- 未执行任何修改系统时间的命令。
- 未自行 push、合并或执行板端验收。

### 9.4 本版修订记录

第 8 节的验收计划按以下五条意见修订，测试代码与缺陷结论未变：

| 意见 | 修订 |
|---|---|
| 1 移除向真实凭证读取、向真实点表写入、要求清告警或控制输出的步骤 | G-03 改为只读检查加未验证项目清单；G-04 第 6 步明确不发起这类请求，改由工具清单与代码确认；策略逻辑的证据移到主机隔离目录用例。同时修正一处错误：上一版把凭证 blob 的加密性列为已由 `test_provision_creds` 确认，实际该用例用桩替代了 seal 与 open，加密性改为未验证项，只保留代码层证据 |
| 2 `date -s` 可达性依据 C 闸门判断，不得实际执行改时间命令 | 第 7 节 7.7 改为发生率观察并声明与可达性无关；4.F-02 的可达性依据为闸门代码加 NSH 源码加 defconfig；8.0 第 3 条禁止执行改时间命令 |
| 3 `ps` 比较基线与新增残留任务，区分执行预算与回收容差 | G-01 改为基线快照加宽限期后对比，判据为相对基线的增量；执行预算与回收容差分成两条独立判据 |
| 4 generation 需说明真实观察方法 | G-02 第 4 步给出接口 `vgagent status` 的 `gen=` 字段与四个观察时点，并说明与点表代次的区别 |
| 5 区分本次审查完成与未来缺陷修复 | 新增 9.1 与 9.2；G-05 通过条件不再要求 F-01 已修复 |
