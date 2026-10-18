# 最终维护构建来源

记录时间：2026-10-18 21:32（Asia/Shanghai）

本记录对应维护主线统一集成后的 clean 构建。构建产物尚未烧录，本文件不能作为
板端固件身份、板端验收或 24 小时长测证据。

## 构建命令与结果

- 入口：`/home/hello19y/openvela/.vg-maintenance-closeout/scripts/build.sh`
- 参数：`--clean`
- 目标：`TARGET=velaguard-lvgl`，预设 `stm32h750b-dk:velaguard-lvgl`
- 开始：2026-10-18 21:28:32 +08:00
- 结束：2026-10-18 21:29:54 +08:00
- 退出码：0
- 原始日志：
  [final-clean-build-20261018-212832.log](final-clean-build-20261018-212832.log)

日志记录 6 条 warning、0 条 error。warning 位于 ESP8266、SDMMC、ai_agent
和诊断页面等既有源码，不能用本构建宣称零 warning。

## 多仓源码身份

| 仓库 | 分支 | 构建提交 | 构建时状态 |
|---|---|---|---|
| 参赛仓 `.vg-maintenance-closeout` | `codex/maintenance-closeout` | `75fc04d21e0f3b6a6fbd4f187c7f2d66a97ce165` | 跟踪源码 clean；开始构建时仅有待写入的构建日志 `?? final-clean-build-20261018-212832.log` |
| NuttX | `velaguard/eth-mii-stm32h750b-dk` | `2373ab4a45ddfebe67706110db460b27f3a8d127` | clean |
| apps | `velaguard/netinit-esp8266` | `0ea954b79740756318ce1f0b13271dd94235fe2a` | 跟踪源码 clean；存在先前已有的三个未跟踪目录：`testing/drivers/nist-sts/.gitee/`、`testing/drivers/nist-sts/.github/`、`testing/drivers/nist-sts/sts/` |
| packages/ai_agent | `velaguard/llm-tls-send-retry` | `7dc0a8816292cd58675863e9a1cde8b1cd96a535` | clean |
| MQTT-C | `velaguard/mqtt-pal-hook` | `4dbb7f321ceeed47822171787e0742df2ddb4959` | clean |

apps 的未跟踪目录没有纳入任何提交。其源码身份必须由表中提交和 dirty 状态共同
解释，不能只记录 apps 提交号。

## 配置来源与哈希

构建脚本在 clean 构建中执行 `distclean`，随后把参赛仓的预设复制到 NuttX
板级目录并生成 `nuttx/.config`。

| 对象 | SHA256 |
|---|---|
| `scripts/configs/velaguard-lvgl.defconfig` | `d02be7adf022e9de81bd9a1eb35b44347be21ee5e5d720e60da9943b3a2a4ccb` |
| NuttX 板级 `configs/velaguard-lvgl/defconfig` | `d02be7adf022e9de81bd9a1eb35b44347be21ee5e5d720e60da9943b3a2a4ccb` |
| 最终生成的 `nuttx/.config` | `c4150e92cb327fd96b210f6db853f1130d21d6c6a349fdae98522079e90ac646` |

参赛仓预设与 NuttX 板级副本一致，最终运行配置另以生成后的 `.config` 哈希固定。

## 构建产物

| 产物 | 大小 | SHA256 |
|---|---:|---|
| `.debug/qspi_bootstub.hex` | 2,076 B | `877aa3cf6072d66582b33eac555ded7cd5603bacd5dc794915fbcba425f6119e` |
| `.debug/nuttx.hex` | 3,560,329 B | `72089db0dd3ea6da88cdf9a6d2f3bcf9a76ecb6f377fae0166b9fe8a7e1e2119` |
| `.debug/nuttx.bin` | 1,265,756 B | `3bd1100d43e241141e314f15d5b73c6292f425fe088507f699bc019f3e3e63f8` |
| `.debug/nuttx.elf` | 1,651,304 B | `ad62c10da3dfe84ec8f87233e569cfe2b9733f2edf3b6641f9b757be66d27765` |

`.debug/nuttx.elf` 与 `/home/hello19y/openvela/nuttx/nuttx` 的 SHA256 相同。

静态内存占用：

- flash：1,265,756 / 134,217,728 B，0.94%
- SRAM：294,092 / 524,288 B，56.09%

## 凭证扫描结论

扫描对象为 `.debug/nuttx.bin` 和 `.debug/nuttx.elf`，扫描过程没有输出 MQTT
键值或口令内容。原始脱敏记录见
[final-security-scan.log](final-security-scan.log)。

- `CONFIG_VG_MQTT_CREDENTIALS` 已启用。
- 镜像中 `CONFIG_VG_MQTT_USERNAME` 和 `CONFIG_VG_MQTT_PASSWORD` 两个明文键名命中数均为 0。
- 已知历史 MQTT 口令值在 bin 和 elf 中的命中数均为 0。
- `CONFIG_VG_WIFI_PASSWORD` 静态值仍在 bin 和 elf 中命中，属于独立保留问题。

MQTT 镜像扫描通过只能满足 SEC-01 的一个条件。最终烧录身份、板端专用测试记录
连接 broker、复位后重连、缺失、损坏和恢复工况仍未完成，SEC-01 保持未关闭。

## 身份边界

本文件固定的是 2026-10-18 21:28 至 21:29 的构建产物。后续只增加证据文档不会
改变参赛仓产品源码 `75fc04d`，但任何产品源码变化都必须重新执行 clean 构建、
凭证扫描并重新计算全部哈希。
