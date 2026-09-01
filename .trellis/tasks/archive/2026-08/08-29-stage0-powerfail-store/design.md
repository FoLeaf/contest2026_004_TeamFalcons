# 设计：掉电安全配置存储

## 1. 边界


| 层      | 位置                                      | 说明                |
| ------ | --------------------------------------- | ----------------- |
| 存储 API | `app/velaguard/`（建议 `config_store.c/h`） | 双槽读写、CRC、seq、默认回退 |
| 板端验证   | NSH 命令或 `VG_BRINGUP_TOOLS` 子命令          | 提交 / 损坏注入 / dump  |
| 主机单测   | `app/velaguard/host_tests/`             | 无硬件算法测试           |
| 不做     | 板级 SDMMC、ai_agent 配置目录正式化               |                   |


## 2. 文件契约

路径（bring-up）：

```text
/mnt/emmc/velaguard/config/
  point_table_a.json
  point_table_b.json
```

每份 JSON 逻辑字段（示意）：

```json
{
  "schema_version": 1,
  "seq": 42,
  "committed": true,
  "crc32": "A1B2C3D4",
  "payload": { }
}
```

- `crc32`：对规范化 payload（或整文件去掉 crc 字段后的字节）计算
- 活动槽：启动时比较两份合法记录的 `seq`

## 3. 提交状态机

```text
identify inactive slot (lower seq or invalid)
→ write full record with committed=false, new_seq=max+1
→ fsync
→ set committed=true (rewrite or second pass)
→ fsync
→ optional: read-back verify CRC
```

崩溃窗口：


| 窗口                 | 恢复行为                        |
| ------------------ | --------------------------- |
| 写 inactive 未完成     | 旧活动槽仍合法                     |
| committed=false 残留 | 忽略该槽，用另一槽                   |
| 两槽皆非法              | factory default + log event |




## 4. API 草图

```c
int vg_config_load(struct vg_point_table *out);   /* boot */
int vg_config_commit(const struct vg_point_table *in);
void vg_config_factory_default(struct vg_point_table *out);
```

首期 `vg_point_table` 可为最小 stub（设备名、少量寄存器条目）。

## 5. 测试策略

1. **host_tests**：内存假块设备或临时文件双槽；截断、坏 CRC、seq 逆序
2. **板端**：commit → reboot；手动截断 a 槽 → reboot 用 b；双删 → 默认



## 6. 回滚

删除 config_store 与测试命令；不影响 `/mnt/emmc` 其它文件。