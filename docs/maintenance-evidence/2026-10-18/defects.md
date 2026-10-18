# 新固件板端缺陷记录（2026-10-18）

本轮在 COM3 上发现两个真实缺陷。两个都在新固件（`75cbc8eb7a5`，构建 2026-10-18 14:37:55）上复现，根因均未定位到具体代码行，下面严格区分"已证实"和"未证实"。

## D-01：候选点表写入恒失败，点表编辑功能不可用

优先级 P0。影响阶段 D（点表确认与持久化）整条流程。

### 最小复现

```text
nsh> vgpoint add -i acltest -a 9 -r 0
vgpoint: ERR cmd=add code=io msg=write_fail
```

同一路径的其他子命令同样失败：

```text
nsh> vgpoint set temp -w 29
vgpoint: ERR cmd=set code=io msg=write_fail
nsh> vgpoint del door
vgpoint: ERR cmd=del code=io msg=write_fail
```

2026-10-18 15:0x 至 15:2x 之间共 5 次尝试，5 次全部 `write_fail`，未见一次成功。

### 期望与实际

- 期望：`add` 把新点写进候选表 `/data/velaguard/discover/point_table_candidate.json`，返回 `vgpoint: OK cmd=add candidate=15`；已确认表保持 14 点不变。
- 实际：返回 `ERR cmd=add code=io msg=write_fail`；候选表未增加新点。

已确认表未被破坏，这是正确的安全行为：`vgpoint list` 与 `vgpoint get` 全程为 `table=committed n=14`。

### 已证实的边界

| 观测 | 证据 |
|---|---|
| 失败发生在"替换已存在文件"，不是"创建文件" | 先 `rm` 掉候选文件再 `add`，命令仍报 `write_fail`，但候选文件被重新生成，大小 2167 字节（14 点由新序列化器重写；旧文件为 2078 字节）。该文件来自 `vg_point_table_ensure_candidate` 的重建，属于创建路径。随后 `vgpoint list -c` 仍为 `n=14`，说明 `add` 自己那一次写入没落地。 |
| `discover/` 目录可写 | `echo dtest > /data/velaguard/discover/dtest.txt` 后 `cat` 读回成功。 |
| 长文件名与 `.bak` 后缀可用 | `echo X > .../point_table_candidate.json.bak` 成功创建。 |
| 同目录 `rename` 可用 | `mv r1.txt r2.txt`（目标不存在）成功。 |
| 目标存在时 `rename` 也会执行 | `mv r1.txt r2.txt`（两者都存在）后 `cat r2.txt` 得到 `AAA`，即源内容已覆盖目标。 |
| 同函数在其他目录可替换 | `vgstoretest 50` → `OK rounds=50 bytes=25600`，其内部用同一个 `vg_file_replace` 反复替换 `/data/velaguard/diagnostics/store-probe.bin`（512 字节），50 轮全部通过。 |
| 不是 JSON 体积上限 | `VG_POINT_TABLE_JSON_MAX` = 16384，候选约 2.2 KB。 |
| 不是点位字段非法 | 14 个已确认点与新增点的字段都满足 `valid_point()`：id ≤ 23 字符、name 允许非 ASCII、addr 1..247、fc 3、qty 1、scale 有限、fail_n ≤ 20。 |
| 不是设备名解析 | 候选 JSON 的 `bus.device` 为 `/dev/rs485`，`devpath` 有 NUL 结尾。 |

### 尚未证实

- 失败的具体步骤未定位。按 `vg_file_store.c:108` 的 `store_file` 流程，可能是 `backup_file` 的复制/回读比较、`rename(bak_tmp, bak)`，或 `rename(tmp, path)` 三者之一。板端只把结果归并成 `write_fail`，没有回传 errno，无法从串口区分。
- 为什么同样走 `vg_file_replace` 的 `vgstoretest` 能通过而候选文件不能，尚未找到解释。候选文件约 2.2 KB、`store-probe.bin` 512 字节，文件大小是目前唯一明确的差异，但没有证据证明大小是原因。
- 未排除 HMI 或发现流程对候选文件的并发访问。代码里只有 `access()` 检查（`vg_ui_backend_board.c:249`），没有长期持有句柄，但不能据此排除时序竞争。

### 修复方向

1. 在 `store_file` 的失败分支把 errno 带到上层错误码，让板端能区分 ENOSPC / EIO / ENOENT / EBUSY。
2. 用主机测试复现"替换已存在文件 + 备份回读"路径，确认是不是 `verify_copy` 或备份 rename 的问题。
3. 核对 NuttX FAT 上 `rename()` 目标已存在时的返回语义，确认是否存在"已完成但返回错误"的情况。

### 原始日志

- `nsh-candidate-add.log`：首次 `add` 失败 + 两张表对照。
- `nsh-candidate-add-fresh.log`：先删候选再 `add`，可见候选被重建为 2167 字节而命令仍报错。
- `nsh-set-del.log`（在 `.debug/`）：`set`、`del` 同样 `write_fail`。

## D-02：`mv` 覆盖已存在文件触发 HardFault，整机失联

优先级 P0。影响运行稳定性判定，也是本机历史重启疑点的新线索。

### 最小复现

在 `/data`（eMMC，FAT）上：

```text
nsh> echo AAA > /data/velaguard/discover/r1.txt
nsh> echo BBB > /data/velaguard/discover/r2.txt
nsh> mv /data/velaguard/discover/r1.txt /data/velaguard/discover/r2.txt
```

### 实际结果

命令输出 8009 字节，紧接命令回显出现故障转储：

```text
HARDFAULT: CFSR=00008200 HFSR=40000000 BFAR=0000003b MMFAR=0000003b
HARDFAULT: stacked PC=90095ff8 LR=9007bfc5
dump_assert_info: Current Version: NuttX  0.0.0 75cbc8eb7a5 Oct 18 2026 14:37:55 arm
dump_assert_info: Assertion failed panic: at file: :0 task: velaguard_app_main process: velaguard_app_main 0x9000caad
```

寄存器与栈转储随后打印，见 `nsh-rename-hardfault.log`。

解码：

- `CFSR=0x00008200`：`BFARVALID`(bit15) 与 `PRECISERR`(bit9) 置位，是一次精确数据访问错误。
- `BFAR=0x0000003b`：出错地址 0x3b，属于接近空指针的偏移访问。
- `HFSR=0x40000000`：`FORCED`，由其它异常升级而来。
- `task: velaguard_app_main`：故障发生在 NSH 线程，也就是执行 `mv` 的任务。
- 用 `arm-none-eabi-addr2line` 解析：`0x9007bfc5` 落在 `nsh_command` 内，`0x9000caad` 落在 `velaguard.c` 的 `nsh_thread`；`0x90095ff8` 只落到 `getpid` 之后，符号表在此处没有更细的信息。

### 后果

故障后整机失联：

- COM3 后续命令全部无回显（`uptime`、`ls`、`cat` 都是 `len=0`）。
- 对 `192.168.137.172` 的 ping 无响应。
- 必须用 SWD 复位恢复：2026-10-18 15:2x 执行，复位后正常启动（`boot-after-fault.log`）。

恢复后核对：`vgcfg: OK seq=6 schema=1 committed=1 name=vgpoint`，配置目录 4 个文件大小不变，`vgpoint list` 仍为 `committed n=14`。故障没有破坏配置和点表。

### 已证实与未证实

- **已证实**：`mv` 覆盖已存在文件时发生 HardFault 并导致整机失联。
- **已证实**：`rename` 本身执行成功，`r2.txt` 内容变成了源文件的 `AAA`。故障发生在重命名完成之后。
- **已证实**：`mv` 目标不存在时正常（第一次测试 `r1.txt` → `r2.txt` 无故障）。
- **未证实**：故障点。`cmd_mv`（`apps/nshlib/nsh_fscmds.c:2109`）在 `rename` 之后只剩 `nsh_freefullpath`，需要源码级调试确认是否在路径释放或 FAT 重命名收尾路径。
- **未证实**：是否与 D-01 同源。两者都涉及"目标已存在"的文件替换，但 D-01 在板端被 `store_file` 吞成错误码而未崩溃，表现不同，不能仅凭相似就合并。

### 与历史重启的关系

TODO 的 QA-02 记录过旧板测 14:28 与 15:20 附近出现重启，当时只有"时间上接近接线操作"的弱证据。本次故障提供了一个可复现的软件侧崩溃路径，但两者是否相关没有证据，不能据此关闭 QA-02。

### 修复方向

1. 用 GDB 复现 `mv` 覆盖场景，取 fault 时的调用栈，定位 0x3b 的解引用来源。
2. 检查 `nsh_getfullpath` / `nsh_freefullpath` 在绝对路径下的分配与释放是否配对。
3. 审查 NuttX FAT `rename()` 在目标已存在时的实现，确认没有对已释放对象继续访问。

### 原始日志

- `nsh-rename-hardfault.log`：完整故障转储。
- `boot-after-fault.log`：复位后的启动日志。

## D-03：MQTT 口令明文存在于仓库与固件镜像

优先级 P1。属于凭据管理问题，不是运行缺陷。

`scripts/configs/velaguard-lvgl.defconfig` 受版本控制，其中含一个字面量键 `CONFIG_VG_MQTT_PASSWORD`，值为明文口令。同一份 defconfig 会被复制成 NuttX 板级配置并参与编译，因此该字符串也会进入固件镜像。

这与 AGENTS.md 的 V4（密钥经 `vgprovision` 加密存于 eMMC，不得写进固件镜像）以及"完整 API key、MQTT token、产品密钥不得出现在 UI、日志、串口输出或 MQTT payload"的要求冲突。

本轮只做记录：没有修改该值，没有在证据文件中复制口令正文，也没有把镜像与 defconfig 做逐字节比对（该比对会读取口令值，已被权限策略拒绝，未尝试绕过）。

`docs/backend-api.md` 里 `MQTT_PASSWORD` 被描述为"可选密码"，没有说明板端 defconfig 为何需要内置一个具体口令。需要人工决定处理方式：改走 `vgprovision` 加密下发，或至少把该值移出仓库改为构建期注入。

## 附：一次瞬时 MQTT 报错

在 2026-10-18 的一次 `vgmqtt` 无参调用（一次性连到 `8.148.67.174:1883` 发布 status）之后，串口出现：

```text
vgnet: mqtt error MQTT_ERROR_SOCKET_ERROR
ERROR: publish failed: MQTT_ERROR_SOCKET_ERROR
```

随后重新执行 `vgnet` 显示 `mqtt=up`，会话自行恢复。未能确认是我这次额外连接造成的干扰，还是一次网络抖动。记为观察，不作为缺陷。同一时段还观察到板端 IP 由 `192.168.137.172` 变为 `192.168.137.123`（DHCP 续租），不影响结论。
