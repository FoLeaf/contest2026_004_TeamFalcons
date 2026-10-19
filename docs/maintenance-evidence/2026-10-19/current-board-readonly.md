# 2026-10-19 板端只读状态复核

采集时间：2026-10-19 08:19（Asia/Shanghai）。仅运行只读 NSH 查询；未编辑候选、未 apply、未复位或烧录。

## 固件与持久化状态

- `uname -a`：`NuttX 0.0.0 2373ab4a45d Oct 18 2026 21:50:34 arm stm32h750b-dk`。该版本早于集成分支最近构建，说明新构建尚未刷入板。
- `vgpoint list -c` 与 `vgpoint list` 均报告 14 点；`temp.name` 都是 `温度验收`，其余已显示字段相同。
- `vgcfg dump` 为 `seq=6 schema=1 committed=1 name=vgpoint`。
- `vgagent status` 为 `state=idle gen=5 owner=none`；显示最近回复写入了 `/data/velaguard/reports/alarm_advice.txt`。本次未读取真实 LLM 凭证。

## 当前连接条件

Windows 串口枚举只列出 COM1 与 ST-Link 虚拟串口 COM3，未列出 COM7。从站模拟器当前不可用。`vgruntime dump` 显示各从站窗口为超时、14 点均为离线；这与当时没有 COM7 从站一致，不据此判定板端固件故障。

板端持续提示 MQTT 凭证缺失，因此 MQTT 上报与重连仍未验收。该缺失与此前用户回复暂无测试凭证一致。

串口后台网络提示与 NSH 输出发生交错。本记录只采用仍完整可辨的固件标识、两张点表和配置序号；运行时快照不作为完整逐行验收证据。屏幕名称及持续刷新未目视确认。

## 原始记录

- [COM3 只读 NSH 查询](current-board-readonly.log)，SHA-256：`7fd4f3f43ebea6b5bdd1f7bc2374eae37a0d33840f6c3157a497b007d5f38601`
- [Windows 串口枚举](serial-port-enumeration.log)，SHA-256：`102e165d24b8c08e6dded92c542b0d3c504ac267cbe077e89c8eff849bedcc8f`
