# G 阶段最终镜像禁用命令复核

记录时间：2026-10-19 09:50（Asia/Shanghai）

对象为 2026-10-18 23:48:40 构建并于 10-19 烧录的最终维护镜像。通过 COM3 单路 NSH 执行只用于拒绝验证的命令：

```text
vgagent tool run_shell '{"command":"vgpoint"}'
```

板端返回 `[shell] Blocked dangerous command: vgpoint`、`tool run_shell rc=-1` 和 `Command 'vgpoint' is blocked for safety`，随后正常返回 `nsh>`。工具闸门在执行命令前拒绝输入，没有进入 `vgpoint` 子命令；本次没有读写点表。

该记录只证明最终镜像上的 `vgpoint` 命令拒绝路径有效，不覆盖其他命令、并发轮次隔离或全部工具权限。此前 2026-10-18 21:50 固件上的工具边界记录见 [G 阶段早期复核](../2026-10-18/final-g-tool-guard-evidence.md)。

原始串口记录：[agent-command-gate-final-image-20261019.log](agent-command-gate-final-image-20261019.log)，SHA256 `7d89c8f9a7a281cd5a623692bc4beb94df8decd69dc186058c88e0c15f5728be`。
