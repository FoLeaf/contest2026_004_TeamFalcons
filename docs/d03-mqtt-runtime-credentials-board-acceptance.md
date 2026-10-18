# D-03 板验清单：MQTT 运行时凭证

本清单**尚未执行**。主机测试通过不等于板验通过，写在这里是为了让集成后能照着一次
做完。

前置条件：

- 共享 `../nuttx` 板级 defconfig 已删除 MQTT 用户名和口令，并打开
  `CONFIG_VG_MQTT_CREDENTIALS`。
- 开发板空闲。另一个会话正在用 COM3 / COM7 做点表验收，必须等它结束再开始。
- 一个可以连的 broker 和一个测试用口令。**不要**在验收过程中使用或打印线上口令。

凭证隔离要求：

- 全程只使用专用测试凭据，不读取、复制或改写 `llm_secrets.v1`。
- 若板上已有真实 MQTT 记录，先把密文文件移到同一目录的
  `mqtt_secrets.v1.acceptance-hold`，验收结束后移回原文件名。记录只以封存 blob
  形式备份，不打印内容。
- `vgstoretest` 只写 `/data/velaguard/diagnostics/store-probe.bin`，不得用来覆盖或
  损坏任何凭证文件。
- `CONFIG_VG_WIFI_PASSWORD` 是独立待办。本清单不能作为 WiFi 凭证已整改的结论。

## 0. 构建与烧录

```bash
bash scripts/build.sh --clean
```

构建日志里检查两点：

- `CONFIG_VG_MQTT_CREDENTIALS=y` 生效；
- `CONFIG_VG_MQTT_USERNAME` / `CONFIG_VG_MQTT_PASSWORD` 不再出现在生成的 `.config` 里。

```bash
powershell.exe -ExecutionPolicy Bypass -File scripts/flash.ps1
```

## 1. 镜像里没有口令（关闭条件之一）

由验收脚本从旧提交的受控 defconfig 中读取 MQTT 键值，在内存中扫描镜像；输出只允许
包含键名、文件 SHA256 和是否命中。不得把口令写入命令行参数、临时明文文件、日志或
串口。检查对象至少包括：

- `.debug/nuttx.bin`
- `.debug/nuttx.elf`
- 实际烧录的 QSPI 二进制或 HEX 中可还原的载荷

期望 MQTT 用户名和口令两个键均为 `hit=no`。同时确认生成的 `nuttx/.config` 不含
`CONFIG_VG_MQTT_USERNAME` / `CONFIG_VG_MQTT_PASSWORD`，含
`CONFIG_VG_MQTT_CREDENTIALS=y`。

## 2. 未 provision 时拒绝连接

板子复位后，在没有 MQTT 记录的板子上让网络管理器尝试建链，串口应出现：

```text
vgnet: no MQTT credentials in the provision store (err 2); provision with 'vgprovision mqtt' and retry
```

逐项确认：

- [ ] 没有出现匿名连接或任何旧口令连接；
- [ ] 没有 CONNACK，`vgnet status` 里 `mqtt` 不是 up；
- [ ] 本地功能不受影响：Modbus 采集继续、首页有点位值、告警页能弹、`vghmi` 不重启；
- [ ] 重试是有节奏的（约 2 秒一次），不是刷屏或死循环。

```text
nsh> vgprovision mqtt status
vgprovision: mqtt credentials: MISSING (err 2)
```

## 3. 写入凭证

按实现说明第 3 节：`vgprovision uid` 取 UID，主机侧用
`scripts/vg-provision-pack.py` 封存 `{"kind":"mqtt","username":...,"password":...}`，
再分段 `vgprovision mqtt put <hex>`，最后 `vgprovision mqtt commit`。

- [ ] `commit` 打印 `committed to .../mqtt_secrets.v1`，没有 `WARNING store is not
      persistent`；
- [ ] 串口输出、NSH 历史里都没有口令明文（只有十六进制 blob）；
- [ ] `vgprovision mqtt status` 回 `OK`，用户名脱敏、口令只报长度。

```text
nsh> vgprovision mqtt status
vgprovision: mqtt credentials: OK user=ve****rd password=set(<n>)
vgprovision: file=/data/velaguard/provision/mqtt_secrets.v1 persistent=yes
```

## 4. 不需要复位就生效

会话离线时网络管理器每 2 秒重试，`commit` 之后应当在两秒左右看到：

```text
vgnet: mqtt online network=... backend=... device=vg-...
```

- [ ] 没有复位板子就建链成功；
- [ ] `vgnet status` 显示 `mqtt=up`；
- [ ] broker 侧看到该 client_id 上线，且**用的是新口令**（旧口令应当被拒绝）。

## 5. 复位后仍然可用（持久化）

- [ ] 复位板子，等网络起来，MQTT 重新 online；
- [ ] `vgprovision mqtt status` 仍回 `OK`；
- [ ] 复位前后 `password=set(<n>)` 的长度一致。

## 6. 重连

- [ ] 拔掉网线再插回（或触发一次出口切换），会话重新建链并 online；
- [ ] 掉线期间本地采集、告警、界面不受影响；
- [ ] 重连后不需要重新 provision。

## 7. 坏记录的行为

每项都先保护现有 MQTT 密文，再只操作 MQTT 路径。不得动 LLM 记录，也不得用
`vgstoretest` 生成损坏数据。

- [ ] 缺失恢复：把 `mqtt_secrets.v1` 移到 `mqtt_secrets.v1.acceptance-hold`，
      触发一次会话重开或复位，确认第 2 节的 MISSING 行为，随后把 `acceptance-hold`
      移回原名并确认 `vgprovision mqtt status` 恢复为 `OK`；
- [ ] 用途隔离：在主机用虚构的 `host`、`model`、`api_key` 和本板 `vgprovision uid`
      返回的 UID 生成一条专用测试 LLM 格式封存记录，只把这条测试 blob 分段
      `vgprovision mqtt put <hex>` 后再 `vgprovision mqtt commit`，应打印
      `mqtt commit refused`，且 `status` 仍显示上一条专用测试 MQTT 记录。全程不得
      读取、复制、打印或改写板上的 `llm_secrets.v1`；
- [ ] 损坏恢复：把 `mqtt_secrets.v1` 移到 `mqtt_secrets.v1.acceptance-hold`，在原名
      路径写入 `not-a-sealed-record` 这几个字节，触发会话重开或复位，确认状态为
      `UNREADABLE`、MQTT 不连且本地功能正常；随后移走坏文件并把
      `acceptance-hold` 移回原名；
- [ ] 恢复后确认 `vgprovision mqtt status` 为 `OK`，MQTT 重新上线，用户名仍脱敏、
      口令仍只显示长度。

## 8. 调试入口

- [ ] 不带 `-u` / `-P` 运行 `vgmqtt`，应当能连上，说明默认取的是运行时记录；
- [ ] 不带 `-u` / `-P` 且在未 provision 的板子上运行 `vgmqtt`，应当报
      `no MQTT credentials in the provision store` 并退出，不连接。

## 9. Agent 不能碰凭证

- [ ] `vgagent tools` / `vgagent tool` 的输出里没有与凭证相关的工具；
- [ ] 让 Agent 调 `run_shell` 执行 `vgprovision status`、`vgmqtt`：都应被白名单挡下；
- [ ] `vgagent status` 的 `provision_file` 只反映 LLM 记录，没有多出可读的口令接口。

## 10. 记录结果

把串口日志（不含口令）放进验收证据目录，并在维护清单里更新 SEC-01：

- [ ] 仓库受控配置里没有明文口令；
- [ ] 烧录镜像里没有明文口令（第 1 节）；
- [ ] 板端仍能连上 broker（第 3、4、5 节）；
- [ ] 旧口令已轮换，旧镜像已作废。
