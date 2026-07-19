/****************************************************************************
 * app/velaguard_app/src/vg_ui_theme.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <string.h>

#include "vg_ui_theme.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void vg_ui_style_screen(FAR lv_obj_t *screen)
{
  lv_obj_set_style_bg_color(screen, lv_color_hex(VG_COLOR_BACKGROUND),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
}

FAR lv_obj_t *vg_ui_panel(FAR lv_obj_t *parent, int32_t x, int32_t y,
                          int32_t width, int32_t height, uint32_t color)
{
  FAR lv_obj_t *panel = lv_obj_create(parent);

  lv_obj_set_pos(panel, x, y);
  lv_obj_set_size(panel, width, height);
  lv_obj_set_style_bg_color(panel, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(panel, VG_PANEL_RADIUS, LV_PART_MAIN);
  lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  return panel;
}

FAR lv_obj_t *vg_ui_label(FAR lv_obj_t *parent, FAR const char *text,
                          uint32_t color, FAR const lv_font_t *font)
{
  FAR lv_obj_t *label = lv_label_create(parent);

  lv_label_set_text(label, text != NULL ? text : "");
  lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  return label;
}

bool vg_ui_label_set_text_if_changed(FAR lv_obj_t *label,
                                     FAR const char *text)
{
  FAR const char *current;

  if (label == NULL)
    {
      return false;
    }

  current = lv_label_get_text(label);
  text = text != NULL ? text : "";
  if (current != NULL && strcmp(current, text) == 0)
    {
      return false;
    }

  lv_label_set_text(label, text);
  return true;
}

bool vg_ui_label_set_color_if_changed(FAR lv_obj_t *label, uint32_t color)
{
  lv_color_t next;

  if (label == NULL)
    {
      return false;
    }

  next = lv_color_hex(color);
  if (lv_color_eq(lv_obj_get_style_text_color(label, LV_PART_MAIN), next))
    {
      return false;
    }

  lv_obj_set_style_text_color(label, next, LV_PART_MAIN);
  return true;
}

void vg_ui_style_pressable(FAR lv_obj_t *obj)
{
  lv_obj_set_style_bg_color(obj, lv_color_hex(VG_COLOR_RAISED),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(obj, lv_color_hex(0x24313c),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(obj, VG_BTN_RADIUS, LV_PART_MAIN);
  lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

FAR lv_obj_t *vg_ui_button(FAR lv_obj_t *parent, FAR const char *text)
{
  FAR lv_obj_t *btn = lv_button_create(parent);
  FAR lv_obj_t *label;

  lv_obj_set_style_bg_color(btn, lv_color_hex(VG_COLOR_ACCENT),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x2f9693),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn, VG_BTN_RADIUS, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(btn, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_ver(btn, 8, LV_PART_MAIN);

  label = vg_ui_label(btn, text, VG_COLOR_BACKGROUND, VG_FONT_BODY);
  lv_obj_center(label);
  return btn;
}

FAR lv_obj_t *vg_ui_header_bar(FAR lv_obj_t *parent, FAR const char *title,
                               lv_event_cb_t back_cb, FAR void *user_data)
{
  FAR lv_obj_t *bar = vg_ui_panel(parent, 6, 4, 468, 36, VG_COLOR_RAISED);
  FAR lv_obj_t *back;
  FAR lv_obj_t *label;

  back = lv_button_create(bar);
  lv_obj_set_size(back, VG_TOUCH_MIN_W + 8, VG_TOUCH_MIN_H);
  lv_obj_align(back, LV_ALIGN_LEFT_MID, 4, 0);
  lv_obj_set_style_bg_color(back, lv_color_hex(VG_COLOR_SURFACE),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_color(back, lv_color_hex(0x24313c),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_radius(back, VG_BTN_RADIUS, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(back, 0, LV_PART_MAIN);
  label = vg_ui_label(back, "返回", VG_COLOR_TEXT, VG_FONT_CAPTION);
  lv_obj_center(label);
  if (back_cb != NULL)
    {
      lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, user_data);
    }

  label = vg_ui_label(bar, title, VG_COLOR_TEXT, VG_FONT_TITLE);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 64, 0);
  return bar;
}

FAR lv_obj_t *vg_ui_icon_button(FAR lv_obj_t *parent,
                                FAR const lv_image_dsc_t *icon,
                                FAR const char *desc,
                                lv_event_cb_t cb, FAR void *user_data)
{
  FAR lv_obj_t *btn = lv_button_create(parent);
  FAR lv_obj_t *img;

  lv_obj_set_size(btn, VG_TOUCH_MIN_W + 8, VG_TOUCH_MIN_H);
  lv_obj_set_style_bg_color(btn, lv_color_hex(VG_COLOR_SURFACE),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x24313c),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn, VG_BTN_RADIUS, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);

  img = lv_image_create(btn);
  lv_image_set_src(img, icon);
  lv_obj_set_style_image_recolor(img, lv_color_hex(VG_COLOR_TEXT),
                                 LV_PART_MAIN);
  lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_center(img);

  if (desc != NULL)
    {
      /* Keep semantic description for accessibility/debug without a text label. */

      lv_obj_set_user_data(btn, (void *)desc);
    }

  if (cb != NULL)
    {
      lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }

  return btn;
}
