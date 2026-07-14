/****************************************************************************
 * app/velaguard_app/src/vg_log.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "vg_identity.h"
#include "vg_log.h"
#include "vg_startup.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VG_HUMAN_LOG "/data/velaguard/logs/latest.log"
#define VG_EVENT_LOG "/data/velaguard/logs/events.jsonl"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int vg_json_string(FAR FILE *stream, FAR const char *value)
{
  FAR const unsigned char *cursor = (FAR const unsigned char *)value;

  if (fputc('"', stream) == EOF)
    {
      return -EIO;
    }

  while (*cursor != '\0')
    {
      int result;

      if (*cursor == '"' || *cursor == '\\')
        {
          result = fprintf(stream, "\\%c", *cursor);
        }
      else if (*cursor < 0x20)
        {
          result = fprintf(stream, "\\u%04x", *cursor);
        }
      else
        {
          result = fputc(*cursor, stream);
        }

      if (result < 0)
        {
          return -EIO;
        }

      cursor++;
    }

  return fputc('"', stream) == EOF ? -EIO : 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int vg_log_human(FAR const char *level, FAR const char *message)
{
  FAR FILE *stream;
  int result = 0;

  if (level == NULL || message == NULL)
    {
      return -EINVAL;
    }

  stream = fopen(VG_HUMAN_LOG, "a");
  if (stream == NULL)
    {
      return -errno;
    }

  if (fprintf(stream, "[%s] %s uptime_ms=%llu\n", level, message,
              (unsigned long long)vg_uptime_ms()) < 0)
    {
      result = -EIO;
    }

  if (fclose(stream) != 0 && result == 0)
    {
      result = -errno;
    }

  return result;
}

int vg_log_event(FAR const char *type, FAR const char *summary,
                 FAR const char *extra_json_object_fields)
{
  FAR const struct vg_identity *identity = vg_identity_get();
  FAR const struct vg_startup_state *startup = vg_startup_get();
  FAR FILE *stream;
  int result = 0;

  if (type == NULL || summary == NULL)
    {
      return -EINVAL;
    }

  stream = fopen(VG_EVENT_LOG, "a");
  if (stream == NULL)
    {
      return -errno;
    }

  if (fputs("{\"type\":", stream) == EOF)
    {
      result = -EIO;
    }

  if (result == 0)
    {
      result = vg_json_string(stream, type);
    }

  if (result == 0 && fputs(",\"summary\":", stream) == EOF)
    {
      result = -EIO;
    }

  if (result == 0)
    {
      result = vg_json_string(stream, summary);
    }

  if (result == 0 &&
      fprintf(stream,
              ",\"device_id\":") < 0)
    {
      result = -EIO;
    }

  if (result == 0)
    {
      result = vg_json_string(stream, identity->device_id);
    }

  if (result == 0 &&
      fprintf(stream,
              ",\"boot_id\":") < 0)
    {
      result = -EIO;
    }

  if (result == 0)
    {
      result = vg_json_string(stream, startup->boot_id);
    }

  if (result == 0 &&
      fprintf(stream, ",\"uptime_ms\":%llu",
              (unsigned long long)vg_uptime_ms()) < 0)
    {
      result = -EIO;
    }

  if (result == 0 && extra_json_object_fields != NULL &&
      extra_json_object_fields[0] != '\0')
    {
      if (fputc(',', stream) == EOF ||
          fputs(extra_json_object_fields, stream) == EOF)
        {
          result = -EIO;
        }
    }

  if (result == 0 && fputs("}\n", stream) == EOF)
    {
      result = -EIO;
    }

  if (fclose(stream) != 0 && result == 0)
    {
      result = -errno;
    }

  return result;
}
