/****************************************************************************
 * app/velaguard_app/src/vg_ui_nav.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Single resident service timer + screen navigation for VelaGuard UI.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>

#include <lvgl/lvgl.h>

#include "vg_acq.h"
#include "vg_alarm.h"
#include "vg_startup.h"
#include "vg_ui_home.h"
#include "vg_ui_nav.h"
#include "vg_ui_network.h"
#include "vg_ui_settings.h"
#include "vg_ui_theme.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct vg_ipv4_editor_ctx
{
  char title[24];
  char value[16];
  void (*on_confirm)(FAR const char *text, FAR void *user_data);
  FAR void *user_data;
  FAR lv_obj_t *ta;
  FAR lv_obj_t *error_label;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR lv_timer_t *g_service_timer;
static enum vg_ui_page g_page = VG_UI_PAGE_HOME;
static FAR lv_obj_t *g_active_screen;
static struct vg_ipv4_editor_ctx g_editor;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void vg_ui_clear_page_refs(void)
{
  vg_ui_home_clear_refs();
  vg_ui_settings_clear_refs();
  vg_ui_network_clear_refs();
  g_editor.ta = NULL;
  g_editor.error_label = NULL;
}

static void vg_ui_screen_delete_cb(FAR lv_event_t *event)
{
  FAR lv_obj_t *target = lv_event_get_target(event);

  if (target == g_active_screen)
    {
      vg_ui_clear_page_refs();
      g_active_screen = NULL;
    }
}

static FAR lv_obj_t *vg_ui_create_screen(void)
{
  FAR lv_obj_t *screen = lv_obj_create(NULL);

  vg_ui_style_screen(screen);
  lv_obj_add_event_cb(screen, vg_ui_screen_delete_cb, LV_EVENT_DELETE, NULL);
  return screen;
}

static void vg_ui_load_screen(FAR lv_obj_t *screen, enum vg_ui_page page)
{
  FAR lv_obj_t *old = g_active_screen;

  g_page = page;
  g_active_screen = screen;
  lv_screen_load(screen);

  /* After load, previous screen can be deleted safely. */

  if (old != NULL && old != screen)
    {
      lv_obj_delete(old);
    }
}

static void vg_ui_service_timer_cb(FAR lv_timer_t *timer)
{
  UNUSED(timer);
  vg_ui_service_poll();
}

static bool vg_ipv4_syntax_ok(FAR const char *text)
{
  int parts[4];
  int count = 0;
  int value = 0;
  bool in_number = false;
  FAR const char *p;

  if (text == NULL || text[0] == '\0' || strlen(text) > 15)
    {
      return false;
    }

  for (p = text; *p != '\0'; p++)
    {
      if (*p >= '0' && *p <= '9')
        {
          if (!in_number)
            {
              if (count >= 4)
                {
                  return false;
                }

              value = 0;
              in_number = true;
            }

          value = value * 10 + (*p - '0');
          if (value > 255)
            {
              return false;
            }
        }
      else if (*p == '.')
        {
          if (!in_number)
            {
              return false;
            }

          parts[count++] = value;
          in_number = false;
        }
      else
        {
          return false;
        }
    }

  if (!in_number || count != 3)
    {
      return false;
    }

  parts[count] = value;
  UNUSED(parts);
  return true;
}

static void vg_editor_cancel_cb(FAR lv_event_t *event)
{
  UNUSED(event);
  vg_ui_nav_show_network();
}

static void vg_editor_confirm_cb(FAR lv_event_t *event)
{
  FAR const char *text;

  UNUSED(event);
  if (g_editor.ta == NULL)
    {
      return;
    }

  text = lv_textarea_get_text(g_editor.ta);
  if (!vg_ipv4_syntax_ok(text))
    {
      if (g_editor.error_label != NULL)
        {
          lv_label_set_text(g_editor.error_label, "无效输入");
          lv_obj_set_style_text_color(g_editor.error_label,
                                      lv_color_hex(VG_COLOR_ALARM),
                                      LV_PART_MAIN);
        }

      return;
    }

  if (g_editor.on_confirm != NULL)
    {
      g_editor.on_confirm(text, g_editor.user_data);
    }

  vg_ui_nav_show_network();
}

static void vg_ui_build_ipv4_editor(FAR lv_obj_t *screen)
{
  FAR lv_obj_t *title;
  FAR lv_obj_t *kb;
  FAR lv_obj_t *cancel;
  FAR lv_obj_t *confirm;
  FAR lv_obj_t *label;

  title = vg_ui_label(screen, g_editor.title, VG_COLOR_TEXT, VG_FONT_TITLE);
  lv_obj_set_pos(title, 12, 8);

  g_editor.ta = lv_textarea_create(screen);
  lv_obj_set_size(g_editor.ta, 456, 36);
  lv_obj_set_pos(g_editor.ta, 12, 40);
  lv_textarea_set_one_line(g_editor.ta, true);
  lv_textarea_set_max_length(g_editor.ta, 15);
  lv_textarea_set_accepted_chars(g_editor.ta, "0123456789.");
  lv_textarea_set_text(g_editor.ta, g_editor.value);
  lv_obj_set_style_text_font(g_editor.ta, VG_FONT_VALUE, LV_PART_MAIN);
  lv_obj_set_style_text_color(g_editor.ta, lv_color_hex(VG_COLOR_TEXT),
                              LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_editor.ta, lv_color_hex(VG_COLOR_SURFACE),
                            LV_PART_MAIN);
  lv_obj_set_style_border_width(g_editor.ta, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_editor.ta, lv_color_hex(VG_COLOR_ACCENT),
                                LV_PART_MAIN);

  g_editor.error_label = vg_ui_label(screen, "", VG_COLOR_MUTED,
                                     VG_FONT_CAPTION);
  lv_obj_set_pos(g_editor.error_label, 12, 82);

  cancel = vg_ui_button(screen, "取消");
  lv_obj_set_size(cancel, 100, 34);
  lv_obj_set_pos(cancel, 12, 108);
  lv_obj_set_style_bg_color(cancel, lv_color_hex(VG_COLOR_RAISED),
                            LV_PART_MAIN);
  label = lv_obj_get_child(cancel, 0);
  if (label != NULL)
    {
      lv_obj_set_style_text_color(label, lv_color_hex(VG_COLOR_TEXT),
                                  LV_PART_MAIN);
    }

  lv_obj_add_event_cb(cancel, vg_editor_cancel_cb, LV_EVENT_CLICKED, NULL);

  confirm = vg_ui_button(screen, "确认");
  lv_obj_set_size(confirm, 100, 34);
  lv_obj_set_pos(confirm, 128, 108);
  lv_obj_add_event_cb(confirm, vg_editor_confirm_cb, LV_EVENT_CLICKED, NULL);

  kb = lv_keyboard_create(screen);
  lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
  lv_keyboard_set_textarea(kb, g_editor.ta);
  lv_obj_set_size(kb, 468, 120);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -4);
  lv_obj_set_style_bg_color(kb, lv_color_hex(VG_COLOR_SURFACE),
                            LV_PART_MAIN);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void vg_ui_service_poll(void)
{
  uint64_t uptime_seconds = vg_uptime_ms() / 1000u;

  vg_ui_home_uptime_checkpoint(uptime_seconds);
  vg_acq_poll();
  vg_alarm_eval();

  if (g_page == VG_UI_PAGE_HOME)
    {
      vg_ui_home_refresh();
    }
  else if (g_page == VG_UI_PAGE_NETWORK)
    {
      vg_ui_network_refresh();
    }
}

void vg_ui_nav_init(void)
{
  if (g_service_timer == NULL)
    {
      g_service_timer = lv_timer_create(vg_ui_service_timer_cb, 1000, NULL);
    }
}

void vg_ui_nav_show_home(void)
{
  FAR lv_obj_t *screen = vg_ui_create_screen();

  vg_ui_home_build(screen);
  vg_ui_load_screen(screen, VG_UI_PAGE_HOME);
}

void vg_ui_nav_show_settings(void)
{
  FAR lv_obj_t *screen = vg_ui_create_screen();

  vg_ui_settings_build(screen);
  vg_ui_load_screen(screen, VG_UI_PAGE_SETTINGS);
}

void vg_ui_nav_show_network(void)
{
  FAR lv_obj_t *screen = vg_ui_create_screen();

  vg_ui_network_build(screen);
  vg_ui_load_screen(screen, VG_UI_PAGE_NETWORK);
}

void vg_ui_nav_show_ipv4_editor(FAR const char *title, FAR const char *value,
                                void (*on_confirm)(FAR const char *text,
                                                   FAR void *user_data),
                                FAR void *user_data)
{
  FAR lv_obj_t *screen = vg_ui_create_screen();

  memset(&g_editor, 0, sizeof(g_editor));
  strlcpy(g_editor.title, title != NULL ? title : "编辑",
          sizeof(g_editor.title));
  strlcpy(g_editor.value, value != NULL ? value : "",
          sizeof(g_editor.value));
  g_editor.on_confirm = on_confirm;
  g_editor.user_data = user_data;

  vg_ui_build_ipv4_editor(screen);
  vg_ui_load_screen(screen, VG_UI_PAGE_IPV4_EDITOR);
}
