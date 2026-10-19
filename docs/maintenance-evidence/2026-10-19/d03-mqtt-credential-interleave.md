# MQTT 凭证并发读取回归复验

复验日期：2026-10-19。仅使用主机临时目录和虚构凭证，不读取板端凭证，也不访问 MQTT broker。

## 结果

把同一份 `test_mqtt_cred.c` 分别编译到 D03 修复前提交 `2b6f929` 和当前集成实现：

- 修复前连续 5 次退出码均为 1。每次都报告第二次读取进入共享缓冲区，以及首个读取者拿到另一条记录。
- 修复后连续 5 次退出码均为 0，均输出 `test_mqtt_cred: OK`。
- 旧版本测试对象来自 `git show 2b6f929:app/velaguard/vg_provision.c`。测试桩不引用 `g_mqtt_blob` 等实现私有符号，所以同一测试可以编译两种实现。
- 两个加载线程在第一次 `read()` 后通过条件变量交错；等待第二次读取的上限为 1 秒。完整实现把同一互斥锁覆盖到文件打开、读取、解密、解析和明文清理，修复后第二线程只能在第一线程退出临界区后读取。
- 并发线程创建失败时记录失败；普通并发读取用已成功创建线程的索引逐一 join，避免创建失败导致 join 未初始化句柄。

## 可追溯文件

| 文件 | SHA256 |
|---|---|
| 修复前输出 [日志](d03-mqtt-cred-interleave-pre-fix-20261019.log) | `6a0c78995e0029f29a69e0961ffabbd12a84372875d2e55f99c76919709fb5f2` |
| 修复后输出 [日志](d03-mqtt-cred-interleave-post-fix-20261019.log) | `88febbdb1318db3101d3427d3a3458a54f51396028c20280bf39f50f7a2d051f` |
| 回归测试 `app/velaguard/host_tests/test_mqtt_cred.c` | `63f2b30412786463677325cc82e8304b99bc33177fc22c9da896b7209625c589` |
| 集成实现 `app/velaguard/vg_provision.c` | `7df500f7ea9121eb03e583f54e171c25fa36ab2e93093444fbd34fa21746d99b` |
| 修复前实现 `2b6f929` 中的 `vg_provision.c` | `0d50f9121b7da187c882d5e5a3b0a3bad190ee593934968b2f1bef6f9b3294ac` |

这是并发回归的主机证据，不代表 MQTT 板端连接与恢复验收。专用测试账号仍未提供，板端 MQTT 项维持待验。
