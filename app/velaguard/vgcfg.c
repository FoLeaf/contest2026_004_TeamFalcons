/****************************************************************************
 * app/velaguard/vgcfg.c
 *
 * NSH tool for dual-slot config store bring-up.
 ****************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nuttx/config.h>

#include "vg_config_store.h"

#ifndef CONFIG_VG_CONFIG_BASEDIR
#  define CONFIG_VG_CONFIG_BASEDIR "/data/velaguard/config"
#endif

static void usage(void)
{
  printf("Usage:\n");
  printf("  vgcfg dump\n");
  printf("  vgcfg commit <device_name>\n");
  printf("  vgcfg damage a|b [trunc|crc]\n");
  printf("  vgcfg probe\n");
  printf("  vgcfg basedir <path>\n");
}

/**
  * @brief  NSH: vgcfg — dual-slot config inspect / commit / damage.
  */
int main(int argc, char *argv[])
{
  struct vg_config cfg;
  int ret;

  vg_config_set_basedir(CONFIG_VG_CONFIG_BASEDIR);

  if (argc < 2)
    {
      usage();
      return 1;
    }

  if (strcmp(argv[1], "basedir") == 0)
    {
      if (argc < 3)
        {
          usage();
          return 1;
        }

      vg_config_set_basedir(argv[2]);
      printf("vgcfg: basedir=%s\n", argv[2]);
      return 0;
    }

  if (strcmp(argv[1], "probe") == 0)
    {
      char msg[128];

      ret = vg_config_probe(msg, sizeof(msg));
      printf("vgcfg: probe %s → %d (%s)\n",
             ret == 0 ? "OK" : "FAIL", ret, msg);
      return (ret == 0) ? 0 : 1;
    }

  if (strcmp(argv[1], "dump") == 0)
    {
      ret = vg_config_load(&cfg);
      if (ret < 0)
        {
          printf("vgcfg: load error %d\n", ret);
          return 1;
        }

      printf("vgcfg: %s seq=%u schema=%u committed=%d name=%s\n",
             (ret == 1) ? "FACTORY" : "OK",
             (unsigned)cfg.seq,
             (unsigned)cfg.schema_version,
             cfg.committed ? 1 : 0,
             cfg.device_name);
      return 0;
    }

  if (strcmp(argv[1], "commit") == 0)
    {
      if (argc < 3)
        {
          usage();
          return 1;
        }

      vg_config_factory_default(&cfg);
      snprintf(cfg.device_name, sizeof(cfg.device_name), "%s", argv[2]);
      ret = vg_config_commit(&cfg);
      if (ret != 0)
        {
          printf("vgcfg: commit failed %d errno=%d (%s)\n",
                 ret, errno, vg_config_last_error());
          return 1;
        }

      ret = vg_config_load(&cfg);
      printf("vgcfg: committed seq=%u name=%s (load=%d)\n",
             (unsigned)cfg.seq, cfg.device_name, ret);
      return 0;
    }

  if (strcmp(argv[1], "damage") == 0)
    {
      int slot;
      int mode = 0;

      if (argc < 3)
        {
          usage();
          return 1;
        }

      if (argv[2][0] == 'a' || argv[2][0] == 'A' || argv[2][0] == '0')
        {
          slot = 0;
        }
      else if (argv[2][0] == 'b' || argv[2][0] == 'B' || argv[2][0] == '1')
        {
          slot = 1;
        }
      else
        {
          usage();
          return 1;
        }

      if (argc >= 4 && strcmp(argv[3], "crc") == 0)
        {
          mode = 1;
        }

      ret = vg_config_damage_slot(slot, mode);
      printf("vgcfg: damage slot %d mode %d → %d\n", slot, mode, ret);
      return (ret == 0) ? 0 : 1;
    }

  usage();
  return 1;
}
