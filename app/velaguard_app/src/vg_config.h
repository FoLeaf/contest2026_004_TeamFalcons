/****************************************************************************
 * app/velaguard_app/src/vg_config.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef APP_VELAGUARD_APP_SRC_VG_CONFIG_H
#define APP_VELAGUARD_APP_SRC_VG_CONFIG_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct vg_alarm_config
{
  float warn_c;
  float crit_c;
  float restore_c;
  unsigned int trigger_ms;
  unsigned int restore_ms;
  bool loaded_from_file;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Load /data/velaguard/configs/alarm.json or write defaults. */

int vg_config_load(void);
FAR const struct vg_alarm_config *vg_config_alarm(void);

#endif /* APP_VELAGUARD_APP_SRC_VG_CONFIG_H */
