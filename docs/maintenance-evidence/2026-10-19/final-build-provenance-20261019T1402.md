# 2026-10-19 最终候选构建来源与产物

记录时间：2026-10-19 14:02（Asia/Shanghai）。

本记录将最终候选镜像的源码提交、构建命令、配置、产物和镜像内构建标识
绑定在一起。记录的是当前候选，尚不代表烧录或最终 24 小时长测通过。

## 构建来源

| 项目 | 值 |
|---|---|
| 竞赛仓分支 | `codex/maintenance-closeout` |
| 竞赛仓提交 | `9133697a8abc57b0c62f0b3c454aa11e062a1ea0` |
| 构建命令 | `bash scripts/build.sh --clean` |
| 目标与预设 | `velaguard-lvgl` / `stm32h750b-dk:velaguard-lvgl` |
| 构建模式 | 发布构建，无调试符号，`FULLOPT` |
| 构建退出码 | `0` |
| 镜像内构建标识 | `2373ab4a45d Oct 19 2026 14:01:50` |
| 构建结果 | 全部源码编译、归档、链接、HEX/BIN 生成和 `.debug` 暂存均完成 |

| 公共树 | 分支 | 提交 |
|---|---|---|
| NuttX | `velaguard/eth-mii-stm32h750b-dk` | `2373ab4a45ddfebe67706110db460b27f3a8d127` |
| apps | `velaguard/netinit-esp8266` | `0ea954b79740756318ce1f0b13271dd94235fe2a` |
| ai_agent | `velaguard/llm-tls-send-retry` | `7dc0a8816292cd58675863e9a1cde8b1cd96a535` |
| MQTT-C | `velaguard/mqtt-pal-hook` | `4dbb7f321ceeed47822171787e0742df2ddb4959` |

竞赛仓、NuttX、ai_agent 和 MQTT-C 在工作树状态记录中为 clean。apps 当时
只有原有的三个 NIST STS 未跟踪目录，未参与本镜像。原始记录：

```text
.debug/final-build-source-manifest-20261019T1402.json
SHA256 76f478a7c54489b927b0e162f0228c25d257894c6ba38dfe06a8a78a6b9685c7
```

构建记录了 6 条既有告警，没有 `error:`、`undefined reference`、
`collect2: error` 或 `make: ***`。链接报告 flash 使用 1,265,784 字节，
占 128 MiB 的 0.94%；SRAM 使用 294,092 字节，占 512 KiB 的 56.09%。

## 产物

| 文件 | 字节 | SHA256 |
|---|---:|---|
| `.debug/nuttx.elf` | 1,651,304 | `44d81eca65924e72676f490fe5610db55069f3ef80a6c5035ded78fde752bc7b` |
| `.debug/nuttx.bin` | 1,265,784 | `048595b566259f0e975194e5ea81080ffd840202feaf25efd01e0b6e9a6f2095` |
| `.debug/nuttx.hex` | 3,560,411 | `9fab4ad47e958a8ef0b668acbe97cf2514f6e04ddcf2795e31bd5ddffb467ef0` |
| `.debug/qspi_bootstub.hex` | 2,076 | `877aa3cf6072d66582b33eac555ded7cd5603bacd5dc794915fbcba425f6119e` |
| `.debug/qspi_bootstub.bin` | 720 | `9e6ddfa058915cdb3fec18ff8f862a1ca7dfe2b6a31134d1bc31df037fc2026d` |
| `../nuttx/.config` | 98,290 | `c4150e92cb327fd96b210f6db853f1130d21d6c6a349fdae98522079e90ac646` |
| `scripts/configs/velaguard-lvgl.defconfig` | 5,797 | `d02be7adf022e9de81bd9a1eb35b44347be21ee5e5d720e60da9943b3a2a4ccb` |

完整构建日志：

```text
.debug/final-clean-build-20261019T1402.log
bytes 145498
SHA256 625e9250c02ca305976ddc7d1b35b88aca646a58b5045f6adc9c665ef8ed49f3
```

## MQTT 凭证扫描

扫描不读取用户提供的 broker 口令，不输出任何凭证值。输入为本次生成的
配置、镜像和 boot stub；键名检查还覆盖历史键名。

| 文件 | `CONFIG_VG_MQTT_USERNAME` | `CONFIG_VG_MQTT_PASSWORD` | 历史明文凭证值 |
|---|---:|---:|---:|
| `../nuttx/.config` | 0 | 0 | 0 |
| `.debug/nuttx.bin` | 0 | 0 | 0 |
| `.debug/nuttx.elf` | 0 | 0 | 0 |
| `.debug/nuttx.hex` | 0 | 0 | 0 |
| `.debug/qspi_bootstub.bin` | 0 | 0 | 0 |
| `.debug/qspi_bootstub.hex` | 0 | 0 | 0 |

原始脱敏扫描输出与哈希：

```text
docs/maintenance-evidence/2026-10-19/final-mqtt-credential-scan-20261019T1402.log
SHA256 66781d525e38efb1fbf31e9daa320d3856743c5a35c98f7850e36c7f7aecd4ea
```

## 边界

- 本记录只证明主机 clean 构建和静态扫描，不证明镜像已烧录。
- 烧录必须同时验证外部 QSPI 主镜像和内部 boot stub。
- 烧录后仍须核对 `uname` 构建时间、14 个点、采集、温度名称和读数、
  MQTT、日报以及 D/E/F/G 现场证据。
- 当前最终候选还必须独立完成连续 24 小时长测；旧镜像的长测结论不能迁移。
- `CONFIG_VG_WIFI_PASSWORD` 是独立的已知问题，不在本次 MQTT 凭证扫描范围内。
