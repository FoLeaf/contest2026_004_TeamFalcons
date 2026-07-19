/****************************************************************************
 * app/velaguard_app/velaguard_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <lvgl/lvgl.h>
#include <nshlib/nshlib.h>

#include "src/vg_acq.h"
#include "src/vg_alarm.h"
#include "src/vg_config.h"
#include "src/vg_identity.h"
#include "src/vg_network.h"
#include "src/vg_startup.h"
#include "src/vg_ui_home.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool g_velaguard_started;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int velaguard_ui_main(int argc, FAR char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;

  UNUSED(argc);
  UNUSED(argv);

  lv_init();
  lv_nuttx_dsc_init(&info);
  info.input_path = CONFIG_LVX_VELAGUARD_INPUT_PATH;
  lv_nuttx_init(&info, &result);
  if (result.disp == NULL)
    {
      fprintf(stderr, "[velaguard] LVGL display initialization failed\n");
      lv_nuttx_deinit(&result);
      lv_deinit();
      return 1;
    }

  if (result.indev == NULL)
    {
      fprintf(stderr,
              "[velaguard] touchscreen unavailable; home remains visible\n");
    }

  vg_ui_home_create();
  printf("[velaguard] UI ready\n");

  while (true)
    {
      uint32_t idle = lv_timer_handler();

      if (idle < 1)
        {
          idle = 1;
        }
      else if (idle > 100)
        {
          idle = 100;
        }

      usleep(idle * 1000);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int velaguard_main(int argc, FAR char *argv[])
{
  int identity_result;
  int storage_result;
  int task_result;

  if (g_velaguard_started)
    {
      fprintf(stderr, "[velaguard] already started\n");
      return 1;
    }

  g_velaguard_started = true;
  nsh_initialize();

  identity_result = vg_identity_init();
  if (identity_result < 0)
    {
      fprintf(stderr, "[velaguard] invalid test Device ID (%d)\n",
              identity_result);
    }

  storage_result = vg_startup_record();
  if (storage_result < 0)
    {
      fprintf(stderr, "[velaguard] storage degraded (%d)\n", storage_result);
    }

  if (vg_acq_init() < 0)
    {
      fprintf(stderr, "[velaguard] acquisition init failed\n");
    }
  else
    {
      printf("[velaguard] acquisition backend: %s\n",
             vg_acq_backend_name());
    }

  if (vg_config_load() < 0)
    {
      fprintf(stderr, "[velaguard] alarm config defaults only\n");
    }

  vg_alarm_init();

  if (vg_network_start() < 0)
    {
      fprintf(stderr, "[velaguard] network service start failed\n");
    }

  task_result = task_create("velaguard_ui",
                            CONFIG_LVX_VELAGUARD_PRIORITY,
                            CONFIG_LVX_VELAGUARD_STACKSIZE,
                            velaguard_ui_main, NULL);
  if (task_result < 0)
    {
      fprintf(stderr, "[velaguard] UI task creation failed: %d\n", errno);
    }

  return nsh_consolemain(argc, argv);
}
