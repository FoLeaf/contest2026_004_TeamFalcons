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
| FAT 大小写不敏感不构成绕过 | `logical_path_allowed` 与 `path_under_resolved_root` 都做大小写敏感比较。受保护路径的大小写变体（如 `/data/velaguard/CONFIG/...`）会被判为受保护并拒绝，`reports` 的大小写变体会被判为不可写并拒绝。两个方向都偏向拒绝，属于安全的一侧。此项由源码推得，未在设备上验证。 |
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

以下结论只能在设备上取得，主机用例的通过不构成证据。

1. **超时后的后代进程回收**。主机分支用 `kill(-pid, SIGKILL)` 杀进程组（`tool_shell_runner.c:161`），NuttX 分支用 `kill(pid, SIGKILL)` 只杀任务本身（`:163`）。主机用例因此通过，而板端若某条命令自行派生了任务，该任务不会随超时被回收。产品侧当前经 `tool_shell_run` 的命令是 `date`、`heap_info`、`net_status`、`router_status`，需要确认它们是否会派生任务。
2. **`task_spawn` 与文件动作在板端的实际行为**。主机用 `posix_spawn` 与 `/bin/sh`；板端走 `task_spawn` 加 `posix_spawn_file_actions`，需要确认重定向后的管道读端在应用退出时确实产生 EOF，且返回的 pid 可被 `waitpid` 回收。
3. **`waitpid` 与 `CONFIG_SCHED_CHILD_STATUS` 的交互**。有界回收循环依赖 `ECHILD` 判定（`:169`），需要在板端确认保留退出状态时的语义。
4. **`usleep(1000)` 轮询的代价**。EOF 之后循环以 1 ms 步进轮询（`:134`），在按 tick 调度的 NuttX 上实际步进取决于 `CONFIG_USEC_PER_TICK`，需要确认这段时间内工具线程的 CPU 占用可接受。
5. **10 秒预算在真实工具线程上的可行性**。工具线程栈为 16 KB，运行器在栈上有 256 字节分块缓冲，需要在板端确认该路径不接近栈上限。
6. **轮次预算与真实 LLM 时延的关系**。`VG_AGENT_ROUND_TIMEOUT_MS` 为 300 秒，`vg_agent_round.c` 与 SDK 使用同一常量，需要实测确认两者不会互相抢先触发。
7. **代理能否在真实工具调用路径上构造出 `date -s` 参数**（F-02 的可达性）。需要观察 LLM 生成的工具参数。
8. **`/data` 是否确实无法承载软链接**（F-01 的可达性）。需要在板端实际尝试创建。
9. **FAT 大小写不敏感下的路径判定**（第 5 节的推论）。

---

## 8. 阶段 G 真机验收步骤

以下步骤只使用已获准的故障注入方式。不向设备端代理临时开放危险工具，也不通过执行真实危险命令来验证禁止规则。所有需要代理执行的命令都走正常交互渠道，且均为只读或本应被拒绝的调用。

前置条件：烧录包含本轮修复的固件；COM3 NSH 可用；模拟从站可用；保留 `docs/maintenance-evidence/2026-10-18/` 下的证据目录用于存放新证据。

### G-01 执行预算与回收（对应 AI-01）

1. 触发一次告警，使代理执行告警解释轮次。在串口日志中记录 `round submitted` 与 `round finished` 的时间差，以及该轮次内所有 `run_shell` 调用的日志行。
2. 在 NSH 中执行 `ps`，确认没有残留的 `agent_shell` 或 `nsh` 任务持有管道。
3. 通过正常交互渠道要求代理执行一条被禁止的命令（例如读取点表），确认闸门立即拒绝，且该轮次仍能正常结束。
4. 在 NSH 中反复读取 `vgruntime dump`，确认文件工作线程的 tick 计数在轮次期间持续增长。

通过条件：
- 日志中不出现任何 `run_shell` 调用超过 10 秒的记录；出现失败时，日志是运行器的 `command failed rc=-110 (10s limit)`，而不是长时间无输出。
- `ps` 中没有残留的 `agent_shell` 或 `nsh` 任务。
- 被拒绝的调用之后，紧接着的轮次能正常完成。
- `vgruntime dump` 的 tick 计数在整段过程中持续增长，无停滞。

已知限制：产品允许的命令集中没有会长时间阻塞的只读命令，因此无法在板端确定性地构造一次运行器超时。执行预算的确定性证据来自主机用例，板端只能提供未挂起与回收正常的观察证据。若需要板端确定性注入，需要先决定是否引入构建期测试钩子，本次不提出新增危险工具。

### G-02 轮次隔离与恢复（对应 AI-02）

1. 触发告警解释轮次，在轮次进行中从 NSH 发起一次人工轮次（`vgagent` 探针），确认返回忙状态。
2. 待告警轮次结束后，再发起一次人工轮次，确认其完成，且日报产物没有被人工轮次覆盖。
3. 在轮次进行中切断网络（关闭 ESP8266 或拔除网线），确认轮次在预算内进入错误状态，且通道随后可再次使用。
4. 记录 `vg_agent_round_generation` 的变化时机，确认只在收到回复时递增。

通过条件：
- 轮次进行中的人工请求被拒绝为忙，且不改变进行中的轮次状态。
- 人工轮次的结果不被日报流程读取，日报产物内容正确。
- 断网后轮次在预算内结束，随后一次正常请求成功。
- 代次只在回复到达时递增；错误结束不递增。

### G-03 文件与凭证保护（对应 AI-03）

1. 从 NSH 确认 `/data/velaguard/provision/llm_secrets.v1` 存在，且内容不是明文密钥（只核对形态，不复制内容到证据文件）。
2. 通过正常交互渠道要求代理读取该路径与 `/data/agent/config/config.json`，确认两次都被拒绝。
3. 要求代理写入 `/data/velaguard/config/points.json`，确认被拒绝；用 `vgpoint list` 确认已确认点表未变。
4. 触发日报流程，确认 `/data/velaguard/reports/` 下出现新的产物，内容为真实统计。
5. 在 NSH 中尝试在 `/data/velaguard/reports/` 下创建软链接。若创建失败，F-01 在当前文件系统上不可达；若创建成功，F-01 在板端可用，需要按缺陷处理。

通过条件：
- 凭证 blob 与代理配置的读取请求均被拒绝。
- 点表写入被拒绝且已确认点表内容不变。
- 日报产物正常落盘且内容来自板上统计。
- 软链接创建结果被记录，作为 F-01 可达性的判定依据。

### G-04 工具注册与权限（对应复核清单第 5、6、9 项）

1. 从 NSH 导出代理实际获得的工具清单，逐项记录。确认不存在点表编辑、配置生效、寄存器写入、清告警或输出控制类工具；并针对 F-05，记录清单中是否有 `fetch_url`、`web_search`、`cron_add` 这类与 VelaGuard 设计无关的条目，以及它们是否被产品配置禁用。
2. 用自然语言要求代理修改一个点位、清除一条告警或改变数字量输出，确认被拒绝，并用 NSH 确认状态未变。
3. 检查 `/data/velaguard/logs/agent_tools.log`，确认被拒绝的调用以 `rc=-2` 记录，且记录中没有完整密钥或口令。
4. 在短时间内连续要求代理读取点位，超过 `VG_TOOL_RATE_MAX_CALLS`（20 次 / 60 秒）后确认出现限流提示，且提示以正常结果返回而不是被当作未知工具。
5. 针对 F-03：连续发起应当失败的敏感工具调用，观察其是否占用限流额度。若始终不被限流，F-03 在板端得到确认。

通过条件：
- 工具清单不含禁止类工具。
- F-05 的清单结果被记录，并据此决定是否需要收窄内建工具集合。
- 越权请求被拒绝，且板端状态经 NSH 复核无变化。
- 审计日志含被拒绝记录，无密钥正文。
- 读点工具在超过额度后返回限流提示。
- F-03 的观察结果被记录。

### G-05 回归

1. 在复核所用提交上重跑完整主机测试（28 个 C 目标与 2 个 Python 检查），结果应为全部通过。
2. 重跑本目录的 `agent_review` 回归集与 `repro` 目标，确认 `repro` 在 F-01 修复前保持失败。

通过条件：主机全部通过；`repro` 的失败在 F-01 修复后被消除，且回归集保持通过。

---

## 9. 范围与未做事项

- 未修改产品实现与公共依赖实现。F-01 至 F-04 只提供复现与修复建议，留待单独集成修复。
- 未访问串口、开发板、真实 LLM 或 MQTT 服务；未复位、未烧录；未运行共享固件构建。
- 未修改 `../nuttx`、`../apps`、`../packages` 的实际 checkout，未修改 vendor 映射。
- 未修改共享维护清单 `docs/maintenance-todo.md`，未修改其他会话在 `docs/maintenance-evidence/2026-10-18/` 下的证据文件。
- 未读取或输出真实凭证，未改写任何历史原始日志。
- 未自行 push、合并或执行板端验收。
