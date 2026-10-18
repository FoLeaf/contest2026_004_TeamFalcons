# 新固件板端缺陷记录（2026-10-18）

本文件记录本轮在 COM3 上发现并跟进的两个缺陷，以及一项凭据问题。D-01 已定位并修复，板端复验通过；D-02 已定位到具体目录并现场修复，但底层成因仍未查明。

固件版本：首次复现于 `75cbc8eb7a5`（构建 2026-10-18 14:37:55），D-01 修复后为同提交的 16:09:56 构建。

---

## D-01：候选点表写入恒失败（已修复并复验）

优先级 P0，影响阶段 D 整条点表确认流程。

### 现象

```text
nsh> vgpoint add -i acltest -a 9 -r 0
vgpoint: ERR cmd=add code=io msg=write_fail
nsh> vgpoint set temp -w 29
vgpoint: ERR cmd=set code=io msg=write_fail
nsh> vgpoint del door
vgpoint: ERR cmd=del code=io msg=write_fail
```

修复前 5 次尝试全部失败。已确认表始终为 `committed n=14`，未被破坏。

### 根因

`store_file()`（`app/velaguard/vg_file_store.c`）在替换文件时，会从调用方路径派生三个名字：

| 名字 | 后缀 | `point_table_candidate.json` 的实际长度 |
|---|---|---|
| 新内容临时文件 | `.tmp` | 30 |
| 备份 | `.bak` | 30 |
| 备份临时文件 | `.bak.tmp` | **34** |

板端 vfat 卷以 `CONFIG_FAT_MAXFNAME=32` 构建，文件名单段超过该值即被拒绝。备份临时文件那一步 `open()` 返回 `ENAMETOOLONG`，`store_file` 随即放弃整次替换并返回错误，`vgpoint` 把它显示成 `write_fail`。

板端实测边界：

| 文件名单段长度 | 结果 |
|---|---|
| 29 / 30 / 31 / 32 | 创建成功 |
| 33 | `nsh: echo: open failed: 36`（36 = ENAMETOOLONG） |
| `point_table_candidate.json.bak`（30） | 成功 |
| `point_table_candidate.json.bak.tmp`（34） | `open failed: 36` |

这解释了全部观测：先删掉候选文件再 `add` 时，命令仍报 `write_fail`，但候选文件被 `vg_point_table_ensure_candidate` 用创建路径重建为 14 点（该路径目标不存在，不需要备份步骤）；随后的 `add` 替换已存在的候选，触发备份步骤而失败。

也解释了为什么主机测试和 `vgstoretest` 都没发现：

- 主机文件系统允许 255 字符名字，旧后缀在主机上不会失败，所以主机用例覆盖不到。
- `vgstoretest` 写 `store-probe.bin`（备份临时名 19 字符）、采集快照写 `values.txt.tmp`（14）、Agent 写 `alarm_advice.txt.tmp`（20），都远在预算内。

### 修复

1. `vg_file_store.h` 增加 `VG_STORE_NAME_MAX`（默认 32，可覆盖），并说明它与板端 `CONFIG_FAT_MAXFNAME` 的关系。
2. `vg_file_store.c` 把备份临时后缀从 `.bak.tmp` 缩短为 `.btmp`，使产品路径派生出的最长名字为 31。
3. `store_file` 在动文件系统之前先用 `store_name_fits()` 校验三个派生名，放不下的路径直接返回 `-ENAMETOOLONG`，不再做半次替换。
4. `test_file_store.c` 增加回归：产品候选名（26 字符）能完成替换并保留 `.bak`；32 字符的名单段被拒绝，且不留下任何文件。

对外协议不变：`docs/velaguard-host-nsh-protocol.md` 第 315 行把 `write_fail` 定义为这几处的固定 `msg`，本次没有改动它。

### 板端复验（2026-10-18 16:09:56 构建）

```text
nsh> uname -a
NuttX  0.0.0 75cbc8eb7a5 Oct 18 2026 16:09:56 arm stm32h750b-dk
nsh> vgpoint add -i acltest -a 9 -r 0
vgpoint: OK cmd=add table=candidate n=15
nsh> vgpoint del acltest
vgpoint: OK cmd=del table=candidate n=14
nsh> vgpoint list
vgpoint: OK cmd=list table=committed n=14
```

同时确认候选编辑不触碰已确认表：`add` 之后 `list` 仍是 `committed n=14`，`list -c` 才是 `n=15`；落盘后 `point_table_candidate.json` 为 2280 字节，旧内容保留在 `point_table_candidate.json.bak`（2167 字节）。

### 原始日志

- `nsh-candidate-add.log`：修复前首次失败 + 两张表对照。
- `nsh-candidate-add-fresh.log`：先删候选再 `add`，候选被重建而命令仍报错。
- `nsh-lfntest.log`：`open failed: 36` 与 30 字符对照。
- `nsh-lfnbound2.log`：29/30/31/32 成功、33 失败的边界。
- `nsh-fix-verify.log` / `nsh-fix-verify2.log` / `nsh-fix-verify3.log`：修复后 add / 隔离 / del。

---

## D-02：`discover` 目录的元数据改写导致整机崩溃（已定位到目录并现场修复）

优先级 P0。表现是整机失联，必须 SWD 复位。

### 现象

在 `/data/velaguard/discover/` 内执行任何会改写该目录条目的操作都会崩溃。最小复现：

```text
nsh> echo AAA > /data/velaguard/discover/r1.txt
nsh> echo BBB > /data/velaguard/discover/r2.txt
nsh> mv /data/velaguard/discover/r1.txt /data/velaguard/discover/r2.txt
HARDFAULT: CFSR=00008200 HFSR=40000000 BFAR=0000003b MMFAR=0000003b
HARDFAULT: stacked PC=90096060 LR=9007c02d
dump_assert_info: Assertion failed panic: at file: :0 task: velaguard_app_main process: velaguard_app_main 0x9000caad
```

把目录本身改名（`mv /data/velaguard/discover /data/velaguard/discover_bad`）触发同样的崩溃。

崩溃后整机失联：COM3 无回显，`192.168.137.172` ping 无响应，只能用 SWD 复位恢复。本次共复现 4 次，逐次复位记录见 `reset-log.txt`。

### 故障定位

用 `arm-none-eabi-addr2line` 和 `nm` 解析 `.debug/nuttx.elf`：

- 故障指令 `0x90096060` 落在 `getpid()` 内，是 `ldr r0, [r0, #48]`。
- `0x9007c02d` 落在 `nsh_command`，正是 `ret = handler(...); vtbl->np.np_lastpid = getpid();` 那一行（`apps/nshlib/nsh_command.c:1321`）。
- `getpid()` 第一步读的全局地址是 `0x24000264`，符号为 `g_readytorun`。
- `BFAR=0x3b` 对应 `r0 = 0x0b` 时执行 `[r0 + 0x30]`，即指针链中一个本应是地址的位置上放着 `0x0b`。

结论：**`cmd_mv` 返回之后，调度器相关内存已被破坏**，`getpid()` 顺着 `g_readytorun` 解引用到坏指针而精确数据访问出错（`CFSR` 的 `BFARVALID|PRECISERR`），升级成 HardFault。

`rename` 的数据操作本身是完成的：崩溃前 `cat r2.txt` 能得到源文件内容 `AAA`；目录改名也实际生效（复位后看到的是 `discover_bad/`）。崩溃发生在操作之后的收尾路径。

### 已排除的范围

| 试验 | 结果 |
|---|---|
| `mv` 覆盖已存在文件，在 `/data/velaguard/diagnostics/` | 不崩溃，`cat` 得到 `AAA` |
| 同上，在新建空目录 `/data/velaguard/scratchmvd/` | 不崩溃 |
| 同上，在仿造目录（放同名 `point_table_candidate.json`、`discover_state.bin` 及 `r1/r2.txt`） | 不崩溃 |
| 同上，在新建目录里放入**逐字节相同**的候选与状态文件副本 | 不崩溃 |
| 同上，在 `discover/` 内，且目录只剩原本那两个文件 | **仍崩溃** |

所以既不是 `cmd_mv` 或 `rename` 的通用缺陷，也不是文件名、文件大小或文件内容的问题，而是**这个目录自身的磁盘状态**。

### 已验证但不成立的假设：NSH 线程栈过小

`ps` 显示控制台线程（PID 4，pthread `velaguard_app_main`）的栈只有 **2024 字节**，而 `velaguard.c` 用 `pthread_create(&tid, NULL, nsh_thread, NULL)`，拿到的是 `CONFIG_PTHREAD_STACK_DEFAULT=2048`。其余任务的栈分别是 8136（主任务）、49104（vghmi）、32696（ai_agent）、12264 等。

同一份仓库里 SYS-01 已经修过同类问题：注释明确写着系统入口任务原来的 2 KiB 栈"overflows inside open()/getumask()"，且在 FAT 写路径上。

疑似机理：控制台栈溢出会冲掉位于栈底的 TCB，而 `getpid()` 正好读 TCB 的 `group` 字段，和观测到的故障点吻合。于是做了实验：新增 `CONFIG_VG_NSH_STACKSIZE=8192` 并显式设置线程栈，重新构建烧录后，在保留的受损目录里重跑同一复现步骤。

**结果：仍然崩溃**，签名完全一致（`CFSR=0x00008200`、`BFAR=0x3b`、`getpid` 内同一指令）。假设不成立，该改动已回退，未留在仓库里。

控制台线程 2 KiB 这个事实本身仍记为观察项（见下），但没有证据说明它在本问题里起作用；在受损目录之外，2 KiB 的线程跑 `vgpoint`、`cp`、`rm` 都正常。

### 现场修复与复验

把受损目录整体改名保留为 `discover_bad/`，新建 `/data/velaguard/discover/`，把两个文件复制回去。修复后同一操作不再崩溃：

```text
nsh> echo AAA > /data/velaguard/discover/w1.txt
nsh> echo BBB > /data/velaguard/discover/w2.txt
nsh> mv /data/velaguard/discover/w1.txt /data/velaguard/discover/w2.txt
nsh> cat /data/velaguard/discover/w2.txt
AAA
```

在该目录里 `vgpoint add` / `del` 也正常（见 D-01 复验）。

受损目录保留在 `/data/velaguard/discover_bad/`（`point_table_candidate.json` 2167 字节、`discover_state.bin` 4552 字节），作为现场证据，未删除。

### 仍未证实

- 目录磁盘状态里具体损坏的是什么。需要导出该目录的原始扇区或对 FAT 结构做离线检查，本轮没有做。
- 损坏来源。与 ST-02 记录的历史 eMMC 目录损坏在时间线上吻合，但本次没有证据把两者认定为同一原因，不据此关闭 ST-02。
- 是否会在别的目录复现。本轮只发现 `discover/` 一处；`config/`、`reports/`、`diagnostics/`、`live/` 的替换操作都正常。

### 修复方向

1. 排查 vfat 目录条目处理：本次现象是"数据操作成功、收尾崩溃"，适合从释放目录缓冲或目录流的地方查起。
2. 用 GDB 复现取调用栈。ST-LINK GDB server 本次无法连接（`Target no device found`），而 `STM32_Programmer_CLI` 用 `mode=UR`（复位下连接）可以，说明需要复位下连接方式，尚未配好。
3. 对 `/data/velaguard` 下的目录做一次完整性检查，确认是否还有别的目录带同类损伤。

### 原始日志

- `nsh-rename-hardfault.log`：首次完整故障转储。
- `nsh-mv-repro2.log`、`nsh-mv-discover3.log`、`nsh-mv-discover4.log`：三次复现。
- `nsh-mv-freshdir.log`、`nsh-mv-mimic.log`、`nsh-mv-copyfiles.log`：三个不崩溃的对照。
- `nsh-rebuild-dir.log`、`nsh-newdiscover.log`：目录改名崩溃与重建后复验。
- `reset-log.txt`：四次复位的时间与原因。

---

## D-03：MQTT 口令明文存在于仓库与固件镜像

优先级 P1。属于凭据管理问题，不是运行缺陷。

`scripts/configs/velaguard-lvgl.defconfig` 受版本控制，其中含一个字面量键 `CONFIG_VG_MQTT_PASSWORD`，值为明文口令。同一份 defconfig 会被复制成 NuttX 板级配置并参与编译，因此该字符串也会进入固件镜像。

这与 AGENTS.md 的 V4（密钥经 `vgprovision` 加密存于 eMMC，不得写进固件镜像）以及"完整 API key、MQTT token、产品密钥不得出现在 UI、日志、串口输出或 MQTT payload"的要求冲突。

本轮只做记录：没有修改该值，没有在证据文件中复制口令正文，也没有把镜像与 defconfig 做逐字节比对（该比对会读取口令值，已被权限策略拒绝，未尝试绕过）。

`docs/backend-api.md` 里 `MQTT_PASSWORD` 被描述为"可选密码"，没有说明板端 defconfig 为何需要内置一个具体口令。需要人工决定处理方式：改走 `vgprovision` 加密下发，或至少把该值移出仓库改为构建期注入。

---

## 附：一次瞬时 MQTT 报错

在 2026-10-18 的一次 `vgmqtt` 无参调用（一次性连到 `8.148.67.174:1883` 发布 status）之后，串口出现：

```text
vgnet: mqtt error MQTT_ERROR_SOCKET_ERROR
ERROR: publish failed: MQTT_ERROR_SOCKET_ERROR
```

随后重新执行 `vgnet` 显示 `mqtt=up`，会话自行恢复。未能确认是这次额外连接造成的干扰还是一次网络抖动。记为观察，不作为缺陷。同一时段还观察到板端 IP 由 `192.168.137.172` 变为 `192.168.137.123`（DHCP 续租），不影响结论。
