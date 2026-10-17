# VelaGuard 维护 TODO

本文件是本轮审查与修复的唯一进度清单，不使用 Trellis 约束工作流。历史任务和用户提供的 2026-09-20 技术报告只作证据来源。更新日期：2026-10-17。

修复前基线为 17 个主机目标、5 个 HMI 用例通过。当前改动覆盖参赛仓与 `../packages/ai_agent` 实际 checkout；公共仓改动尚未提交 PR。原有日志、提交材料和其他任务的改动予以保留。

状态含义：代码完成 / 主机通过不等于真机验收完成。没有板端证据的项目不关闭。

| 编号 | 优先级 / 当前状态 | 已确认问题与代码证据 | 复现、进展、依赖与关闭条件 |
|---|---|---|---|
| ST-01 | P0 / 代码完成，待板验 | 点表、凭证 blob、config.json 直接截断旧文件。`app/velaguard/vg_file_store.c`、`vg_point_table.c`、`vg_provision.c` | 改前 `test_maintenance` 9项失败。已统一临时文件写全、同步、回读比较、旧文件备份及替换；模拟 NuttX 删除目标后 rename 失败时恢复旧文件。`test_file_store` 覆盖 ENOSPC、短写、回读、同步、rename、并发写入。仍需真机故障与掉电测试。 |
| ST-02 | P0 / 待真机定位 | 历史 eMMC 目录损坏、文件丢失。`../nuttx/arch/arm/src/stm32h7/stm32_sdmmc.c` | 当前 PIO FIFO monitor 已含历史临界区修复，不能再次认定同一根因未修。已提供 `vgstoretest` 读写/备份比较与长跑脚本；需接板、原理图/BSP交叉核对、压力测试证据后修实际驱动。当前不作推测性驱动修改。 |
| AI-012 | P0 / 代码完成，待板验 | shell 工具 `fread/pclose` 无执行截止时间，历史记录有 vgmodbus 子进程不返回后队列满。`../packages/ai_agent/src/tools/tool_shell_runner.c` | 新执行器使用单调时钟、非阻塞管道、10秒执行预算和有界回收。产品 builtin 直接启动，避免只终止 NSH 包装任务；超限输出显式失败。主机已覆盖挂住、关闭stdout仍不退出、大输出、失败后的恢复和回收。NuttX终止行为仍需板验。 |
| AI-02 | P0 / 代码完成，主机通过，待板验 | 第二次请求覆盖回调、迟到回复误记到新轮次；轮次状态分步更新。SDK local client 与 `app/velaguard/vg_agent_round.c` | 改前 busy/迟到回调测试失败。SDK忙时返回EBUSY，独立请求标识过滤过期和重复回复；板端另以请求序号检查回调，在锁内完成状态转换。`test_local_client`、`test_agent_round` 已覆盖；NSH命令不变。 |
| AI-03 | P0 / 代码完成，主机通过，待板验 | 文件保护将 realpath 后路径与逻辑路径比较，软链接下可失效；不存在目标的父目录未核对。公共仓 `tool_files.c`、`tool_shell.c` | 配置和凭证禁止经文件工具读取；板端确定性数据禁止写入，报告产物保留受限写入。保护解析到父目录，覆盖软链接和别名；产品shell仅暴露元数据和只读子命令。回归同时验证合法日报/记忆仍可写，以及0/1/小缓冲精确canary。 |
| PT-01 | P1 / 代码完成，主机通过，待板验 | JSON字符串未转义、倍率精度丢失、损坏记录静默丢弃、空发现结果可覆盖表。`vg_point_table.c` | 复用现有cJSON完整解析/序列化，拒绝非有限数、非法字段、重复id、截断及超限；支持32个长名称点位往返。发现apply拒绝空结果，人工编辑删空仍沿用旧NSH协议。非法旧tag-only记录改为整表错误，已同步协议。 |
| PT-02 | P1 / 代码完成，待板验 | 点表提交重置设备名，dump或扫描覆盖候选编辑；HMI在显示试读结果之前就能生效。`vgpoint.c`、`vgdiscover.c`、`vg_ui_backend_board.c` | 点表提交不再提交默认配置元数据；已有候选拒绝覆盖。扫描页仅生成候选，通过NSH试读后由人单独apply。落盘成功后才更换内存表、通知MQTT。需实测候选编辑、人工确认、首页和上报的一致性。 |
| DATA-01 | P1 / 代码完成，主机通过，待板验 | 内存点表及generation复制无互斥，读失败清空正在运行的点表；坏快照被当作空数据。`vg_point_table.c` | 加互斥保证表/代次成组复制，失败重载保留内存副本；快照校验声明数量、完整行、重复id和有限值。已覆盖10000轮并发替换/读取及坏快照。 |
| BUS-01 | P1 / 代码完成，待板验 | 发现CLI缺总线锁，HMI等锁无上限，vgmodbus在锁外打开/清空串口。`vgdiscover.c`、`vgscan.c`、`modbus_collector.c` | 获取锁最多等待1秒，串口打开/清空、读取与关闭均在锁内；CLI不再共用HMI可变发现结构。非法参数拒绝发送；真实并发帧与锁释放仍需板验。 |
| BUS-02 | P1 / 代码完成，主机通过，待板验 | qty让窄寄存器点误读失败、errno丢失、容量/部分失败不提示。`vg_discover_modbus.c` | 标量按实际1个寄存器读取，越界在I/O前拒绝，保存负错误码；限制与部分失败有标志，CLI/HMI显示提示。`test_discover_io` 使用真实nanoMODBUS和模拟RTU响应覆盖。保持固定9600、原默认扫描范围。 |
| BUS-03 | P1 / 已定位，待修复 | 板端发送后约2-3ms接收盲区（tcdrain轮询+DIR保持+settle+tcflush），响应快于约3ms的真实从站会整帧丢失。`modbus_port_openvela.c`写后flush、`../nuttx/drivers/serial/serial.c` uart_tcdrain | 板测定位与缓解见板端进展三。修复：去掉写后`tcflush(TCIFLUSH)`并回归模拟快从站（`<3ms`延迟）用例；驱动侧改动按C2走fork+PR。mock侧已用`-ReplyDelayMs 150`规避。 |
| AL-01 | P1 / 代码完成，主机通过，待板验 | 界面离线判断叠加连续失败与滑窗；测试规则与运行路径不同。`vg_alarm_eval.c`、`gui/main/ui/model/vg_model.c` | 共用纯C滑窗：最近8次真实采样、阈值min(max(fail_n,5),8)、成功采样恢复。改前2个边界测试失败，改后覆盖离散失败、连续失败、阈值与恢复；仍需实际拔线/阈值注入。 |
| NET-01 | P1 / 代码完成，待板验 | 长点表可能被MQTT读取截断，发布失败仍标记已发送。`vg_mqtt_session.c` | 点表文件/载荷上限统一16KiB，发送缓冲18KiB；拒绝截断读取，仅成功入发送队列才标记。为完整32点字段组合增加缓冲预算，BSS比本轮中间构建增加16KiB，需板端资源复核。 |
| QA-01 | P0 / 板测进行中 | 缺当前固件24小时、物理注入与恢复证据 | 板已接 COM3/COM4，13:04 与 16:23（SWD 受控复位）各烧录/重启一轮。已板验：启动、存储、内存、采集 100%（盲区缓解后）、阈值/离线告警注入与恢复、告警 AI 解释、带工具日报、LLM 端到端对话、MQTT 上行。剩余：24 小时 soak（16:43 已启动，COM3 独占）、期间零非预期复位确认、屏幕告警页人工目视。两次疑似供电抖动重启（14:28、15:20）发生在接线操作时段，soak 期间不再动硬件。 |
| ARCH-01 | P1 / 部分完成 | 采集、文件和Agent调度仍集中在板端后端 | 已抽出可恢复文件替换、告警滑窗和shell执行器，删除重复JSON扫描及界面判定。其余文件工作与调度职责在稳定性板验后继续拆分，以行为回归约束改动。 |

## 后续功能与边界

| 编号 | 状态 | 内容、依赖及验收方向 |
|---|---|---|
| NEXT-01 | P1 / 后续设计 | 实际点记录双槽CRC/seq；当前双槽只保存配置元数据。依赖ST-02稳定，须验证断电选取完整旧版或新版。临时替换与备份不等于FAT掉电原子性。 |
| NEXT-02 | P1 / 后续，保持旧协议 | 试读结果与候选版本绑定。本轮不增加强制试读校验；需另定上位机兼容方案。 |
| NEXT-03 | P2 / 未实现 | 更多寄存器数据类型/字序、帧间隔违规和异常码单独计数。 |
| NEXT-04 | P2 / 未实现 | 诊断、日志、系统页和周报；依赖真实数据接口、存储及Agent稳定性。 |
| NEXT-05 | P2 / 未实现 | MQTTS、每设备鉴权；依赖部署证书和服务端策略。 |
| NEXT-06 | 延期，待授权 | MQTT-only OTA；依赖eMMC、片内启动与恢复验证，QSPI XIP运行中禁止擦写。 |
| NEXT-07 | 延期，待授权 | 确定性故障归因、屏幕配置编辑、手册上传解析。 |
| NEXT-08 | 配套仓后续 | Windows上位机与只读云看板独立审查。需评估当前空表不发布的MQTT合同对云端旧点位保留的影响，再确定清除策略。 |
| NEXT-09 | P1 / 待设计验证 | 快照与完整点表版本/本次启动标识的强绑定；需测试同id改寄存器后、重启后旧快照的隔离。当前只按采样tick计算age，并检查文件结构。 |
| OUT-01 | 不实施 | 语音、旧MQTT AI Bridge、波特率矩阵。README旧规划已校正，不恢复废止架构。 |

## 验收记录

本轮本地证据位于 [maintenance-evidence/2026-10-17](maintenance-evidence/2026-10-17/)，保留原始命令输出，不能替代板端记录：

- `make -C app/velaguard/host_tests test`：25个目标；新增故障写入、解析、并发、Modbus边界、shell执行、回调和路径保护回归。
- HMI源码重新构建及 `ctest --test-dir .debug/hmi-headless --output-on-failure`：5/5。
- 维护回归以AddressSanitizer和UndefinedBehaviorSanitizer运行：通过，未报告越界或未定义行为。
- `bash scripts/build.sh`：默认velaguard-lvgl，最终输出和告警情况见build.log。链接占用为静态布局，不表示实际运行堆栈余量；修复前真机性能未重新测量。
- 真机 `vgpoint test` 输出必须展示，随后停下等待人确认，才可单独发送 `vgpoint apply --confirm`。本轮未执行真实试读或apply。

### 板端进展（2026-10-17 13:00 起，COM3/COM4 已接）

以下为真机观察，原始输出在上述证据目录：

- 启动崩溃修复已板验：`CONFIG_INIT_STACKSIZE=8192` 重烧后正常启动，启动任务刷新设备 Skill 不再 HardFault（flash-child-status.log）。
- LCD G0/G1 引脚修复（PG0/PG1 改 PJ7/PJ8）后，内存统计自洽：total=7,014,016、free 约 6.65MB，堆尾哨兵完整；`vgstoretest` 50 轮写读备份比较两次通过。历史 SDRAM 别名问题与此吻合，但长期证据继续由 soak 累积。
- `SCHED_CHILD_STATUS` 等配置补齐后，板端 Agent shell 工具成功执行只读 `vgcfg dump`，禁用的 `vgpoint` 仍被拒绝（shell-child-status-retry.log）。
- 共地修复前的过渡窗口内，板端真实产生离线告警 raise/clear 事件（`modbus_fail_offline`、`offline_recovered`，见 soak-smoke-console.log），滑窗判定在真机上工作；真实拔线与阈值注入仍待做。
- 485 链路：人工修复共地后，RTU 从站模拟累计 9031 请求、9031 回复、0 CRC 错误（rtu-slave-status.log）。板端采集在线数恢复情况待 COM3 空闲后复查（13:14 冒烟时 online=3，为共地修复前数据）。
- MiMo：PC 侧域名已解析到真实 IP 且 HTTPS 可达，DNS 例外生效；板端 `nslookup` 与 LLM 对话待 COM3 空闲后复验。
- `scripts/maintenance_soak.ps1`：语法解析通过，冒烟 1 轮通过；24 小时长跑尚未开始。
- COM3 于 14:50 起被串口工具占用，板端复查暂停，等待释放。

### 板端进展二（2026-10-17 15:00-15:45，网络恢复与采集排查）

- 板子 14:28 与 15:20 前后各重启一次，均与现场接线/拔插时段吻合，疑似供电抖动而非固件崩溃；24 小时验收要求零非预期复位，期间须避免动板端供电与接线，重测时继续观察。
- 板端断网根因：主机侧连接板子的有线网卡被禁用（Realtek Code 22），后经用户换接 USB 网卡恢复；ICS 192.168.137.1 在新网卡上生效，板端 DHCP、出口 ping、DNS 全部恢复。MiMo 域名板端解析到真实 IP（220.181.104.191），TLS 握手与 LLM 对话端到端通过（问 1+1=2 答 2），时间同步恢复。
- COM4 反复被占的机制：scomm 串口工具会对重新插拔的串口自动重连；PnP 重启 MacroSilicon USB 串口设备可释放。已按用户先前授权结束 scomm 进程。
- 采集 ok 率仍低（12%-16%，超时为主）且与从站实现无关：PowerShell 从站与 Modbus Slave 原生从站结果一致，多数成功来自第二次尝试（延迟 300-800ms，即第一次读超时后由重试收割迟到回复）；单次新鲜扫描会话可稳定发现从站。板端驱动历史记录（docs/velaguard-bringup-known-issues.md 第4节）已确认：LA 证明板端 DI/DIR 时序正确后，A/B 乱码归因于 USB-RS485 收发器，换适配器后误码大幅下降。当前 COM4 适配器为 MacroSilicon MS2109 方案，列为头号嫌疑。
- 待办：换用可靠 USB-RS485 适配器后复测采集 ok 率与告警注入；`VGMODBUS_BYTE_TO_MS=20` 的字节间隔超时对 USB 虚拟从站偏紧，若换适配器后仍复现再评估放宽。

### 板端进展三（2026-10-17 16:05-16:45，采集根因定位与验收注入）

- **采集 ok 率根因定位**：裸收发与 30ms 换向测试 6/6、3/3 全过，适配器与板端收发器均排除。真正根因是板端发送后的接收盲区（约 2-3ms：`uart_tcdrain` 以 `POLL_DELAY_USEC=1ms` 轮询 `up_txempty`，加上 400µs DIR 保持、500µs settle 和 `tcflush(TCIFLUSH)`）：快从站回复（Modbus Slave 约 2-8ms）落进盲区整帧丢失，慢从站回复（旧 PS 从站 300-700ms）超出 500ms 读超时，第二次尝试只能收割迟到帧，两种从站都表现为约 90% 首试失败。验证与缓解：PS 从站新增 `-ReplyDelayMs`（默认 150ms）落在盲区后、读超时前，板端 `vgmodbus` 换向测试与采集随即 100% 通过（重启后 bus 290/290，warn 注入期间 1311/1311 全 ok）。
- **遗留固件项（新）**：盲区对响应快于约 3ms 的真实从站仍会丢帧。后续修复方向：去掉写后 `tcflush(TCIFLUSH)`（它在 DIR 落地后还会吞掉早到回复）、评估在 `uart_tcdrain` 等待 TC 时缩短轮询或由驱动 TC 中断直接通知；涉及公共 NuttX 仓改动按 C2 走 fork+PR。
- **告警注入全部通过**：warn（temp=30 越 warn=28 ge）raise、恢复后 threshold_cleared、slave 静默后 14 点离线 raise、恢复后 offline_recovered。advice 轮次由真实告警触发，agent 经只读工具取数并产出 `VGADV1` 解释（引用实测 30.0、阈值 28、通信成功率 100%），写入 `/data/velaguard/reports/alarm_advice.txt` 并挂接告警集。
- **日报与带工具 ask 通过**：`vgagent ask` 触发 ReAct 循环，读取设备 Skill `operations_report.md`、调用 `vg_run_report`，生成中文日报（2 轮迭代、88.7s LLM）。
- **运维教训（新增）**：flat 构建下 LVGL 初始化标志为全局状态，`kill vghmi` 后新实例会因 `lv_is_initialized()` 静默退出，NSH 无 reboot 命令；恢复手段是 `.debug/nsh_reset_only.ps1` SWD 硬复位。后续如需停采集，优先设计运行时开关，不走 kill。
- **24 小时 soak 已启动**（16:43，`-MinimumOnline 14 -IntervalSec 120`），首轮 CYCLE 1 通过（online=14，内存 total 7,014,016 / used 约 318KB）；从站以 30 小时寿命覆盖 soak 窗口。发现故障自动停止，不自动复位。期间 COM3 由 soak 独占，避免动板端供电与接线。
