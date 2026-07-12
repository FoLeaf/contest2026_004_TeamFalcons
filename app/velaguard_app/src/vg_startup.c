/****************************************************************************
 * app/velaguard_app/src/vg_startup.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "vg_identity.h"
#include "vg_startup.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VG_DATA_DIR    "/data/velaguard"
#define VG_CONFIG_DIR  "/data/velaguard/configs"
#define VG_LOG_DIR     "/data/velaguard/logs"
#define VG_HUMAN_LOG   "/data/velaguard/logs/latest.log"
#define VG_EVENT_LOG   "/data/velaguard/logs/events.jsonl"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct vg_startup_state g_startup;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void vg_store_error(int error)
{
  if (g_startup.storage_error == 0)
    {
      g_startup.storage_error = error < 0 ? error : -error;
    }
}

static int vg_ensure_directory(FAR const char *path)
{
  struct stat status;

  if (mkdir(path, 0755) == 0)
    {
      return 0;
    }

  if (errno != EEXIST)
    {
      return -errno;
    }

  if (stat(path, &status) < 0)
    {
      return -errno;
    }

  return S_ISDIR(status.st_mode) ? 0 : -ENOTDIR;
}

static uint64_t vg_realtime_ms(enum vg_time_quality *quality)
{
  struct timespec value;
  struct tm calendar;

  *quality = VG_TIME_UNKNOWN;
  if (clock_gettime(CLOCK_REALTIME, &value) < 0)
    {
      return 0;
    }

  if (gmtime_r(&value.tv_sec, &calendar) != NULL &&
      calendar.tm_year >= 120)
    {
      *quality = VG_TIME_RTC;
    }

  return (uint64_t)value.tv_sec * 1000u +
         (uint64_t)value.tv_nsec / 1000000u;
}

static uint64_t vg_boot_fallback(void)
{
  FAR const char *device_id = vg_identity_get()->device_id;
  uint64_t hash = 1469598103934665603ull ^ vg_uptime_ms();

  while (*device_id != '\0')
    {
      hash ^= (uint8_t)*device_id++;
      hash *= 1099511628211ull;
    }

  return hash ^ g_startup.ts_ms;
}

static void vg_generate_boot_id(void)
{
  uint64_t random_value = 0;
  ssize_t bytes_read = -1;
  int fd = open("/dev/urandom", O_RDONLY);

  if (fd >= 0)
    {
      bytes_read = read(fd, &random_value, sizeof(random_value));
      close(fd);
    }

  if (bytes_read != sizeof(random_value))
    {
      random_value = vg_boot_fallback();
    }

  snprintf(g_startup.boot_id, sizeof(g_startup.boot_id),
           "boot-%016llx", (unsigned long long)random_value);
}

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

static int vg_write_human_log(void)
{
  FAR const struct vg_identity *identity = vg_identity_get();
  FAR FILE *stream = fopen(VG_HUMAN_LOG, "a");
  int result = 0;

  if (stream == NULL)
    {
      return -errno;
    }

  if (fprintf(stream,
              "[INFO] VelaGuard boot device_id=%s mode=%s firmware=%s "
              "boot_id=%s ts_ms=%llu uptime_ms=%llu time_quality=%s\n",
              identity->device_id,
              vg_build_mode_string(identity->build_mode),
              identity->firmware_version,
              g_startup.boot_id,
              (unsigned long long)g_startup.ts_ms,
              (unsigned long long)g_startup.uptime_ms,
              vg_time_quality_string(g_startup.time_quality)) < 0)
    {
      result = -EIO;
    }

  if (fclose(stream) != 0 && result == 0)
    {
      result = -errno;
    }

  return result;
}

static int vg_write_event(void)
{
  FAR const struct vg_identity *identity = vg_identity_get();
  FAR FILE *stream = fopen(VG_EVENT_LOG, "a");
  int result = 0;

  if (stream == NULL)
    {
      return -errno;
    }

#define VG_JSON_LITERAL(value) \
  do \
    { \
      if (result == 0 && fputs(value, stream) == EOF) \
        { \
          result = -EIO; \
        } \
    } \
  while (0)

#define VG_JSON_STRING(value) \
  do \
    { \
      if (result == 0) \
        { \
          result = vg_json_string(stream, value); \
        } \
    } \
  while (0)

  VG_JSON_LITERAL("{\"type\":\"boot\",\"device_id\":");
  VG_JSON_STRING(identity->device_id);
  VG_JSON_LITERAL(",\"build_mode\":");
  VG_JSON_STRING(vg_build_mode_string(identity->build_mode));
  VG_JSON_LITERAL(",\"firmware_version\":");
  VG_JSON_STRING(identity->firmware_version);
  VG_JSON_LITERAL(",\"boot_id\":");
  VG_JSON_STRING(g_startup.boot_id);
  if (result == 0 &&
      fprintf(stream,
              ",\"ts_ms\":%llu,\"uptime_ms\":%llu,\"time_quality\":",
              (unsigned long long)g_startup.ts_ms,
              (unsigned long long)g_startup.uptime_ms) < 0)
    {
      result = -EIO;
    }

  VG_JSON_STRING(vg_time_quality_string(g_startup.time_quality));
  VG_JSON_LITERAL("}\n");

#undef VG_JSON_STRING
#undef VG_JSON_LITERAL

  if (fclose(stream) != 0 && result == 0)
    {
      result = -errno;
    }

  return result;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

uint64_t vg_uptime_ms(void)
{
  struct timespec value;

  if (clock_gettime(CLOCK_MONOTONIC, &value) < 0)
    {
      return 0;
    }

  return (uint64_t)value.tv_sec * 1000u +
         (uint64_t)value.tv_nsec / 1000000u;
}

FAR const char *vg_time_quality_string(enum vg_time_quality quality)
{
  switch (quality)
    {
      case VG_TIME_RTC:
        return "rtc";
      case VG_TIME_NTP:
        return "ntp";
      case VG_TIME_CLOUD:
        return "cloud";
      case VG_TIME_UNKNOWN:
      default:
        return "unknown";
    }
}

int vg_startup_record(void)
{
  static FAR const char *directories[] =
  {
    "/data",
    VG_DATA_DIR,
    VG_CONFIG_DIR,
    VG_LOG_DIR
  };

  size_t index;
  int result;

  memset(&g_startup, 0, sizeof(g_startup));
  g_startup.uptime_ms = vg_uptime_ms();
  g_startup.ts_ms = vg_realtime_ms(&g_startup.time_quality);
  vg_generate_boot_id();

  for (index = 0;
       index < sizeof(directories) / sizeof(directories[0]);
       index++)
    {
      result = vg_ensure_directory(directories[index]);
      if (result < 0)
        {
          fprintf(stderr, "[velaguard] directory failed: %s (%d)\n",
                  directories[index], result);
          vg_store_error(result);
          break;
        }
    }

  g_startup.directories_ready = index ==
    sizeof(directories) / sizeof(directories[0]);

  if (g_startup.directories_ready)
    {
      result = vg_write_human_log();
      g_startup.human_log_written = result == 0;
      if (result < 0)
        {
          fprintf(stderr, "[velaguard] human log failed: %s (%d)\n",
                  VG_HUMAN_LOG, result);
          vg_store_error(result);
        }

      result = vg_write_event();
      g_startup.event_written = result == 0;
      if (result < 0)
        {
          fprintf(stderr, "[velaguard] event log failed: %s (%d)\n",
                  VG_EVENT_LOG, result);
          vg_store_error(result);
        }
    }

  printf("[velaguard] boot device_id=%s mode=%s firmware=%s\n",
         vg_identity_get()->device_id,
         vg_build_mode_string(vg_identity_get()->build_mode),
         vg_identity_get()->firmware_version);
  printf("[velaguard] boot_id=%s ts_ms=%llu uptime_ms=%llu "
         "time_quality=%s storage=%s\n",
         g_startup.boot_id,
         (unsigned long long)g_startup.ts_ms,
         (unsigned long long)g_startup.uptime_ms,
         vg_time_quality_string(g_startup.time_quality),
         vg_startup_storage_ready() ? "READY" : "DEGRADED");

  return g_startup.storage_error;
}

FAR const struct vg_startup_state *vg_startup_get(void)
{
  return &g_startup;
}

bool vg_startup_storage_ready(void)
{
  return g_startup.directories_ready &&
         g_startup.human_log_written &&
         g_startup.event_written;
}
