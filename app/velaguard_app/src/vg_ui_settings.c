/****************************************************************************
 * app/velaguard_app/src/vg_ui_settings.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <string.h>

#include <lvgl/lvgl.h>

#include "vg_ui_nav.h"
#include "vg_ui_settings.h"
#include "vg_ui_theme.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct vg_settings_item
{
  FAR const char *title;
  bool available;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct vg_settings_item g_items[] =
{
  { "网络设置", true },
  { "采集与传感器", false },
  { "告警规则", false },
  { "云端与 MQTT", false },
  { "声音", false },
  { "系统", false },
  { "固件更新", false },
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void vg_settings_back_cb(FAR lv_event_t *event)
{
  UNUSED(event);
  vg_ui_nav_show_home();
}

static void vg_settings_network_cb(FAR lv_event_t *event)
{
  UNUSED(event);
  vg_ui_nav_show_network();
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void vg_ui_settings_clear_refs(void)
{
  /* Settings page is static; no live object refs. */
}

void vg_ui_settings_build(FAR lv_obj_t *screen)
{
  FAR lv_obj_t *list;
  unsigned int i;

  vg_ui_header_bar(screen, "设置", vg_settings_back_cb, NULL);

  list = lv_obj_create(screen);
  lv_obj_set_pos(list, 6, 44);
  lv_obj_set_size(list, 468, 222);
  lv_obj_set_style_bg_color(list, lv_color_hex(VG_COLOR_BACKGROUND),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(list, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(list, 6, LV_PART_MAIN);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

  for (i = 0; i < sizeof(g_items) / sizeof(g_items[0]); i++)
    {
      FAR lv_obj_t *row = lv_obj_create(list);
      FAR lv_obj_t *title;
      FAR lv_obj_t *status;

      lv_obj_set_size(row, 468, 40);
      lv_obj_set_style_bg_color(row, lv_color_hex(VG_COLOR_SURFACE),
                                LV_PART_MAIN);
      lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
      lv_obj_set_style_radius(row, VG_PANEL_RADIUS, LV_PART_MAIN);
      lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
      lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

      title = vg_ui_label(row, g_items[i].title, VG_COLOR_TEXT,
                          VG_FONT_BODY);
      lv_obj_align(title, LV_ALIGN_LEFT_MID, 12, 0);

      if (g_items[i].available)
        {
          status = vg_ui_label(row, "可用", VG_COLOR_ACCENT,
                               VG_FONT_CAPTION);
          lv_obj_align(status, LV_ALIGN_RIGHT_MID, -12, 0);
          lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
          lv_obj_set_style_bg_color(row, lv_color_hex(0x24313c),
                                    LV_PART_MAIN | LV_STATE_PRESSED);
          lv_obj_add_event_cb(row, vg_settings_network_cb, LV_EVENT_CLICKED,
                              NULL);
        }
      else
        {
          status = vg_ui_label(row, "暂未开放", VG_COLOR_MUTED,
                               VG_FONT_CAPTION);
          lv_obj_align(status, LV_ALIGN_RIGHT_MID, -12, 0);
          lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
        }
    }
}
