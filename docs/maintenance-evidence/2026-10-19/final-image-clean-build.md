# 日报修复后最终候选镜像构建

记录时间：2026-10-19 13:47（Asia/Shanghai）。

## 构建来源

| 项目 | 值 |
|---|---|
| 竞赛仓维护分支 | `codex/maintenance-closeout` |
| 构建提交 | `bc71a3ccda4487f41c95ecee7dca2f7c07a4ba7b` |
| 功能修复提交 | `22a9bfe900e8ebb9955a356708e77b1d5394fddb` |
| 构建命令 | `bash scripts/build.sh --clean` |
| 目标与预设 | `velaguard-lvgl` / `stm32h750b-dk:velaguard-lvgl` |
| 构建模式 | 发布构建，无调试符号，`FULLOPT` |
| ELF 内构建标识 | `2373ab4a45d Oct 19 2026 13:46:02` |
| `nuttx/.config` SHA256 | `c4150e92cb327fd96b210f6db853f1130d21d6c6a349fdae98522079e90ac646` |

该镜像与正在长测的旧镜像使用相同公共树 NuttX 基线版本，但构建时间不同。
日报修复位于竞赛仓应用层，所以只比较 NuttX 版本号不能区分新旧镜像。

公共树来源：

| 树 | 分支 | 提交 |
|---|---|---|
| NuttX | `velaguard/eth-mii-stm32h750b-dk` | `2373ab4a45ddfebe67706110db460b27f3a8d127` |
| apps | `velaguard/netinit-esp8266` | `0ea954b79740756318ce1f0b13271dd94235fe2a` |
| ai_agent | `velaguard/llm-tls-send-retry` | `7dc0a8816292cd58675863e9a1cde8b1cd96a535` |
| MQTT-C | `velaguard/mqtt-pal-hook` | `4dbb7f321ceeed47822171787e0742df2ddb4959` |

`apps` 工作树原有三个 NIST STS 未跟踪目录，未参与本次修改或提交。其他公共树
均在参与构建的代码范围内保持干净。

## 产物

| 文件 | 字节 | SHA256 |
|---|---:|---|
| `.debug/nuttx.elf` | 1,651,304 | `14dc04380c10faefa32678ac91adb6d6a95077aeec11a607367bccbe6fea7937` |
| `.debug/nuttx.bin` | 1,265,792 | `25a1e73c4558d51e165fe08f957b31a646e12ee38644ca44cd3df394285e3794` |
| `.debug/nuttx.hex` | 3,560,427 | `9e735f6251aa422dd69d04435eea41d6398892113d175bdd31e546f8a8fc3fe8` |
| `.debug/qspi_bootstub.hex` | 2,076 | `877aa3cf6072d66582b33eac555ded7cd5603bacd5dc794915fbcba425f6119e` |

`arm-none-eabi-size`：

```text
text=1251768 data=14024 bss=280044 dec=1545836 hex=17966c
```

链接器报告 flash 使用 1,265,792 字节，占 128 MiB 的 0.94%；sram 使用
294,092 字节，占 512 KiB 的 56.09%。

## 构建结果

clean 构建退出码 0。原始输出见
[final-image-clean-build-20261019.log](final-image-clean-build-20261019.log)，
SHA256 `a80b5221f271d0626bb3efcc734f07d748443af79ec49429ef9bdfd4f79f8a66`。

日志内没有 `error:`、`undefined reference`、`collect2: error` 或
`make: ***`。共有 6 处编译告警，位于既有 ESP8266、诊断页局部变量、
ai_agent 字符串长度与格式类型代码；本次没有以隐藏告警或修改别的任务代码的
方式改变构建结果。

## 密钥检查

使用最新 broker 密码做固定字符串扫描，范围为 `app/`、`scripts/`、`docs/`、
`../nuttx`、`../packages/ai_agent` 和 MQTT-C checkout，排除 `.git`、
`logs/` 与 `.debug/`。所有范围返回“未发现”，退出码均为 1。该结果没有回显
密码。

完整的 `../apps` 树含若干外部依赖失效链接，全树扫描会因不可读路径返回错误。
因此本次只对实际参与本次网络改动的 MQTT-C checkout 做定向扫描，不能宣称
已扫描 apps 全树。

## 尚未完成

- 新镜像尚未烧录。旧镜像的 24 小时长测仍在进行，完成后才允许复位和烧录。
- 烧录后仍须验证启动、14 点采集、首页温度刷新、首页自动日报、MQTT 真机
  上报，并用新镜像重新完成连续 24 小时长测。
