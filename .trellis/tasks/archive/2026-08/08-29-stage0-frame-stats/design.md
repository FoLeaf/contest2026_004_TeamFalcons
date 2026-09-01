# 设计：帧级质量统计（MVP B）

## 1. 边界

| 层 | 位置 | 职责 |
|----|------|------|
| 统计核 | `vg_frame_stats.c/h` | 分从站窗口、聚合、inject |
| 挂钩 | `modbus_collector.c`（vgmodbus） | 每次读写结束后 `record` |
| 板测 | NSH `vgstats` | dump / inject / reset |
| 单测 | `host_tests/test_frame_stats.c` | 无硬件 |

不做：采集调度重构、Agent 绑定、分位数。

## 2. 数据模型

```c
enum vg_fs_result {
  VG_FS_OK = 0,
  VG_FS_CRC,
  VG_FS_TIMEOUT,
  VG_FS_ECHO,   /* 收到疑似本机回声（可选检测；可先仅 inject） */
  VG_FS_OTHER
};

struct vg_fs_summary {
  uint32_t total;
  uint32_t ok;
  uint32_t crc_err;
  uint32_t timeout;
  uint32_t echo;
  uint32_t other;
  uint32_t lat_min_ms;   /* ok==0 → 无效 */
  uint32_t lat_max_ms;
  uint32_t lat_avg_ms;
};
```

每从站：环形缓冲最近 N 条 `{result, latency_ms}`，或只维护运行总和 + 环形延迟样本。  
MVP 推荐：**计数器 + 对成功延迟维护 min/max/sum/count**（不必存满 N 条结果，窗口用「最近 N 次」时可用环形计数器简化为固定容量事件环）。

默认：`VG_FS_WINDOW=64`，`VG_FS_MAX_SLAVES=8`（按 addr 映射或小表）。

## 3. API

```c
void vg_fs_init(void);
int  vg_fs_record(uint8_t slave, enum vg_fs_result r, uint32_t latency_ms);
int  vg_fs_summary(uint8_t slave, struct vg_fs_summary *out);
int  vg_fs_reset(uint8_t slave);      /* slave=0 → all */
int  vg_fs_inject(uint8_t slave, enum vg_fs_result r, uint32_t latency_ms);
```

`inject` ≡ `record`（板测别名）。

## 4. 挂钩

`vgmodbus` 每次 `nmbs_read_*` 返回后：

- 成功 → `VG_FS_OK` + 测得 ms  
- `NMBS_ERROR_CRC` → `VG_FS_CRC`  
- `NMBS_ERROR_TIMEOUT` → `VG_FS_TIMEOUT`  
- 其它负错误 → `VG_FS_OTHER`  
- 回声：stage0 以 inject 验证计数为主；若 port 层易检测再挂（不阻塞 AC）

## 5. 风险

| 风险 | 应对 |
|------|------|
| 无真实从站 | inject + host 单测仍过 AC1；AC2 用 inject |
| 从站数膨胀 | 固定 MAX_SLAVES；超出返回 `-ENOMEM` |
| avg 溢出 | sum 用 `uint64_t` |
