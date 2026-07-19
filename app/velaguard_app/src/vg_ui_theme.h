/****************************************************************************
 * app/velaguard_app/src/vg_ui_theme.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef APP_VELAGUARD_APP_SRC_VG_UI_THEME_H
#define APP_VELAGUARD_APP_SRC_VG_UI_THEME_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <lvgl/lvgl.h>

#include "fonts/vg_font_cn_16.h"
#include "fonts/vg_font_cn_20.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VG_COLOR_BACKGROUND  0x0c1218
#define VG_COLOR_SURFACE     0x151e26
#define VG_COLOR_RAISED      0x1c2731
#define VG_COLOR_TEXT        0xe7edf2
#define VG_COLOR_MUTED       0x8d9aa5
#define VG_COLOR_ACCENT      0x39b6b2
#define VG_COLOR_ALARM       0xe05b5b
#define VG_COLOR_WARNING     0xd7a84a
#define VG_PANEL_RADIUS      6
#define VG_BTN_RADIUS        6
#define VG_TOUCH_MIN_W       40
#define VG_TOUCH_MIN_H       32

#define VG_FONT_CAPTION      (&vg_font_cn_16)
#define VG_FONT_BODY         (&vg_font_cn_16)
#define VG_FONT_VALUE        (&vg_font_cn_20)
#define VG_FONT_TITLE        (&vg_font_cn_20)

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void vg_ui_style_screen(FAR lv_obj_t *screen);
FAR lv_obj_t *vg_ui_panel(FAR lv_obj_t *parent, int32_t x, int32_t y,
                          int32_t width, int32_t height, uint32_t color);
FAR lv_obj_t *vg_ui_label(FAR lv_obj_t *parent, FAR const char *text,
                          uint32_t color, FAR const lv_font_t *font);
FAR lv_obj_t *vg_ui_button(FAR lv_obj_t *parent, FAR const char *text);
void vg_ui_style_pressable(FAR lv_obj_t *obj);
FAR lv_obj_t *vg_ui_header_bar(FAR lv_obj_t *parent, FAR const char *title,
                               lv_event_cb_t back_cb, FAR void *user_data);
FAR lv_obj_t *vg_ui_icon_button(FAR lv_obj_t *parent,
                                FAR const lv_image_dsc_t *icon,
                                FAR const char *desc,
                                lv_event_cb_t cb, FAR void *user_data);

#endif /* APP_VELAGUARD_APP_SRC_VG_UI_THEME_H */
