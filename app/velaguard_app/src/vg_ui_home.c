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

#include "assets/vg_icons.h"
#include "vg_acq.h"
#include "vg_alarm.h"
#include "vg_identity.h"
#include "vg_network.h"
#include "vg_startup.h"
#include "vg_ui_home.h"
#include "vg_ui_nav.h"
#include "vg_ui_theme.h"

/****************************************************************************
 * Public Data
 ****************************************************************************/

volatile uint64_t g_velaguard_uptime_seconds;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR lv_obj_t *g_uptime_label;
static FAR lv_obj_t *g_net_status_label;
static FAR lv_obj_t *g_acq_title_label;
static FAR lv_obj_t *g_acq_value_label;
static FAR lv_obj_t *g_acq_quality_label;
static FAR lv_obj_t *g_alarm_title_label;
static FAR lv_obj_t *g_alarm_value_label;
static FAR lv_obj_t *g_alarm_hist_labels[VG_ALARM_HISTORY_MAX];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void vg_add_state(FAR lv_obj_t *parent, FAR const char *title,
                         FAR const char *value, int32_t x, int32_t y,
                         uint32_t value_color)
{
  FAR lv_obj_t *label = vg_ui_label(parent, title, VG_COLOR_MUTED,
                                    VG_FONT_CAPTION);

  lv_obj_set_pos(label, x, y);
  label = vg_ui_label(parent, value, value_color, VG_FONT_BODY);
  lv_obj_set_pos(label, x, y + 20);
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

static void vg_settings_clicked(FAR lv_event_t *event)
{
  UNUSED(event);
  vg_ui_nav_show_settings();
}

static void vg_refresh_network_summary(void)
{
  struct vg_network_status status;
  char text[72];
  uint32_t color = VG_COLOR_MUTED;

  if (g_net_status_label == NULL)
    {
      return;
    }

  vg_network_get_status(&status);
  switch (status.state)
    {
      case VG_NETWORK_ONLINE:
        if (status.ipv4[0] != '\0')
          {
            snprintf(text, sizeof(text), "%s", status.ipv4);
          }
        else
          {
            snprintf(text, sizeof(text), "已联网");
          }

        color = VG_COLOR_ACCENT;
        break;

      case VG_NETWORK_ADDRESSING:
        snprintf(text, sizeof(text), "%s",
                 status.message[0] != '\0' ? status.message : "正在获取地址");
        color = VG_COLOR_WARNING;
        break;

      case VG_NETWORK_APPLYING:
        snprintf(text, sizeof(text), "正在应用");
        color = VG_COLOR_WARNING;
        break;

      case VG_NETWORK_ERROR:
        snprintf(text, sizeof(text), "网络异常");
        color = VG_COLOR_ALARM;
        break;

      case VG_NETWORK_DOWN:
      default:
        snprintf(text, sizeof(text), "无链路");
        color = VG_COLOR_MUTED;
        break;
    }

  vg_ui_label_set_text_if_changed(g_net_status_label, text);
  vg_ui_label_set_color_if_changed(g_net_status_label, color);
}

static void vg_refresh_acquisition_labels(void)
{
  FAR const struct vg_acq_point *point = vg_acq_primary();
  char title[40];
  char value[48];

  snprintf(title, sizeof(title), "采集/%s", vg_backend_label());
  if (g_acq_title_label != NULL)
    {
      vg_ui_label_set_text_if_changed(g_acq_title_label, title);
    }

  if (point == NULL || point->quality == VG_ACQ_Q_NONE)
    {
      snprintf(value, sizeof(value), "未配置");
    }
  else
    {
      int tenths = (int)(point->value * 10.0f + (point->value >= 0 ? 0.5f : -0.5f));

      snprintf(value, sizeof(value), "%d.%d %s",
               tenths / 10, tenths < 0 ? (-tenths) % 10 : tenths % 10,
               point->unit != NULL ? point->unit : "");
    }

  if (g_acq_value_label != NULL)
    {
      vg_ui_label_set_text_if_changed(g_acq_value_label, value);
      if (point != NULL && point->quality == VG_ACQ_Q_GOOD)
        {
          vg_ui_label_set_color_if_changed(g_acq_value_label,
                                           VG_COLOR_ACCENT);
        }
      else
        {
          vg_ui_label_set_color_if_changed(g_acq_value_label, VG_COLOR_TEXT);
        }
    }

  if (g_acq_quality_label != NULL)
    {
      if (point != NULL && point->quality != VG_ACQ_Q_NONE)
        {
          vg_ui_label_set_text_if_changed(g_acq_quality_label,
                                          vg_quality_label(point->quality));
        }
      else
        {
          vg_ui_label_set_text_if_changed(g_acq_quality_label, "");
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
      snprintf(value, sizeof(value), "正常");
      color = VG_COLOR_ACCENT;
    }

  if (g_alarm_title_label != NULL)
    {
      vg_ui_label_set_text_if_changed(g_alarm_title_label, "告警");
    }

  if (g_alarm_value_label != NULL)
    {
      vg_ui_label_set_text_if_changed(g_alarm_value_label, value);
      vg_ui_label_set_color_if_changed(g_alarm_value_label, color);
    }

  {
    unsigned int count = vg_alarm_history_count();
    unsigned int i;

    for (i = 0; i < VG_ALARM_HISTORY_MAX; i++)
      {
        if (g_alarm_hist_labels[i] == NULL)
          {
            continue;
          }

        if (i < count)
          {
            vg_ui_label_set_text_if_changed(g_alarm_hist_labels[i],
                                            vg_alarm_history_line(i));
          }
        else if (i == 0)
          {
            vg_ui_label_set_text_if_changed(g_alarm_hist_labels[i], "尚无事件");
          }
        else
          {
            vg_ui_label_set_text_if_changed(g_alarm_hist_labels[i], "");
          }
      }
  }
}

static void vg_create_header(FAR lv_obj_t *screen)
{
  FAR lv_obj_t *panel = vg_ui_panel(screen, 6, 4, 468, 36, VG_COLOR_RAISED);
  FAR lv_obj_t *label = vg_ui_label(panel, "VelaGuard", VG_COLOR_ACCENT,
                                    VG_FONT_TITLE);
  FAR lv_obj_t *settings;

  lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);

  g_net_status_label = vg_ui_label(panel, "无链路", VG_COLOR_MUTED,
                                   VG_FONT_CAPTION);
  lv_obj_align(g_net_status_label, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_long_mode(g_net_status_label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(g_net_status_label, 220);

  settings = vg_ui_icon_button(panel, &vg_img_setting, "设置",
                               vg_settings_clicked, NULL);
  lv_obj_align(settings, LV_ALIGN_RIGHT_MID, -6, 0);
}

static void vg_create_identity(FAR lv_obj_t *screen)
{
  FAR const struct vg_identity *identity = vg_identity_get();
  FAR lv_obj_t *panel = vg_ui_panel(screen, 6, 44, 468, 48, VG_COLOR_RAISED);
  FAR lv_obj_t *label;
  char text[48];
  uint32_t identity_color = vg_identity_valid() ?
                            VG_COLOR_TEXT : VG_COLOR_WARNING;
  uint32_t storage_color = vg_startup_storage_ready() ?
                           VG_COLOR_ACCENT : VG_COLOR_WARNING;
  FAR const char *mode =
    identity->build_mode == VG_BUILD_TEST ? "试验" : "量产";

  label = vg_ui_label(panel, "本地系统", VG_COLOR_MUTED, VG_FONT_CAPTION);
  lv_obj_set_pos(label, 10, 4);

  label = vg_ui_label(panel,
                      vg_identity_valid() ? identity->device_id :
                      "设备标识异常",
                      identity_color, VG_FONT_CAPTION);
  lv_obj_set_pos(label, 10, 26);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(label, 220);

  snprintf(text, sizeof(text), "%s  v%s", mode, identity->firmware_version);
  label = vg_ui_label(panel, text,
                      identity->build_mode == VG_BUILD_TEST ?
                      VG_COLOR_WARNING : VG_COLOR_MUTED,
                      VG_FONT_CAPTION);
  lv_obj_align(label, LV_ALIGN_TOP_RIGHT, -10, 4);

  snprintf(text, sizeof(text), "存储 %s",
           vg_startup_storage_ready() ? "就绪" : "降级");
  label = vg_ui_label(panel, text, storage_color, VG_FONT_CAPTION);
  lv_obj_align(label, LV_ALIGN_BOTTOM_RIGHT, -120, -4);

  vg_format_uptime(text, sizeof(text), vg_uptime_ms() / 1000u);
  g_uptime_label = vg_ui_label(panel, text, VG_COLOR_MUTED, VG_FONT_CAPTION);
  lv_obj_align(g_uptime_label, LV_ALIGN_BOTTOM_RIGHT, -10, -4);
}

static void vg_create_primary_states(FAR lv_obj_t *screen)
{
  FAR lv_obj_t *panel = vg_ui_panel(screen, 6, 94, 308, 174,
                                    VG_COLOR_SURFACE);
  FAR lv_obj_t *divider;
  FAR const struct vg_acq_point *point = vg_acq_primary();
  char title[40];
  char value[48];
  uint32_t value_color = VG_COLOR_TEXT;
  unsigned int i;

  snprintf(title, sizeof(title), "采集/%s", vg_backend_label());
  if (point != NULL && point->quality == VG_ACQ_Q_GOOD)
    {
      int tenths = (int)(point->value * 10.0f + 0.5f);

      value_color = VG_COLOR_ACCENT;
      snprintf(value, sizeof(value), "%d.%d %s",
               tenths / 10, tenths % 10,
               point->unit != NULL ? point->unit : "");
    }
  else
    {
      snprintf(value, sizeof(value), "未配置");
    }

  g_acq_title_label = vg_ui_label(panel, title, VG_COLOR_MUTED,
                                  VG_FONT_CAPTION);
  lv_obj_set_pos(g_acq_title_label, 10, 6);
  g_acq_value_label = vg_ui_label(panel, value, value_color, VG_FONT_VALUE);
  lv_obj_set_pos(g_acq_value_label, 10, 26);
  g_acq_quality_label = vg_ui_label(panel,
                                    point != NULL &&
                                    point->quality != VG_ACQ_Q_NONE ?
                                    vg_quality_label(point->quality) : "",
                                    VG_COLOR_MUTED, VG_FONT_CAPTION);
  lv_obj_set_pos(g_acq_quality_label, 10, 52);

  divider = lv_obj_create(panel);
  lv_obj_set_pos(divider, 10, 74);
  lv_obj_set_size(divider, 288, 1);
  lv_obj_set_style_bg_color(divider, lv_color_hex(0x2a3640), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(divider, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(divider, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(divider, 0, LV_PART_MAIN);
  lv_obj_remove_flag(divider, LV_OBJ_FLAG_SCROLLABLE);

  g_alarm_title_label = vg_ui_label(panel, "告警", VG_COLOR_MUTED,
                                    VG_FONT_CAPTION);
  lv_obj_set_pos(g_alarm_title_label, 10, 78);
  g_alarm_value_label = vg_ui_label(panel, "未启用", VG_COLOR_TEXT,
                                    VG_FONT_VALUE);
  lv_obj_set_pos(g_alarm_value_label, 10, 96);
  lv_label_set_long_mode(g_alarm_value_label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(g_alarm_value_label, 288);

  for (i = 0; i < VG_ALARM_HISTORY_MAX; i++)
    {
      int32_t y = 122 + (int32_t)i * 22;

      g_alarm_hist_labels[i] =
        vg_ui_label(panel, i == 0 ? "尚无事件" : "", VG_COLOR_MUTED,
                    VG_FONT_CAPTION);
      lv_obj_set_pos(g_alarm_hist_labels[i], 10, y);
      lv_label_set_long_mode(g_alarm_hist_labels[i], LV_LABEL_LONG_DOT);
      lv_obj_set_width(g_alarm_hist_labels[i], 288);
    }
}

static void vg_create_supporting_states(FAR lv_obj_t *screen)
{
  FAR const struct vg_startup_state *startup = vg_startup_get();
  FAR lv_obj_t *panel = vg_ui_panel(screen, 320, 94, 154, 174,
                                    VG_COLOR_SURFACE);
  FAR const char *time_value = startup->time_quality == VG_TIME_UNKNOWN ?
                               "未同步" : "已同步";

  vg_add_state(panel, "声音", "不可用", 10, 18, VG_COLOR_TEXT);
  vg_add_state(panel, "时刻", time_value, 10, 90, VG_COLOR_TEXT);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void vg_ui_home_clear_refs(void)
{
  unsigned int i;

  g_uptime_label = NULL;
  g_net_status_label = NULL;
  g_acq_title_label = NULL;
  g_acq_value_label = NULL;
  g_acq_quality_label = NULL;
  g_alarm_title_label = NULL;
  g_alarm_value_label = NULL;
  for (i = 0; i < VG_ALARM_HISTORY_MAX; i++)
    {
      g_alarm_hist_labels[i] = NULL;
    }
}

void vg_ui_home_refresh(void)
{
  char text[32];
  uint64_t uptime_seconds = vg_uptime_ms() / 1000u;

  if (g_uptime_label != NULL)
    {
      vg_format_uptime(text, sizeof(text), uptime_seconds);
      vg_ui_label_set_text_if_changed(g_uptime_label, text);
    }

  vg_refresh_network_summary();
  vg_refresh_acquisition_labels();
  vg_refresh_alarm_labels();
}

void vg_ui_home_build(FAR lv_obj_t *screen)
{
  vg_ui_home_clear_refs();
  vg_create_header(screen);
  vg_create_identity(screen);
  vg_create_primary_states(screen);
  vg_create_supporting_states(screen);
  vg_ui_home_refresh();
}

__attribute__((noinline))
void vg_ui_home_uptime_checkpoint(uint64_t uptime_seconds)
{
  g_velaguard_uptime_seconds = uptime_seconds;
}

void vg_ui_home_create(void)
{
  vg_ui_nav_init();
  vg_ui_nav_show_home();
}
