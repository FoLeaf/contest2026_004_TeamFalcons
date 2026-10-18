/* Headless integration check for the phase F alarm scenarios.
 *
 * Compiles the real gui/main/ui model with VG_HMI_BOARD (the same switch the
 * firmware uses) and drives vg_model_set_live with the raw register values the
 * simulated Modbus slave returns, so the rule evaluation, the per-point alarm
 * episode, the offline sliding window and the primary-alarm derivation all run
 * the product code. No board, no serial port, no LVGL rendering assertions.
 *
 * Data comes from scripts/alarm_scenarios/alarm_scenarios_fixture.h, generated
 * by scripts/alarm_scenarios/gen_alarm_scenarios.py from the committed table
 * copy. Expected values are written in that generator from the protocol rules,
 * not derived here.
 *
 * Usage: alarm_scenario_check [out-dir]
 */

#include "harness_common.h"
#include "model/vg_model.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "alarm_scenarios_fixture.h"

static int g_fail;

static void check(int cond, const char * case_id, const char * label,
                  const char * fmt, ...)
{
  va_list ap;

  if (cond)
    {
      return;
    }

  fprintf(stderr, "FAIL %s/%s: ", case_id, label);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  g_fail++;
}

static vg_severity_t severity_of_kind(int kind)
{
  switch (kind)
    {
      case 1: return VG_SEV_WARN;
      case 2: return VG_SEV_CRIT;
      case 3: return VG_SEV_OFFLINE;
      default: return VG_SEV_OK;
    }
}

static const char * severity_name(vg_severity_t sev)
{
  switch (sev)
    {
      case VG_SEV_WARN: return "warn";
      case VG_SEV_CRIT: return "crit";
      case VG_SEV_OFFLINE: return "offline";
      default: return "ok";
    }
}

static float decode_raw(const struct vg_afx_point * p, unsigned raw)
{
  float v = (strcmp(p->dtype, "uint16") == 0) ? (float)raw
                                             : (float)(int16_t)raw;

  return v * p->scale;
}

static void import_table(void)
{
  vg_runtime_point_t pts[VG_AFX_POINT_COUNT];
  unsigned i;

  memset(pts, 0, sizeof(pts));
  for (i = 0; i < VG_AFX_POINT_COUNT; i++)
    {
      const struct vg_afx_point * p = &vg_afx_points[i];

      snprintf(pts[i].id, sizeof(pts[i].id), "%s", p->id);
      snprintf(pts[i].name, sizeof(pts[i].name), "%s", p->name);
      snprintf(pts[i].unit, sizeof(pts[i].unit), "%s", p->unit);
      snprintf(pts[i].dtype, sizeof(pts[i].dtype), "%s", p->dtype);
      snprintf(pts[i].cmp, sizeof(pts[i].cmp), "%s", p->cmp);
      pts[i].addr = (uint8_t)p->addr;
      pts[i].fc = (uint8_t)p->fc;
      pts[i].reg = (uint16_t)p->reg;
      pts[i].scale = p->scale;
      pts[i].has_warn = (uint8_t)p->has_warn;
      pts[i].has_crit = (uint8_t)p->has_crit;
      pts[i].warn = p->warn;
      pts[i].crit = p->crit;
      pts[i].fail_n = (uint8_t)p->fail_n;
    }

  vg_model_import_runtime_points(pts, VG_AFX_POINT_COUNT);
}

static void feed_baseline(void)
{
  unsigned i;

  for (i = 0; i < VG_AFX_POINT_COUNT; i++)
    {
      vg_model_set_live((uint16_t)i, decode_raw(&vg_afx_points[i],
                                                vg_afx_baseline[i]), true);
    }
}

static const vg_sensor_t * get_sensor(unsigned point, const char * case_id,
                                      const char * label)
{
  const vg_sensor_t * sensors;
  uint16_t n = 0;

  sensors = vg_model_get_sensors(&n);
  if (point >= n)
    {
      check(0, case_id, label, "点下标 %u 超出模型点数 %u，预期 %d", point,
            (unsigned)n, VG_AFX_POINT_COUNT);
      return NULL;
    }

  return &sensors[point];
}

static void assert_step(const struct vg_afx_step * step)
{
  unsigned j;

  for (j = 0; j < step->count; j++)
    {
      const struct vg_afx_expect * e = &vg_afx_expects[step->first + j];
      const struct vg_afx_point * p = &vg_afx_points[e->point];
      const vg_sensor_t * sensors;
      uint16_t n = 0;
      vg_severity_t want = severity_of_kind(e->kind);

      sensors = vg_model_get_sensors(&n);
      if (e->point >= n)
        {
          check(0, step->case_id, step->label, "%s 不在模型点表里", p->id);
          continue;
        }

      check(sensors[e->point].severity == want, step->case_id, step->label,
            "%s 等级 %s，预期 %s", p->id,
            severity_name(sensors[e->point].severity), severity_name(want));

      if (e->online_after >= 0)
        {
          check(sensors[e->point].online == (e->online_after != 0),
                step->case_id, step->label, "%s 在线标志 %d，预期 %d", p->id,
                sensors[e->point].online ? 1 : 0, e->online_after);
        }

      check(sensors[e->point].al_active == (e->kind != 0), step->case_id,
            step->label, "%s 告警期 %d，预期 %d", p->id,
            sensors[e->point].al_active ? 1 : 0, e->kind != 0 ? 1 : 0);
    }

  {
    uint16_t active = vg_model_active_alarm_count();

    check(active == (uint16_t)step->expect_active, step->case_id, step->label,
          "活动告警数 %u，预期 %d", (unsigned)active, step->expect_active);
  }

  if (step->primary_kind >= 0)
    {
      const vg_alarm_t * a = vg_model_get_active_alarm();
      vg_severity_t want = severity_of_kind(step->primary_kind);

      if (step->primary_kind == 0)
        {
          check(a == NULL || !a->active, step->case_id, step->label,
                "主告警应为空，实际 active=%d", a ? (int)a->active : -1);
        }
      else
        {
          const struct vg_afx_point * p = &vg_afx_points[step->primary_point];

          check(a != NULL && a->active, step->case_id, step->label,
                "主告警应为 %s，实际为空", p->id);
          if (a != NULL && a->active)
            {
              check(strcmp(a->sensor_id, p->id) == 0, step->case_id,
                    step->label, "主告警点 %s，预期 %s", a->sensor_id, p->id);
              check(a->severity == want, step->case_id, step->label,
                    "主告警等级 %s，预期 %s", severity_name(a->severity),
                    severity_name(want));
            }
        }
    }
}

static void run_case(unsigned first, unsigned count)
{
  int online[VG_AFX_POINT_COUNT];
  unsigned i;

  import_table();
  feed_baseline();
  for (i = 0; i < VG_AFX_POINT_COUNT; i++)
    {
      online[i] = 1;
    }

  for (i = first; i < first + count; i++)
    {
      const struct vg_afx_step * step = &vg_afx_steps[i];
      unsigned j;

      /* 控制行在板端由维护从站脚本解释；这里直接把原始值/失败采样喂进模型。
       * 步骤的 expects 顺序就是板端一轮采集里点的读取顺序。 */
      for (j = 0; j < step->count; j++)
        {
          const struct vg_afx_expect * e = &vg_afx_expects[step->first + j];
          const struct vg_afx_point * p = &vg_afx_points[e->point];
          const vg_sensor_t * sensor;
          int online_before_step;
          float value = decode_raw(p, e->raw);
          unsigned k;

          sensor = get_sensor((unsigned)e->point, step->case_id, step->label);
          if (sensor == NULL)
            {
              continue;
            }

          check(sensor->fail_window.failures == e->offline_before,
                step->case_id, step->label,
                "%s 进入本步前应有 %u 个历史失败位，实际 %u", p->id,
                e->offline_before, (unsigned)sensor->fail_window.failures);
          check(sensor->online == (online[e->point] != 0), step->case_id,
                step->label, "%s 进入本步前在线标志 %d，预期 %d", p->id,
                sensor->online ? 1 : 0, online[e->point]);
          online_before_step = online[e->point];

          if (e->offline_n == 0)
            {
              vg_model_set_live((uint16_t)e->point, value, true);
              online[e->point] = 1;
            }
          else
            {
              for (k = 0; k < e->offline_n; k++)
                {
                  int expected_online;

                  vg_model_set_live((uint16_t)e->point, value, false);
                  sensor = get_sensor((unsigned)e->point, step->case_id,
                                      step->label);
                  if (sensor == NULL)
                    {
                      break;
                    }

                  expected_online =
                    online_before_step &&
                    (e->offline_trigger == 0 ||
                     k + 1 < e->offline_trigger);
                  check(sensor->online == (expected_online != 0),
                        step->case_id, step->label,
                        "%s 第 %u 次失败在线标志 %d，预期 %d", p->id, k + 1,
                        sensor->online ? 1 : 0, expected_online);
                }

              if (k == e->offline_n)
                {
                  sensor = get_sensor((unsigned)e->point, step->case_id,
                                      step->label);
                }
              if (sensor != NULL)
                {
                  online[e->point] = sensor->online ? 1 : 0;
                }
            }

          sensor = get_sensor((unsigned)e->point, step->case_id, step->label);
          if (sensor != NULL)
            {
              online[e->point] = sensor->online ? 1 : 0;
              check(sensor->fail_window.failures == e->offline_after,
                    step->case_id, step->label,
                    "%s 完成后应有 %u 个历史失败位，实际 %u", p->id,
                    e->offline_after,
                    (unsigned)sensor->fail_window.failures);
            }
        }

      assert_step(step);
    }
}

int main(int argc, char ** argv)
{
  unsigned i;
  unsigned cases = 0;
  uint16_t n = 0;
  const vg_sensor_t * sensors;

  (void)argc;
  (void)argv;

  hg_init();

  /* 导入后模型点数与顺序必须与点表一致，后面的下标断言才有意义 */
  import_table();
  sensors = vg_model_get_sensors(&n);
  check(n == VG_AFX_POINT_COUNT, "fixture", "import",
        "模型点数 %u，预期 %d", (unsigned)n, VG_AFX_POINT_COUNT);
  for (i = 0; i < VG_AFX_POINT_COUNT && i < n; i++)
    {
      check(strcmp(sensors[i].id, vg_afx_points[i].id) == 0, "fixture",
            "import", "第 %u 个点 id %s，预期 %s", i, sensors[i].id,
            vg_afx_points[i].id);
    }

  /* 按 case_id 分组顺序执行；生成器保证同一工况的步骤连续 */
  i = 0;
  while (i < VG_AFX_STEP_COUNT)
    {
      const char * case_id = vg_afx_steps[i].case_id;
      unsigned first = i;

      while (i < VG_AFX_STEP_COUNT &&
             strcmp(vg_afx_steps[i].case_id, case_id) == 0)
        {
          i++;
        }

      run_case(first, i - first);
      cases++;
    }

  if (g_fail)
    {
      fprintf(stderr, "alarm_scenario_check: %d fail(s)\n", g_fail);
      return 1;
    }

  printf("alarm_scenario_check: ok (%u cases, %d steps, %d points)\n",
         cases, VG_AFX_STEP_COUNT, VG_AFX_POINT_COUNT);
  return 0;
}
