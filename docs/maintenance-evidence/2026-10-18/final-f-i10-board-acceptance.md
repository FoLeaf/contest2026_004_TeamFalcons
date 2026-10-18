# F 阶段 I10 板端验收记录

记录时间：2026-10-18 22:37 至 22:39（Asia/Shanghai）

对象为本轮候选固件
`NuttX 0.0.0 2373ab4a45d Oct 18 2026 21:50:34`，产品源码
`c20177f21e01bd70a4c96d12dc11095ebfcd08fd`。该固件尚未包含集成分支后续提交，
不能作为最终冻结固件身份。

## 基线

22:37 的只读快照显示已确认表 14 点全部在线：

```text
vgpoint: OK cmd=list table=committed n=14
vgruntime: point id=ups_load slave=5 online=1 ... value=40 kind=none
vgruntime: point id=ups_soc slave=5 online=1 ... value=80 kind=none
vgruntime: point id=ups_bypass slave=5 online=1 ... value=0 kind=none
```

同次检查中 `ls /data/velaguard/pending_alarm.txt` 返回 `stat failed: 2`，表明
场景开始前没有待处理告警文件。

## 注入

22:38:12 写入 `drop 5 0 20`：

```text
CONTROL 2026-10-18T22:38:12.5288719+08:00
line="drop 5 0 20" effect=drop slave=5 reg=0 n=20
```

从站对从机 5 寄存器 0 连续记录 20 次 drop，`remaining` 从 19 递减到 0。
寄存器 1 和寄存器 2 的请求在同一时段持续正常应答，`crc_scan=0`。

22:38:27 板端产生唯一一条离线告警：

```text
vgruntime: event t_boot_s=810 wall=2026-10-18T22:38:27
id=ups_load action=raise kind=offline value=40 thr=0 cmp=ge
reason=modbus_fail_offline
```

22:38:31 的只读快照进一步确认计数只落在 `ups_load`：

```text
vgruntime: point id=ups_load slave=5 online=0 ... kind=offline
vgruntime: point id=ups_soc slave=5 online=1 ... kind=none
vgruntime: point id=ups_bypass slave=5 online=1 ... kind=none
```

同从站其他 11 个点均保持 `online=1 kind=none`。这一点证明同一从站的不同寄存器
使用独立失败计数，不能把寄存器 0 的连续失败传播到寄存器 1 和寄存器 2。

## 恢复

20 次 drop 在 22:38:43 耗尽。22:38:45 从机 5 寄存器 0 首次恢复应答，22:38:47
板端记录恢复事件：

```text
vgruntime: event t_boot_s=830 wall=2026-10-18T22:38:47
id=ups_load action=clear kind=offline value=40 thr=0 cmp=ge
reason=offline_recovered
```

22:38:48 写入 `normal` 完成场景复位。22:39:00 的复查显示 14 点全部
`online=1 kind=none`，`ls /data/velaguard/pending_alarm.txt` 返回
`stat failed: 2`，没有残留待处理告警文件。

## 结论与边界

- I10 通过。同一从站的指定寄存器连续失败只影响该寄存器对应点，恢复后自动清除。
- 本轮从站累计 `dropped` 从 1 增至 21，增量等于本次 20 次 drop；`crc_scan=0`。
- 本轮没有在离线告警活动期间取得 `pending_alarm.txt` 内容。该文件缺失检查只覆盖
  场景前后，不能替代后续活动告警期间的文件内容验收。
- 板端在本次场景中保持 NSH 响应，没有出现 I8 的零响应现场。该结果不能关闭
  QA-04，也不能把 I8 判通过。

## 原始证据

- 基线：[final-f-i10-baseline-20261018-2237.log](final-f-i10-baseline-20261018-2237.log)
- 离线快照：[final-f-i10-offline-20261018-2238.log](final-f-i10-offline-20261018-2238.log)
- 恢复快照：[final-f-i10-recovery-20261018-2239.log](final-f-i10-recovery-20261018-2239.log)
- 从站日志快照：[final-g-i10-slave-20261018-2239.log](final-g-i10-slave-20261018-2239.log)，
  SHA256 `f10cb207799287f04d6e9e8b38ccbfd750886dbdc19b5d842a0e2f634c3b9548`
