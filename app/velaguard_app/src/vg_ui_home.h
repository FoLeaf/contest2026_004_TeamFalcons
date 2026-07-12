/****************************************************************************
 * app/velaguard_app/src/vg_ui_home.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef APP_VELAGUARD_APP_SRC_VG_UI_HOME_H
#define APP_VELAGUARD_APP_SRC_VG_UI_HOME_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void vg_ui_home_create(void);
void vg_ui_home_uptime_checkpoint(uint64_t uptime_seconds);

/****************************************************************************
 * Public Data
 ****************************************************************************/

extern volatile uint64_t g_velaguard_uptime_seconds;

#endif /* APP_VELAGUARD_APP_SRC_VG_UI_HOME_H */
