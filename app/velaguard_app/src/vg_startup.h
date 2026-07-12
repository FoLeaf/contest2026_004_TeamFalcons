/****************************************************************************
 * app/velaguard_app/src/vg_startup.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef APP_VELAGUARD_APP_SRC_VG_STARTUP_H
#define APP_VELAGUARD_APP_SRC_VG_STARTUP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

enum vg_time_quality
{
  VG_TIME_UNKNOWN = 0,
  VG_TIME_RTC,
  VG_TIME_NTP,
  VG_TIME_CLOUD
};

struct vg_startup_state
{
  bool directories_ready;
  bool human_log_written;
  bool event_written;
  int storage_error;
  char boot_id[24];
  uint64_t ts_ms;
  uint64_t uptime_ms;
  enum vg_time_quality time_quality;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int vg_startup_record(void);
FAR const struct vg_startup_state *vg_startup_get(void);
bool vg_startup_storage_ready(void);
uint64_t vg_uptime_ms(void);
FAR const char *vg_time_quality_string(enum vg_time_quality quality);

#endif /* APP_VELAGUARD_APP_SRC_VG_STARTUP_H */
