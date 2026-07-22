/****************************************************************************
 * app/velaguard_app/src/vg_ui_network.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>

#include <lvgl/lvgl.h>

#include "assets/vg_icons.h"
#include "vg_config.h"
#include "vg_network.h"
#include "vg_ui_nav.h"
#include "vg_ui_network.h"
#include "vg_ui_theme.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

enum vg_field_id
{
  VG_FIELD_IPV4 = 0,
  VG_FIELD_NETMASK,
  VG_FIELD_GATEWAY,
  VG_FIELD_DNS,
  VG_FIELD_COUNT
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR lv_obj_t *g_status_label;
static FAR lv_obj_t *g_carrier_label;
static FAR lv_obj_t *g_ipv4_label;
static FAR lv_obj_t *g_mode_dhcp_btn;
static FAR lv_obj_t *g_mode_static_btn;
static FAR lv_obj_t *g_field_labels[VG_FIELD_COUNT];
static FAR lv_obj_t *g_field_rows[VG_FIELD_COUNT];
static FAR lv_obj_t *g_wifi_row;
static FAR lv_obj_t *g_apply_btn;
static FAR lv_obj_t *g_feedback_label;
static FAR lv_obj_t *g_confirm_box;
static struct vg_network_config g_draft;
static bool g_draft_valid;
static bool g_static_visible = true;

/* Layout anchors (relative to scroll body).  DHCP collapses static rows. */

#define VG_NET_FIELD0_Y      146
#define VG_NET_FIELD_STEP    40
#define VG_NET_WIFI_STATIC_Y 310
#define VG_NET_WIFI_DHCP_Y   146
#define VG_NET_APPLY_GAP     12

static const FAR char *const g_field_titles[VG_FIELD_COUNT] =
{
  "IPv4",
  "掩码",
  "网关",
  "DNS"
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void vg_network_back_cb(FAR lv_event_t *event)
{
  UNUSED(event);
  g_draft_valid = false;
  vg_ui_nav_show_settings();
}

static void vg_set_mode_button_style(FAR lv_obj_t *btn, bool selected)
{
  FAR lv_obj_t *label;

  if (btn == NULL)
    {
      return;
    }

  lv_obj_set_style_bg_color(btn,
                            lv_color_hex(selected ? VG_COLOR_ACCENT :
                                         VG_COLOR_RAISED),
                            LV_PART_MAIN);

  /* Selected: dark text on accent. Unselected: light text on raised surface. */

  label = lv_obj_get_child(btn, 0);
  if (label != NULL)
    {
      lv_obj_set_style_text_color(label,
                                  lv_color_hex(selected ?
                                               VG_COLOR_BACKGROUND :
                                               VG_COLOR_TEXT),
                                  LV_PART_MAIN);
    }
}

static void vg_update_mode_buttons(void)
{
  bool dhcp = g_draft.mode == VG_NETWORK_MODE_DHCP;

  vg_set_mode_button_style(g_mode_dhcp_btn, dhcp);
  vg_set_mode_button_style(g_mode_static_btn, !dhcp);
}

static void vg_reflow_action_rows(void)
{
  int32_t wifi_y;
  int32_t apply_y;

  if (g_static_visible)
    {
      wifi_y = VG_NET_WIFI_STATIC_Y;
    }
  else
    {
      wifi_y = VG_NET_WIFI_DHCP_Y;
    }

  apply_y = wifi_y + 40 + VG_NET_APPLY_GAP;

  if (g_wifi_row != NULL)
    {
      lv_obj_set_pos(g_wifi_row, 0, wifi_y);
    }

  if (g_apply_btn != NULL)
    {
      lv_obj_set_pos(g_apply_btn, 0, apply_y);
    }

  if (g_feedback_label != NULL)
    {
      lv_obj_set_pos(g_feedback_label, 150, apply_y + 8);
    }
}

static void vg_set_fields_enabled(bool enabled)
{
  unsigned int i;

  g_static_visible = enabled;
  for (i = 0; i < VG_FIELD_COUNT; i++)
    {
      if (g_field_rows[i] == NULL)
        {
          continue;
        }

      if (enabled)
        {
          lv_obj_clear_flag(g_field_rows[i], LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(g_field_rows[i], LV_OBJ_FLAG_CLICKABLE);
          lv_obj_set_style_opa(g_field_rows[i], LV_OPA_COVER, LV_PART_MAIN);
          lv_obj_set_pos(g_field_rows[i], 0,
                         VG_NET_FIELD0_Y + (int32_t)i * VG_NET_FIELD_STEP);
        }
      else
        {
          lv_obj_add_flag(g_field_rows[i], LV_OBJ_FLAG_HIDDEN);
          lv_obj_remove_flag(g_field_rows[i], LV_OBJ_FLAG_CLICKABLE);
        }
    }

  /* Collapse blank static-field space in DHCP mode. */

  vg_reflow_action_rows();
}

static void vg_refresh_field_labels(void)
{
  FAR const char *values[VG_FIELD_COUNT] =
  {
    g_draft.ipv4,
    g_draft.netmask,
    g_draft.gateway,
    g_draft.dns
  };
  unsigned int i;

  for (i = 0; i < VG_FIELD_COUNT; i++)
    {
      if (g_field_labels[i] != NULL)
        {
          FAR const char *value = values[i][0] != '\0' ? values[i] : "-";

          vg_ui_label_set_text_if_changed(g_field_labels[i], value);
        }
    }
}

static void vg_field_confirmed(FAR const char *text, FAR void *user_data)
{
  uintptr_t field = (uintptr_t)user_data;

  switch (field)
    {
      case VG_FIELD_IPV4:
        strlcpy(g_draft.ipv4, text, sizeof(g_draft.ipv4));
        break;

      case VG_FIELD_NETMASK:
        strlcpy(g_draft.netmask, text, sizeof(g_draft.netmask));
        break;

      case VG_FIELD_GATEWAY:
        strlcpy(g_draft.gateway, text, sizeof(g_draft.gateway));
        break;

      case VG_FIELD_DNS:
        strlcpy(g_draft.dns, text, sizeof(g_draft.dns));
        break;

      default:
        break;
    }

  g_draft_valid = true;
  g_draft.mode = VG_NETWORK_MODE_STATIC;
  g_draft.version = VG_NETWORK_CFG_VERSION;
}

static void vg_field_clicked(FAR lv_event_t *event)
{
  struct vg_network_status status;
  uintptr_t field = (uintptr_t)lv_event_get_user_data(event);
  FAR const char *value = "";
  FAR const char *title = g_field_titles[field];

  if (g_draft.mode != VG_NETWORK_MODE_STATIC)
    {
      return;
    }

  vg_network_get_status(&status);
  if (status.state == VG_NETWORK_APPLYING)
    {
      return;
    }

  switch (field)
    {
      case VG_FIELD_IPV4:
        value = g_draft.ipv4;
        break;

      case VG_FIELD_NETMASK:
        value = g_draft.netmask;
        break;

      case VG_FIELD_GATEWAY:
        value = g_draft.gateway;
        break;

      case VG_FIELD_DNS:
        value = g_draft.dns;
        break;

      default:
        return;
    }

  vg_ui_nav_show_ipv4_editor(title, value, vg_field_confirmed,
                             (FAR void *)field);
}

static void vg_mode_dhcp_cb(FAR lv_event_t *event)
{
  struct vg_network_status status;

  UNUSED(event);
  vg_network_get_status(&status);
  if (status.state == VG_NETWORK_APPLYING)
    {
      return;
    }

  g_draft.mode = VG_NETWORK_MODE_DHCP;
  vg_update_mode_buttons();
  vg_set_fields_enabled(false);
  if (g_feedback_label != NULL)
    {
      lv_label_set_text(g_feedback_label, "DHCP 模式将自动获取地址");
      lv_obj_set_style_text_color(g_feedback_label,
                                  lv_color_hex(VG_COLOR_MUTED),
                                  LV_PART_MAIN);
    }
}

static void vg_mode_static_cb(FAR lv_event_t *event)
{
  struct vg_network_status status;

  UNUSED(event);
  vg_network_get_status(&status);
  if (status.state == VG_NETWORK_APPLYING)
    {
      return;
    }

  g_draft.mode = VG_NETWORK_MODE_STATIC;
  vg_update_mode_buttons();
  vg_set_fields_enabled(true);
  vg_refresh_field_labels();
  if (g_feedback_label != NULL)
    {
      lv_label_set_text(g_feedback_label, "静态模式需填写完整地址");
      lv_obj_set_style_text_color(g_feedback_label,
                                  lv_color_hex(VG_COLOR_MUTED),
                                  LV_PART_MAIN);
    }
}

static void vg_close_confirm(void)
{
  if (g_confirm_box != NULL)
    {
      lv_obj_delete(g_confirm_box);
      g_confirm_box = NULL;
    }
}

static void vg_confirm_cancel_cb(FAR lv_event_t *event)
{
  UNUSED(event);
  vg_close_confirm();
}

static void vg_confirm_ok_cb(FAR lv_event_t *event)
{
  int result;

  UNUSED(event);
  vg_close_confirm();

  result = vg_network_request_apply(&g_draft);
  if (result == 0)
    {
      /* Keep draft until apply finishes; refresh will show runtime result. */
    }

  if (g_feedback_label != NULL)
    {
      if (result == 0)
        {
          lv_label_set_text(g_feedback_label, "正在应用");
          lv_obj_set_style_text_color(g_feedback_label,
                                      lv_color_hex(VG_COLOR_WARNING),
                                      LV_PART_MAIN);
        }
      else if (result == -EINVAL)
        {
          lv_label_set_text(g_feedback_label, "配置无效,未保存");
          lv_obj_set_style_text_color(g_feedback_label,
                                      lv_color_hex(VG_COLOR_ALARM),
                                      LV_PART_MAIN);
        }
      else if (result == -EBUSY)
        {
          lv_label_set_text(g_feedback_label, "正在应用,请稍后");
          lv_obj_set_style_text_color(g_feedback_label,
                                      lv_color_hex(VG_COLOR_WARNING),
                                      LV_PART_MAIN);
        }
      else
        {
          lv_label_set_text(g_feedback_label, "提交失败");
          lv_obj_set_style_text_color(g_feedback_label,
                                      lv_color_hex(VG_COLOR_ALARM),
                                      LV_PART_MAIN);
        }
    }
}

static void vg_show_confirm(void)
{
  FAR lv_obj_t *box;
  FAR lv_obj_t *title;
  FAR lv_obj_t *body;
  FAR lv_obj_t *cancel;
  FAR lv_obj_t *ok;
  char text[160];

  vg_close_confirm();

  box = lv_obj_create(lv_screen_active());
  g_confirm_box = box;
  lv_obj_set_size(box, 360, 170);
  lv_obj_center(box);
  lv_obj_set_style_bg_color(box, lv_color_hex(VG_COLOR_RAISED),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(box, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(box, lv_color_hex(VG_COLOR_ACCENT),
                                LV_PART_MAIN);
  lv_obj_set_style_radius(box, VG_PANEL_RADIUS, LV_PART_MAIN);
  lv_obj_set_style_pad_all(box, 12, LV_PART_MAIN);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

  title = vg_ui_label(box, "确认应用", VG_COLOR_TEXT, VG_FONT_TITLE);
  lv_obj_set_pos(title, 0, 0);

  if (g_draft.mode == VG_NETWORK_MODE_DHCP)
    {
      snprintf(text, sizeof(text), "将切换为 DHCP 并自动获取地址");
    }
  else
    {
      snprintf(text, sizeof(text),
               "IP %s\n掩码 %s\n网关 %s\nDNS %s",
               g_draft.ipv4, g_draft.netmask, g_draft.gateway, g_draft.dns);
    }

  body = vg_ui_label(box, text, VG_COLOR_MUTED, VG_FONT_CAPTION);
  lv_obj_set_pos(body, 0, 28);
  lv_obj_set_width(body, 330);

  cancel = vg_ui_button(box, "取消");
  lv_obj_set_size(cancel, 100, 32);
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_color(cancel, lv_color_hex(VG_COLOR_SURFACE),
                            LV_PART_MAIN);
  {
    FAR lv_obj_t *cancel_label = lv_obj_get_child(cancel, 0);

    if (cancel_label != NULL)
      {
        lv_obj_set_style_text_color(cancel_label,
                                    lv_color_hex(VG_COLOR_TEXT),
                                    LV_PART_MAIN);
      }
  }

  lv_obj_add_event_cb(cancel, vg_confirm_cancel_cb, LV_EVENT_CLICKED, NULL);

  ok = vg_ui_button(box, "确认");
  lv_obj_set_size(ok, 100, 32);
  lv_obj_align(ok, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(ok, vg_confirm_ok_cb, LV_EVENT_CLICKED, NULL);
}

static void vg_apply_clicked(FAR lv_event_t *event)
{
  struct vg_network_status status;
  int result;

  UNUSED(event);
  vg_network_get_status(&status);
  if (status.state == VG_NETWORK_APPLYING)
    {
      return;
    }

  g_draft.version = VG_NETWORK_CFG_VERSION;
  result = vg_config_network_validate(&g_draft);
  if (result < 0)
    {
      if (g_feedback_label != NULL)
        {
          lv_label_set_text(g_feedback_label, "配置无效,请检查字段");
          lv_obj_set_style_text_color(g_feedback_label,
                                      lv_color_hex(VG_COLOR_ALARM),
                                      LV_PART_MAIN);
        }

      return;
    }

  vg_show_confirm();
}

static FAR lv_obj_t *vg_make_field_row(FAR lv_obj_t *parent, int32_t y,
                                       enum vg_field_id field)
{
  FAR lv_obj_t *row = vg_ui_panel(parent, 0, y, 444, 34, VG_COLOR_RAISED);
  FAR lv_obj_t *title = vg_ui_label(row, g_field_titles[field],
                                    VG_COLOR_MUTED, VG_FONT_CAPTION);
  FAR lv_obj_t *value;

  lv_obj_set_style_radius(row, 0, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);
  value = vg_ui_label(row, "-", VG_COLOR_TEXT, VG_FONT_BODY);
  lv_obj_align(value, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_label_set_long_mode(value, LV_LABEL_LONG_DOT);
  lv_obj_set_width(value, 280);

  g_field_labels[field] = value;
  g_field_rows[field] = row;
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(row, lv_color_hex(0x24313c),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_add_event_cb(row, vg_field_clicked, LV_EVENT_CLICKED,
                      (FAR void *)(uintptr_t)field);
  return row;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void vg_ui_network_clear_refs(void)
{
  unsigned int i;

  g_status_label = NULL;
  g_carrier_label = NULL;
  g_ipv4_label = NULL;
  g_mode_dhcp_btn = NULL;
  g_mode_static_btn = NULL;
  g_wifi_row = NULL;
  g_apply_btn = NULL;
  g_feedback_label = NULL;
  g_confirm_box = NULL;
  for (i = 0; i < VG_FIELD_COUNT; i++)
    {
      g_field_labels[i] = NULL;
      g_field_rows[i] = NULL;
    }
}

void vg_ui_network_refresh(void)
{
  struct vg_network_status status;
  char text[80];
  bool applying;

  if (g_status_label == NULL)
    {
      return;
    }

  vg_network_get_status(&status);
  applying = status.state == VG_NETWORK_APPLYING;

  snprintf(text, sizeof(text), "状态  %s",
           status.message[0] != '\0' ? status.message : "-");
  vg_ui_label_set_text_if_changed(g_status_label, text);

  if (g_carrier_label != NULL)
    {
      vg_ui_label_set_text_if_changed(g_carrier_label,
                                      status.carrier ? "链路  已连接" :
                                      "链路  无链路");
      vg_ui_label_set_color_if_changed(g_carrier_label,
                                       status.carrier ? VG_COLOR_ACCENT :
                                       VG_COLOR_MUTED);
    }

  if (g_ipv4_label != NULL)
    {
      snprintf(text, sizeof(text), "地址  %s",
               status.ipv4[0] != '\0' ? status.ipv4 : "-");
      vg_ui_label_set_text_if_changed(g_ipv4_label, text);
    }

  if (g_apply_btn != NULL)
    {
      if (applying)
        {
          lv_obj_add_state(g_apply_btn, LV_STATE_DISABLED);
        }
      else
        {
          lv_obj_remove_state(g_apply_btn, LV_STATE_DISABLED);
        }
    }

  if (g_mode_dhcp_btn != NULL)
    {
      if (applying)
        {
          lv_obj_add_state(g_mode_dhcp_btn, LV_STATE_DISABLED);
          lv_obj_add_state(g_mode_static_btn, LV_STATE_DISABLED);
        }
      else
        {
          lv_obj_remove_state(g_mode_dhcp_btn, LV_STATE_DISABLED);
          lv_obj_remove_state(g_mode_static_btn, LV_STATE_DISABLED);
        }
    }

  if (g_feedback_label != NULL && !applying)
    {
      if (status.feedback[0] != '\0')
        {
          bool success = strncmp(status.feedback, "应用成功",
                                 strlen("应用成功")) == 0;

          vg_ui_label_set_text_if_changed(g_feedback_label, status.feedback);
          vg_ui_label_set_color_if_changed(g_feedback_label,
                                           success ? VG_COLOR_ACCENT :
                                           VG_COLOR_ALARM);
        }
    }
}

void vg_ui_network_build(FAR lv_obj_t *screen)
{
  FAR lv_obj_t *body;
  FAR lv_obj_t *status_panel;
  FAR lv_obj_t *eth_icon;
  FAR lv_obj_t *wifi_row;
  FAR lv_obj_t *wifi_icon;
  FAR lv_obj_t *label;
  unsigned int i;

  vg_ui_network_clear_refs();
  if (!g_draft_valid)
    {
      vg_network_get_config(&g_draft);
      g_draft.version = VG_NETWORK_CFG_VERSION;
      g_draft_valid = true;
    }

  vg_ui_header_bar(screen, "网络设置", vg_network_back_cb, NULL);

  body = lv_obj_create(screen);
  lv_obj_set_pos(body, 6, 44);
  lv_obj_set_size(body, 468, 222);
  lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(body, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(body, 0, LV_PART_MAIN);
  lv_obj_set_scroll_dir(body, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);

  status_panel = vg_ui_panel(body, 0, 0, 468, 70, VG_COLOR_SURFACE);
  eth_icon = lv_image_create(status_panel);
  lv_image_set_src(eth_icon, &vg_img_ethernet);
  lv_obj_set_style_image_recolor(eth_icon, lv_color_hex(VG_COLOR_ACCENT),
                                 LV_PART_MAIN);
  lv_obj_set_style_image_recolor_opa(eth_icon, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_pos(eth_icon, 10, 22);

  g_status_label = vg_ui_label(status_panel, "状态  -", VG_COLOR_TEXT,
                               VG_FONT_BODY);
  lv_obj_set_pos(g_status_label, 44, 8);
  g_carrier_label = vg_ui_label(status_panel, "链路  -", VG_COLOR_MUTED,
                                VG_FONT_CAPTION);
  lv_obj_set_pos(g_carrier_label, 44, 30);
  g_ipv4_label = vg_ui_label(status_panel, "地址  -", VG_COLOR_MUTED,
                             VG_FONT_CAPTION);
  lv_obj_set_pos(g_ipv4_label, 44, 48);

  label = vg_ui_label(body, "模式", VG_COLOR_MUTED, VG_FONT_CAPTION);
  lv_obj_set_pos(label, 0, 80);

  g_mode_dhcp_btn = vg_ui_button(body, "DHCP");
  lv_obj_set_size(g_mode_dhcp_btn, 100, 32);
  lv_obj_set_pos(g_mode_dhcp_btn, 0, 102);
  lv_obj_add_event_cb(g_mode_dhcp_btn, vg_mode_dhcp_cb, LV_EVENT_CLICKED,
                      NULL);

  g_mode_static_btn = vg_ui_button(body, "静态");
  lv_obj_set_size(g_mode_static_btn, 100, 32);
  lv_obj_set_pos(g_mode_static_btn, 112, 102);
  lv_obj_add_event_cb(g_mode_static_btn, vg_mode_static_cb, LV_EVENT_CLICKED,
                      NULL);

  for (i = 0; i < VG_FIELD_COUNT; i++)
    {
      vg_make_field_row(body, 146 + (int32_t)i * 40, (enum vg_field_id)i);
    }

  wifi_row = vg_ui_panel(body, 0, VG_NET_WIFI_STATIC_Y, 468, 40,
                         VG_COLOR_SURFACE);
  g_wifi_row = wifi_row;
  wifi_icon = lv_image_create(wifi_row);
  lv_image_set_src(wifi_icon, &vg_img_wifi);
  lv_obj_set_style_image_recolor(wifi_icon, lv_color_hex(VG_COLOR_MUTED),
                                 LV_PART_MAIN);
  lv_obj_set_style_image_recolor_opa(wifi_icon, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_align(wifi_icon, LV_ALIGN_LEFT_MID, 10, 0);
  label = vg_ui_label(wifi_row, "Wi-Fi", VG_COLOR_MUTED, VG_FONT_BODY);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 44, 0);
  label = vg_ui_label(wifi_row, "暂未开放", VG_COLOR_MUTED, VG_FONT_CAPTION);
  lv_obj_align(label, LV_ALIGN_RIGHT_MID, -12, 0);
  lv_obj_remove_flag(wifi_row, LV_OBJ_FLAG_CLICKABLE);

  g_apply_btn = vg_ui_button(body, "保存并应用");
  lv_obj_set_size(g_apply_btn, 140, 36);
  lv_obj_add_event_cb(g_apply_btn, vg_apply_clicked, LV_EVENT_CLICKED, NULL);

  g_feedback_label = vg_ui_label(body, "", VG_COLOR_MUTED, VG_FONT_CAPTION);
  lv_obj_set_width(g_feedback_label, 300);
  lv_label_set_long_mode(g_feedback_label, LV_LABEL_LONG_DOT);

  vg_update_mode_buttons();
  vg_set_fields_enabled(g_draft.mode == VG_NETWORK_MODE_STATIC);
  vg_refresh_field_labels();
  vg_ui_network_refresh();
}
