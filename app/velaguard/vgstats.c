/****************************************************************************
 * app/velaguard/vgstats.c
 *
 * NSH tool for frame quality stats (bring-up).
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nuttx/config.h>

#include "vg_frame_stats.h"

static void usage(void)
{
  printf("Usage:\n");
  printf("  vgstats dump [slave]\n");
  printf("  vgstats inject <slave> ok|crc|timeout|echo|other [latency_ms]\n");
  printf("  vgstats reset [slave]\n");
}

static int parse_result(const char *name, enum vg_fs_result *out)
{
  if (strcmp(name, "ok") == 0)
    {
      *out = VG_FS_OK;
      return 0;
    }

  if (strcmp(name, "crc") == 0)
    {
      *out = VG_FS_CRC;
      return 0;
    }

  if (strcmp(name, "timeout") == 0)
    {
      *out = VG_FS_TIMEOUT;
      return 0;
    }

  if (strcmp(name, "echo") == 0)
    {
      *out = VG_FS_ECHO;
      return 0;
    }

  if (strcmp(name, "other") == 0)
    {
      *out = VG_FS_OTHER;
      return 0;
    }

  return -1;
}

static void print_summary(uint8_t slave, const struct vg_fs_summary *s)
{
  printf("vgstats: slave=%u window=%d total=%u ok=%u crc=%u timeout=%u "
         "echo=%u other=%u lat_min=%u lat_max=%u lat_avg=%u\n",
         (unsigned)slave, VG_FS_WINDOW,
         (unsigned)s->total, (unsigned)s->ok, (unsigned)s->crc_err,
         (unsigned)s->timeout, (unsigned)s->echo, (unsigned)s->other,
         (unsigned)s->lat_min_ms, (unsigned)s->lat_max_ms,
         (unsigned)s->lat_avg_ms);
}

/**
  * @brief  NSH: vgstats — frame quality dump / inject / reset.
  */
int main(int argc, char *argv[])
{
  vg_fs_init();

  if (argc < 2)
    {
      usage();
      return 1;
    }

  if (strcmp(argv[1], "dump") == 0)
    {
      if (argc >= 3)
        {
          struct vg_fs_summary s;
          unsigned long slave = strtoul(argv[2], NULL, 0);

          vg_fs_summary((uint8_t)slave, &s);
          print_summary((uint8_t)slave, &s);
          return 0;
        }

      /* Dump all allocated buckets. */

      for (unsigned int i = 1; i <= 247; i++)
        {
          struct vg_fs_summary s;

          vg_fs_summary((uint8_t)i, &s);
          if (s.total > 0)
            {
              print_summary((uint8_t)i, &s);
            }
        }

      return 0;
    }

  if (strcmp(argv[1], "inject") == 0)
    {
      enum vg_fs_result result;
      unsigned long slave;
      unsigned long lat = 0;
      int ret;

      if (argc < 4)
        {
          usage();
          return 1;
        }

      slave = strtoul(argv[2], NULL, 0);
      if (slave < 1 || slave > 247)
        {
          printf("vgstats: slave must be 1..247\n");
          return 1;
        }

      if (parse_result(argv[3], &result) != 0)
        {
          usage();
          return 1;
        }

      if (argc >= 5)
        {
          lat = strtoul(argv[4], NULL, 0);
        }
      else if (result == VG_FS_OK)
        {
          lat = 10;
        }

      ret = vg_fs_inject((uint8_t)slave, result, (uint32_t)lat);
      if (ret != 0)
        {
          printf("vgstats: inject failed %d\n", ret);
          return 1;
        }

      printf("vgstats: injected slave=%lu %s lat=%lu\n",
             slave, vg_fs_result_name(result), lat);
      return 0;
    }

  if (strcmp(argv[1], "reset") == 0)
    {
      unsigned long slave = 0;

      if (argc >= 3)
        {
          slave = strtoul(argv[2], NULL, 0);
        }

      vg_fs_reset((uint8_t)slave);
      printf("vgstats: reset slave=%lu\n", slave);
      return 0;
    }

  usage();
  return 1;
}
