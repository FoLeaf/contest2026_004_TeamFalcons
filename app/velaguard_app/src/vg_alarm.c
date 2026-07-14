/****************************************************************************
 * app/velaguard_app/src/vg_alarm.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <stdio.h>

#include "vg_acq.h"
#include "vg_alarm.h"
#include "vg_config.h"
#include "vg_log.h"
#include "vg_startup.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct vg_alarm_status g_alarm;
static enum vg_alarm_level g_logged_level;
static uint32_t g_above_warn_ms;
static uint32_t g_above_crit_ms;
static uint32_t g_below_restore_ms;
static uint32_t g_last_eval_ms;

static void vg_alarm_log_transition(enum vg_alarm_level previous,
                                    enum vg_alarm_level next)
{
  char human[96];
  char extra[80];
  int tenths;

  if (previous == next)
    {
      return;
    }

  tenths = (int)(g_alarm.current_c * 10.0f + 0.5f);
  snprintf(human, sizeof(human), "alarm %s -> %s temp=%d.%dC thr=%.0f",
           vg_alarm_level_string(previous),
           vg_alarm_level_string(next),
           tenths / 10, tenths % 10,
           (double)g_alarm.threshold_c);
  snprintf(g_alarm.last_event, sizeof(g_alarm.last_event), "%s", human);
  vg_log_human("ALARM", human);

  snprintf(extra, sizeof(extra),
           "\"level\":\"%s\",\"previous\":\"%s\",\"temp_c\":%d.%d,"
           "\"threshold_c\":%.0f",
           vg_alarm_level_string(next),
           vg_alarm_level_string(previous),
           tenths / 10, tenths % 10,
           (double)g_alarm.threshold_c);
  vg_log_event("alarm_transition", human, extra);
  g_logged_level = next;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void vg_alarm_init(void)
{
  FAR const struct vg_alarm_config *cfg = vg_config_alarm();

  g_alarm.level = VG_ALARM_OK;
  g_alarm.title = "Not armed";
  g_alarm.threshold_c = cfg->warn_c;
  g_alarm.current_c = 0.0f;
  g_alarm.armed = false;
  g_alarm.last_event[0] = '\0';
  g_logged_level = VG_ALARM_OK;
  g_above_warn_ms = 0;
  g_above_crit_ms = 0;
  g_below_restore_ms = 0;
  g_last_eval_ms = 0;
}

void vg_alarm_eval(void)
{
  FAR const struct vg_acq_point *point = vg_acq_primary();
  FAR const struct vg_alarm_config *cfg = vg_config_alarm();
  uint32_t now_ms = (uint32_t)vg_uptime_ms();
  uint32_t dt_ms;
  enum vg_alarm_level previous = g_alarm.level;

  if (g_last_eval_ms == 0)
    {
      g_last_eval_ms = now_ms;
      dt_ms = 0;
    }
  else
    {
      dt_ms = now_ms - g_last_eval_ms;
      g_last_eval_ms = now_ms;
    }

  if (point == NULL || point->quality != VG_ACQ_Q_GOOD)
    {
      g_alarm.armed = false;
      g_alarm.level = VG_ALARM_OK;
      g_alarm.title = "Not armed";
      g_above_warn_ms = 0;
      g_above_crit_ms = 0;
      g_below_restore_ms = 0;
      if (previous != VG_ALARM_OK)
        {
          vg_alarm_log_transition(previous, VG_ALARM_OK);
        }

      return;
    }

  g_alarm.armed = true;
  g_alarm.current_c = point->value;
  g_alarm.threshold_c = cfg->warn_c;

  if (point->value >= cfg->crit_c)
    {
      g_above_crit_ms += dt_ms;
      g_above_warn_ms += dt_ms;
      g_below_restore_ms = 0;
    }
  else if (point->value >= cfg->warn_c)
    {
      g_above_warn_ms += dt_ms;
      g_above_crit_ms = 0;
      g_below_restore_ms = 0;
    }
  else if (point->value < cfg->restore_c)
    {
      g_below_restore_ms += dt_ms;
      g_above_warn_ms = 0;
      g_above_crit_ms = 0;
    }
  else
    {
      /* Hysteresis band: hold timers. */

      g_above_warn_ms = 0;
      g_above_crit_ms = 0;
    }

  if (g_above_crit_ms >= cfg->trigger_ms)
    {
      g_alarm.level = VG_ALARM_CRITICAL;
      g_alarm.title = "Temp critical";
      g_alarm.threshold_c = cfg->crit_c;
    }
  else if (g_above_warn_ms >= cfg->trigger_ms)
    {
      g_alarm.level = VG_ALARM_WARNING;
      g_alarm.title = "Temp high";
      g_alarm.threshold_c = cfg->warn_c;
    }
  else if (g_alarm.level != VG_ALARM_OK &&
           g_below_restore_ms >= cfg->restore_ms)
    {
      g_alarm.level = VG_ALARM_OK;
      g_alarm.title = "Normal";
    }
  else if (g_alarm.level == VG_ALARM_OK)
    {
      g_alarm.title = "Normal";
    }

  if (g_alarm.level != g_logged_level)
    {
      vg_alarm_log_transition(g_logged_level, g_alarm.level);
    }
}

FAR const struct vg_alarm_status *vg_alarm_get(void)
{
  return &g_alarm;
}

FAR const char *vg_alarm_level_string(enum vg_alarm_level level)
{
  switch (level)
    {
      case VG_ALARM_WARNING:
        return "WARNING";

      case VG_ALARM_CRITICAL:
        return "CRITICAL";

      default:
        return "OK";
    }
}
