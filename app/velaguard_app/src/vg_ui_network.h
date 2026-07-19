/****************************************************************************
 * app/velaguard_app/src/vg_ui_network.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef APP_VELAGUARD_APP_SRC_VG_UI_NETWORK_H
#define APP_VELAGUARD_APP_SRC_VG_UI_NETWORK_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <lvgl/lvgl.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void vg_ui_network_build(FAR lv_obj_t *screen);
void vg_ui_network_clear_refs(void);
void vg_ui_network_refresh(void);

#endif /* APP_VELAGUARD_APP_SRC_VG_UI_NETWORK_H */
