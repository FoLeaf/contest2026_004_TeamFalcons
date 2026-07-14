/****************************************************************************
 * app/velaguard_app/src/vg_config.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal alarm config load/save for demo (issue #05 slice).
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vg_config.h"
#include "vg_log.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VG_ALARM_CFG_PATH "/data/velaguard/configs/alarm.json"

#define VG_DEFAULT_WARN_C     70.0f
#define VG_DEFAULT_CRIT_C     80.0f
#define VG_DEFAULT_RESTORE_C  68.0f
#define VG_DEFAULT_TRIGGER_MS 2000u
#define VG_DEFAULT_RESTORE_MS 2000u

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct vg_alarm_config g_alarm_cfg;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void vg_config_set_defaults(void)
{
  g_alarm_cfg.warn_c = VG_DEFAULT_WARN_C;
  g_alarm_cfg.crit_c = VG_DEFAULT_CRIT_C;
  g_alarm_cfg.restore_c = VG_DEFAULT_RESTORE_C;
  g_alarm_cfg.trigger_ms = VG_DEFAULT_TRIGGER_MS;
  g_alarm_cfg.restore_ms = VG_DEFAULT_RESTORE_MS;
  g_alarm_cfg.loaded_from_file = false;
}

static bool vg_parse_float_field(FAR const char *json, FAR const char *key,
                                 FAR float *out)
{
  FAR char *pos;
  char pattern[48];

  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  pos = strstr(json, pattern);
  if (pos == NULL)
    {
      return false;
    }

  pos = strchr(pos, ':');
  if (pos == NULL)
    {
      return false;
    }

  *out = strtof(pos + 1, NULL);
  return true;
}

static bool vg_parse_uint_field(FAR const char *json, FAR const char *key,
                                FAR unsigned int *out)
{
  float value;

  if (!vg_parse_float_field(json, key, &value))
    {
      return false;
    }

  if (value < 0.0f)
    {
      return false;
    }

  *out = (unsigned int)value;
  return true;
}

static int vg_config_write_defaults(void)
{
  FAR FILE *stream = fopen(VG_ALARM_CFG_PATH, "w");
  int result = 0;

  if (stream == NULL)
    {
      return -errno;
    }

  if (fprintf(stream,
              "{\n"
              "  \"warn_c\": %.1f,\n"
              "  \"crit_c\": %.1f,\n"
              "  \"restore_c\": %.1f,\n"
              "  \"trigger_ms\": %u,\n"
              "  \"restore_ms\": %u\n"
              "}\n",
              (double)g_alarm_cfg.warn_c,
              (double)g_alarm_cfg.crit_c,
              (double)g_alarm_cfg.restore_c,
              g_alarm_cfg.trigger_ms,
              g_alarm_cfg.restore_ms) < 0)
    {
      result = -EIO;
    }

  if (fclose(stream) != 0 && result == 0)
    {
      result = -errno;
    }

  return result;
}

static int vg_config_read_file(void)
{
  char buffer[512];
  size_t nread;
  FAR FILE *stream = fopen(VG_ALARM_CFG_PATH, "r");
  float warn_c;
  float crit_c;
  float restore_c;
  unsigned int trigger_ms;
  unsigned int restore_ms;
  bool ok;

  if (stream == NULL)
    {
      return -errno;
    }

  nread = fread(buffer, 1, sizeof(buffer) - 1, stream);
  buffer[nread] = '\0';
  fclose(stream);

  if (nread == 0)
    {
      return -EINVAL;
    }

  ok = vg_parse_float_field(buffer, "warn_c", &warn_c) &&
       vg_parse_float_field(buffer, "crit_c", &crit_c) &&
       vg_parse_float_field(buffer, "restore_c", &restore_c) &&
       vg_parse_uint_field(buffer, "trigger_ms", &trigger_ms) &&
       vg_parse_uint_field(buffer, "restore_ms", &restore_ms);

  if (!ok || warn_c >= crit_c || restore_c > warn_c ||
      trigger_ms == 0 || restore_ms == 0)
    {
      return -EINVAL;
    }

  g_alarm_cfg.warn_c = warn_c;
  g_alarm_cfg.crit_c = crit_c;
  g_alarm_cfg.restore_c = restore_c;
  g_alarm_cfg.trigger_ms = trigger_ms;
  g_alarm_cfg.restore_ms = restore_ms;
  g_alarm_cfg.loaded_from_file = true;
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int vg_config_load(void)
{
  int result;

  vg_config_set_defaults();
  result = vg_config_read_file();
  if (result == 0)
    {
      vg_log_human("INFO", "alarm config loaded from file");
      return 0;
    }

  result = vg_config_write_defaults();
  if (result == 0)
    {
      vg_log_human("INFO", "alarm config defaults written");
      g_alarm_cfg.loaded_from_file = true;
    }
  else
    {
      vg_log_human("WARN", "alarm config using built-in defaults");
      g_alarm_cfg.loaded_from_file = false;
    }

  return result;
}

FAR const struct vg_alarm_config *vg_config_alarm(void)
{
  return &g_alarm_cfg;
}
