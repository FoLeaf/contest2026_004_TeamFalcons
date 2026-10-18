# D-06 补修后的最终维护构建来源

记录时间：2026-10-18 22:02（Asia/Shanghai）

本构建取代 `75fc04d` 的 21:29 构建。取代原因是阶段 D 板验发现 1000 ms
总线等待仍会与完整 14 点采集轮次冲突，因此把上界调整为 3000 ms，并加入持锁
1200 ms 的确定性回归。记录写入时产物尚未烧录，不能单独作为板端验收证据。

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

## 身份边界

本文件固定的是 2026-10-18 21:49 至 21:50 的构建产物。后续只增加证据文档不会
改变参赛仓产品源码 `c20177f`；任何产品源码或配置变化都必须重新执行 clean
构建、凭证扫描、烧录和全部相关板验。
