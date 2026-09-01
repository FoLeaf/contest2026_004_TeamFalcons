/****************************************************************************
 * app/velaguard/host_tests/test_config_store.c
 *
 * Host dual-slot tests (AC1).
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "vg_config_store.h"

static int g_fail;
static char g_dir[256];

static void expect(int cond, const char *msg)
{
  if (!cond)
    {
      fprintf(stderr, "FAIL: %s\n", msg);
      g_fail++;
    }
}

static void setup_dir(void)
{
  char cmd[320];

  snprintf(g_dir, sizeof(g_dir), "/tmp/vgcfg_test_%d", (int)getpid());
  snprintf(cmd, sizeof(cmd), "rm -rf %s", g_dir);
  system(cmd);
  vg_config_set_basedir(g_dir);
}

static void cleanup_dir(void)
{
  char cmd[320];

  snprintf(cmd, sizeof(cmd), "rm -rf %s", g_dir);
  system(cmd);
  vg_config_set_basedir(NULL);
}

static void test_factory_when_empty(void)
{
  struct vg_config cfg;
  int ret;

  setup_dir();
  ret = vg_config_load(&cfg);
  expect(ret == 1, "empty load returns factory sentinel");
  expect(strcmp(cfg.device_name, "factory") == 0, "factory name");
  expect(cfg.seq == 0, "factory seq 0");
  cleanup_dir();
}

static void test_commit_and_load(void)
{
  struct vg_config in;
  struct vg_config out;
  int ret;

  setup_dir();
  vg_config_factory_default(&in);
  snprintf(in.device_name, sizeof(in.device_name), "unit1");
  ret = vg_config_commit(&in);
  expect(ret == 0, "first commit ok");
  ret = vg_config_load(&out);
  expect(ret == 0, "load after commit");
  expect(out.seq == 1, "seq starts at 1");
  expect(strcmp(out.device_name, "unit1") == 0, "name persisted");

  snprintf(in.device_name, sizeof(in.device_name), "unit2");
  ret = vg_config_commit(&in);
  expect(ret == 0, "second commit ok");
  ret = vg_config_load(&out);
  expect(ret == 0, "load after second");
  expect(out.seq == 2, "seq increments");
  expect(strcmp(out.device_name, "unit2") == 0, "newer name");
  cleanup_dir();
}

static void test_damage_active_uses_other(void)
{
  struct vg_config in;
  struct vg_config out;

  setup_dir();
  vg_config_factory_default(&in);
  snprintf(in.device_name, sizeof(in.device_name), "slotA");
  expect(vg_config_commit(&in) == 0, "commit A path");
  snprintf(in.device_name, sizeof(in.device_name), "slotB");
  expect(vg_config_commit(&in) == 0, "commit B path");

  expect(vg_config_load(&out) == 0, "pre-damage load");
  expect(out.seq == 2, "active is seq2");
  expect(strcmp(out.device_name, "slotB") == 0, "active name slotB");

  /* Second commit wrote inactive slot 1 → damage slot 1. */

  expect(vg_config_damage_slot(1, 0) == 0, "truncate slot1");
  expect(vg_config_load(&out) == 0, "fallback load");
  expect(out.seq == 1, "fell back to seq1");
  expect(strcmp(out.device_name, "slotA") == 0, "fell back to slotA");
  cleanup_dir();
}

static void test_both_bad_factory(void)
{
  struct vg_config in;
  struct vg_config out;
  int ret;

  setup_dir();
  vg_config_factory_default(&in);
  snprintf(in.device_name, sizeof(in.device_name), "x");
  expect(vg_config_commit(&in) == 0, "commit");
  expect(vg_config_commit(&in) == 0, "commit2");
  expect(vg_config_damage_slot(0, 0) == 0, "trunc0");
  expect(vg_config_damage_slot(1, 0) == 0, "trunc1");
  ret = vg_config_load(&out);
  expect(ret == 1, "both bad → factory");
  expect(strcmp(out.device_name, "factory") == 0, "factory name");
  cleanup_dir();
}

static void test_corrupt_crc(void)
{
  struct vg_config in;
  struct vg_config out;
  int ret;

  setup_dir();
  vg_config_factory_default(&in);
  snprintf(in.device_name, sizeof(in.device_name), "good");
  expect(vg_config_commit(&in) == 0, "commit");
  expect(vg_config_damage_slot(0, 1) == 0, "corrupt crc slot0");
  /* Only one slot written on first commit (inactive=0). */
  ret = vg_config_load(&out);
  expect(ret == 1, "corrupt only slot → factory");
  cleanup_dir();
}

int main(void)
{
  test_factory_when_empty();
  test_commit_and_load();
  test_damage_active_uses_other();
  test_both_bad_factory();
  test_corrupt_crc();

  if (g_fail)
    {
      fprintf(stderr, "%d failure(s)\n", g_fail);
      return 1;
    }

  printf("test_config_store: OK\n");
  return 0;
}
