/****************************************************************************
 * app/velaguard_app/src/vg_ui_settings.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef APP_VELAGUARD_APP_SRC_VG_UI_SETTINGS_H
#define APP_VELAGUARD_APP_SRC_VG_UI_SETTINGS_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <lvgl/lvgl.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void vg_ui_settings_build(FAR lv_obj_t *screen);
void vg_ui_settings_clear_refs(void);

#endif /* APP_VELAGUARD_APP_SRC_VG_UI_SETTINGS_H */
