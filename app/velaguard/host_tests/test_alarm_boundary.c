/****************************************************************************
 * app/velaguard/host_tests/test_alarm_boundary.c
 *
 * 阶段 F 阈值告警与恢复的离线验证（主机侧，不开串口、不连板）。
 *
 * 数据来自 scripts/alarm_scenarios/alarm_scenarios_fixture.h，它由
 * scripts/alarm_scenarios/gen_alarm_scenarios.py 依据已确认点表副本
 * scripts/vgpoint_scene_room.json 生成。预期结果按
 * docs/velaguard-host-nsh-protocol.md 第 4 节的规则写成，不复用本文件的
 * 实现去推导。
 *
 * 覆盖：
 *   1. 点表里的原始值编码（int16 有符号 / uint16）经 scale 换算后，
 *      走真实 vg_alarm_eval 得到的种类与工况预期一致；
 *   2. 阈值可精确命中的前置条件（等于阈值的工况只有在 raw*scale 正好
 *      落在阈值上时才成立）；
 *   3. 离线滑窗的触发位置与成功恢复；
 *   4. 点表当前覆盖不到、需要合成规则的数值精度与 fail_n 区间。
 ****************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../vg_alarm_eval.h"
#include "alarm_scenarios_fixture.h"

/* 滑窗长度，与 vg_alarm_eval.c 的 vg_alarm_sample 一致；test_window_shape
 * 用真实实现反过来确认这个常量。 */
#define WINDOW_SIZE 8

static int g_fail;

#define CHECK(cond, ...) do { \
    if (!(cond)) { \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
      fprintf(stderr, __VA_ARGS__); \
      fprintf(stderr, "\n"); \
      g_fail++; \
    } \
  } while (0)

static enum vg_alarm_kind kind_from_int(int kind)
{
  switch (kind)
    {
      case 1: return VG_ALARM_KIND_WARN;
      case 2: return VG_ALARM_KIND_CRIT;
      case 3: return VG_ALARM_KIND_OFFLINE;
      default: return VG_ALARM_KIND_NONE;
    }
}

static const char *kind_name(enum vg_alarm_kind kind)
{
  switch (kind)
    {
      case VG_ALARM_KIND_WARN: return "warn";
      case VG_ALARM_KIND_CRIT: return "crit";
      case VG_ALARM_KIND_OFFLINE: return "offline";
      default: return "none";
    }
}

/* 与 vg_ui_backend_board.c 的换算一致：先按 dtype 取有符号/无符号，再乘 scale。 */
static float decode_raw(const struct vg_afx_point *p, unsigned raw)
{
  float v;

  if (strcmp(p->dtype, "uint16") == 0)
    {
      v = (float)raw;
    }
  else
    {
      v = (float)(int16_t)raw;
    }

  return v * p->scale;
}

static void rule_of(const struct vg_afx_point *p, struct vg_alarm_rule *rule)
{
  memset(rule, 0, sizeof(*rule));
  snprintf(rule->cmp, sizeof(rule->cmp), "%s", p->cmp);
  rule->has_warn = (uint8_t)p->has_warn;
  rule->has_crit = (uint8_t)p->has_crit;
  rule->warn = p->warn;
  rule->crit = p->crit;
  rule->fail_n = (uint8_t)p->fail_n;
}

/* 1. 点表与工况数据自检 */
static void test_fixture_shape(void)
{
  unsigned i;
  unsigned j;

  CHECK(VG_AFX_POINT_COUNT == 14, "点表点数应为 14，实际 %d",
        VG_AFX_POINT_COUNT);

  for (i = 0; i < VG_AFX_POINT_COUNT; i++)
    {
      const struct vg_afx_point *p = &vg_afx_points[i];

      CHECK(p->addr >= 1 && p->addr <= 247, "%s 从站地址越界", p->id);
      CHECK(p->fc == 3 || p->fc == 4, "%s 功能码非法", p->id);
      CHECK(p->scale > 0.0f, "%s 倍率应为正", p->id);

      for (j = i + 1; j < VG_AFX_POINT_COUNT; j++)
        {
          CHECK(strcmp(p->id, vg_afx_points[j].id) != 0,
                "点 id 重复：%s", p->id);
        }
    }

  /* 每个步骤的 expects 区间必须落在数组内 */
  for (i = 0; i < VG_AFX_STEP_COUNT; i++)
    {
      const struct vg_afx_step *s = &vg_afx_steps[i];

      CHECK(s->first + s->count <= VG_AFX_EXPECT_COUNT,
            "%s/%s 的 expects 越界", s->case_id, s->label);
      CHECK(s->primary_point == -1 || s->primary_kind == -1 ||
            (s->primary_point >= 0 &&
             (unsigned)s->primary_point < VG_AFX_POINT_COUNT),
            "%s/%s 的主告警点越界", s->case_id, s->label);
    }
}

/* 2. 阈值可精确命中：等于阈值的工况前提 */
static void test_thresholds_exactly_representable(void)
{
  unsigned i;

  for (i = 0; i < VG_AFX_POINT_COUNT; i++)
    {
      const struct vg_afx_point *p = &vg_afx_points[i];
      const float thresholds[2] = { p->warn, p->crit };
      const int has[2] = { p->has_warn, p->has_crit };
      int t;

      if (p->cmp[0] == '\0' || strcmp(p->cmp, "eq") == 0)
        {
          continue;
        }

      for (t = 0; t < 2; t++)
        {
          float raw_f;
          long raw;
          float value;

          if (!has[t])
            {
              continue;
            }

          raw_f = thresholds[t] / p->scale;
          raw = lroundf(raw_f);
          CHECK(fabsf(raw_f - (float)raw) < 0.001f,
                "%s 的阈值 %.6g 不在倍率 %.6g 的网格上，无法用整数原始值精确命中",
                p->id, (double)thresholds[t], (double)p->scale);

          value = decode_raw(p, (unsigned)raw);
          CHECK(value == thresholds[t],
                "%s 的阈值 %.9g 无法精确命中：raw=%ld 解出 %.9g",
                p->id, (double)thresholds[t], raw, (double)value);
        }
    }
}

/* 3. 逐工况验证：原始值 -> 换算 -> 真实规则 -> 与预期比较 */
static void test_fixture_steps(void)
{
  unsigned i;

  i = 0;
  while (i < VG_AFX_STEP_COUNT)
    {
      const char *case_id = vg_afx_steps[i].case_id;
      struct vg_alarm_window windows[VG_AFX_POINT_COUNT];
      int online[VG_AFX_POINT_COUNT];
      unsigned j;
      unsigned p;

      memset(windows, 0, sizeof(windows));
      for (p = 0; p < VG_AFX_POINT_COUNT; p++)
        {
          struct vg_alarm_rule rule;
          float thr = 0.0f;

          online[p] = 1;
          rule_of(&vg_afx_points[p], &rule);
          vg_alarm_sample(&rule, &windows[p], 1,
                          decode_raw(&vg_afx_points[p],
                                     vg_afx_baseline[p]), &thr);
        }

      /* 每个点的窗口要跨步骤保留。I8 用一次 drop 生成一段连续失败，
       * 前 4 次不触发，第 5 次进入离线；不能假设两次人工控制之间没有
       * 成功的采样。 */

      while (i < VG_AFX_STEP_COUNT &&
             strcmp(vg_afx_steps[i].case_id, case_id) == 0)
        {
          const struct vg_afx_step *s = &vg_afx_steps[i];

          for (j = 0; j < s->count; j++)
            {
              const struct vg_afx_expect *e = &vg_afx_expects[s->first + j];
              const struct vg_afx_point *point = &vg_afx_points[e->point];
              struct vg_alarm_rule rule;
              enum vg_alarm_kind want = kind_from_int(e->kind);
              enum vg_alarm_kind got;
              float value = decode_raw(point, e->raw);
              float thr = 0.0f;

              rule_of(point, &rule);
              CHECK(windows[e->point].failures == e->offline_before,
                    "%s/%s：%s 进入本步前应有 %u 个历史失败位，实际 %u",
                    s->case_id, s->label, point->id, e->offline_before,
                    (unsigned)windows[e->point].failures);

              if (e->offline_n == 0)
                {
                  got = vg_alarm_sample(&rule, &windows[e->point], 1, value,
                                        &thr);
                  online[e->point] = 1;
                }
              else
                {
                  unsigned k;
                  unsigned trigger = 0;
                  int online_before_step = online[e->point];

                  for (k = 0; k < e->offline_n; k++)
                    {
                      int before_online = online[e->point];
                      int expected_offline;

                      got = vg_alarm_sample(&rule, &windows[e->point], 0,
                                            value, &thr);
                      expected_offline =
                        !online_before_step ||
                        (e->offline_trigger != 0 &&
                         k + 1 >= e->offline_trigger);
                      CHECK((got == VG_ALARM_KIND_OFFLINE) == expected_offline,
                            "%s/%s：%s 第 %u 次失败离线状态错误",
                            s->case_id, s->label, point->id, k + 1);

                      online[e->point] =
                        got != VG_ALARM_KIND_OFFLINE;
                      if (before_online && !online[e->point] && trigger == 0)
                        {
                          trigger = k + 1;
                        }
                    }

                  CHECK(trigger == e->offline_trigger,
                        "%s/%s：%s 离线应在本步第 %u 次失败触发，实际第 %u 次",
                        s->case_id, s->label, point->id, e->offline_trigger,
                        trigger);
                }

              CHECK(got == want,
                    "%s/%s：%s raw=%u 值=%.9g 判成 %s，预期 %s",
                    s->case_id, s->label, point->id, e->raw, (double)value,
                    kind_name(got), kind_name(want));
              CHECK(windows[e->point].failures == e->offline_after,
                    "%s/%s：%s 完成后应有 %u 个历史失败位，实际 %u",
                    s->case_id, s->label, point->id, e->offline_after,
                    (unsigned)windows[e->point].failures);
              if (e->online_after >= 0)
                {
                  CHECK(online[e->point] == e->online_after,
                        "%s/%s：%s 在线标志为 %d，预期 %d",
                        s->case_id, s->label, point->id, online[e->point],
                        e->online_after);
                }

              if (want == VG_ALARM_KIND_WARN)
                {
                  CHECK(thr == point->warn,
                        "%s/%s：%s 触发阈值应为 warn %.9g，实际 %.9g",
                        s->case_id, s->label, point->id,
                        (double)point->warn, (double)thr);
                }
              else if (want == VG_ALARM_KIND_CRIT)
                {
                  CHECK(thr == point->crit,
                        "%s/%s：%s 触发阈值应为 crit %.9g，实际 %.9g",
                        s->case_id, s->label, point->id,
                        (double)point->crit, (double)thr);
                }
              else
                {
                  CHECK(thr == 0.0f, "%s/%s：无告警时 thr 应为 0",
                        s->case_id, s->label);
                }
            }

          i++;
        }
    }
}

/* 4. 点表覆盖不到的精度与区间，用合成规则验证 */
static void test_synthetic_precision(void)
{
  struct vg_alarm_rule rule;
  float thr;
  enum vg_alarm_kind kind;

  /* eq 的容差带 VG_ALARM_EQ_EPS = 0.0005。
   * 取 crit = 0.9f，值取 3 * 0.3f：两者都是单精度，但不相等，
   * 差值 5.96e-8 落在容差内，没有容差就会漏判。 */
  memset(&rule, 0, sizeof(rule));
  snprintf(rule.cmp, sizeof(rule.cmp), "eq");
  rule.has_crit = 1;
  rule.crit = 0.9f;
  rule.fail_n = 3;

  CHECK(3.0f * 0.3f != 0.9f,
        "前提检查：3*0.3f 与 0.9f 应不相等，实际相等");

  kind = vg_alarm_eval(&rule, 1, 0, 3.0f * 0.3f, &thr);
  CHECK(kind == VG_ALARM_KIND_CRIT,
        "eq 容差内的浮点差应判 crit，实际 %s", kind_name(kind));

  kind = vg_alarm_eval(&rule, 1, 0, 0.9004f, &thr);
  CHECK(kind == VG_ALARM_KIND_CRIT,
        "eq 容差内（+0.0004）应判 crit，实际 %s", kind_name(kind));

  kind = vg_alarm_eval(&rule, 1, 0, 0.8996f, &thr);
  CHECK(kind == VG_ALARM_KIND_CRIT,
        "eq 容差内（-0.0004）应判 crit，实际 %s", kind_name(kind));

  kind = vg_alarm_eval(&rule, 1, 0, 0.9006f, &thr);
  CHECK(kind == VG_ALARM_KIND_NONE,
        "eq 容差外（+0.0006）应判 none，实际 %s", kind_name(kind));

  kind = vg_alarm_eval(&rule, 1, 0, 0.8994f, &thr);
  CHECK(kind == VG_ALARM_KIND_NONE,
        "eq 容差外（-0.0006）应判 none，实际 %s", kind_name(kind));

  /* eq 只用 crit；warn 为空时即使命中 warn 也无告警 */
  kind = vg_alarm_eval(&rule, 1, 0, 0.0f, &thr);
  CHECK(kind == VG_ALARM_KIND_NONE, "eq 未命中应判 none，实际 %s",
        kind_name(kind));

  /* ge 只有 crit：阈值下方无告警 */
  memset(&rule, 0, sizeof(rule));
  snprintf(rule.cmp, sizeof(rule.cmp), "ge");
  rule.has_crit = 1;
  rule.crit = 10.0f;
  rule.fail_n = 3;
  kind = vg_alarm_eval(&rule, 1, 0, 9.99f, &thr);
  CHECK(kind == VG_ALARM_KIND_NONE, "ge 只有 crit 时阈值下方应 none，实际 %s",
        kind_name(kind));
  kind = vg_alarm_eval(&rule, 1, 0, 10.0f, &thr);
  CHECK(kind == VG_ALARM_KIND_CRIT, "ge 只有 crit 时等于阈值应 crit，实际 %s",
        kind_name(kind));

  /* cmp 有值但 warn / crit 都空：不做模拟量告警 */
  memset(&rule, 0, sizeof(rule));
  snprintf(rule.cmp, sizeof(rule.cmp), "ge");
  rule.fail_n = 3;
  kind = vg_alarm_eval(&rule, 1, 0, 1.0e9f, &thr);
  CHECK(kind == VG_ALARM_KIND_NONE,
        "ge 无阈值时大值也应 none，实际 %s", kind_name(kind));
  snprintf(rule.cmp, sizeof(rule.cmp), "le");
  kind = vg_alarm_eval(&rule, 1, 0, -1.0e9f, &thr);
  CHECK(kind == VG_ALARM_KIND_NONE,
        "le 无阈值时小值也应 none，实际 %s", kind_name(kind));

  /* 未知 cmp 不产生模拟量告警 */
  memset(&rule, 0, sizeof(rule));
  snprintf(rule.cmp, sizeof(rule.cmp), "gt");
  rule.has_crit = 1;
  rule.crit = 1.0f;
  rule.fail_n = 3;
  kind = vg_alarm_eval(&rule, 1, 0, 999.0f, &thr);
  CHECK(kind == VG_ALARM_KIND_NONE, "未知 cmp 应 none，实际 %s",
        kind_name(kind));

  /* fail_n 的有效区间 */
  memset(&rule, 0, sizeof(rule));
  snprintf(rule.cmp, sizeof(rule.cmp), "ge");
  rule.has_crit = 1;
  rule.crit = 1.0f;
  rule.fail_n = 1;
  CHECK(vg_alarm_fail_n(&rule) == 5, "fail_n=1 应抬高到 5");
  CHECK(vg_alarm_eval(&rule, 0, 4, 0, &thr) == VG_ALARM_KIND_NONE,
        "fail_n=1 时第 4 次失败仍不应触发");
  CHECK(vg_alarm_eval(&rule, 0, 5, 0, &thr) == VG_ALARM_KIND_OFFLINE,
        "fail_n=1 时第 5 次失败应触发");

  rule.fail_n = 8;
  CHECK(vg_alarm_fail_n(&rule) == 8, "fail_n=8 应保持 8");
  CHECK(vg_alarm_eval(&rule, 0, 7, 0, &thr) == VG_ALARM_KIND_NONE,
        "fail_n=8 时第 7 次失败不应触发");
  CHECK(vg_alarm_eval(&rule, 0, 8, 0, &thr) == VG_ALARM_KIND_OFFLINE,
        "fail_n=8 时第 8 次失败应触发");

  rule.fail_n = 20;
  CHECK(vg_alarm_fail_n(&rule) == 8, "fail_n=20 应封顶到 8");

  /* 空规则指针 */
  CHECK(vg_alarm_eval(NULL, 1, 0, 0, &thr) == VG_ALARM_KIND_NONE,
        "空规则应 none");

  /* 失败未达阈值时不得返回告警，否则会提前清掉既有告警期 */
  memset(&rule, 0, sizeof(rule));
  snprintf(rule.cmp, sizeof(rule.cmp), "ge");
  rule.has_warn = 1;
  rule.warn = 40.0f;
  rule.fail_n = 3;
  CHECK(vg_alarm_eval(&rule, 0, 1, 99.0f, &thr) == VG_ALARM_KIND_NONE,
        "零星失败应 none，且不得按旧值判越限");
}

/* 5. 滑窗形状与恢复后再触发的实际行为 */
static void test_window_shape(void)
{
  struct vg_alarm_rule rule;
  struct vg_alarm_window window;
  float thr;
  unsigned i;

  memset(&rule, 0, sizeof(rule));
  snprintf(rule.cmp, sizeof(rule.cmp), "ge");
  rule.has_warn = 1;
  rule.warn = 40.0f;
  rule.fail_n = 3;

  /* 窗口就是 8 个槽：第 9 次失败会覆盖第 1 次，失败数不会超过 8 */
  memset(&window, 0, sizeof(window));
  for (i = 0; i < WINDOW_SIZE; i++)
    {
      vg_alarm_sample(&rule, &window, 0, 20.0f, &thr);
    }

  CHECK(window.failures == WINDOW_SIZE,
        "连续 %d 次失败后滑窗失败数应为 %d，实际 %u",
        WINDOW_SIZE, WINDOW_SIZE, (unsigned)window.failures);

  vg_alarm_sample(&rule, &window, 0, 20.0f, &thr);
  CHECK(window.failures == WINDOW_SIZE,
        "第 %d 次失败覆盖最旧槽位后失败数仍应为 %d，实际 %u",
        WINDOW_SIZE + 1, WINDOW_SIZE, (unsigned)window.failures);

  /* 恢复后再触发：历史失败位仍在，一次失败就可能直接重新判离线。
   * 这是 8 轮失败率窗口的既有行为，记录在验收清单里，供板验对照。 */
  memset(&window, 0, sizeof(window));
  for (i = 0; i < 6; i++)
    {
      vg_alarm_sample(&rule, &window, 0, 20.0f, &thr);
    }

  CHECK(window.failures == 6, "6 次失败后应为 6，实际 %u",
        (unsigned)window.failures);
  CHECK(vg_alarm_sample(&rule, &window, 1, 20.0f, &thr) == VG_ALARM_KIND_NONE,
        "恢复采样应判 none");
  CHECK(vg_alarm_sample(&rule, &window, 0, 20.0f, &thr) ==
        VG_ALARM_KIND_OFFLINE,
        "恢复后第一次失败即重新判离线（历史失败位仍在）");

  /* 散布失败：8 轮里不足 5 次失败不触发 */
  {
    static const int online_seq[16] =
      { 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1 };

    memset(&window, 0, sizeof(window));
    for (i = 0; i < 16; i++)
      {
        enum vg_alarm_kind kind =
          vg_alarm_sample(&rule, &window, online_seq[i], 20.0f, &thr);

        CHECK(kind == VG_ALARM_KIND_NONE,
              "第 %u 轮散布失败不应触发，实际 %s", i + 1, kind_name(kind));
        CHECK(window.failures <= 4,
              "第 %u 轮失败数应不超过 4，实际 %u", i + 1,
              (unsigned)window.failures);
      }
  }
}

int main(void)
{
  test_fixture_shape();
  test_thresholds_exactly_representable();
  test_fixture_steps();
  test_synthetic_precision();
  test_window_shape();

  if (g_fail)
    {
      fprintf(stderr, "test_alarm_boundary: %d fail(s)\n", g_fail);
      return 1;
    }

  printf("test_alarm_boundary: ok (%d points, %d steps, %d expects)\n",
         VG_AFX_POINT_COUNT, VG_AFX_STEP_COUNT, VG_AFX_EXPECT_COUNT);
  return 0;
}
