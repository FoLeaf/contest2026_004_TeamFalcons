# G 阶段最终镜像限流复核

记录时间：2026-10-19 09:40（Asia/Shanghai）

设备为 2026-10-18 23:48:40 构建并于 10-19 烧录的最终维护镜像。通过 COM3 单路 NSH 顺序执行 22 次 `vgagent tool vg_point_read ups_load`，未并行发起其他串口命令。

- 前 20 次均返回 `rc=0`，读数为 UPS负载 40%。
- 第 21、22 次均返回 `rc=0` 和 `reason=rate_limited`。
- 随后 `vgagent status` 显示轮次 `state=idle`、`owner=none`，LLM 凭证为 ready。
- 最终镜像源码先调用 `vg_agent_rate_allow()`，未通过时直接返回 `rate_limited`，早于 `vg_point_read` 的参数解析与执行分支：[vg_agent_tools.c](../../../app/velaguard/vg_agent_tools.c#L459-L493)。

该复核证明限流返回路径在最终镜像上生效。此次没有记录前后总线计数差值，因此不把限流调用期间的总线活动单独作为测量结论。原始串口输出：[rate-limit-final-image-20261019.log](rate-limit-final-image-20261019.log)，SHA256 `70451dd3edc98abd4f03b44c84d3e9fb13dc8f6b411c38c951f8002bcaa89041`。
