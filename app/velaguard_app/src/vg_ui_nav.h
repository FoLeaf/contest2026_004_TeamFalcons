/****************************************************************************
 * app/velaguard_app/src/vg_ui_nav.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef APP_VELAGUARD_APP_SRC_VG_UI_NAV_H
#define APP_VELAGUARD_APP_SRC_VG_UI_NAV_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

enum vg_ui_page
{
  VG_UI_PAGE_HOME = 0,
  VG_UI_PAGE_SETTINGS,
  VG_UI_PAGE_NETWORK,
  VG_UI_PAGE_IPV4_EDITOR
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void vg_ui_nav_init(void);
void vg_ui_nav_show_home(void);
void vg_ui_nav_show_settings(void);
void vg_ui_nav_show_network(void);
void vg_ui_nav_show_ipv4_editor(FAR const char *title, FAR const char *value,
                                void (*on_confirm)(FAR const char *text,
                                                   FAR void *user_data),
                                FAR void *user_data);

/* Service timer helpers used by page modules. */

void vg_ui_service_poll(void);

#endif /* APP_VELAGUARD_APP_SRC_VG_UI_NAV_H */
