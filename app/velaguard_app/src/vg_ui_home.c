/****************************************************************************
 * app/velaguard_app/src/vg_ui_home.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <lvgl/lvgl.h>

#include "vg_identity.h"
#include "vg_startup.h"
#include "vg_ui_home.h"

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
#define VG_PANEL_RADIUS      8

/****************************************************************************
 * Public Data
 ****************************************************************************/

volatile uint64_t g_velaguard_uptime_seconds;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR lv_obj_t *g_uptime_label;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static FAR lv_obj_t *vg_panel(FAR lv_obj_t *parent, int32_t x, int32_t y,
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

static FAR lv_obj_t *vg_label(FAR lv_obj_t *parent, FAR const char *text,
                              uint32_t color, FAR const lv_font_t *font)
{
  FAR lv_obj_t *label = lv_label_create(parent);

  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  return label;
}

static void vg_add_state(FAR lv_obj_t *parent, FAR const char *title,
                         FAR const char *value, int32_t x, int32_t y,
                         uint32_t value_color)
{
  FAR lv_obj_t *label = vg_label(parent, title, VG_COLOR_MUTED,
                                 &lv_font_montserrat_10);

  lv_obj_set_pos(label, x, y);
  label = vg_label(parent, value, value_color, &lv_font_montserrat_14);
  lv_obj_set_pos(label, x, y + 18);
}

static void vg_format_uptime(char *buffer, size_t buffer_size,
                             uint64_t uptime_seconds)
{
  uint64_t hours = uptime_seconds / 3600u;
  uint64_t minutes = (uptime_seconds / 60u) % 60u;
  uint64_t seconds = uptime_seconds % 60u;

  snprintf(buffer, buffer_size, "Uptime %02llu:%02llu:%02llu",
           (unsigned long long)hours,
           (unsigned long long)minutes,
           (unsigned long long)seconds);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

__attribute__((noinline))
void vg_ui_home_uptime_checkpoint(uint64_t uptime_seconds)
{
  g_velaguard_uptime_seconds = uptime_seconds;
}

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void vg_uptime_timer(FAR lv_timer_t *timer)
{
  char text[32];
  uint64_t uptime_seconds = vg_uptime_ms() / 1000u;

  UNUSED(timer);
  vg_ui_home_uptime_checkpoint(uptime_seconds);
  vg_format_uptime(text, sizeof(text), uptime_seconds);
  lv_label_set_text(g_uptime_label, text);
}

static void vg_create_header(FAR lv_obj_t *screen)
{
  FAR const struct vg_identity *identity = vg_identity_get();
  FAR lv_obj_t *panel = vg_panel(screen, 8, 6, 464, 34, VG_COLOR_RAISED);
  FAR lv_obj_t *label = vg_label(panel, "VelaGuard", VG_COLOR_ACCENT,
                                 &lv_font_montserrat_20);
  char firmware[24];

  lv_obj_align(label, LV_ALIGN_LEFT_MID, 12, 0);

  label = vg_label(panel, vg_build_mode_string(identity->build_mode),
                   identity->build_mode == VG_BUILD_TEST ?
                   VG_COLOR_WARNING : VG_COLOR_ACCENT,
                   &lv_font_montserrat_12);
  lv_obj_align(label, LV_ALIGN_RIGHT_MID, -84, 0);

  snprintf(firmware, sizeof(firmware), "FW %s", identity->firmware_version);
  label = vg_label(panel, firmware, VG_COLOR_MUTED, &lv_font_montserrat_10);
  lv_obj_align(label, LV_ALIGN_RIGHT_MID, -12, 0);
}

static FAR const char *vg_boot_suffix(FAR const char *boot_id)
{
  size_t length = strlen(boot_id);

  return length > 8 ? &boot_id[length - 8] : boot_id;
}

static void vg_create_identity(FAR lv_obj_t *screen)
{
  FAR const struct vg_identity *identity = vg_identity_get();
  FAR const struct vg_startup_state *startup = vg_startup_get();
  FAR lv_obj_t *panel = vg_panel(screen, 8, 46, 464, 62, VG_COLOR_RAISED);
  FAR lv_obj_t *label;
  char text[48];
  uint32_t identity_color = vg_identity_valid() ?
                            VG_COLOR_TEXT : VG_COLOR_WARNING;
  uint32_t storage_color = vg_startup_storage_ready() ?
                           VG_COLOR_ACCENT : VG_COLOR_WARNING;

  label = vg_label(panel, "LOCAL GATEWAY", VG_COLOR_MUTED,
                   &lv_font_montserrat_10);
  lv_obj_set_pos(label, 12, 6);

  label = vg_label(panel,
                   vg_identity_valid() ? identity->device_id :
                   "IDENTITY ERROR",
                   identity_color, &lv_font_montserrat_12);
  lv_obj_set_pos(label, 12, 24);

  snprintf(text, sizeof(text), "Boot %s", vg_boot_suffix(startup->boot_id));
  label = vg_label(panel, text, VG_COLOR_MUTED, &lv_font_montserrat_10);
  lv_obj_set_pos(label, 12, 44);

  snprintf(text, sizeof(text), "Storage: %s",
           vg_startup_storage_ready() ? "READY" : "DEGRADED");
  label = vg_label(panel, text, storage_color, &lv_font_montserrat_12);
  lv_obj_align(label, LV_ALIGN_TOP_RIGHT, -12, 10);

  vg_format_uptime(text, sizeof(text), vg_uptime_ms() / 1000u);
  g_uptime_label = vg_label(panel, text, VG_COLOR_MUTED,
                            &lv_font_montserrat_10);
  lv_obj_align(g_uptime_label, LV_ALIGN_BOTTOM_RIGHT, -12, -8);
}

static void vg_create_primary_states(FAR lv_obj_t *screen)
{
  FAR lv_obj_t *panel = vg_panel(screen, 8, 114, 304, 150,
                                 VG_COLOR_SURFACE);
  FAR lv_obj_t *divider;

  vg_add_state(panel, "ACQUISITION", "Not configured", 14, 14,
               VG_COLOR_TEXT);

  divider = lv_obj_create(panel);
  lv_obj_set_pos(divider, 14, 75);
  lv_obj_set_size(divider, 276, 1);
  lv_obj_set_style_bg_color(divider, lv_color_hex(VG_COLOR_RAISED),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(divider, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(divider, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(divider, 0, LV_PART_MAIN);
  lv_obj_remove_flag(divider, LV_OBJ_FLAG_SCROLLABLE);

  vg_add_state(panel, "ALARM", "Not armed", 14, 88, VG_COLOR_TEXT);
}

static void vg_create_supporting_states(FAR lv_obj_t *screen)
{
  FAR const struct vg_startup_state *startup = vg_startup_get();
  FAR lv_obj_t *panel = vg_panel(screen, 318, 114, 154, 150,
                                 VG_COLOR_SURFACE);
  FAR const char *time_value = startup->time_quality == VG_TIME_UNKNOWN ?
                               "Unsynced" : "RTC available";

  vg_add_state(panel, "NETWORK", "Offline", 12, 10, VG_COLOR_TEXT);
  vg_add_state(panel, "AUDIO", "Unavailable", 12, 58, VG_COLOR_TEXT);
  vg_add_state(panel, "TIME", time_value, 12, 106, VG_COLOR_TEXT);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void vg_ui_home_create(void)
{
  FAR lv_obj_t *screen = lv_screen_active();

  lv_obj_set_style_bg_color(screen, lv_color_hex(VG_COLOR_BACKGROUND),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  vg_create_header(screen);
  vg_create_identity(screen);
  vg_create_primary_states(screen);
  vg_create_supporting_states(screen);
  lv_timer_create(vg_uptime_timer, 1000, NULL);
}
