# 阶段 F 阈值告警与恢复：工况准备与验收清单

本清单只负责把阶段 F 的阈值告警与恢复验收工况准备好，并给出可重复的观察方法。
清单本身不代表任何一项已经通过。真机验收由操作者在开发板上执行；本轮准备没有
连接开发板、没有打开 COM 口、没有烧录或复位，也没有修改点表。

- 从站脚本：`scripts/maintenance_rtu_slave.ps1`（本任务扩展，默认行为保持兼容）
- 工况数据：`scripts/alarm_scenarios/gen_alarm_scenarios.py` 生成
  `scripts/alarm_scenarios/scenarios.json`（板验用）与
  `scripts/alarm_scenarios/alarm_scenarios_fixture.h`（离线测试用）
- 离线测试：`app/velaguard/host_tests/test_alarm_boundary.c`（规则与原始值编码）、
  `gui/headless/alarm_scenario_check_main.c`（板端同一份模型代码的集成行为）
- 本轮离线结果见第 8 节；离线通过不等于板验通过。

## 1. 前置条件

1. 固件为作品主线 `velaguard-lvgl`，由本仓根目录 `bash scripts/build.sh` 构建，
   已在板上运行，HMI 与 `ai_agent` 都已自启。
2. 已确认点表必须与 `scripts/vgpoint_scene_room.json` 一致。抓取板端输出后核对：

   ```
   nsh> vgpoint list
   ```

   把输出存成文件（例如 `vgpoint-list-<日期>.txt`），然后执行：

   ```
   python3 scripts/alarm_scenarios/gen_alarm_scenarios.py --list-dump <该文件>
   ```

   输出“点表核对通过”才可以使用本清单里的原始值。提示不一致时，说明阈值已经变化，
   必须按新点表重新生成本任务的数据并重新核对，不能沿用这里的原始值。
3. 从站脚本用 Windows PowerShell 运行，控制文件默认
   `../.debug/maintenance-slave-mode.txt`，日志默认 `../.debug/maintenance-slave.log`。
   板验期间建议加 `-RequestLog`，日志里会有每次请求与应答的逐条记录。
4. RS485 总线上只能有板端一个主站。跑本清单时不要再开扫描、`vgpoint test`
   或其他采集会话，也不要让主机模拟第二个主站。
5. 全部数值都是 16 位寄存器原始值，不是显示值：显示值 = 原始值 × 点表 `scale`。
   有符号点按 int16 解释，写 65436 等价于 -100。

## 2. 按代码核对的实现规则

以下结论来自实际运行路径，不是文档推测。

| 规则 | 代码位置 | 结论 |
|---|---|---|
| `cmp` 为空 | `app/velaguard/vg_alarm_eval.c:92` | 不做模拟量告警，仍采集 |
| `ge` | `vg_alarm_eval.c:122` | 先判 `value >= crit` 记严重，再判 `value >= warn` 记预警；都不到为无告警 |
| `le` | `vg_alarm_eval.c:147` | 先判 `value <= crit` 记严重，再判 `value <= warn` 记预警 |
| `eq` | `vg_alarm_eval.c:97` | 只用 `crit` 做等于判定；`warn` 有值时才再判一次 `warn` |
| `eq` 精度 | `vg_alarm_eval.c:9`、`:51` | 绝对差 `<= 0.0005` 即算命中 |
| 未知 `cmp` | `vg_alarm_eval.c:172` | 不产生告警 |
| 离线窗口 | `vg_alarm_eval.c:13`、`:80` | 最近 8 次采样里失败数达到 `max(fail_n,5)`（上限 8）记离线 |
| 零星失败 | `vg_alarm_eval.c:82`、`vg_model.c:1931` | 失败未达阈值时返回无告警，模型保持原来的在线标志与等级，不清掉已触发的告警期 |
| 恢复 | `vg_model.c:1875` | 一次成功采样立即恢复在线；在线读数回到无告警时该点告警期结束 |
| 优先关系 | `vg_model.c:1054` | 严重 > 离线 > 预警；主告警取最高等级里点表顺序最靠前的一个 |
| 多点同时告警 | `vg_model.c:1871` | 每个点各自持有告警期，低等级告警不会被高等级丢弃 |
| warn 升级为 crit | `vg_model.c:1871` | 已在告警期时不重开，`al_epoch` 不变 |
| `pending_alarm.txt` | `app/velaguard/vg_ui_backend_board.c:1318` | 只写主告警一条；标记是 `<点id>/<offline 或 threshold>`；标记变化才重写；全部活动告警恢复才删除 |
| 写文件方式 | `vg_ui_backend_board.c:841` | 写入前先删除，所以是重写不是追加 |
| 事件日志 | `app/velaguard/vg_runtime.c:317` | 只在阈值种类进入或离开时各记一条；warn 升级为 crit 不新增事件 |
| 事件容量 | `app/velaguard/vg_runtime.h:20` | 环形缓冲 32 条，超过会覆盖最旧的 |
| 单点采集 | `app/velaguard/vg_discover_modbus.c:330` | 一次读失败后 100 ms 重试一次，共 2 次尝试；读超时 500 ms，字节超时 50 ms，点间隔 50 ms |
| 阈值顺序校验 | `app/velaguard/vg_point_table.c:317` | `ge` 的 `warn > crit`、`le` 的 `warn < crit` 会被点表校验拒绝，非法顺序的表进不了板子 |

## 3. 与现有文档不一致或没写清楚的地方

这几条会影响观察判定，板验前先确认。

1. **离线触发时间**。`velaguard-alarm-to-screen` skill 写“板端一轮约 200 ms”“拔线约 1 s 触发”。
   代码里 200 ms 只是采集线程的休眠，一轮还要加上 14 个点的读取与 50 ms 点间隔。
   按代码推算：正常一轮约 1.1 s；触发需要 5 次失败采样，单个从站静默约 20 s，
   全部静默约 80 s（每个点 2 次 500 ms 超时）。板验必须实测一轮耗时并把实测值
   写进验收记录，不能沿用 1 s。
2. **恢复后的滑窗（实现现状与产品风险）**。一次成功采样立即恢复在线，但只清掉当前
   槽位那一位；滑窗里其余历史失败位要再经过 8 轮才被覆盖。实现上，恢复后短时间内
   再出现一次失败就可能重新判离线。这个提前重触发可能造成告警抖动，现有代码和测试
   只说明行为如此，不能证明产品已接受该风险；后续需要单独决定是否清空滑窗或延长恢复
   判定。
3. **事件日志不区分 warn 与 crit**。`vgruntime dump` 的事件行只有 `kind=threshold`，
   没有等级字段。等级要看屏幕和 `pending_alarm.txt` 的 `threshold` 数值。
4. **`pending_alarm.txt` 会滞后（实现现状与产品风险）**。标记是
   `<点id>/<offline 或 threshold>`，同一点从 warn 升到 crit 时标记不变，文件不会被
   重写，里面的 `threshold` 仍是标记变化那一刻的值。实现现状会让文件和当前主告警
   等级、阈值不一致，依赖该文件显示或联动的功能可能误判；不能仅因旧代码如此就认定
   行为合理，需作为待决策的产品风险保留。

## 4. 工况清单

控制列是写进控制文件的内容，可以一次写多行；同一个工况里各步按顺序执行。
`value <从站> <寄存器> <原始值>` 覆盖读数，`offline <从站>` 抑制该从站应答，
`drop <从站> <寄存器> <次数>` 丢掉接下来若干次对该寄存器的应答，`normal` 回到基线，
`reset` 清掉覆盖值与静默。

| 编号 | 目的 | 控制 | 应观察 | 通过判定 |
|---|---|---|---|---|
| B1 | 正常读数基线 | `reset` | 14 点在线，首页无告警 chip，`ls /data/velaguard/pending_alarm.txt` 不存在 | 无任何告警事件，无待处理文件 |
| B2 | 脚本旧三档模式兼容 | `warn` / `critical` / `normal` | 温度 30.0 → 预警，36.0 → 严重，24.0 → 恢复 | 三步等级与预期一致，恢复后文件被删除 |
| B3 | 逐点阈值边界 | 见 4.1 | 等于阈值即触发，阈值两侧判定符合方向 | 每个点 6 个边界值全部与表一致 |
| B4 | int16 负数解码 | `value 4 0 65436` | 回风温度显示 -10.0 C，不告警 | 显示为负值且无告警；若显示 6543.6 C 说明按 uint16 解码，判失败 |
| B5 | `cmp` 为空的点 | `value 4 1 30000`、`value 4 2 65535`、`value 6 2 65535` | 设定温度、空调运行、正向电能只更新数值 | 三点都不产生告警事件 |
| I1 | 同点 warn 升级为 crit | `value 1 0 280` 然后 `value 1 0 350` | 告警期延续，等级变严重 | 只有 1 条 raise 事件，`al_epoch` 不变 |
| I2 | 阈值恢复 | `value 1 0 350` 然后 `value 1 0 240` | 告警页收起，待处理文件删除 | 新增 1 条 clear 事件 |
| I3 | 恢复后再次触发 | `value 1 0 280` → 保持 → `value 1 0 240` → `value 1 0 280` | 第一次触发、恢复、第二次触发 | 恰好 2 条 raise、1 条 clear，没有重复 raise |
| I4 | 严重压过预警 | `value 1 0 280` + `value 2 2 1`，然后 `value 2 2 0` | 主告警先是水浸，水浸恢复后切到温度 | 待处理文件的 tag 从 `flood/threshold` 变为 `temp/threshold`，温度告警期不被水浸恢复带走 |
| I5 | 严重压过离线 | `value 1 0 350` + `offline 5` | 5 号从站 3 点离线，主告警仍是温度 | 活动告警 4 条，主告警为 temp 且等级严重 |
| I6 | 单从站静默 | `offline 5`，然后 `normal` | 只有 5 号从站 3 点离线，其余 11 点正常 | 主告警为 `ups_load`（离线）；恢复后 3 条告警期同时结束 |
| I7 | 全部静默再恢复 | `offline`，然后 `normal` | 14 点全部离线，主告警为 `temp` | 恢复后待处理文件删除，无残留告警 |
| I8 | 离线滑窗计数 | 先用 `reset` 取得至少 8 个正常应用样本，再只下发一次 `drop 7 0 32`，不再修改控制文件 | 从站请求日志中出现连续 16 个失败采样，每轮包含两次 `action=drop`；前 4 个失败采样无告警，第 5 个触发离线，第 16 个仍离线，drop 用尽后的首个成功采样恢复 | RequestLog 的实际请求、重试和恢复顺序与上述连续序列一致，板端事件只在第 5 个失败采样进入离线，恢复后为 `online=1`。若无法确认基线已清零，本次 I8 记为未验证，不对触发次数下结论 |
| I9 | 单次丢帧被重试吸收 | `drop 7 0 1` | 无失败采样 | 无告警事件，`vgruntime dump` 里该点 `online=1` |
| I10 | 同从站内各点独立计数 | `drop 5 0 20` | 只有 `ups_load` 离线 | `ups_soc`、`ups_bypass` 仍在线，活动告警 1 条 |

离线模型把一次采集轮次里的两次失败请求折算为一个失败采样。`drop 7 0 32`
对应 16 个连续失败采样；模型在该步骤内不插入成功采样。板验必须用 RequestLog
证明这 16 个失败采样确实连续，并在 drop 用尽后取得成功采样；如果基线清零或连续
性无法确认，I8 只能记为未验证。

### 4.1 阈值边界的具体原始值

边界值由生成器按点表的 `scale` 与 `dtype` 算出，保证“等于阈值”这一步的解码值
正好等于阈值。完整清单见 `scripts/alarm_scenarios/scenarios.json` 的 `cases`，
下面是主要几条。

| 点 | 比较 | 阈值 | 阈值下方 | 等于阈值 | 阈值上方 |
|---|---|---|---|---|---|
| temp（scale 0.1） | ge | warn 28 | 279 → 27.9 无告警 | 280 → 28.0 预警 | 281 → 28.1 预警 |
| temp | ge | crit 35 | 349 → 34.9 预警 | 350 → 35.0 严重 | 351 → 35.1 严重 |
| humid（scale 0.1） | ge | warn 70 / crit 85 | 699 / 849 | 700 / 850 | 701 / 851 |
| smoke（scale 0.1） | ge | warn 5 / crit 10 | 49 / 99 | 50 / 100 | 51 / 101 |
| ups_load（scale 0.1） | ge | warn 70 / crit 90 | 699 / 899 | 700 / 900 | 701 / 901 |
| acu_ret（scale 0.1） | ge | warn 28 / crit 32 | 279 / 319 | 280 / 320 | 281 / 321 |
| pwr_kw（scale 0.01） | ge | warn 8 / crit 10 | 799 / 999 | 800 / 1000 | 801 / 1001 |
| ups_soc（scale 1） | le | warn 40 / crit 20 | 39 / 19（预警 / 严重） | 40 / 20（预警 / 严重） | 41 / 21（无告警 / 预警） |
| volt_a（scale 0.1） | le | warn 200 / crit 180 | 1999 / 1799（预警 / 严重） | 2000 / 1800（预警 / 严重） | 2001 / 1801（无告警 / 预警） |
| flood、ups_bypass、door（scale 1） | eq | crit 1 | 0 → 无告警 | 1 → 严重 | 2 → 无告警 |

`le` 的方向与 `ge` 相反：数值越小越严重，表头按原始值大小排列，因此
`阈值下方`列是更危险的取值，括号里写的是该点的实际预期。执行时逐条改一个寄存器
即可，其他点保持基线。

## 5. 每个工况需要的前置点表字段

| 工况 | 需要的字段 |
|---|---|
| B1、I6、I7 | 任意已确认点；`addr` 与 `reg` 必须与从站脚本的基线表一致 |
| B3、B4 | `dtype`、`scale`、`cmp`、`warn`、`crit` |
| B5 | `cmp` 为空 |
| I1、I2、I3 | 至少一个 `ge` 或 `le` 点带 `warn` 与 `crit`（本表为 `temp`） |
| I4、I5 | 至少两个点能同时告警，且等级不同 |
| I8、I9、I10 | `fail_n`（当前表全为 3，有效窗口为 5）；I10 需要同一从站有多个点 |
| I6、I7 | 至少两个从站，且每站至少一个点 |

## 6. 证据保存

从站侧（Windows 日志，`-RequestLog` 时逐条记录）：

- `START`：端口、波特率、回复延迟、是否开请求日志
- `CONTROL`：每行控制命令、生效效果、控制文件的写入时间
- `MODE`：旧三档模式变化
- `REQ`：每次请求的从站、功能码、寄存器、数量、动作（reply / drop / suppress）、数值
- `STATUS`：每 10 s 一行，累计请求数、应答数、抑制数、丢帧数、静默从站、覆盖值个数
- `STOP`：收尾累计计数

板端（NSH，串口日志）：

```
vgpoint list
vgpoint get
vgruntime dump
vgstats dump <从站>
cat /data/velaguard/pending_alarm.txt
ls /data/velaguard/pending_alarm.txt
```

屏幕侧：告警页弹出、分栏数量与等级，用手机或相机拍照；照片文件名带上工况编号。

建议的证据目录（由操作者决定最终位置，本任务不预先占用维护验收目录）：

```
docs/alarm-acceptance/evidence/<日期>-board/
```

对照方法：以从站日志的 `CONTROL` 行时间为工况切换点，和板端事件行
`vgruntime: event t_boot_s=... wall=...` 的时间对齐，据此计算实际触发窗口。

## 7. 退出工况并恢复

1. 把控制文件写成 `normal`：恢复基线读数、取消静默、清掉覆盖值。
2. 确认板端 `vgruntime dump` 里 14 个点 `online=1`、`kind=none`。
3. 确认 `ls /data/velaguard/pending_alarm.txt` 不存在；存在时按第 3 节第 4 条核对是否
   还有活动告警，不要直接删除文件。
4. 需要停机时把控制文件写成 `stop`，脚本会写 `STOP` 行后退出。
5. 关闭脚本后再做其他总线操作，避免和采集线程抢 RS485。

## 8. 本轮离线验证结果

全部在独立 worktree（`.vg-alarm-wt`，分支 `codex/alarm-test-scenarios`）内执行，
没有打开串口、没有连接开发板、没有连接 broker。

| 项目 | 命令 | 结果 | 证据 |
|---|---|---|---|
| 工况数据自检 | `python3 -I scripts/alarm_scenarios/gen_alarm_scenarios.py` | 25 个工况、83 个步骤、4 项前置缺口 | 见第 9 节 |
| 产物是否最新 | `python3 -I scripts/alarm_scenarios/gen_alarm_scenarios.py --check` | 通过 | 同上 |
| 点表漂移检查 | `python3 -I ... --list-dump <vgpoint list 输出>` | 一致时通过；改动 `temp` 的 crit 后报不一致并返回 1 | 本轮用合成样本验证过两个方向 |
| 规则与编码单元测试 | `make -C app/velaguard/host_tests test_alarm_boundary` | ok（14 点、83 步骤、132 条预期） | `evidence/2026-10-18-current-i8-boundary.log` |
| 板端模型集成检查 | `cmake --build .debug/alarm-headless --target alarm_scenario_check` 后运行 | ok（25 个工况、83 个步骤、14 点） | `evidence/2026-10-18-current-i8-boundary.log` |
| 无头检查回归 | `ctest --test-dir .debug/alarm-headless` | 6 项全过（原有 5 项 + 新增 1 项） | `evidence/2026-10-18-ctest-headless.log` |
| 从站脚本自检 | `powershell -File scripts/maintenance_rtu_slave.ps1 -SelfTest` | SELF-TEST: ok，185 条 PASS 行；其中工况原始值 131 项、应答帧 3 项 | `evidence/2026-10-18-current-rtu-selftest.log` |
| 全量主机测试 | `make -C app/velaguard/host_tests test` | 全部通过 | 同上 host-tests 日志 |
| 负向对照 | 手工改坏工况数据里两条预期后重跑 | 单元测试报 2 条失败，集成检查报 3 条失败 | 见本轮报告 |

离线测试覆盖了什么：原始值编码（int16 有符号、uint16）、`scale` 换算、`ge`/`le`/`eq`
的方向与优先级、等于阈值的精确命中、离线滑窗的触发位置与恢复、告警期开始与结束、
主告警优先级、活动告警计数、`eq` 容差带、`fail_n` 的有效区间、CRC、地址匹配、
选择性静默、drop 计数与默认行为兼容。

### 8.1 复现离线验证

```text
# 重新生成工况数据（改了 scripts/vgpoint_scene_room.json 之后必须重跑）
python3 scripts/alarm_scenarios/gen_alarm_scenarios.py

# 校验已提交的产物是否最新
python3 scripts/alarm_scenarios/gen_alarm_scenarios.py --check

# 规则与原始值编码
make -C app/velaguard/host_tests test_alarm_boundary
./app/velaguard/host_tests/test_alarm_boundary

# 板端同一份模型代码的集成行为
cmake -S gui/headless -B .debug/alarm-headless \
  -DVG_LVGL_DIR="$PWD/../apps/graphics/lvgl/lvgl" -DVG_HEADLESS_COLOR_DEPTH=16
cmake --build .debug/alarm-headless --target alarm_scenario_check
./.debug/alarm-headless/alarm_scenario_check .

# 从站脚本逻辑自检（Windows PowerShell，不开串口）
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/maintenance_rtu_slave.ps1 -SelfTest
```

`alarm_scenarios_fixture.h` 由生成器写出，不要手工编辑；`--check` 会报出过期产物。

离线测试没有覆盖什么：串口收发本身（脚本自检不开串口）、屏幕渲染、`pending_alarm.txt`
的真实落盘、MQTT 与 Agent 链路、以及一切需要真实从站或真实总线的时序。

## 9. 测试前置缺口

这四项在当前点表下无法在板端覆盖，需要改点表才能补，属于候选表变更，必须走
“候选、试读、展示结果、人工确认、单独 apply”的流程，本任务不执行。

| 编号 | 缺口 | 原因 | 现有覆盖 |
|---|---|---|---|
| G1 | `eq` 判定精度（0.0005 容差带） | 点表里 `eq` 点的 `scale` 都是 1，阈值 1 能被整数原始值精确命中，容差带取不到 | `test_alarm_boundary.c` 用合成规则验证：`crit=0.9`、值为 `3*0.3f` 时两个单精度数不相等但差值在容差内，判严重；差值 0.0006 判无告警 |
| G2 | `fail_n` 的有效区间 | 点表里 `fail_n` 全为 3，有效阈值恒为 5；`fail_n>5`（上限 8）与被抬高到 5 的路径取不到 | `test_alarm_eval.c` 与 `test_alarm_boundary.c` 用合成规则覆盖 |
| G3 | FC04 输入寄存器路径 | 点表里所有点都是 `fc=3`，没有点会走 FC04 | 无；需要一张含 `fc=4` 点的候选表 |
| G4 | `cmp` 有值但 `warn`/`crit` 都空的点 | 点表里没有这样的点 | 无；需要候选表补充 |

## 10. 仍需人工确认、物理操作或真机观察的项目

- 真机验收本身：本清单的所有工况都要在开发板上执行一次，离线结果不能替代。
- 屏幕目视：告警页弹出、分栏数量、等级文字、状态栏告警 chip 变暗时机。
- 物理拔线：本清单用从站静默模拟离线，不等于物理拔掉 485 线；拔线作为人工步骤单独记录。
- 一轮采集耗时：按代码推算正常约 1.1 s、全部静默约 16 s，必须实测。
- `pending_alarm.txt` 的真实落盘内容与删除时机。
- 从站脚本的串口收发路径：自检只验证逻辑，真实 COM 口收发需要板验时确认。
- 点表是否变化：每次板验前重新抓 `vgpoint list` 并跑一次漂移检查。
