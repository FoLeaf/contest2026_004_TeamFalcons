# D-06 补修后的最终维护构建来源

记录时间：2026-10-18 22:10（Asia/Shanghai）

本构建取代 `75fc04d` 的 21:29 构建。取代原因是阶段 D 板验发现 1000 ms
总线等待仍会与完整 14 点采集轮次冲突，因此把上界调整为 3000 ms，并加入持锁
1200 ms 的确定性回归。

## 构建命令与结果

- 入口：`/home/hello19y/openvela/.vg-maintenance-closeout/scripts/build.sh`
- 参数：`--clean`
- 目标：`TARGET=velaguard-lvgl`，预设 `stm32h750b-dk:velaguard-lvgl`
- 日志开始：2026-10-18 21:49:46 +08:00
- 产物完成：2026-10-18 21:50:39 +08:00
- 退出码：0
- 原始日志：
  [final-clean-build-20261018-2151.log](final-clean-build-20261018-2151.log)

日志记录 6 条既有 warning、0 条 error。warning 位于 ESP8266、SDMMC、
ai_agent、诊断页面和 skill_loader，本构建没有新增 warning。

## 多仓源码身份

| 仓库 | 分支 | 构建提交 | 构建时状态 |
|---|---|---|---|
| 参赛仓 `.vg-maintenance-closeout` | `codex/maintenance-closeout` | `c20177f21e01bd70a4c96d12dc11095ebfcd08fd` | 跟踪源码 clean；构建日志为未跟踪证据文件 |
| NuttX | `velaguard/eth-mii-stm32h750b-dk` | `2373ab4a45ddfebe67706110db460b27f3a8d127` | clean |
| apps | `velaguard/netinit-esp8266` | `0ea954b79740756318ce1f0b13271dd94235fe2a` | 跟踪源码 clean；存在先前已有的三个 `testing/drivers/nist-sts/` 未跟踪目录 |
| packages/ai_agent | `velaguard/llm-tls-send-retry` | `7dc0a8816292cd58675863e9a1cde8b1cd96a535` | clean |
| MQTT-C | `velaguard/mqtt-pal-hook` | `4dbb7f321ceeed47822171787e0742df2ddb4959` | clean |

## 配置来源与哈希

| 对象 | SHA256 |
|---|---|
| `scripts/configs/velaguard-lvgl.defconfig` | `d02be7adf022e9de81bd9a1eb35b44347be21ee5e5d720e60da9943b3a2a4ccb` |
| NuttX 板级 `configs/velaguard-lvgl/defconfig` | `d02be7adf022e9de81bd9a1eb35b44347be21ee5e5d720e60da9943b3a2a4ccb` |
| 最终生成的 `nuttx/.config` | `c4150e92cb327fd96b210f6db853f1130d21d6c6a349fdae98522079e90ac646` |

参赛仓预设与 NuttX 板级副本一致。生成配置已启用
`CONFIG_VG_MQTT_CREDENTIALS=y`，不包含 MQTT 用户名或口令配置键。

## 构建产物

| 产物 | 大小 | SHA256 |
|---|---:|---|
| `.debug/qspi_bootstub.hex` | 2,076 B | `877aa3cf6072d66582b33eac555ded7cd5603bacd5dc794915fbcba425f6119e` |
| `.debug/nuttx.hex` | 3,560,337 B | `7691d831488b9b108a3eec2e7d7771a9e6afa3a0f06e5308b8642d21a707e015` |
| `.debug/nuttx.bin` | 1,265,760 B | `02e745bdd268bb6695b23e148045db3fcd237742752919d6cf7b06d76c748db4` |
| `.debug/nuttx.elf` | 1,651,304 B | `ac15ea0d234bf0aeb3d885769037b8d48a1fd58a0e1ec09cde92b54aca56b774` |

静态内存占用：

- flash：1,265,760 / 134,217,728 B，0.94%
- SRAM：294,092 / 524,288 B，56.09%

## 主机回归

`make -C app/velaguard/host_tests clean test` 退出码 0，30 个 C 目标和
2 个 Python 检查通过。D-06 定向测试持锁 1200 ms，超过旧的 1000 ms 上界，
验证命令会等待完整采集轮次。原始输出见
[host-tests-full-after-d06-current.log](host-tests-full-after-d06-current.log)
和 [D-06 定向日志](d06-bus-wait-3000-host-test.log)。

## 凭证扫描结论

扫描对象为 `.debug/nuttx.bin` 和 `.debug/nuttx.elf`，过程只输出计数和结论。
原始脱敏记录见
[final-security-scan-after-d06.log](final-security-scan-after-d06.log)。

- 两个 MQTT 明文配置键名在 bin/elf 中命中数为 0。
- 历史 MQTT 口令候选值在 bin/elf 中命中数为 0。
- `CONFIG_VG_WIFI_PASSWORD` 静态值仍在镜像中命中，保留为独立问题。
- SEC-01 仍需板端 MQTT 连接、复位、缺失、损坏和恢复验收。

## 烧录与板端身份

- 烧录时间：2026-10-18 21:52（Asia/Shanghai）
- 烧录入口：`scripts/flash.ps1`
- 烧录结果：外部 QSPI 主镜像和内部 boot stub 均为
  `Download verified successfully`，随后由脚本复位目标。
- 原始日志：
  [烧录记录](final-flash-d06-20261018-2204.log)；
  [烧录对象校验](final-flash-d06-validate.log)
- 板端 `uname -a`：
  `NuttX 0.0.0 2373ab4a45d Oct 18 2026 21:50:34 arm stm32h750b-dk`
- 板端身份记录：
  [final-board-identity-after-d06.log](final-board-identity-after-d06.log)

板端内嵌构建时间 `21:50:34` 与本页 21:50:39 完成的产物一致。`uname` 仅表示
NuttX 提交和构建时间；完整多仓源码身份仍以上表的五个提交为准。

## D-06 板端试读

补修后的固件在周期采集持续运行时连续执行 8 次 `vgpoint test`，8 次均返回
`OK cmd=test table=candidate n=14`。8 次共产生 112 条 `READ`，全部
`ok=1`，没有 `bus_busy`。同一时间窗内 COM7 从站最终记录 2264 个请求和
2264 个应答，`crc_scan=0`。

候选表与已确认表均为 14 点，唯一差异是 `temp.name` 在候选中为 `温度验收`，
在已确认表中仍为 `温度`。截至本页写入时，真实用户尚未确认 `apply`，因此
只读复查仍显示候选未生效、`vgcfg seq=6` 未变化。

原始记录：

- [补修后 8 次试读](final-d06-candidate-tests-after-fix.log)
- [从站请求与应答](final-d06-slave-after-fix.log)
- [当前候选与只读状态](final-d06-state-before-confirm.log)
- [最终固件 D-02 复验](final-d02-recheck-after-d06.log)

## 身份边界

本文件固定的是 2026-10-18 21:49 至 21:50 的构建产物。后续只增加证据文档不会
改变参赛仓产品源码 `c20177f`；任何产品源码或配置变化都必须重新执行 clean
构建、凭证扫描、烧录和全部相关板验。
