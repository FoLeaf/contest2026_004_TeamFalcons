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

#include <lvgl/lvgl.h>
#include <stdint.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Bootstrap entry used by velaguard_main. */

void vg_ui_home_create(void);

/* Build the home page on an existing screen object. */

void vg_ui_home_build(FAR lv_obj_t *screen);
void vg_ui_home_clear_refs(void);
void vg_ui_home_refresh(void);
void vg_ui_home_uptime_checkpoint(uint64_t uptime_seconds);

/****************************************************************************
 * Public Data
 ****************************************************************************/

extern volatile uint64_t g_velaguard_uptime_seconds;

#endif /* APP_VELAGUARD_APP_SRC_VG_UI_HOME_H */
