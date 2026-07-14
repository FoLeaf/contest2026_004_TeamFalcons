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

#include "fonts/vg_font_cn_16.h"
#include "vg_acq.h"
#include "vg_alarm.h"
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
#define VG_FONT_UI           (&vg_font_cn_16)

/****************************************************************************
 * Public Data
 ****************************************************************************/

volatile uint64_t g_velaguard_uptime_seconds;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR lv_obj_t *g_uptime_label;
static FAR lv_obj_t *g_acq_title_label;
static FAR lv_obj_t *g_acq_value_label;
static FAR lv_obj_t *g_alarm_title_label;
static FAR lv_obj_t *g_alarm_value_label;
static FAR lv_obj_t *g_alarm_event_label;

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
  FAR lv_obj_t *label = vg_label(parent, title, VG_COLOR_MUTED, VG_FONT_UI);

  lv_obj_set_pos(label, x, y);
  label = vg_label(parent, value, value_color, VG_FONT_UI);
  lv_obj_set_pos(label, x, y + 18);
}

static void vg_format_uptime(char *buffer, size_t buffer_size,
                             uint64_t uptime_seconds)
{
  uint64_t hours = uptime_seconds / 3600u;
  uint64_t minutes = (uptime_seconds / 60u) % 60u;
  uint64_t seconds = uptime_seconds % 60u;

  snprintf(buffer, buffer_size, "运行 %02llu:%02llu:%02llu",
           (unsigned long long)hours,
           (unsigned long long)minutes,
           (unsigned long long)seconds);
}

static FAR const char *vg_backend_label(void)
{
  FAR const char *name = vg_acq_backend_name();

  if (strcmp(name, "MOCK") == 0)
    {
      return "模拟";
    }

  if (strcmp(name, "UART") == 0)
    {
      return "串口";
    }

  return "无";
}

static FAR const char *vg_quality_label(enum vg_acq_quality quality)
{
  switch (quality)
    {
      case VG_ACQ_Q_GOOD:
        return "良好";

      case VG_ACQ_Q_BAD:
        return "异常";

      default:
        return "无";
    }
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

static void vg_refresh_acquisition_labels(void)
{
  FAR const struct vg_acq_point *point = vg_acq_primary();
  char title[40];
  char value[48];

  snprintf(title, sizeof(title), "采集 [%s]", vg_backend_label());
  if (g_acq_title_label != NULL)
    {
      lv_label_set_text(g_acq_title_label, title);
    }

  if (point == NULL || point->quality == VG_ACQ_Q_NONE)
    {
      snprintf(value, sizeof(value), "未配置");
    }
  else
    {
      int tenths = (int)(point->value * 10.0f + (point->value >= 0 ? 0.5f : -0.5f));

      snprintf(value, sizeof(value), "%d.%d %s  %s",
               tenths / 10, tenths < 0 ? (-tenths) % 10 : tenths % 10,
               point->unit != NULL ? point->unit : "",
               vg_quality_label(point->quality));
    }

  if (g_acq_value_label != NULL)
    {
      lv_label_set_text(g_acq_value_label, value);
      if (point != NULL && point->quality == VG_ACQ_Q_GOOD)
        {
          lv_obj_set_style_text_color(g_acq_value_label,
                                      lv_color_hex(VG_COLOR_ACCENT),
                                      LV_PART_MAIN);
        }
      else
        {
          lv_obj_set_style_text_color(g_acq_value_label,
                                      lv_color_hex(VG_COLOR_TEXT),
                                      LV_PART_MAIN);
        }
    }
}

static void vg_refresh_alarm_labels(void)
{
  FAR const struct vg_alarm_status *alarm = vg_alarm_get();
  char value[48];
  uint32_t color = VG_COLOR_TEXT;

  if (alarm == NULL || !alarm->armed)
    {
      snprintf(value, sizeof(value), "未启用");
      color = VG_COLOR_TEXT;
    }
  else if (alarm->level == VG_ALARM_CRITICAL)
    {
      int tenths = (int)(alarm->current_c * 10.0f + 0.5f);

      snprintf(value, sizeof(value), "危险  %d.%d C",
               tenths / 10, tenths % 10);
      color = VG_COLOR_ALARM;
    }
  else if (alarm->level == VG_ALARM_WARNING)
    {
      int tenths = (int)(alarm->current_c * 10.0f + 0.5f);

      snprintf(value, sizeof(value), "注意  %d.%d C",
               tenths / 10, tenths % 10);
      color = VG_COLOR_WARNING;
    }
  else
    {
      snprintf(value, sizeof(value), "正常  (< %.0f C)",
               (double)alarm->threshold_c);
      color = VG_COLOR_ACCENT;
    }

  if (g_alarm_title_label != NULL)
    {
      lv_label_set_text(g_alarm_title_label, "告警");
    }

  if (g_alarm_value_label != NULL)
    {
      lv_label_set_text(g_alarm_value_label, value);
      lv_obj_set_style_text_color(g_alarm_value_label, lv_color_hex(color),
                                  LV_PART_MAIN);
    }

  if (g_alarm_event_label != NULL)
    {
      if (alarm != NULL && alarm->last_event[0] != '\0')
        {
          lv_label_set_text(g_alarm_event_label, alarm->last_event);
        }
      else
        {
          lv_label_set_text(g_alarm_event_label, "尚无事件");
        }
    }
}

static void vg_uptime_timer(FAR lv_timer_t *timer)
{
  char text[32];
  uint64_t uptime_seconds = vg_uptime_ms() / 1000u;

  UNUSED(timer);
  vg_ui_home_uptime_checkpoint(uptime_seconds);
  vg_format_uptime(text, sizeof(text), uptime_seconds);
  lv_label_set_text(g_uptime_label, text);

  vg_acq_poll();
  vg_alarm_eval();
  vg_refresh_acquisition_labels();
  vg_refresh_alarm_labels();
}

static void vg_create_header(FAR lv_obj_t *screen)
{
  FAR const struct vg_identity *identity = vg_identity_get();
  FAR lv_obj_t *panel = vg_panel(screen, 8, 6, 464, 34, VG_COLOR_RAISED);
  FAR lv_obj_t *label = vg_label(panel, "VelaGuard", VG_COLOR_ACCENT,
                                 VG_FONT_UI);
  char firmware[32];
  FAR const char *mode =
    identity->build_mode == VG_BUILD_TEST ? "试验" : "量产";

  lv_obj_align(label, LV_ALIGN_LEFT_MID, 12, 0);

  label = vg_label(panel, mode,
                   identity->build_mode == VG_BUILD_TEST ?
                   VG_COLOR_WARNING : VG_COLOR_ACCENT,
                   VG_FONT_UI);
  lv_obj_align(label, LV_ALIGN_RIGHT_MID, -96, 0);

  snprintf(firmware, sizeof(firmware), "版本 %s",
           identity->firmware_version);
  label = vg_label(panel, firmware, VG_COLOR_MUTED, VG_FONT_UI);
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

  label = vg_label(panel, "本地系统", VG_COLOR_MUTED, VG_FONT_UI);
  lv_obj_set_pos(label, 12, 6);

  label = vg_label(panel,
                   vg_identity_valid() ? identity->device_id :
                   "设备标识异常",
                   identity_color, VG_FONT_UI);
  lv_obj_set_pos(label, 12, 24);

  snprintf(text, sizeof(text), "开机 %s", vg_boot_suffix(startup->boot_id));
  label = vg_label(panel, text, VG_COLOR_MUTED, VG_FONT_UI);
  lv_obj_set_pos(label, 12, 44);

  snprintf(text, sizeof(text), "存储: %s",
           vg_startup_storage_ready() ? "就绪" : "降级");
  label = vg_label(panel, text, storage_color, VG_FONT_UI);
  lv_obj_align(label, LV_ALIGN_TOP_RIGHT, -12, 10);

  vg_format_uptime(text, sizeof(text), vg_uptime_ms() / 1000u);
  g_uptime_label = vg_label(panel, text, VG_COLOR_MUTED, VG_FONT_UI);
  lv_obj_align(g_uptime_label, LV_ALIGN_BOTTOM_RIGHT, -12, -8);
}

static void vg_create_primary_states(FAR lv_obj_t *screen)
{
  FAR lv_obj_t *panel = vg_panel(screen, 8, 114, 304, 150,
                                 VG_COLOR_SURFACE);
  FAR lv_obj_t *divider;
  FAR const struct vg_acq_point *point = vg_acq_primary();
  char title[40];
  char value[48];
  uint32_t value_color = VG_COLOR_TEXT;

  snprintf(title, sizeof(title), "采集 [%s]", vg_backend_label());
  if (point != NULL && point->quality == VG_ACQ_Q_GOOD)
    {
      int tenths = (int)(point->value * 10.0f + 0.5f);

      value_color = VG_COLOR_ACCENT;
      snprintf(value, sizeof(value), "%d.%d %s  %s",
               tenths / 10, tenths % 10,
               point->unit != NULL ? point->unit : "",
               vg_quality_label(point->quality));
    }
  else
    {
      snprintf(value, sizeof(value), "未配置");
    }

  g_acq_title_label = vg_label(panel, title, VG_COLOR_MUTED, VG_FONT_UI);
  lv_obj_set_pos(g_acq_title_label, 14, 14);
  g_acq_value_label = vg_label(panel, value, value_color, VG_FONT_UI);
  lv_obj_set_pos(g_acq_value_label, 14, 32);

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

  g_alarm_title_label = vg_label(panel, "告警", VG_COLOR_MUTED, VG_FONT_UI);
  lv_obj_set_pos(g_alarm_title_label, 14, 82);
  g_alarm_value_label = vg_label(panel, "未启用", VG_COLOR_TEXT, VG_FONT_UI);
  lv_obj_set_pos(g_alarm_value_label, 14, 98);
  g_alarm_event_label = vg_label(panel, "尚无事件", VG_COLOR_MUTED, VG_FONT_UI);
  lv_obj_set_pos(g_alarm_event_label, 14, 122);
  lv_label_set_long_mode(g_alarm_event_label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(g_alarm_event_label, 276);
}

static void vg_create_supporting_states(FAR lv_obj_t *screen)
{
  FAR const struct vg_startup_state *startup = vg_startup_get();
  FAR lv_obj_t *panel = vg_panel(screen, 318, 114, 154, 150,
                                 VG_COLOR_SURFACE);
  FAR const char *time_value = startup->time_quality == VG_TIME_UNKNOWN ?
                               "未同步" : "已同步";

  vg_add_state(panel, "网络", "离线", 12, 10, VG_COLOR_TEXT);
  vg_add_state(panel, "声音", "不可用", 12, 58, VG_COLOR_TEXT);
  vg_add_state(panel, "时刻", time_value, 12, 106, VG_COLOR_TEXT);
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
