# 自动日报调度线程启动修复

记录时间：2026-10-19 13:44（Asia/Shanghai）。

## 修改

`vg_ui_backend_acq_start()` 在启动采集线程前调用
`ensure_file_worker_started()`。文件工作线程负责：

- `vg_agent_round_tick()` 的队列提交和超时监视；
- 当天日报是否存在、是否需要请求；
- 告警解释的请求与结果消费。

修改前，文件工作线程只在报表请求、报表快照或 pending alarm 写入时惰性启动。
因此复位后停在首页且没有新告警时，自动日报调度不会执行。修改后，HMI 启动采集就
同时启动文件工作线程；现有报表页和告警写入仍可调用同一幂等启动函数。

改动位于 `app/velaguard/vg_ui_backend_board.c`。线程启动已有互斥锁、单实例标志、
显式 8192 字节栈和失败状态恢复；本次没有改变这些属性，也没有改变日报请求内容、
重试间隔或人工/Agent 权限。

## 回归

新增 `app/velaguard/host_tests/test_hmi_worker_start.py`，静态检查：

- 采集启动先请求文件工作线程，再创建采集线程；
- 文件工作线程启动是单实例，创建失败恢复未启动状态；
- 文件工作线程设置显式栈大小；
- 成功路径只 detach 一次并释放启动锁。

在维护工作树执行完整套件：

```bash
make -C app/velaguard/host_tests test
```

所有 C 测试、`test_hmi_worker_start.py`、`test_boot_stack.py` 和
`test_board_pins.py` 均通过，退出码 0。原始输出见
[主机完整套件](host-tests-daily-worker-fix-20261019.log)，SHA256
`000233eca4c24a597c24e228199e783348f24a613f62e57bbc9d66e08968a7c3`。

## 尚未完成的验收

- 新源码尚未构建并烧录，现运行板仍是 `2373ab4a45d Oct 18 2026 23:48:40`
  镜像。
- 需要在复位后停在首页、不触发告警、删除当天日报的条件下，确认自动请求出现。
- 修复会生成新镜像，因此正在进行的长测只验收旧镜像；最终连续 24 小时长测必须在
  包括本修复的新镜像上重跑。
- 本次静态接线测试只证明启动关系，不替代板端日报生成、Agent 轮次和文件校验证据。
