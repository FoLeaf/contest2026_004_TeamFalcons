# 最终维护固件 D-02 与 D-04 板端复验

记录时间：2026-10-18 21:43（Asia/Shanghai）

对象为最终维护 clean 构建的已烧录固件。构建来源和产物哈希见
[final-build-provenance.md](final-build-provenance.md)。

## 烧录与板端身份

- 烧录校验日志：[final-flash-validate.log](final-flash-validate.log)
- 烧录原始日志：[final-flash-20261018-2135.log](final-flash-20261018-2135.log)
- 外部 QSPI 主镜像和内部 boot stub 均显示 `Download verified successfully`。
- ST-LINK SN：`0043001D3137511133333639`
- 板端电压：烧录时约 3.27-3.28 V
- 烧录后执行 `MCU Reset`
- 完整启动日志：[final-boot.log](final-boot.log)
- 板端身份日志：[final-board-identity.log](final-board-identity.log)

板端 `uname -a`：

```text
NuttX  0.0.0 2373ab4a45d Oct 18 2026 21:29:20 arm stm32h750b-dk
```

`uname` 只固定本次 NuttX 版本字符串，多仓完整身份仍以构建来源记录为准。

启动日志显示 eMMC 挂载、`/data` 链接、LVGL 480x272、触摸输入、HMI
`home fleet n=14`、采集启动和 `ai_agent` autostart 均正常。MQTT 当前明确报告
provision store 中没有 MQTT 凭证，因此本次启动没有建立 MQTT 连接。

## D-02 控制台栈与原崩溃路径

原始问题为 2 KiB NSH 控制台线程栈在执行 vfat 覆盖改名时溢出，破坏栈底的
`tls_info_s`，随后在 `getpid()` 中 HardFault 并导致整机失联。

本次复验步骤：

```text
vgruntime dump
echo CMD-A > /data/velaguard/discover_bad/stack_a.txt
echo CMD-B > /data/velaguard/discover_bad/stack_b.txt
mv /data/velaguard/discover_bad/stack_a.txt /data/velaguard/discover_bad/stack_b.txt
cat /data/velaguard/discover_bad/stack_b.txt
ls /data/velaguard/discover_bad
ps
```

结果：

- 覆盖改名命令返回提示符，控制台没有失联。
- `cat` 输出 `CMD-A`，符合覆盖改名后的目标内容。
- 后续 `ls`、`ps`、`vgruntime` 和 COM3 命令继续可用。
- `ps` 中 PID 4 的栈为 `0008168`，未回到修复前的 `0002024`。
- 原始记录：[final-d02-recheck.log](final-d02-recheck.log)

结论：D-02 原崩溃路径在最终维护固件的隔离目录中通过复验。

## D-04 四种从站回复延迟

每组在启动 COM7 从站后先取一次板端累计计数，再等待约 45 秒取第二次计数。
验收使用同一上电期间的两次累计值差值，避免把无 slave 启动阶段的旧超时混入
本组结果。

| 回复延迟 | 总请求增量 | 成功增量 | 超时增量 | CRC 增量 | echo 增量 | other 增量 | 在线点位 |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 ms | 389 | 389 | 0 | 0 | 0 | 0 | 14 / 14 |
| 5 ms | 368 | 368 | 0 | 0 | 0 | 0 | 14 / 14 |
| 20 ms | 328 | 328 | 0 | 0 | 0 | 0 | 14 / 14 |
| 150 ms | 179 | 179 | 0 | 0 | 0 | 0 | 14 / 14 |

从站侧完整会话计数同样没有丢帧：

| 回复延迟 | 请求 | 回复 | suppressed | crc_scan | dropped |
|---:|---:|---:|---:|---:|---:|
| 0 ms | 482 | 482 | 0 | 0 | 0 |
| 5 ms | 470 | 470 | 0 | 0 | 0 |
| 20 ms | 415 | 415 | 0 | 0 | 0 |
| 150 ms | 226 | 226 | 0 | 0 | 0 |

板端四组 `vgruntime` 快照均显示 14 个点位在线，基线读数为 temp 24.0 C、
humid 50.0 %RH、flood 0、smoke 1.0 %、acu_ret 25.0 C、acu_set 24.0 C、
acu_run 1、ups_load 40.0 %、ups_soc 80 %、ups_bypass 0、volt_a 230.0 V、
pwr_kw 3.00 kW、energy_kwh 123.4 kWh、door 0。

原始记录：

- 0 ms：[before](final-d04-0ms-before.log)、[after](final-d04-0ms-after.log)、[slave](final-d04-0ms-slave.log)
- 5 ms：[before](final-d04-5ms-before.log)、[after](final-d04-5ms-after.log)、[slave](final-d04-5ms-slave.log)
- 20 ms：[before](final-d04-20ms-before.log)、[after](final-d04-20ms-after.log)、[slave](final-d04-20ms-slave.log)
- 150 ms：[before](final-d04-150ms-before.log)、[after](final-d04-150ms-after.log)、[slave](final-d04-150ms-slave.log)

before 快照仍包含从站启动前的历史累计超时，部分 64 项滚动窗口也尚未完全淘汰旧
记录。本页没有把 before 快照写成全绿窗口，结论只使用表中同组累计差值、最终
14/14 在线状态和从站侧请求回复计数。

结论：D-04 的 0/5/20/150 ms 四种工况在最终维护固件上复验通过。

## 尚未覆盖

- D-06 的候选试读与周期采集争用尚未在最终固件上执行。
- MQTT 尚未配置凭证，尚未验证连接、发布、重连和恢复。
- D/E/F/G 的故障、告警、物理拔线和 Agent 恢复项仍未全部完成。
