/****************************************************************************
 * app/velaguard/host_tests/test_frame_stats.c
 *
 * Host tests for vg_frame_stats (AC1).
 ****************************************************************************/

#include <stdio.h>
#include <stdint.h>

#include "vg_frame_stats.h"

static int g_fail;

static void expect(int cond, const char *msg)
{
  if (!cond)
    {
      fprintf(stderr, "FAIL: %s\n", msg);
      g_fail++;
    }
}

static void test_inject_and_summary(void)
{
  struct vg_fs_summary s;

  vg_fs_reset(0);
  expect(vg_fs_inject(1, VG_FS_OK, 10) == 0, "inject ok");
  expect(vg_fs_inject(1, VG_FS_OK, 30) == 0, "inject ok2");
  expect(vg_fs_inject(1, VG_FS_CRC, 0) == 0, "inject crc");
  expect(vg_fs_inject(1, VG_FS_TIMEOUT, 0) == 0, "inject timeout");
  expect(vg_fs_inject(1, VG_FS_ECHO, 0) == 0, "inject echo");

  vg_fs_summary(1, &s);
  expect(s.total == 5, "total 5");
  expect(s.ok == 2, "ok 2");
  expect(s.crc_err == 1, "crc 1");
  expect(s.timeout == 1, "timeout 1");
  expect(s.echo == 1, "echo 1");
  expect(s.lat_min_ms == 10, "lat min");
  expect(s.lat_max_ms == 30, "lat max");
  expect(s.lat_avg_ms == 20, "lat avg");
}

static void test_window_eviction(void)
{
  struct vg_fs_summary s;
  unsigned int i;

  vg_fs_reset(0);
  for (i = 0; i < VG_FS_WINDOW; i++)
    {
      expect(vg_fs_inject(2, VG_FS_OK, 5) == 0, "fill window");
    }

  expect(vg_fs_inject(2, VG_FS_CRC, 0) == 0, "evict oldest");
  vg_fs_summary(2, &s);
  expect(s.total == VG_FS_WINDOW, "window size");
  expect(s.ok == VG_FS_WINDOW - 1, "one crc after eviction");
  expect(s.crc_err == 1, "crc present");
}

static void test_reset_slave(void)
{
  struct vg_fs_summary s;

  vg_fs_reset(0);
  expect(vg_fs_inject(3, VG_FS_OK, 1) == 0, "seed");
  vg_fs_reset(3);
  vg_fs_summary(3, &s);
  expect(s.total == 0, "reset clears slave");
}

static void test_init_idempotent(void)
{
  struct vg_fs_summary s;

  vg_fs_reset(0);
  expect(vg_fs_inject(4, VG_FS_CRC, 0) == 0, "seed before re-init");
  vg_fs_init();
  vg_fs_summary(4, &s);
  expect(s.total == 1, "re-init must not wipe buckets");
  expect(s.crc_err == 1, "crc survives re-init");
}

int main(void)
{
  vg_fs_init();
  test_inject_and_summary();
  test_window_eviction();
  test_reset_slave();
  test_init_idempotent();

  if (g_fail != 0)
    {
      fprintf(stderr, "test_frame_stats: %d failure(s)\n", g_fail);
      return 1;
    }

  printf("test_frame_stats: OK\n");
  return 0;
}
