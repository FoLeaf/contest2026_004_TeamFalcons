# D-06 人工确认、生效与重启持久化

记录日期：2026-10-18（Asia/Shanghai）。本页接续原会话的候选试读，不宣称最终固件全部验收完成。

## 固件与确认对象

板端仍为 `NuttX 0.0.0 2373ab4a45d Oct 18 2026 21:50:34`，产物来源见
[D-06 构建记录](final-build-provenance-after-d06.md)。后续新增的 Agent 等级刷新补修尚未烧入本板。

两张表均为 14 点，唯一差异为 `temp.name`：已确认表为 `温度`，候选为 `温度验收`。
续接后先重新列举两表，再独立执行 `vgpoint test`，14 条 READ 全部 `ok=1`，返回
`OK cmd=test table=candidate n=14`，温度为 24°C。

开发助手向用户展示上述差异和本次试读结果后停止等待。用户明确回复：

> 确认，将温度验收候选生效

收到回复后，独立的一次串口调用只发送 `vgpoint apply --confirm`，返回
`OK cmd=apply table=committed n=14`。未把试读与 apply 合并执行。

## 生效后与重启后检查

| 检查 | 实际结果 |
|---|---|
| 运行时已确认表 | 14 点，`temp.name=温度验收` |
| eMMC `config/points.json` | `temp.name=温度验收`，其余字段与确认对象一致 |
| 正常采集 | 14 点全部 `ok=1`，温度 24°C |
| 配置元数据 | `seq=6 schema=1 committed=1 name=vgpoint`，未被点表提交重置 |
| 用户屏幕观察 | 用户回复 `24`，仅计为读数观察；名称和持续刷新未得到明确回复 |
| MQTT | `mqtt=down`，缺失 MQTT 封存凭证，上报一致性仍未验证 |
| 重启持久化 | ST-LINK 复位后仍加载 14 点，名称为 `温度验收`，全部 `ok=1`，配置序号仍为 6 |

NSH `reboot` 返回 `command not found`，该次调用未引起重启。随后主机于
23:39:24 左右使用 `STM32_Programmer_CLI -c port=SWD mode=UR -rst` 完成一次有意复位，
没有重烧固件。启动日志确认 eMMC、HMI 和 Agent 启动，HMI 为 `home fleet n=14`、
`live ok=14/14 a1=24.0`。该结果不替代屏幕名称目视。

重启初期板端从保存值恢复时钟，串口 `uptime` 的墙上时间与主机可能不同。采集清单使用
主机 Asia/Shanghai 完成时间；早期 `.debug` 文件名中的时间标签不是可靠时间戳。
归档文件内容逐字复制，哈希和主机时间见 [采集清单](d06-confirmation-capture-manifest.json)。

## 原始记录

- [续接基线和两表](d06-resumed-baseline.log)
- [重新试读](d06-resumed-test.log)
- [人工确认后的单独 apply](d06-human-confirmed-apply.log)
- [生效后文件、运行时和采集](d06-after-human-apply.log)
- [NSH 重启命令不可用](d06-nsh-reboot-unavailable.log)
- [ST-LINK 有意复位](d06-persistence-reset.log)
- [启动与持久化复查](d06-persistence-after-confirm.log)

## 尚未关闭

D-06 的人工确认和持久化节点已经完成。首页名称目视、MQTT 一致性和最终补修固件的复验
仍待完成；QA-04 仍未定位，24 小时长测尚未开始。当前名称保留为 `温度验收`，恢复原名称
须重新编辑候选、试读、展示并取得新的人工确认。
