# VelaGuard 维护 TODO

本文件是本轮审查与修复的唯一进度清单，不使用 Trellis 约束工作流。历史任务和用户提供的 2026-09-20 技术报告只作证据来源。更新日期：2026-10-18。

修复前基线为 17 个主机目标、5 个 HMI 用例通过。当前改动覆盖参赛仓及 `../nuttx`、`../packages/ai_agent` 实际 checkout；公共仓修复已分别本地提交，PR 仍待提交。原有日志、提交材料和其他任务的改动予以保留。

状态含义：代码完成 / 主机通过不等于真机验收完成。没有板端证据的项目不关闭。

| 编号 | 优先级 / 当前状态 | 已确认问题与代码证据 | 复现、进展、依赖与关闭条件 |
|---|---|---|---|
| ST-01 | P0 / 代码完成，待板验 | 点表、凭证 blob、config.json 直接截断旧文件。`app/velaguard/vg_file_store.c`、`vg_point_table.c`、`vg_provision.c` | 改前 `test_maintenance` 9项失败。已统一临时文件写全、同步、回读比较、旧文件备份及替换；模拟 NuttX 删除目标后 rename 失败时恢复旧文件。`test_file_store` 覆盖 ENOSPC、短写、回读、同步、rename、并发写入。复核又发现备份未回读，已补校验；定向注入只损坏备份写入，修前失败、修后通过。仍需新固件真机故障与掉电测试。 |
| ST-02 | P0 / 待真机定位 | 历史 eMMC 目录损坏、文件丢失。`../nuttx/arch/arm/src/stm32h7/stm32_sdmmc.c` | 当前 PIO FIFO monitor 已含历史临界区修复，不能再次认定同一根因未修。板已接，`vgstoretest 50` 与持续存储检查已有通过记录。另已按原理图/BSP确认并修正LCD占用SDRAM地址引脚，但历史eMMC损坏是否相关仍无充分证据；保持待定位，须真实故障/恢复及长期压力证据后关闭。 |
| AI-01 | P0 / 代码完成，待板验 | shell 工具 `fread/pclose` 无执行截止时间，历史记录有 vgmodbus 子进程不返回后队列满。`../packages/ai_agent/src/tools/tool_shell_runner.c` | 新执行器使用单调时钟、非阻塞管道、10秒执行预算和有界回收。产品 builtin 直接启动，避免只终止 NSH 包装任务；超限输出显式失败。主机已覆盖挂住、关闭stdout仍不退出、大输出、失败后的恢复和回收。NuttX终止行为仍需板验。 |
| AI-02 | P0 / 代码完成，主机通过，待板验 | 第二次请求覆盖回调、迟到回复误记到新轮次；轮次状态分步更新。SDK local client 与 `app/velaguard/vg_agent_round.c` | 改前 busy/迟到回调测试失败。SDK忙时返回EBUSY，独立请求标识过滤过期和重复回复；板端另以请求序号检查回调，在锁内完成状态转换。`test_local_client`、`test_agent_round` 已覆盖；新增progress标志，避免中间状态被当作最终回复。旧固件实测只读工具/LLM恢复，新错误注入仍待板验；NSH命令不变。 |
| AI-03 | P0 / 代码完成，主机通过，待板验 | 文件保护将 realpath 后路径与逻辑路径比较，软链接下可失效；不存在目标的父目录未核对。公共仓 `tool_files.c`、`tool_shell.c` | 配置和凭证禁止经文件工具读取；板端确定性数据禁止写入，报告产物保留受限写入。保护解析到父目录，覆盖软链接和别名；产品shell仅暴露元数据和只读子命令。回归同时验证合法日报/记忆仍可写，以及0/1/小缓冲精确canary。 |
| PT-01 | P1 / 代码完成，主机通过，待板验 | JSON字符串未转义、倍率精度丢失、损坏记录静默丢弃、空发现结果可覆盖表。`vg_point_table.c` | 复用现有cJSON完整解析/序列化，拒绝非有限数、非法字段、重复id、截断及超限；支持32个长名称点位往返。发现apply拒绝空结果，人工编辑删空仍沿用旧NSH协议。非法旧tag-only记录改为整表错误，已同步协议。 |
| PT-02 | P1 / **已定位并修复，板端复验通过** | 点表提交重置设备名，dump或扫描覆盖候选编辑；HMI在显示试读结果之前就能生效。`vgpoint.c`、`vgdiscover.c`、`vg_ui_backend_board.c` | 点表提交不再提交默认配置元数据；已有候选拒绝覆盖。扫描页仅生成候选，通过NSH试读后由人单独apply。落盘成功后才更换内存表、通知MQTT。**板验暴露 D-01：候选编辑恒失败。**根因是 `vg_file_store.c` 的备份临时名 `<path>.bak.tmp` 对 `point_table_candidate.json` 长 34 字符，超过板端 `CONFIG_FAT_MAXFNAME=32`，`open()` 返回 ENAMETOOLONG 后整次替换被放弃。已把后缀缩短为 `.btmp` 并加派生名预算校验与回归用例。板端复验：`add` → `OK cmd=add table=candidate n=15`，`del` → `OK cmd=del table=candidate n=14`，`list` 始终 `committed n=14`。详见 [defects.md](maintenance-evidence/2026-10-18/defects.md) 的 D-01。仍待实测人工确认与首页/上报一致性。 |
| DATA-01 | P1 / 代码完成，主机通过，待板验 | 内存点表及generation复制无互斥，读失败清空正在运行的点表；坏快照被当作空数据。`vg_point_table.c` | 加互斥保证表/代次成组复制，失败重载保留内存副本；快照校验声明数量、完整行、重复id和有限值。已覆盖10000轮并发替换/读取及坏快照；完整提交另见DATA-02。 |
| BUS-01 | P1 / 代码完成，待板验 | 发现CLI缺总线锁，HMI等锁无上限，vgmodbus在锁外打开/清空串口。`vgdiscover.c`、`vgscan.c`、`modbus_collector.c` | 获取锁最多等待1秒，串口打开/清空、读取与关闭均在锁内；CLI不再共用HMI可变发现结构。非法参数拒绝发送；真实并发帧与锁释放仍需板验。 |
| BUS-02 | P1 / 代码完成，主机通过，待板验 | qty让窄寄存器点误读失败、errno丢失、容量/部分失败不提示。`vg_discover_modbus.c` | 标量按实际1个寄存器读取，越界在I/O前拒绝，保存负错误码；限制与部分失败有标志，CLI/HMI显示提示。`test_discover_io` 使用真实nanoMODBUS和模拟RTU响应覆盖。保持固定9600、原默认扫描范围。 |
| BUS-03 | P1 / **已关闭** | `modbus_port_openvela.c` 在 `tcdrain` 后延迟并清空RX，会删除已经到达的有效回复。旧模拟器默认150ms回复绕过该缺陷。 | `test_modbus_port` 在drain期间注入回复，修前失败、修后10次均保留；移除发送后settle/flush，保留发送前清理。剩余驱动窗口在 2026-10-18 傍晚定位为 D-04：DIR 释放挂在 `up_txempty()` 上，而它由 `tcdrain()` 的轮询驱动，`nxsig_usleep(POLL_DELAY_USEC=1000us)` 向上取整到 `CONFIG_USEC_PER_TICK=10000` 的 10 ms tick，板端实测一次轮询 12.7-13.8 ms，TC 后最多约 13 ms 才释放 DIR。已把 DIR 释放挪到 TX 完成中断并删掉那里 400 µs 的忙等。修复后 0 ms 从站由 14%/67% 变为 649/649 全通，0/5/20/150 ms 四组 `boot_timeout=boot_crc=boot_echo=boot_other=0`、14 个点位全部在线。详见 [defects.md](maintenance-evidence/2026-10-18/defects.md) 的 D-04。 |
| AL-01 | P1 / 代码完成，主机通过，待板验 | 界面离线判断叠加连续失败与滑窗；测试规则与运行路径不同。`vg_alarm_eval.c`、`gui/main/ui/model/vg_model.c` | 共用纯C滑窗：最近8次真实采样、阈值min(max(fail_n,5),8)、成功采样恢复。改前2个边界测试失败，改后覆盖离散失败、连续失败、阈值与恢复；仍需实际拔线/阈值注入。 |
| NET-01 | P1 / 代码完成，主机通过，待新固件板验 | 长点表读取截断、发送失败误记成功；旧版本曾成功后，新版本发布失败不重试。`vg_mqtt_session.c` | 点表16KiB/发送18KiB，拒绝截断，仅入队成功才记已发。复核增加 `test_mqtt_session`，真实poll路径注入新版本发送失败，修前不重试、修后下个周期重试成功。清空此前成功标志，原有空表上报约定暂不改变；新固件需核对MQTT与采集表一致。 |
| QA-01 | P0 / 旧固件长测**失败归档**，新固件板验进行中 | 当前缺完整24小时及全部故障恢复证据。`scripts/maintenance_soak.ps1` | 旧固件长测 2026-10-17T16:40:30 启动，2026-10-17T19:53:57 因 COM3 端口被关闭异常退出，实际只跑 3 小时 13 分（13.4%），脚本写 `FAIL` 而非 `PASS`，**不得记为通过**。部分结果：57 轮全部 `online=14`、57 次 `vgstoretest 50` 全 OK、58 次点表读取恒为 `committed n=14`、堆 318,104→318,608 B 无增长、`uptime_s` 连续无复位。该长测用 13:03:04 旧固件与 150ms 慢速从站，不验证本次补修复。详见 [old-soak-verdict.md](maintenance-evidence/2026-10-18/old-soak-verdict.md)。剩余：新固件快速从站、候选编辑/试读后人工确认、crit与物理拔线、屏幕目视、断网/缺凭证/工具超时恢复、定时日报有效落盘、24小时零异常复位、响应/周期/资源前后对比。 |
| DATA-02 | P1 / 补修复完成，主机通过，待板验 | 文件替换锁在RAM换表前已释放，两个提交可让磁盘B而内存A。`vg_point_table.c`、`vgpoint.c` | 新增统一 `vg_point_table_commit`，在同一提交互斥区内落盘、换内存/代次、通知MQTT；启动重载也与提交互斥。`test_point_commit` 定向暂停A再提交B，修前不一致、修后通过；ENOSPC不更新内存/通知，显式空表仍兼容。依赖ST-01；需板上确认两入口一致。 |
| SYS-01 | P0 / 旧固件板验通过，长测中 | 系统入口实际2KiB栈，在刷新Skill时越界HardFault。defconfig、`velaguard.c`、`test_boot_stack.py` | 系统栈改8192并设构建检查，重烧后正常启动。保留复现日志；新修复仍需后续烧录及长期运行复核。 |
| SYS-02 | P0 / 引脚修复已板验，长期观察中 | LCD G0/G1误配PG0/PG1，占用SDRAM A10/A11，堆统计失真。NuttX板级头文件、`test_board_pins.py` | 本地官方MB1381 B-01原理图第3页U6B及ST BSP确认PJ7/PJ8；改后哨兵与堆统计正常。保留2MiB保守显示预留；构建前拒绝错误引脚。eMMC历史损坏仍见ST-02。 |
| DEP-01 | P0 / 本地依赖已提交，待公共仓PR | 仅参赛仓PR无法交付LCD及Agent修复；主机测试依赖公共仓新增源文件。 | NuttX本地提交 `75cbc8eb7a5`，ai_agent本地提交 `131642d`；构建前检查runner、回调关联与progress过滤等必要能力。按C2分别提PR并集成复验后关闭；本次未推送或发布公共仓PR。 |
| QA-02 | P0 / 重启原因待定位 | 旧板测14:28与15:20附近出现重启，仅时间上接近接线操作。 | 尚无复位原因寄存器或供电波形证据，不能认定为供电抖动或排除固件。保留日志，后续采集复位原因并结合24小时运行定位。2026-10-18 新固件另有一条可复现的软件崩溃路径，已定位为控制台线程栈溢出并修复（见 FS-01）；两次旧重启的日志里没有 HardFault 转储，与栈溢出是否相关无证据，不据此关闭本项。 |
| FS-01 | P0 / **根因已定位并修复，板端复验通过** | `/data/velaguard/discover/` 的元数据改写触发 HardFault 并导致整机失联。根因在 `velaguard.c` 的控制台线程栈，不在 vfat 或 eMMC。 | 新固件上复现 4 次：该目录内 `mv <文件> <已存在文件>` 或把该目录改名，都会 `CFSR=0x00008200`（PRECISERR+BFARVALID）、`BFAR=0x3b`、`HFSR=0x40000000`，故障指令在 `getpid()` 内，由 `nsh_command` 在 `handler()` 返回后调用。用 OpenOCD 复位下连接（`ST-LINK_gdbserver` 报 `Target no device found`，改用 `scripts/gdb/stlink-h7.cfg`）在 `arm_hardfault` 入口和 `tls_info_s->tl_task` 上取现场，抓到写入者：`sp` 落在栈分配最低地址 `0x38002be0`，写入值 `0x0b`，调用链为 `cmd_mv -> rename -> fat_rename -> fat_dirnamewrite -> fat_putlfname -> fat_findsfnentry -> fat_fscacheread -> fat_hwread -> mmcsd_read -> mmcsd_eventwait -> nxsem_wait_slow -> nxsched_switch -> SVC`。NuttX 把 `tls_info_s` 放在栈底，栈溢出先打坏 `tl_task`，随后 `getpid()` 解引用坏指针。板端实测该路径需要 **2048 字节**，而修复前可用 **2024 字节**（pthread 默认 2048 减 24 字节 `tls_info_s`），差 24 字节。对照：同样的 `mv` 在 `diagnostics/` 里监视点不触发。修复：新增 `CONFIG_VG_NSH_STACKSIZE`（默认 8192）并用 `pthread_attr_setstacksize` 设置控制台线程栈；`ps` 里 PID 4 从 `0002024` 变为 `0008168`。复验：同一串命令通过，`cat` 得到 `AAA`，填充法量出高水位 2048 / 可用 8168 字节，`tl_task` 前后均为 `0x38000580`。详见 [d02-root-cause.md](maintenance-evidence/2026-10-18/d02-root-cause.md)。 |
| SEC-01 | P1 / 新发现，待处理 | MQTT broker 口令以明文写在受版本控制的 `scripts/configs/velaguard-lvgl.defconfig`（键名 `CONFIG_VG_MQTT_PASSWORD`），并随 Kconfig 字符串进入固件镜像。 | 与 V4"密钥不得写进固件镜像"以及 AGENTS.md"完整密钥不得出现在日志、串口输出或 MQTT payload"的要求冲突；仓库与镜像同时持有该口令。本轮只做记录，未改动该值，也未在证据文件中复制口令正文。需要人工决定：改走 `vgprovision` 加密下发、还是至少把该值移出仓库并改为构建期注入。关闭条件：仓库与固件镜像中都不再含明文口令，且板端仍能连上 broker。 |
| RISK-01 | P2 / **已按此加固，问题确认成立** | 控制台 shell 线程栈原为 2024 字节。`velaguard.c` 用 `pthread_create(&tid, NULL, nsh_thread, NULL)`，取 `CONFIG_PTHREAD_STACK_DEFAULT=2048`。 | 这个判断是对的：FS-01 的根因就是该线程栈不足（见上）。已改为显式 8192 字节（`CONFIG_VG_NSH_STACKSIZE`），板端实测该路径高水位 2048 字节，余量 6120 字节。上一轮记录的"提到 8192 后签名不变"本轮无法重现，可能原因是那次增量构建没有真正改上去；本轮没有留下那次构建的 `.config` 或 `ps` 输出，不把它当已证实的结论。其余线程暂未调整：PID 7（ESP8266）、PID 8（netinit）仍是 2024 字节，但本轮没有证据说明它们会走到同样深的 vfat/SDMMC 路径。 |
| QA-03 | P1 / 漏提交已修正 | Makefile调用的两个Python回归脚本被通配ignore忽略，提交缺文件。`.gitignore`、`host_tests/Makefile` | 放行并提交 `test_boot_stack.py`、`test_board_pins.py`；28个C目标与2个Python检查重新构建执行通过。关闭条件：参赛仓提交实际包含二者；公共源码依赖仍见DEP-01。 |
| ARCH-01 | P1 / 部分完成 | 采集、文件和Agent调度仍集中在板端后端 | 已抽出可恢复文件替换、告警滑窗和shell执行器，删除重复JSON扫描及界面判定。其余文件工作与调度职责在稳定性板验后继续拆分，以行为回归约束改动。 |

## 后续功能与边界

| 编号 | 状态 | 内容、依赖及验收方向 |
|---|---|---|
| NEXT-01 | P1 / 后续设计 | 实际点记录双槽CRC/seq；当前双槽只保存配置元数据。依赖ST-02稳定，须验证断电选取完整旧版或新版。临时替换与备份不等于FAT掉电原子性。 |
| NEXT-02 | P1 / 协议草案，待实现 | MQTT候选接入取消试读，板端二次确认与最新受理版本/摘要绑定；修改使旧确认失效。新增入口需独立候选与配置会话互斥，现行NSH先试读再人工确认的步骤及命令格式保持。见 [候选协议](velaguard-candidate-mqtt-protocol.md)。 |
| NEXT-03 | P2 / 未实现 | 更多寄存器数据类型/字序、帧间隔违规和异常码单独计数。 |
| NEXT-04 | P2 / 未实现 | 诊断、日志、系统页和周报；依赖真实数据接口、存储及Agent稳定性。 |
| NEXT-05 | P2 / 未实现 | MQTTS、每设备鉴权；依赖部署证书和服务端策略。 |
| NEXT-06 | 后续设计，待实现 | OTA已改为MQTTS控制+HTTPS下载，见ADR 0006；依赖eMMC、片内启动与恢复验证，QSPI XIP运行中禁止擦写。旧MQTT-only分片方案已替代。 |
| NEXT-07 | 延期，待授权 | 确定性故障归因、屏幕任意配置编辑；手册上传解析已纳入目标，另见CFG-01。 |
| NEXT-08 | 配套仓后续 | Windows上位机与只读云看板独立审查。需评估当前空表不发布的MQTT合同对云端旧点位保留的影响，再确定清除策略。 |
| NEXT-09 | P1 / 待设计验证 | 快照与完整点表版本/本次启动标识的强绑定；需测试同id改寄存器后、重启后旧快照的隔离。当前只按采样tick计算age，并检查文件结构。 |
| CFG-01 | P1 / 通道已选择，协议草案，待实现 | 用户2026-10-18选择MQTT候选传输，异常与恢复由开发助手设计；按最新更正，手机接入不试读，板端配置校验后弹出二次确认，人工确认后直接添加，全部远程配置状态仅在手机显示。[ADR 0007](adr/0007-mqtt-candidate-import-with-local-confirmation.md)及[协议草案](velaguard-candidate-mqtt-protocol.md)已同步。手机PDF+从机地址上传、云侧解析、绑定鉴权、TLS/ACL、候选接收、配置状态、板端人工确认与NSH互斥均待实现及验收；添加成功与正常采集成功分别取证，不影响正在进行的板测固件。 |
| VOICE-01 | P1 / 已纳入目标，待设计与实现 | 手机主动录音/播放、云侧ASR/TTS、板载Agent只读查询；不经MQTT传AI请求或音频，不赋予配置及确认权限。 |
| OUT-01 | 不实施 | 旧MQTT AI Bridge、波特率矩阵、板端音频及语音改配置。不恢复废止架构。 |

## 验收记录

最新板端记录见 [2026-10-18 目录](maintenance-evidence/2026-10-18/)：`board-baseline.md`（新固件基线）、`defects.md`（D-01/D-02 两个缺陷）、`old-soak-verdict.md`（旧长测失败归档）。上一轮复核见 [review.md](maintenance-evidence/2026-10-17-review/review.md)。

- `make -C app/velaguard/host_tests clean test`：30个目标全部通过。
- HMI重新构建，CTest 5/5。
- 默认 `bash scripts/build.sh` 通过，本次增量构建无warning/error；静态SRAM 292,252/524,288。
- 新固件已烧录并核对哈希，启动基线、存储诊断、只读工具与禁止工具拒绝均已取证。
- 新固件发现两个 P0 缺陷，均已在本地定位：D-01 候选点表写入恒失败（PT-02），根因是备份临时名超过 `CONFIG_FAT_MAXFNAME=32`，已修复并板端复验通过；D-02 `discover` 目录元数据改写触发 HardFault 并导致整机失联（FS-01），已定位到该目录自身的磁盘状态，保留受损目录为 `discover_bad/` 并重建，现场复验正常，底层成因仍待查。
- D-01 修复后重新构建并烧录，在板固件为 `75cbc8eb7a5`，构建 2026-10-18 16:58:34（`--clean` 全量，6 处既有告警，改动文件无新增告警，SHA256 见 `firmware-hashes.txt`）。板上复验：`add` → `OK cmd=add table=candidate n=15`、`del` → `OK cmd=del table=candidate n=14`、`list` → `committed n=14`、`vgstoretest 10` → `OK rounds=10`。
- FS-01/D-02 根因定位：`ST-LINK_gdbserver` 对运行中的目标始终报 `Target no device found`，改用 OpenOCD 0.12（`scripts/gdb/stlink-h7.cfg`）复位下连接成功。在 `arm_hardfault` 入口和 `tls_info_s->tl_task` 上取现场，确认控制台线程栈耗尽、溢出打坏栈底的 `tls_info_s`。板端实测该 vfat rename 路径需要 2048 字节，修复前可用 2024 字节。
- FS-01/D-02 修复后重新构建并烧录，在板固件为 `75cbc8eb7a5`，构建 2026-10-18 17:43:24（`--clean` 全量，同样 6 处既有告警，SHA256 见 `firmware-hashes.txt`）。`ps` 确认 PID 4 栈由 `0002024` 变为 `0008168`；复现命令通过，`cat` 得到 `AAA`；填充法量出高水位 2048 / 可用 8168 字节，`tl_task` 前后未变。该构建已被下面 18:41:50 的 D-04 构建取代，D-02 的复测在那一版上重做并仍通过。
- 上一轮"提到 8192 后签名不变"的否定结果本轮无法重现，已在 RISK-01 中说明可能原因并标注为未证实。
- COM4（RS485 从站）此前被未知进程独占，`access denied`。2026-10-18 傍晚用户更换了收发器，新设备实际枚举为 **COM7**（USB-SERIAL CH340），不是 COM6；COM3（ST-LINK VCP）与 COM7 均已能打开。RS485 链路首次打通：从站在 20 s 内收到 69 个请求并全部应答，`crc_scan=0`。
- RS485 延迟工况实测（修复前，`75cbc8eb7a5` + COM7 新收发器）：从站回复 5 ms 时 1183/1183 成功、14 个点位全部在线；150 ms 时 265/265；20 ms 时 795/805；**0 ms 时两次只有 14% 与 67% 成功**。这一条记为 D-04，是 BUS-03 关闭条件里点名的剩余驱动窗口。
- D-04 根因与修复：DIR 释放挂在 `up_txempty()` 上，而它由 `tcdrain()` 轮询驱动，`nxsig_usleep(1000)` 向上取整到 10 ms 系统 tick，板端用 DWT 实测一次轮询 12.7-13.8 ms。已把 DIR 释放挪到 TX 完成中断（与上游 NuttX 一致），并删掉那里 400 µs 的忙等；`up_txempty()` 改回纯查询语义。同一版固件上 `up_udelay(400)` 实测 354-420 µs，标定是准的，问题不在那 400 µs。
- D-04 修复后重新构建并烧录，在板固件为 `43702275f28`，构建 2026-10-18 18:41:50（增量，0 告警，SHA256 见 `firmware-hashes.txt`）。复验：0 ms 从站 649/649 成功、5 ms 525/525、20 ms 471/471、150 ms 454/454，四组 `boot_timeout=boot_crc=boot_echo=boot_other=0`，14 个点位全部在线；同一版固件上 D-02 复测仍通过。
- 24 小时长测仍未开始。0 ms 快速从站现在可用，长测可以按 0 ms 从站跑；从站侧 `crc_scan` 在 20/150 ms 两组各有 1-2 次早期一次性事件，长测期间要继续观察这一项。
- 本次未修改真实候选表以外的配置，未执行 `vgpoint apply`。候选编辑试验只到写入失败为止，已确认表始终为 `committed n=14`。后续试读仍必须展示结果并停下等人确认，再单独发送 `vgpoint apply --confirm`。
