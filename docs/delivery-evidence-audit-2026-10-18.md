# VelaGuard 交付与证据只读审计（2026-10-18）

本文件是一次**时间点快照**的只读审计结果，不实现功能、不操作开发板、不改变任何现有文档的结论。基线之后新增的提交、推送或验收都必须重新核对本文件的判断。

修订记录：初版提交 `8c46891`。本次修订修正了提交差距的方向、撤回若干超出本地证据的远端判断、更正凭据边界的处置方向、补全尾部杂字节的实测数据，并撤回日志采集中缺少依据的结论。原始日志未做任何改动。

## 1. 审计时间、基线与范围

| 项目 | 值 |
|---|---|
| 审计日期 | 2026-10-18（Asia/Shanghai） |
| 审计 worktree | `/home/hello19y/openvela/.vg-delivery-audit-wt` |
| 分支 | `codex/delivery-evidence-audit` |
| 基线提交 | `ff140ddc91035bfcc5b356d30f218a445c164f33`（参赛仓 `velaguard/maintenance-stability` 当时的已提交 HEAD） |
| 原工作区未提交改动 | 未复制进本 worktree；被删除的 `VelaGuard_推进方案.md` 未恢复 |
| 排除项 | 未访问串口、开发板、broker 与 LLM 服务；未烧录、未复位、未构建；未 push、未开 PR；未改写日志 |

范围内工作：

- 参赛仓当前 HEAD 的 manifest、构建脚本、验收证据与文档。
- 只读检查 `../nuttx`、`../apps`、`../packages`、`../packages/ai_agent`、`../apps/netutils/mqttc/MQTT-C` 的分支、提交与未提交改动。这些公共仓与另外两个 worktree（`codex/agent-fault-review`、`codex/soak-script-validation`）未被修改、未切分支、未清理。

范围外：未重新核验官方竞赛要求，本文件不构成合规证明。需要核对官方来源的争议见第 6 节末。

判定用词：

- **有证据**：本地文件或提交可直接支撑。
- **记录不足**：有相关记录但缺少重现所需字段。
- **未核实**：本地没有证据，或需要远端状态、真机或人工确认。本地 remote-tracking ref 只反映本地已知的推送历史，不等于远端当前状态。

## 2. 多仓库依赖表

采集时间 2026-10-18。公共仓路径相对 `/home/hello19y/openvela`。

| 仓库 | 分支 | 提交 | 未提交改动 | VelaGuard 依赖的具体能力 | manifest 覆盖 |
|---|---|---|---|---|---|
| 参赛仓 `contest2026_004_TeamFalcons` | `velaguard/maintenance-stability` | `ff140ddc91` | 原工作区有未提交日志与证据文件 | 产品代码 `app/velaguard`、`board/contest_board`、`quickapp/hello_quickapp` | manifest 的默认 revision 为 `dev-ai-contest-2026`；本地跟踪引用 `openvela/dev-ai-contest-2026` 停在 `0a25f7f`，是 `ff140dd` 的祖先，`ff140dd` 比它多 19 个提交 |
| `nuttx` | `velaguard/eth-mii-stm32h750b-dk` | `ff29d9b1a58` | 0 | RS485 DIR 在 TX 完成中断释放（`ff29d9b1a58`，即 D-04 修复）、控制台线程栈 defconfig（`43702275f28`）、SDRAM 引脚保持（`75cbc8eb7a5`）、QSPI XIP 启动、LTDC 加速、FT5x06 触摸、ETH MII PHY 轮询、SDMMC/eMMC | 否 |
| `apps` | `velaguard/netinit-esp8266` | `0ea954b797` | 3（均为 `testing/drivers/nist-sts/` 下未跟踪目录） | netinit carrier poll 与 IPv4 策略、ESP8266 LESP 兼容 | 否 |
| `packages` | `velaguard/agent-hmi-stack-heap-fix` | `927618869e` | 0 | 只提供 `packages/demos` 挂载点（参赛 app 软链目标）；相对本地跟踪引用 `openvela/dev-ai-contest-2026` 没有多出的提交 | 是 |
| `packages/ai_agent` | `velaguard/llm-tls-send-retry` | `131642da1d` | 0 | shell 执行边界（`tool_shell_runner.c`）、请求关联与进度过滤、`tool_files.c` 路径保护、SDK local client | 否 |
| `apps/netutils/mqttc/MQTT-C` | `velaguard/mqtt-pal-hook` | `4dbb7f321c` | 0 | `mqtt_pal.c` 的弱钩子 `vg_mqtt_pal_try_sendall` / `vg_mqtt_pal_try_recvall` | 否 |

依赖契约的权威枚举在 [ensure-upstream-velaguard-trees.sh:216-339](../scripts/ensure-upstream-velaguard-trees.sh)，构建前逐项 grep 校验。

### 2.1 linkfile 映射核对

[contest2026_004_TeamFalcons.xml:4-8](../contest2026_004_TeamFalcons.xml) 的三条映射，源目录均真实存在，本地软链均已建立：

| 源 | 目标 | 参与 VelaGuard 构建 |
|---|---|---|
| `app/velaguard` | `packages/demos/contest2026_004_hello_app` | 是 |
| `quickapp/hello_quickapp` | `packages/apps/contest2026_004_hello_quickapp` | 否，`build.sh` 未引用 |
| `board/contest_board` | `vendor/openvela/boards/contest2026_004_board` | 否，`board/contest_board/Kconfig:5` 门控 `ARCH_BOARD_CONTEST2026_000_BOARD`，主线 defconfig 用 `CONFIG_ARCH_BOARD_STM32H750B_DK=y` |

`scripts/` 下不存在 `openvela-*.patch` 或 `apply-openvela-*-patch.sh`，与 AGENTS.md C2 的禁令一致（`git ls-files` 只命中 `.cursor/rules/public-trees-no-patch.mdc`）。

### 2.2 仅 clone 参赛仓后缺失的公开提交

`scripts/build.sh:72-81` 依赖父级 openvela 工作区（`prebuilts/gcc`、kconfig 等），只 clone 参赛仓无法构建。除工作区之外，还缺以下未进入官方 manifest 的本地分支上的提交。

数字由 `git rev-list --left-right --count <引用>...<分支>` 得到。左列是跟踪引用独有的提交数，中列是该分支独有的提交数，右列同法对 `foleaf/<分支>` 计算。

| 仓库 | 分支 | 本地跟踪引用 `openvela/dev-ai-contest-2026` 独有的提交 | 该分支独有的提交 | 相对 `foleaf` 跟踪引用多出的提交 |
|---|---|---|---|---|
| `nuttx` | `velaguard/eth-mii-stm32h750b-dk` | 0 | 18 | 3（`ff29d9b1a58`、`43702275f28`、`75cbc8eb7a5`） |
| `nuttx` | `velaguard/integration` | 0 | 9，与 `eth-mii` 已分叉 | 本地没有该分支的 `foleaf` 跟踪引用 |
| `apps` | `velaguard/netinit-esp8266` | 0 | 1 | 0 |
| `packages/ai_agent` | `velaguard/llm-tls-send-retry` | 1 | 12 | 1（`131642da1d`） |
| `packages/ai_agent` | `velaguard/agent-hmi-fixes` | 1 | 6 | 1（`3257edd`），未出现在 README 的 PR 表 |
| `packages` | `velaguard/agent-hmi-stack-heap-fix` | 0 | 0 | 本地没有该分支的 `foleaf` 跟踪引用 |
| `apps/netutils/mqttc/MQTT-C` | `velaguard/mqtt-pal-hook` | 0 | 1 | 0 |

两点读法说明：

- 两个 `packages/ai_agent` 分支在跟踪引用里有 1 个提交是它们没有的，说明这两条分支与跟踪引用已经分叉，不是单纯的快进关系。
- 上表全部是**本地视角**。本地跟踪引用只反映本仓库上一次 fetch 时的状态，本地没有某条分支的 `foleaf` 跟踪引用，不能推出远端没有同名分支，也不能推出该分支从未推送过。本地 reflog 里有一条 2026-09-18 的 `update by push` 记录，只能说明本机当时执行过一次推送，既不代表远端当前状态，也不代表此后没有别的推送。

集成步骤（[build.sh:168-184](../scripts/build.sh)）：`ensure-openvela-links.sh` 建软链 → `apps/tools/mkkconfig.sh` 重生成 `packages/demos/Kconfig` → `ensure-upstream-velaguard-trees.sh` 校验能力，并把 `scripts/configs/*.defconfig` 复制到 nuttx 板级 `configs/`。`--sync-upstream` 会 `git checkout` 本地分支 `velaguard/integration`(nuttx)、`velaguard/netinit-esp8266`(apps)、`velaguard/mqtt-pal-hook`(MQTT-C)。

### 2.3 脚本与实际的偏差

- [ensure-upstream-velaguard-trees.sh:25](../scripts/ensure-upstream-velaguard-trees.sh) 期望 nuttx 在 `velaguard/integration`，实际工作树在 `velaguard/eth-mii-stm32h750b-dk`，两个分支已分叉。
- [push-upstream-prs.sh:48-93](../scripts/push-upstream-prs.sh) 只推送 nuttx 5 个 feature 分支加 apps 与 MQTT-C，**不含** `packages/ai_agent`、`packages` 与 nuttx `velaguard/wdog-activelist-recover`。README 的 PR 表已列 11 个 PR（含 wdog 与 3 个 ai_agent PR），脚本明显落后于 README。

### 2.4 主机测试对公共仓的硬依赖

`app/velaguard/host_tests/Makefile` 的 30 个目标中有 4 个直接编译 `packages/ai_agent` 的源码：`test_shell_runner`、`test_local_client`、`test_file_policy`、`test_shell_output`（`Makefile:120-149`）。只 clone 参赛仓时 `make -C app/velaguard/host_tests test` 无法跑通，这是 DEP-01 的具体形态。

## 3. 固件与证据对应表

逐次构建台账只有 [firmware-hashes.txt](maintenance-evidence/2026-10-18/firmware-hashes.txt) 一份。它记录了 5 次构建；16:52:43 的栈试验构建只在 [nsh-stackfix-disproved.log:97](maintenance-evidence/2026-10-18/nsh-stackfix-disproved.log) 留下 uname，没有哈希。

| 构建（内嵌时间） | uname 中的 nuttx 标识 | nuttx.hex SHA256 前 12 位 | nuttx.bin SHA256 前 12 位 | 源码是否已提交 | 烧录日志 |
|---|---|---|---|---|---|
| 14:37:55 | `75cbc8eb7a5` | `afd8f1e30d42` | `68b2f32f37d8` | 已提交（参赛仓 `31f1cdd`，见 [repos.txt](maintenance-evidence/2026-10-18/repos.txt)） | `flash.log` |
| 16:09:56 | `75cbc8eb7a5` | `90350897cae9` | `f38a5356ab61` | 未提交，D-01 修复在 16:47:03 才提交（`ae6f4da`） | `flash-d01-fix.log` |
| 16:52:43 | `75cbc8eb7a5-dirty` | 未记录 | 未记录 | 未提交，栈试验 | `flash-stack-experiment.log` |
| 16:58:34 | `75cbc8eb7a5-dirty` | `a2fbb03784f3` | `9f8f62a172f7` | 已提交（`ae6f4da`） | `flash-final.log` |
| 17:43:24 | `75cbc8eb7a5-dirty` | `b4c708943a1f` | `006eef2a8c23` | 未提交，FS-01 修复在 18:01:16 才提交（`60df66f` / nuttx `43702275f28`） | `flash-d02-fix.log` |
| 18:41:50 | `43702275f28-dirty` | `2964f76ed3a3` | `afdfd5767937` | 未提交，D-04 修复在 18:58 才提交（`ff140dd` / nuttx `ff29d9b1a58`） | `flash-d04-fix.log` |

uname 原文见 [board-baseline.md:9](maintenance-evidence/2026-10-18/board-baseline.md)、[defects.md:69](maintenance-evidence/2026-10-18/defects.md)、[defects.md:177](maintenance-evidence/2026-10-18/defects.md)、[d04-fixed-vgruntime-0ms.log:114](maintenance-evidence/2026-10-18/d04-fixed-vgruntime-0ms.log)、[nsh-final-committed.log:79](maintenance-evidence/2026-10-18/nsh-final-committed.log)、[nsh-stackfix-disproved.log:97](maintenance-evidence/2026-10-18/nsh-stackfix-disproved.log)。

### 3.1 同一 uname 标识对应不同产物【有证据】

uname 只编码 nuttx 树的提交与 dirty 标记，不编码参赛仓、apps、ai_agent 与 MQTT-C。已确认两组冲突：

| 冲突 | 标识 | 产物 |
|---|---|---|
| 参赛仓改动不进 uname | 14:37:55 与 16:09:56 两次构建的提交标识都是 `75cbc8eb7a5`，且都没有 dirty 标记 | 两份不同二进制：`68b2f32f37d8` 与 `f38a5356ab61`。原因是 D-01 修复位于参赛仓 `vg_file_store.c`，nuttx 树是干净的 |
| 同一 dirty 标识多次构建 | 16:52:43、16:58:34、17:43:24 三次构建的标识都是 `75cbc8eb7a5-dirty` | 至少三份不同产物，其中两份有哈希：`a2fbb03784f3` 与 `b4c708943a1f` |

完整的 uname 行还带内嵌构建时间，两个 14:37:55 / 16:09:56 可以靠时间区分。只按提交标识判断会合并成一份。

[maintenance-todo.md:62](maintenance-todo.md) 与 [:64](maintenance-todo.md) 把两次不同的在板固件都写作在板固件为 `75cbc8eb7a5`，只能靠构建时间区分，本身就是标识不足的旁证。

### 3.2 记录不足的部分

- **参赛仓提交号**：只有第 1 次构建有记录（`repos.txt` 的 `31f1cdd`）。第 2 至第 6 次没有记录，且对应修复提交都晚于构建时刻，说明取自未提交工作区，dirty 的具体内容没有留证。
- **apps / ai_agent / MQTT-C 提交**：只对第 1 次构建记录过一次（[firmware-hashes.txt:34-37](maintenance-evidence/2026-10-18/firmware-hashes.txt)），第 2 至第 6 次缺失。
- **产物哈希**：16:52:43 的栈试验构建与 2026-10-17 长测固件 `581e576b65a-dirty` 完全没有哈希。第 4、第 6 次构建缺 `qspi_bootstub.hex` 哈希。
- **烧录与产物的绑定**：`flash*.log` 只记录写入 `nuttx.hex`、QSPI 地址区间与 `Download verified successfully`（见 [flash-d04-fix.log:1-54](maintenance-evidence/2026-10-18/flash-d04-fix.log)），不含固件哈希，也没有 uname 输出。板端固件与某份哈希的对应关系靠同目录关联与文档叙述，不靠烧录日志本身。
- **构建日志**：`build*.log` 首行只有 TARGET、preset、MODE、DEBUG，不含提交号、dirty 状态或 defconfig 内容哈希。
- **配置来源**：统一来自 `scripts/configs/velaguard-lvgl.defconfig`（[build.sh:90-91](../scripts/build.sh)），其内容哈希只在 2026-09-15 记录过一次。

### 3.3 未核实

- 18:41:50 构建的 `43702275f28-dirty` 是否等于后来的 `ff29d9b1a58`，本地没有 `.config` 或 diff 可核对。**按现有记录该固件不可精确重现。**
- 2026-09-13/14 的 `18bec160782` 系列（`report-evidence.json:181`、`hardfault-20260914.log:2`）缺 defconfig、dirty 状态与产物的对应记录。

## 4. A-H 验收矩阵

### 4.1 定义出处

A-H 的完整定义**不在任何已提交文档中**。已提交文档只有零散引用：[board-baseline.md:1](maintenance-evidence/2026-10-18/board-baseline.md)（阶段 B）、[defects.md:13](maintenance-evidence/2026-10-18/defects.md)（阶段 D）、[com4-blocker.md:46-49](maintenance-evidence/2026-10-18/com4-blocker.md)（阶段 C/D/F/H）。

完整定义出现在维护任务的原始提示词里，被 AI Coding 采集器写入 `logs/Foleaf/2026-10-18/claude-code__450e3df9-48f2-4ccc-bd86-841dd9d6862a.jsonl`。该文件在本审计基线时**尚未提交**，本文件不复制其正文，只记录该定义来自任务提示。这是证据链上的一处弱点：验收阶段的口径没有进入仓库的文档体系。

### 4.2 矩阵

状态取值：代码完成 / 主机测试通过 / 真机通过 / 部分通过 / 未验证 / 历史失败或已被替代 / 进行中。

| 阶段 | 状态 | 关键证据 | 缺口 |
|---|---|---|---|
| A 接续检查与旧长测归档 | 部分通过（归档类，无功能验收含义） | [old-soak-verdict.md](maintenance-evidence/2026-10-18/old-soak-verdict.md)、[com4-blocker.md:1-56](maintenance-evidence/2026-10-18/com4-blocker.md)、[reset-log.txt](maintenance-evidence/2026-10-18/reset-log.txt) | 无 |
| B 构建、烧录与启动基线 | **真机通过** | [board-baseline.md:9-27](maintenance-evidence/2026-10-18/board-baseline.md)（烧录与 uname）、[:40-62](maintenance-evidence/2026-10-18/board-baseline.md)（启动基线）、[:30-38](maintenance-evidence/2026-10-18/board-baseline.md)（主机 30 目标） | 板端设备名显示为 `vgpoint`（观感问题，[:64-74](maintenance-evidence/2026-10-18/board-baseline.md)） |
| C RS485 与采集 | 部分通过 | [d04-fixed-vgruntime-0ms.log:114](maintenance-evidence/2026-10-18/d04-fixed-vgruntime-0ms.log)（`boot_total=649 boot_ok=649 boot_timeout=0`）、[d04-fixed-0ms-slave.log](maintenance-evidence/2026-10-18/d04-fixed-0ms-slave.log) | 已执行的复验为 0/5/20/150 ms 四组（0 ms 649/649、5 ms 525/525、20 ms 471/471、150 ms 454/454，见 [maintenance-todo.md:69](maintenance-todo.md)），本审计按记录接受这四组。另有验收口径差异单列于 6.4，不据此否定已完成复验。BUS-01、BUS-02 的板端并发与参数校验仍待验 |
| D 点表确认与持久化 | 部分通过 | [defects.md:65-78](maintenance-evidence/2026-10-18/defects.md)（`add` n=15、`del` n=14、`list` 恒为 `committed n=14`） | 板端 `vgpoint test` 试读与 `apply --confirm` 未执行，确认后首页与 MQTT 一致性未验；证据中没有板端 test/apply 记录 |
| E 存储与故障恢复 | 部分通过 | [board-baseline.md:78](maintenance-evidence/2026-10-18/board-baseline.md)（`vgstoretest 50` OK） | 短写、ENOSPC、备份损坏等故障注入只有主机证据，板端注入缺失；MQTT 重发板验待做 |
| F 告警与恢复 | **未验证（新固件）** | 旧固件记录 [board-history.md:32](maintenance-evidence/2026-10-17/board-history.md)（warn 与模拟静默） | 新固件无 warn/crit 阈值注入、无物理拔线、无屏幕目视；旧记录只有 warn 与模拟静默，且属旧固件 |
| G Agent、网络与 MQTT | 部分通过，另有进行中 | [board-baseline.md:84-89](maintenance-evidence/2026-10-18/board-baseline.md)（工具注册与 C 层拒绝）、[:93-113](maintenance-evidence/2026-10-18/board-baseline.md)（日报校验、`alarm_advice.txt` VGADV1 格式） | 断网、MiMo 不可达、凭证缺失恢复、工具超时回收的板验待做；LLM 看门狗超时见 4.4 |
| H 新固件 24 小时运行 | **未验证（未开始）**；旧长测为历史失败 | [maintenance-todo.md:70](maintenance-todo.md)（长测仍未开始）、[old-soak-verdict.md](maintenance-evidence/2026-10-18/old-soak-verdict.md) | 新固件 24 小时长测未启动 |

只有阶段 B 可判真机通过。主机单测 30 个目标通过（[maintenance-todo.md:57](maintenance-todo.md)）与编译通过都不计入真机通过。

### 4.3 重点项

- **D-01 候选写入恒失败**：有证据。根因是 `CONFIG_FAT_MAXFNAME=32` 下备份临时名派生长度 34（[defects.md:11-87](maintenance-evidence/2026-10-18/defects.md)），板端复验见 `defects.md:65-78` 与 `nsh-fix-verify*.log`。
- **D-02 HardFault**：有证据。根因是 NSH 控制台线程栈 2024 字节不足，修复后 8168 字节（[d02-root-cause.md](maintenance-evidence/2026-10-18/d02-root-cause.md)、`gdb-*.log`）。板端 `ps` 中 PID 4 由 `0002024` 变为 `0008168`。
- **D-04 快速从站丢帧（`ff140dd` 声明关闭）**：有证据支撑关闭该时序缺陷。[d04-fixed-vgruntime-0ms.log](maintenance-evidence/2026-10-18/d04-fixed-vgruntime-0ms.log) 显示 0 ms 从站 649/649、四组延迟 `boot_timeout=boot_crc=boot_echo=boot_other=0`、14 个点位全部在线。
- **尾部杂字节仍是观察项**：[defects.md:362-365](maintenance-evidence/2026-10-18/defects.md) 记录两层内容。当前这支收发器上的实测：0 ms 与 5 ms 两组全程 `crc_scan=0`，20 ms 组出现 1 次、150 ms 组出现 2 次，都落在开始后 20 至 90 秒内，之后 600 多个请求不再增加，按启动瞬态处理，未当作稳定现象，也没有证据说与本次改动相关。适用范围：`crc_scan=0` 的结论只对当前这支收发器成立，未覆盖历史上出问题的那支适配器，并保留了换回旧适配器时症状可能重现的判断。该观察项未闭环，需要在长测中继续观察。
- **D-03 明文口令**：[defects.md:216-226](maintenance-evidence/2026-10-18/defects.md) 与 [maintenance-todo.md:31](maintenance-todo.md) 的 SEC-01 记录：受版本控制的 `scripts/configs/velaguard-lvgl.defconfig` 含字面量键 `CONFIG_VG_MQTT_PASSWORD`，该文件被复制为板级配置并参与编译，字符串进入固件镜像，与 V4 的密钥边界冲突。按用户 2026-10-18 说明，本项已选定运行时加密存储的方向，由其他会话跟进；本审计只记录现状，不评估其实现，也不为它另提方案。审计另发现同一份 defconfig 还含 `CONFIG_VG_WIFI_PASSWORD`，这是独立于 D-03 的待办，本文件只记录其存在，不为它指定处置方案。两处值均未在本文件与证据文件中复制。
- **eMMC 损坏（ST-02）**：待定位。[maintenance-todo.md:12](maintenance-todo.md) 记录当前不能再次认定历史临界区为同一根因，缺真实故障与长期压力证据。
- **不明重启（QA-02）**：待定位。[maintenance-todo.md:29](maintenance-todo.md)、[board-history.md:22](maintenance-evidence/2026-10-17/board-history.md) 记录两次旧重启，无复位原因寄存器或供电波形证据。
- **旧长测**：已按失败归档。[old-soak-verdict.md:1-9](maintenance-evidence/2026-10-18/old-soak-verdict.md) 说明只跑 3 小时 13 分（13.4%），脚本只在跑满后写 `PASS`，归档中只有一条 `FAIL`，未记为通过。
- **报告与告警解释类文件的证明边界**：[docs/velaguard-agent-query-acceptance.md:8-17](velaguard-agent-query-acceptance.md) 定义 A1-A8 断言，其实测记录为 2026-09-17 旧固件（[:183-190](velaguard-agent-query-acceptance.md)，`pass=37 fail=0`），**不能证明新固件通过阶段 G**。[docs/submission/tech-report.md:400](submission/tech-report.md) 自认 24 小时稳定性、物理告警注入与恢复测试仍待完成，不构成逐项验收证据。[board-baseline.md:93-113](maintenance-evidence/2026-10-18/board-baseline.md) 证明当日日报经板端校验落盘、`alarm_advice.txt` 为 VGADV1 格式，但那些离线告警来自从站缺失，不来自物理拔线。
- **公共依赖对他人复现的影响**：第 2.2 与 2.4 节列出的、未进入官方 manifest 的公共仓分支与 ai_agent 依赖，使第三方在只有参赛仓与官方 manifest 时无法构建固件，也无法跑通 30 个主机目标中的 4 个。这里说的是 manifest 未覆盖这些提交，与远端是否已经收到它们无关。

### 4.4 进行中

- 主仓未跟踪的 `docs/maintenance-evidence/2026-10-18/d05-*.log`（`d05-baseline.log`、`d05-mqtt-baseline.log`、`d05-mqtt-wildcard.log`、`d05-tables-raw.log`，2026-10-18 19:06 至 19:11）。`d05-baseline.log` 记录 `LLM watchdog: call took 128150 ms (limit 120s) treating as timeout`，属阶段 G 的真实问题。同时 `d04-fixed-150ms-slave.log` 有未提交修改。这些改动不在本审计基线内，**不提前判定通过**。
- worktree `codex/soak-script-validation`（`/home/hello19y/openvela/.vg-soak-wt`）有未提交的 `scripts/maintenance_soak.ps1` 修改。
- worktree `codex/agent-fault-review`（`/home/hello19y/openvela/.vg-agent-review-wt`）工作区干净，无领先提交。
- 审计进行期间（19:17 至 19:20），主工作区又新增 `docs/maintenance-evidence/2026-10-18/d06-01-baseline.log`、`d06-02-mqtt-before.log`、`d06-03-hmi-model-before.log`，属另一个会话的 D-06 工作，均未提交。`d06-01-baseline.log` 在基线里直接采集 `uname -a` 输出，方向与第 6.1 节第 2 步一致。本审计不评估这些文件的内容。
- D-03 明文口令按用户 2026-10-18 说明已选定运行时加密存储方向，由其他会话跟进，本审计不重复处置、不评估其实现进度。

以上进行中的条目全部晚于本审计基线，需在集成前重新核对。

## 5. 文档冲突及建议修改位置

以下区分历史归档与当前状态描述。历史归档（`MIGRATED_FROM_RESTART.md`、`VelaGuard_推进方案.md`、`docs/learn/*`、`docs/backend-api.md`）按项目规则不要求批量重写，但活动入口文档中的过时表述应更正。

| # | 主题 | 现有说法 | 权威来源 | 建议修改位置 | 严重度 |
|---|---|---|---|---|---|
| 1 | OTA 仍写 MQTT 分片 | [README.md:241](../README.md) 把 MQTT-only OTA 列为阶段 3 并引 ADR 0005 | [ADR 0006](../docs/adr/0006-mqtts-control-https-download-ota.md)、手册 §16.9、CONTEXT.md | README 该行改引 ADR 0006，措辞改 MQTTS 控制加 HTTPS 下载 | 高 |
| 2 | OTA 分片仍作现行设计 | [tech-report.md:154](../docs/submission/tech-report.md) 写应用经 MQTT 分片把新镜像拉取到 eMMC，`:34`、`:439` 引 ADR 0005 | ADR 0006 | 技术报告 OTA 段落改写 | 高 |
| 3 | AI Bridge 仍作现行或规划 | [docs/plan-cloud-backend.md:1](../docs/plan-cloud-backend.md)、`:68-75`、`:102` 把 Bridge 当现行方案；[作品介绍文档-大纲.md:71](../docs/submission/作品介绍文档-大纲.md) 列云端 Bridge 规划 | CONTEXT.md（AI Bridge 已移除，不恢复）、AGENTS.md V4/V10 | 大纲删除该规划；plan-cloud-backend 标历史 | 高 |
| 4 | learn 版后端文档缺历史标注 | [docs/learn/backend-api.md:3](../docs/learn/backend-api.md) 称适用范围为当前仓库中的 AI Bridge | [docs/backend-api.md:3](../docs/backend-api.md) 已有历史快照标注，仓库无 ai_bridge 代码 | 给 learn 版补历史快照标注 | 中 |
| 5 | ADR 0002 未标已替代 | [ADR 0002](../docs/adr/0002-mqtt-broker-and-ai-bridge-instead-of-board-direct-mimo.md) 通篇把 MQTT 加 AI Bridge 当决策 | CONTEXT.md、ADR 0005 的已替代范式 | ADR 0002 顶部加已被取代的标注 | 中 |
| 6 | NSH 协议自相矛盾 | [velaguard-host-nsh-protocol.md:439](../docs/velaguard-host-nsh-protocol.md) 写本次不做 MQTT 云端下发点表，`:20` 写不经 MQTT 下发配置 | 同文件 `:214` 已承认 ADR 0007 的候选通道 | 在 §1 与 §11 补注候选通道为后续例外 | 中 |
| 7 | PR 数量不一致 | [tech-report.md:170](../docs/submission/tech-report.md)、`:242` 写 9 个 PR | [README.md:49](../README.md)、`:524` 的 PR 表列出 11 行 | 统一为 11；大纲 §9 的该行作废 | 中 |
| 8 | 主机测试目标数 | [README.md:244](../README.md)、`:320` 写 25 个目标 | [maintenance-todo.md:57](maintenance-todo.md) 写 30；Makefile 实为 28 个 C 目标加 2 个 Python 目标 | README 改 30 | 中 |
| 9 | 技术报告把已移除与待实现混列 | [tech-report.md:400](../docs/submission/tech-report.md) 把 Bridge 与语音并列写为尚未交付 | Bridge 已移除；语音为已授权待实现 | 拆开表述 | 中 |
| 10 | 推进方案版本与基准过时 | [VelaGuard_推进方案.md:3-5](../VelaGuard_推进方案.md) 写版本 v3、基准手册 v3、2026-08-29 | 手册现为 v3.6 | 顶部加历史标注，或更新基准 | 低 |
| 11 | 断链引用 | [demo-video-script.md:338](../docs/demo-video-script.md)、[plan-cloud-backend.md:4](../docs/plan-cloud-backend.md) 引不存在的 `docs/plan-917-submission.md` | 文件缺失 | 删除引用或补文件 | 低 |
| 12 | ADR 编号错引 | [plan-cloud-backend.md:39](../docs/plan-cloud-backend.md) 称新 ADR-0006 为 LLM 直连 MiMo | 实际 ADR 0006 为 OTA | 更正编号 | 低 |
| 13 | 边界文档旧指针 | [velaguard-expansion-board.md:8](../docs/velaguard-expansion-board.md) 引 BOUNDARY V11；`docs/learn/*` 引 BOUNDARY C2 | BOUNDARY.md 只保留来源与编号索引，正文在 AGENTS.md | 指针改指 `AGENTS.md#v11`、`#c2` | 低 |
| 14 | 日志目录 README 仍是模板措辞 | [logs/README.md](../logs/README.md) 称本目录现在是示例，请替换成真实日志并删掉示例的 `your-github-login/` | 实际 `logs/Foleaf/` 已有 289 个会话 | 更新该段为实际状态 | 低 |
| 15 | 验收手册过时数字 | [velaguard-agent-query-acceptance.md:185](../docs/velaguard-agent-query-acceptance.md) 写 17 个目标 | 现为 30 | 标注该记录日期，不改数字 | 低 |

一致且正确、无需修改的部分：手机接入、语音巡检、MQTTS 与 OTA 在所有活动文档中均标为待实现或规划，没有被写成已完成；`docs/agents/BOUNDARY.md` 只作来源与编号索引，未承载已废止的规则正文；README 的目录结构、`scripts/build.sh` 默认目标 `velaguard-lvgl`、GUI 构建路径与 manifest 的 linkfile 映射与实际仓库一致。

## 6. 集成与交付顺序、前置条件与仍缺的证据

### 6.1 本地可继续的工作

这些工作只依赖本地检出，不需要先推送。推送与否不影响它们的可行性。

1. **统一分支口径**。`ensure-upstream-velaguard-trees.sh` 期望 nuttx 在 `velaguard/integration`，实际工作树在 `velaguard/eth-mii-stm32h750b-dk`，两分支已分叉。需要明确哪一个是交付分支，并让脚本与文档一致。
2. **补全追溯字段的构建流程**。让 `build.sh` 或烧录脚本记录参赛仓与各公共仓提交号、dirty 状态、defconfig 哈希与固件哈希，并让烧录日志记录被烧写镜像的哈希与板端 uname 输出，使板端固件与产物能强绑定。审计期间另一个会话已在 `d06-01-baseline.log` 里直接采集 `uname -a`，方向与此一致。
3. **按 6.4 的清单补齐板端与主机证据**。这些项都可以在本地检出上推进。

### 6.2 交付与发布面的顺序

以下涉及远端与交付物，按依赖关系排列。它们不是 6.1 各项的前置。

1. **先落定官方时间线**。AGENTS.md C4 记录的提交截止是 2026-09-20，而本地工作持续到 2026-10-18，本地分支比跟踪引用多 14 个提交。本审计不核验官方公告，也不判断是否已逾期；在推进推送与 PR 之前，需要先按官方来源确认当前有效的截止时间与提交范围。
2. **推送参赛仓分支**。本地 `velaguard/maintenance-stability` 相对本地跟踪引用 `foleaf/velaguard/maintenance-stability` 多 14 个提交，相对 `openvela/dev-ai-contest-2026` 多 19 个提交。D-01、FS-01、D-04 三个修复与全部 2026-10-18 证据都在其中。远端当前是否已收到这些提交，本地无法判断。
3. **推送公共仓承载能力的 feature 分支**。清单见第 2.2 节，含 nuttx `velaguard/eth-mii-stm32h750b-dk` 与 `velaguard/wdog-activelist-recover`、`packages/ai_agent` 的两条分支、`packages` 的 `velaguard/agent-hmi-stack-heap-fix`。
4. **修正并补齐 PR 脚本**。`push-upstream-prs.sh` 缺 `packages/ai_agent`、`packages` 与 nuttx wdog 分支，与 README 的 11 个 PR 表不一致；补全后再按 C2 提 PR。
5. **解决 manifest 与实际依赖的落差**。`openvela.xml` 固定 `dev-ai-contest-2026`，不含第 2.2 节的任何公共仓改动。要么在交付文档中固定记录每个公共仓的确切提交号，要么提供可用的 manifest revision。否则第三方无法复现构建。
6. **凭据边界**。D-03 已按用户说明选定运行时加密存储方向并由其他会话跟进，本审计不重复处置，也不另提方案。`CONFIG_VG_WIFI_PASSWORD` 是独立待办，需要单独决定。
7. **补齐交付物**。仓库内没有演示视频（无 `.mp4` 或 `.mov`）；技术报告、README 的过时数字按第 5 节更正。

### 6.3 前置条件

- 构建需要父级完整 openvela 工作区，只 clone 参赛仓不够。
- 主机测试的 30 个目标中有 4 个依赖打过补丁的 `packages/ai_agent` 检出。
- 公共仓改动按 C2 走 fork 加 PR，不使用补丁；当前 `scripts/` 已无补丁文件，符合要求。
- 真机验收需要 RS485 从站。设备在 2026-10-18 傍晚更换收发器后枚举为 COM7，不再是 COM4 或 COM6。

### 6.4 仍缺的证据

下列各项分别列出，彼此不互相代替。

| 缺口 | 对应阶段或条目 | 需要的证据 |
|---|---|---|
| 新固件告警与恢复 | F | warn/crit 阈值注入、单从站与全部静默、恢复、`ge/le/eq` 覆盖、物理拔线、屏幕目视 |
| 新固件 24 小时运行 | H | 24 小时零异常复位长测；[maintenance-todo.md:70](maintenance-todo.md) 说明长测可按 0 ms 从站跑 |
| 板端存储故障注入 | E | 短写、ENOSPC、备份损坏的板端注入与掉电测试 |
| 板端试读与确认 | D | `vgpoint test` 试读、人工确认、`apply --confirm`，以及确认后首页与 MQTT 一致性 |
| 验收口径差异 | C | 核对 [com4-blocker.md:46](maintenance-evidence/2026-10-18/com4-blocker.md) 写的 0/5/30/150 ms 与实际执行的 0/5/20/150 ms 之间的差异，确认验收条件以哪个为准。已执行的 0/5/20/150 ms 四组按记录有效，不因这项差异作废 |
| 尾部杂字节闭环 | C | 在历史出问题的适配器上复测，或明确该观察项的适用范围；当前收发器上 20 ms 出现 1 次、150 ms 出现 2 次 `crc_scan` 仍需在长测中观察 |
| BUS-01、BUS-02 板端验证 | C | 真实并发帧与锁释放、越界参数在 I/O 前被拒绝 |
| Agent 与网络恢复 | G | 断网、MiMo 不可达、凭证缺失、工具超时回收的板验；当前 LLM 看门狗超时由其他会话跟进 |
| 存储与重启根因 | ST-02、QA-02 | eMMC 损坏的真实故障与长期压力证据；两次重启的复位原因与供电波形 |
| 凭据边界 | D-03、WiFi 口令 | D-03 由其他会话按运行时加密存储方向跟进；`CONFIG_VG_WIFI_PASSWORD` 需要单独决定 |
| 可重现性 | 第 3 节 | 各次构建的参赛仓与公共仓提交号、dirty 状态、产物哈希与烧录绑定 |
| 演示视频 | 交付物 | 不超过 5 分钟的演示视频 |

### 6.5 需要核对官方来源的争议

- AGENTS.md C4 记录的提交截止 2026-09-20 与本地持续到 2026-10-18 的开发之间的关系。本文件不引用旧记录冒充最新结论，需要在官方公告中确认当前有效的时间线与提交范围。
- AGENTS.md 记录官方资料上次核查日期为 2026-09-13。本审计未重新核验官方竞赛要求，第 5 节与本节都不构成合规证明。

## 7. 附：日志采集现状（只报告，未修改）

- 采集覆盖 2026-06-21 至 2026-10-18，manifest 登记 289 个会话，其 `health` 字段全部为 `ok`（[manifest.json](../logs/Foleaf/manifest.json)）。工具分布：claude-code 75、codex 64、cursor 64、opencode 50、grok-build 21、zcode 16。`health` 是采集器写入的自报字段，本审计未执行健康检查，因此它不构成日志完整的证据。
- **缺口**：2026-09-21 至 2026-10-16 没有任何会话目录。无提交不等于无开发，提交历史不能用来推断该区间有没有开发活动，因此本审计既不断言采集器漏采，也不断言采集与开发一致。这是一个尚未解释的观察项。
- **未入仓**：2026-10-18 的会话 `logs/Foleaf/2026-10-18/claude-code__450e3df9-48f2-4ccc-bd86-841dd9d6862a.jsonl` 在原工作区处于未跟踪状态，`manifest.json` 有未提交修改，2026-10-17 的一个会话文件也有未提交追加。这些改动不在本审计基线内，manifest 因此仍在变化。
- **历史错误**：`logs/Foleaf/errors/` 有 4 条 2026-08-28 的 `manifest_read` 错误（`Expecting value: line 1 column 1`），来自 cursor 工具读取当时的 `manifest.json`。该目录被 `.gitignore` 的 `logs/**/errors/` 规则忽略，不随仓库提交，也就不会出现在交付物里。本审计未运行健康检查，不判断这些错误是否已解决。
- `.gitignore` 第 88-93 行放行 `logs/` 与 `logs/**`，只忽略 `logs/**/errors/`，与 C5 的不得忽略 `logs/` 一致。
- `secrets/` 下只有 `secrets/agent_llm.key.example` 被版本控制，未发现密钥文件入仓。
- **未执行**：本审计没有运行 `verify-setup.sh` 健康检查，也没有运行官方日志校验命令 `python3 ../.claude/skills/contest-log-collector/tools/validate-log.py logs/`。因此本文件不对采集配置健康、日志完整性或日志校验结果作任何结论。同时未修改、未补写、未重新导出任何日志文件。
