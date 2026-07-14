/****************************************************************************
 * app/velaguard_app/src/vg_alarm.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal local threshold loop for mock/demo (issue #03 slice).
 ****************************************************************************/

#ifndef APP_VELAGUARD_APP_SRC_VG_ALARM_H
#define APP_VELAGUARD_APP_SRC_VG_ALARM_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

enum vg_alarm_level
{
  VG_ALARM_OK = 0,
  VG_ALARM_WARNING,
  VG_ALARM_CRITICAL
};

struct vg_alarm_status
{
  enum vg_alarm_level level;
  FAR const char *title;
  float threshold_c;
  float current_c;
  bool armed;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void vg_alarm_init(void);
void vg_alarm_eval(void);
FAR const struct vg_alarm_status *vg_alarm_get(void);
FAR const char *vg_alarm_level_string(enum vg_alarm_level level);

#endif /* APP_VELAGUARD_APP_SRC_VG_ALARM_H */
