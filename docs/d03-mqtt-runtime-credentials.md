# D-03 / SEC-01：MQTT 运行时凭证

分支 `codex/d03-mqtt-runtime-credentials`，从 `ff140dd` 起。本文件说明实现范围、
配置流程、兼容性和失败行为，以及集成时还需要一起改的地方。

板验清单见 [d03-mqtt-runtime-credentials-board-acceptance.md](d03-mqtt-runtime-credentials-board-acceptance.md)。

## 1. 修的是什么

改动前，broker 的用户名和口令来自 Kconfig 字符串：

| 位置 | 改动前 | 改动后 |
|---|---|---|
| `scripts/configs/velaguard-lvgl.defconfig` | `CONFIG_VG_MQTT_USERNAME`、`CONFIG_VG_MQTT_PASSWORD` 两个字面量键 | 两个键都删除 |
| `app/velaguard/Kconfig` | 两个 `string` 配置项，口令带明文 `default` | 两个配置项删除，新增 `CONFIG_VG_MQTT_CREDENTIALS` |
| `app/velaguard/vg_mqtt_session.c` | 常驻会话读 `CONFIG_VG_MQTT_USERNAME/PASSWORD` | 读 eMMC 上的封存记录 |
| `app/velaguard/velaguard_mqtt.c` | `vgmqtt` 的默认凭据同上 | 同上，`-u` / `-P` 保留为显式覆盖 |

这条口令同时存在于受版本控制的 defconfig 和固件镜像里，与 V4（密钥经 `vgprovision`
加密存于 eMMC，不得写进固件镜像）以及 AGENTS.md 关于完整密钥不得出现在日志、串口
输出或 MQTT payload 的要求冲突。

改动后，参赛仓受控配置里不再有 MQTT 口令，代码里也没有编译期回退：读不到封存记录
就不连，并打印一条不含凭证的错误。

## 2. 记录格式与存放位置

新记录与 LLM 记录共用一套封存能力，但文件、格式和用途都分开：

| | LLM 记录 | MQTT 记录 |
|---|---|---|
| 文件 | `/data/velaguard/provision/llm_secrets.v1` | `/data/velaguard/provision/mqtt_secrets.v1` |
| 明文 JSON | `{"host","path","port","model","api_key"}` | `{"kind":"mqtt","username","password"}` |
| 读取者 | `vg_provision_load()`，开机写入 `config.json` | `vg_mqtt_cred_load()`，每次开会话时读 |
| 生效时机 | 开机（`vgprovision commit` 之后要复位） | 下一次 MQTT 建链，不需要复位 |
| 暂存文件 | `.staging` | `.staging.mqtt` |

封存用的是 `vg_provision_crypto_seal()` / `_open()`（`VGPR` 头 + 设备 UID 派生密钥 +
HMAC-SHA256 标签），与 LLM 记录同一套实现，没有更换加密方案，也没有新增密钥。

### 用途隔离

MQTT 明文里带 `"kind":"mqtt"` 标签，LLM 记录没有这个字段。两个方向的误用都会被拒绝：

- 把 LLM blob 放到 `mqtt_secrets.v1`：解析器找不到 `kind` 标签，回 `-EINVAL`。
- 把 MQTT blob 放到 `llm_secrets.v1`：LLM 解析器找不到 `host`，回 `-ENOENT`。

标签加在 MQTT 记录上而不是 LLM 记录上，是为了让已经 provision 过的板子继续可读；
`llm_secrets.v1` 的格式一个字节都没动。

文件不同、暂存名不同，所以两类记录也不会互相覆盖。

## 3. 配置流程

口令的明文只在主机侧，串口上走的是已经封存好的 blob，因此口令不会出现在 NSH 回显、
NSH 命令历史、串口日志或任何板端输出里。

```text
nsh> vgprovision uid
vgprovision: uid=<24 位十六进制>
```

```bash
python3 scripts/vg-provision-pack.py <uid> - > .debug/mqtt_secrets.v1 <<'EOF'
{"kind":"mqtt","username":"<broker 用户名>","password":"<broker 口令>"}
EOF
```

封好的 blob 按 32 字节（64 个十六进制字符）一段推进暂存文件，然后提交：

```text
nsh> vgprovision mqtt put <hex 段 1>
nsh> vgprovision mqtt put <hex 段 2>
...
nsh> vgprovision mqtt commit
vgprovision: mqtt credentials committed to /data/velaguard/provision/mqtt_secrets.v1 (144 bytes)
nsh> vgprovision mqtt status
vgprovision: mqtt credentials: OK user=ve****rd password=set(32)
vgprovision: file=/data/velaguard/provision/mqtt_secrets.v1 persistent=yes
```

正常流程不需要先 wipe：`commit` 无论成功与否都会删掉暂存文件，所以暂存文件平时是
空的，新记录只可能由一次通过校验的 `commit` 替换。`vgprovision mqtt wipe` 同时删除
暂存文件和已有记录，用于放弃一次中断的推送或彻底移除记录。

`status` 只输出是否存在、脱敏后的用户名、口令长度、文件路径和存储是否持久，没有
任何接口可以读回完整口令。

## 4. 运行时读取

- 常驻会话（`vg_mqtt_session.c`）：`vg_mqtt_session_open()` 在**打开 TCP 之前**读记录。
  读不到就直接返回失败，不会先去连一个注定认证不过的 broker。网络管理器在会话离线
  时每 2 秒重试一次，所以记录一提交，通常 2 秒内就会用上，不需要复位。会话已经在线
  时，新记录在下一次重连（出口切换、掉线或复位）才生效。
- 调试入口（`velaguard_mqtt.c`）：没有 `-u` / `-P` 时读同一条记录；显式给出这两个
  参数时用命令行值，并在日志里说明这一点。命令行值会留在 NSH 历史里，只建议调试
  其它 broker 时用。
- 生命周期：记录读进一个模块静态结构体，`mqtt_connect()` 同步把 CONNECT 打包进发送
  缓冲后立刻清零。MQTT-C 会把打包好的报文留在发送缓冲里直到队列回收，这部分明文
  驻留是 MQTT-C 的既有行为，本模块没有改动，也不宣称已经消除。
- 并发：`vg_mqtt_cred_load()` 的模块级互斥锁覆盖封存文件打开、读取、UID 派生、
  解密、解析和明文清零。常驻网络管理器与 `vgmqtt` 调试入口同时读取时，共享的
  封存字节、解密文本和 UID 缓冲不会互相覆盖；`out` 在任何失败路径上都会被清零，
  调用方即使忽略返回值也拿不到半个凭证。
- 长度与终止：`username` 缓冲 64 字节、`password` 缓冲 128 字节，扫描器最多接受
  比缓冲少一个字符的值，超长返回 `-ENOSPC`，不会截断成一个更短但能通过认证的值。

## 5. 失败行为

| 情况 | 返回 | 板端表现 |
|---|---|---|
| 没有记录 | `-ENOENT` | `vgnet: no MQTT credentials in the provision store (err 2)`，不连 |
| 完整性校验不过 | `-EACCES` | 同上，`err 13` |
| 明文不是 MQTT 记录 | `-EINVAL` | 同上，`err 22` |
| 存储不可读 | 其它负值 | 同上 |
| 写入中途失败 | `vg_file_replace` 的错误 | 保留上一条记录，`commit` 打印 refused |

`CONFIG_VG_MQTT_CREDENTIALS` 未打开的构建（`CRYPTO_MBEDTLS` 缺失的 bring-up 预设）
返回 `-ENOSYS` 并打印 `this build has no MQTT credential store`，同样不连。

所有路径都不使用旧编译口令，也不降级成匿名连接。MQTT 会话失败只影响 MQTT：本地
采集、告警、点表、LED 和界面走各自的线程和文件，不经过这条路径。

## 6. 兼容性

- LLM 记录的文件路径、格式、加密方案和 `put` / `commit` / `apply` 流程没有改动，
  已 provision 的板子不需要重录。
- `vgprovision uid` / `status` / `wipe` / `put` / `commit` / `apply` 的行为不变。
- `vgmqtt` 的 `-h` / `-p` / `-t` / `-m` / `-q` / `-r` / `-w` 不变，`-u` / `-P` 保留。
- 顺手修掉 `json_get_string()` 的越界判断：原来是 `i + 1 >= outsz`，会拒绝一个刚好
  填满缓冲的值。改成 `i >= outsz` 之后，每个调用方的实际上限与数组大小一致。这只
  放宽一个字节，已有的 LLM 记录不受影响。

### 对 bring-up 预设的影响

`velaguard-min` / `velaguard-emmc` / `velaguard-net` / `velaguard-ai-probe` 四个预设
的 defconfig 里本来就没有 MQTT 用户名和口令，改动前 `mqtt_user()` / `mqtt_pass()`
返回 NULL，也就是匿名连接。改动后这几个预设同样没有编译期凭据可用：

- 有 `CRYPTO_MBEDTLS` 的（`velaguard-net`、`velaguard-ai-probe`）走封存记录，
  没 provision 就拒绝连接；
- 没有 `CRYPTO_MBEDTLS` 的（`velaguard-min`、`velaguard-emmc`）`VG_MQTT_CREDENTIALS`
  默认关，直接回 `-ENOSYS` 并打印 `this build has no MQTT credential store`。

也就是说这几个预设不再匿名上线。这是"没有编译期回退"的直接结果，也符合本任务
要求；如果某个预设确实需要匿名连接，得单独开一个显式开关，不要靠恢复默认口令。

## 7. 主机验证（已完成）

在 worktree 里运行：

```bash
make -C app/velaguard/host_tests test
```

`test_mqtt_cred` 覆盖：

- 合法记录往返（用户名和口令都逐字节比对）
- 长度边界：刚好填满缓冲的用户名可以往返，超一个字符返回 `-ENOSPC`
- 空用户名、空口令、外来 `kind` 标签被拒绝，且失败后不残留任何字段
- 用途隔离双向：LLM 记录放到 MQTT 路径被拒，MQTT 记录放到 LLM 路径被拒
- 缺失记录（`-ENOENT`）、完整性校验失败（`-EACCES`）及对应的 `status` 文案
- `status` 输出不含口令、不含完整用户名，用户名按 `ve****rd` 脱敏
- 写入中途失败（`--wrap=rename` 让发布那一步失败）后旧记录逐字节不变
- 外来记录在 `install` 阶段被拒，旧记录同样不变
- 空 blob、超长 blob 被拒；合法 blob 能替换记录
- 两个加载并发时不能同时进入共享封存缓冲；测试固定了第一个加载已读、尚未校验时的
  窗口，第二个记录替换后仍必须各自读到自己打开时的内容
- 四个普通并发加载都要成功并返回同一份记录

共享缓冲测试对 `9e88868` 的修前源码先失败，再对修复源码通过：

- [修前原始输出](maintenance-evidence/2026-10-18/d03-mqtt-cred-interleave-pre-fix.log)
  为 `EXIT_STATUS=1`，两条并发断言失败。
- [修后原始输出](maintenance-evidence/2026-10-18/d03-mqtt-cred-interleave-post-fix.log)
  为 `EXIT_STATUS=0`。

认证入口测试使用真实的 `vg_mqtt_session_open()`、`vgmqtt` 参数解析和默认凭证函数，
只替换设备 UID、封存加解密、MQTT-C、传输或网络系统调用：

- `test_mqtt_session_auth`：记录缺失时 `vg_tcp_open()` 调用数为 0；首次会话读取记录；
  关闭后替换记录，下一次会话重新读取新用户名和口令，不复用上一会话的明文副本。
- `test_mqtt_vgmqtt_auth`：不传 `-u` / `-P` 时从同一封存路径读取；替换记录后重新读取
  新值；记录缺失时完整入口在网络包装函数被调用前返回。网络系统调用计数必须为 0。

原始构建和运行输出见
[d03-mqtt-auth-entrypoints.log](maintenance-evidence/2026-10-18/d03-mqtt-auth-entrypoints.log)。

另外用 `gcc -fsyntax-only -Wall -Wextra -Werror` 检查了 `vgprovision.c`、
`velaguard_mqtt.c`、`vg_mqtt_session.c`，`CONFIG_VG_MQTT_CREDENTIALS` 打开和关闭两种
分支都编译干净。

这些是主机结果，不是板验结论。

## 8. 尚未执行

- **没有编译固件。** 本会话不运行会改动共享 `../nuttx` 配置或产物的构建。
- **没有真机验收。** 没有连真实 broker，没有改线上口令，没有动开发板。
- 因此不能声称新固件能连上 broker，也不能声称板端已经用上运行时凭证。

## 9. 集成时需要一起改的地方

1. **共享 `../nuttx` 板级 defconfig 还留着口令。**
   `/home/hello19y/openvela/nuttx/boards/arm/stm32h7/stm32h750b-dk/configs/velaguard-lvgl/defconfig`
   里仍有 `CONFIG_VG_MQTT_USERNAME` 和 `CONFIG_VG_MQTT_PASSWORD` 两个键，值就是那个
   明文口令。它是实际参与编译的配置，本仓的 `scripts/configs/*.defconfig` 只是参考
   副本。本会话按约束没有改共享树，需要在 nuttx 的 `velaguard/*` 分支上同步删除这
   两行（属于 AGENTS.md 允许的配置复制）。改之前构建出的镜像仍然带口令。
   `velaguard-min` / `velaguard-emmc` / `velaguard-net` / `velaguard-ai-probe` 四个预设
   没有这两个键，不需要动。

2. **协议文档。** `docs/velaguard-host-nsh-protocol.md` 的 `vgprovision` 一节需要补
   `mqtt put` / `mqtt commit` / `mqtt status` / `mqtt wipe` 与新的失败码；
   `docs/velaguard-mqtt-contract.md` 里"凭证来自 Kconfig"的说法要改成运行时记录。
   按会话约束本次没有改这两份文档。

3. **维护清单。** `docs/maintenance-todo.md` 的 SEC-01 一行可以在集成时关闭，关闭
   条件（仓库与镜像都不再含明文口令、板端仍能连上 broker）里"镜像"这一半要等第 1
   项和板验都完成。

## 10. 遗留事项

1. **旧口令必须轮换。** 它已经进入 git 历史、AI Coding 日志和烧录过的镜像，删除
   当前文件不能回收它。轮换要在板验窗口之外单独做，避免打断正在进行的验收。
2. **旧镜像处置。** 带口令的 `.debug/nuttx.hex` / `nuttx.bin` 及任何归档镜像应作废
   并重新烧写，不要继续分发。
3. **历史日志里的口令** 按 C5 不能删改片段，只能由本人决定是否删除整份会话文件。
4. **强制重连。** 现在没有 NSH 命令可以让已在线会话立刻换用新记录，只能等出口切换
   或复位。如果需要，可以后续加一条 `vgnet` 子命令触发 `tcp_reconnect`。
5. **`vgagent status`** 目前只报 LLM 记录是否存在。是否需要增加一行 MQTT 记录是否
   存在（只报存在性）留给集成时决定。
6. **LLM `commit` 仍是裸 rename。** MQTT 的 `commit` 会先校验再走 `vg_file_replace`，
   失败时保留旧记录；LLM 那条路径保持原样没有动，因为本任务要求不改变现有 LLM 流程。
   两条路径对齐与否，留给集成时决定。

## 11. 涉及文件

| 文件 | 改动 |
|---|---|
| `app/velaguard/vg_provision.h` | MQTT 记录的结构、常量与 API；记录路径可用宏覆盖；新增 `VG_PROVISION_BLOB_MAX` |
| `app/velaguard/vg_provision.c` | `vg_mqtt_cred_parse/install/load/present/status/mask_user`；修 `json_get_string` 越界判断；`VG_PROVISION_DIR` 可用宏覆盖 |
| `app/velaguard/vgprovision.c` | `mqtt put/commit/status/wipe` 子命令；`put` 泛化为带目标路径；持久化提示抽成函数 |
| `app/velaguard/vg_mqtt_session.c` | 凭据改从封存记录读取，去掉编译期回退；建链后清零 |
| `app/velaguard/velaguard_mqtt.c` | 默认凭据改从封存记录读取；去掉 Kconfig 默认 |
| `app/velaguard/Kconfig` | 删除两个 string 配置项；新增 `VG_MQTT_CREDENTIALS`（默认 y，依赖 `VG_BRINGUP_TOOLS && CRYPTO_MBEDTLS`） |
| `app/velaguard/Makefile` | 封存模块按 `EXAMPLES_AI_AGENT_VELA` 或 `VG_MQTT_CREDENTIALS` 之一构建；`vgprovision` 随封存模块一起构建 |
| `scripts/configs/velaguard-lvgl.defconfig` | 删除用户名和口令两行，加 `CONFIG_VG_MQTT_CREDENTIALS=y` |
| `app/velaguard/host_tests/test_mqtt_cred.c` | 新增行为测试 |
| `app/velaguard/host_tests/Makefile` | 新增 `test_mqtt_cred` 目标并接入 `test` |
