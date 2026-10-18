# 集成后离线回归记录

记录时间：2026-10-18 23:09 至 23:11（Asia/Shanghai）

- 工作区：`/home/hello19y/openvela/.vg-maintenance-closeout`
- 分支：`codex/maintenance-closeout`
- 提交：`7acaf84bdfdeb1581a074c8b1c5d4d0ced2138b3`
- 测试前状态：clean
- 测试后状态：临时测试二进制已清理，仍为 clean

## 结果

| 范围 | 命令 | 结果 | 原始日志 |
|---|---|---|---|
| 完整主机回归 | `make -C app/velaguard/host_tests clean test` | 32 个 C 目标与 2 个 Python 检查全部通过 | [host-tests-full-closeout-current.log](host-tests-full-closeout-current.log) |
| Agent 专项 | `make -C app/velaguard/host_tests/agent_review clean test` | 8 个目标全部通过，F-01 复现程序判定缺陷已修复 | [agent-review-closeout-current.log](agent-review-closeout-current.log) |
| 长测脚本离线集 | Windows PowerShell 执行 `scripts/tests/soak/test_soak.ps1` | 202 项断言通过，0 失败 | [soak-offline-closeout-current.log](soak-offline-closeout-current.log) |
| 从站自检 | Windows PowerShell 执行 `scripts/maintenance_rtu_slave.ps1 -SelfTest` | 全部检查通过，`SELF-TEST: ok` | [slave-selftest-closeout-current.log](slave-selftest-closeout-current.log) |

## 日志哈希

```text
9859ce837b75910850c814c316b644978c4b15223eb896eb918ae708284689bd  host-tests-full-closeout-current.log
c02f1c475191f2c27acd45e9ce4caa225cb818bf836a4ca497e57006d4461658  agent-review-closeout-current.log
5fe15e812f6afac846ca56109d3847ab8337a9c67730d0ee331e4273d2c09b51  soak-offline-closeout-current.log
795db3ab6309dc210c6e3a9a30c30e315d8c05f25bd2536c0b8f604d55093ebd  slave-selftest-closeout-current.log
```

## 边界

- 本记录只证明主机、离线脚本和 RTU 从站自检结果。
- 主机测试使用的 MQTT 凭证均为虚构数据。
- 本记录不证明最终固件身份、真机验收或 24 小时长测已经完成。
- D-06 的人工确认仍未发生，未发送 `vgpoint apply --confirm`。
