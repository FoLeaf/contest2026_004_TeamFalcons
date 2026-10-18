# D-06 串口控制台失联与人工恢复记录

记录时间：2026-10-18 20:45 至 20:58（Asia/Shanghai）。

本记录只保留现场可证事实。失联原因仍然未知，人工复位恢复成功不能证明问题已经消失。

## 1. 失联现场

`d06-08-current-baseline.log` 在文件时间 20:46:56 完成。串口先收到正常心跳和
`vgtime` 同步输出，随后以下命令全部返回 `len=0`：

- `uname -a`
- `vgruntime dump`
- `vgpoint list -c`
- `vgpoint list`
- `vgpoint get`
- `vgnet status`
- `vgcfg dump`
- `ps`

失联前串口没有出现 panic、HardFault、断言或其它明确错误文本，最后可见的是
`[heartbeat] skip LLM` 行。不能据此排除未输出到串口的异常。

复位前通过现有 OpenOCD Telnet 会话观察到：

- `stm32h7x.cpu0` 的 `curstate` 为 `running`。
- 尝试读取 `reg pc` 失败，没有取得 PC。
- 没有取得任务栈、异常帧、CFSR/HFSR、看门狗状态或其它 CPU 现场。

OpenOCD 查询没有保存原始文本记录，本节依据当时的操作输出；
`d06-08-current-baseline.log` 是保留的原始串口证据。

## 2. 人工恢复

约 20:47，通过现有 OpenOCD 的 Telnet 端口 4444 发送 `reset run`。这是人工恢复动作，
不是目标自行恢复。

`d06-09-recovery-boot.log` 保存了复位后的完整启动输出，文件中可见 eMMC 挂载、
`/data` 链接、LVGL 启动、14 点采集、`ai_agent` 启动、以太网和 MQTT 上线。

恢复后 `d06-10-current-baseline-recovered.log` 记录：

```text
uname: NuttX 0.0.0 43702275f28-dirty Oct 18 2026 18:41:50 arm stm32h750b-dk
committed: 14 点，temp 名称为 温度
candidate: 14 点，temp 名称为 温度验收
runtime: 14 点在线，MQTT up，全部从站 boot_timeout/boot_crc/boot_echo/boot_other 为 0
```

候选表与已确认表除 `temp` 显示名外没有语义差异。候选表没有执行 apply。

复位后读取到的是 18:41:50 构建。失联瞬间无法重新读取 `uname`，因此不能仅凭复位后
输出证明失联前的整份源码、配置和镜像身份；这里只确认人工复位重新启动的是这支固件。

## 3. D-06 板端尝试

同一支 18:41:50 固件不包含 20:41:50 提交的 D-06 总线有界等待修复。

- `d06-11-candidate-test-after-recovery.log`：1 次 `vgpoint test`，返回 `bus_busy`。
- `d06-12-candidate-test-retries-after-recovery.log`：8 次重试，全部返回 `bus_busy`。

因此本轮没有可提交给人工确认的新试读成功结果，也没有执行 `vgpoint apply --confirm`。
这些日志证明当前固件上的失败现象，不能作为 D-06 修复后的板端通过证据。

## 4. 原始证据

| 文件 | 文件时间 | SHA256 |
|---|---|---|
| `d06-08-current-baseline.log` | 20:46:56 | `06c648ec6a16c3794bb13d1a0078456d67e80c6f86a2ceadd78cd5607f5a3c23` |
| `d06-09-recovery-boot.log` | 20:48:17 | `9bab2fc4ae495070428a27cd626258c723fdc843188cab1142fcbadf1e207213` |
| `d06-10-current-baseline-recovered.log` | 20:50:46 | `934751c7cefa907384904a4d9c4a7af3baa8b16d637e8be8cd8312467393574c` |
| `d06-11-candidate-test-after-recovery.log` | 20:52:19 | `b9c1d85aac24684c47d6a2107fa7a2879215e06a61ef14449bf93f08e30cfc65` |
| `d06-12-candidate-test-retries-after-recovery.log` | 20:58:35 | `76773413f76af05953328b728740a7a437fa791ad863fb86295e37ebdc574a23` |

## 5. 结论与未关闭事项

- 串口控制台失联是实际发生的事件，原因未知。
- 人工 `reset run` 恢复了启动和采集，不能据此关闭失联或长期稳定性问题。
- D-06 修复尚未进入板端固件，也没有修复后的试读成功记录。
- 下次出现同类失联时，应先保存串口、OpenOCD 状态和可读取的 CPU 现场，再决定是否复位。

