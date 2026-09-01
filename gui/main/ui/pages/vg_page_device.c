#include "vg_pages.h"
#include "widgets/vg_widgets.h"
#include "theme/vg_theme.h"
#include "model/vg_model.h"
#include "vg_display.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    lv_obj_t * root;
    lv_obj_t * title;
    lv_obj_t * value;
    lv_obj_t * chip;
    lv_obj_t * rows[8];
} device_ctx_t;

static device_ctx_t s_dev;

static void fmt_f1(char * buf, size_t n, float v)
{
    int vi = (int)v;
    int vf = (int)((v - (float)vi) * 10.0f);
    if(vf < 0) vf = -vf;
    lv_snprintf(buf, n, "%d.%d", vi, vf);
}

static void refresh_device(void * user)
{
    const vg_sensor_t * s;
    char buf[48];
    char a[16], b[16];
    LV_UNUSED(user);
    if(vg_nav_current() != VG_PAGE_DEVICE) return;
    if(s_dev.root == NULL || !lv_obj_is_valid(s_dev.root)) return;
    s = vg_model_get_selected_sensor();
    if(s == NULL) return;

    lv_label_set_text(s_dev.title, s->name);
    if(s->online) {
        fmt_f1(a, sizeof(a), s->value);
        lv_snprintf(buf, sizeof(buf), "%s %s", a, s->unit);
    }
    else {
        lv_snprintf(buf, sizeof(buf), "-- %s", s->unit);
    }
    lv_label_set_text(s_dev.value, buf);
    lv_obj_set_style_text_color(s_dev.value, vg_color_severity(s->severity), 0);
    vg_status_chip_set(s_dev.chip, vg_severity_label_zh(s->severity), s->severity);

    lv_snprintf(buf, sizeof(buf), "%d%%", s->quality_pct);
    vg_metric_row_set_value(s_dev.rows[0], buf);

    lv_snprintf(buf, sizeof(buf), "%d ms", s->period_ms);
    vg_metric_row_set_value(s_dev.rows[1], buf);

    lv_snprintf(buf, sizeof(buf), "0x%04X", (unsigned)s->reg_addr);
    vg_metric_row_set_value(s_dev.rows[2], buf);

    fmt_f1(a, sizeof(a), s->thr_warn);
    fmt_f1(b, sizeof(b), s->thr_crit);
    lv_snprintf(buf, sizeof(buf), "%s / %s %s", a, b, s->unit);
    vg_metric_row_set_value(s_dev.rows[3], buf);

    fmt_f1(a, sizeof(a), s->thr_low);
    lv_snprintf(buf, sizeof(buf), "%s %s", a, s->unit);
    vg_metric_row_set_value(s_dev.rows[4], buf);

    lv_snprintf(buf, sizeof(buf), "%d s 前", s->age_sec);
    vg_metric_row_set_value(s_dev.rows[5], buf);

    vg_metric_row_set_value(s_dev.rows[6], s->online ? "在线" : "离线");
    vg_metric_row_set_value(s_dev.rows[7], s->id);
}

static void on_trend(lv_event_t * e)
{
    LV_UNUSED(e);
    vg_nav_goto(VG_PAGE_TREND, NULL);
}

static void on_device_delete(lv_event_t * e)
{
    LV_UNUSED(e);
    vg_model_off_change(refresh_device, NULL);
    memset(&s_dev, 0, sizeof(s_dev));
}

void vg_page_device_create(lv_obj_t * parent, const void * args)
{
    lv_obj_t * head;
    lv_obj_t * body;
    lv_obj_t * btn;
    lv_obj_t * lab;
    LV_UNUSED(args);
    memset(&s_dev, 0, sizeof(s_dev));
    s_dev.root = parent;
    lv_obj_add_event_cb(parent, on_device_delete, LV_EVENT_DELETE, NULL);

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    /* Tight vertical budget on 244px content: head + body + CTA must fit */
    lv_obj_set_style_pad_row(parent, 2, 0);
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    head = lv_obj_create(parent);
    vg_style_apply_card(head);
    lv_obj_set_width(head, lv_pct(100));
    lv_obj_set_height(head, 40);
    lv_obj_set_style_pad_all(head, 4, 0);
    lv_obj_set_style_min_height(head, 40, 0);
    lv_obj_set_style_max_height(head, 40, 0);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * left = lv_obj_create(head);
    lv_obj_remove_flag(left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(left, 0, 0);

    s_dev.title = lv_label_create(left);
    vg_style_apply_label(s_dev.title, false);

    s_dev.value = lv_label_create(left);
    lv_obj_set_style_text_font(s_dev.value, vg_font_metric(), 0);
    lv_obj_set_style_text_color(s_dev.value, vg_color_text(), 0);
    lv_obj_set_style_pad_top(s_dev.value, 0, 0);

    s_dev.chip = vg_status_chip_create(head, "正常", VG_SEV_OK);

    body = lv_obj_create(parent);
    vg_style_apply_card(body);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_min_height(body, 0, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, 0, 0);
    lv_obj_set_style_pad_ver(body, 1, 0);
    lv_obj_set_style_pad_hor(body, 6, 0);
    lv_obj_set_style_pad_bottom(body, 2, 0);
    /* Keep scroll so last fields never hide under the CTA */
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);

    s_dev.rows[0] = vg_metric_row_create(body, "通信质量", "--");
    s_dev.rows[1] = vg_metric_row_create(body, "采集周期", "--");
    s_dev.rows[2] = vg_metric_row_create(body, "寄存器", "--");
    s_dev.rows[3] = vg_metric_row_create(body, "预警/严重阈值", "--");
    s_dev.rows[4] = vg_metric_row_create(body, "下限阈值", "--");
    s_dev.rows[5] = vg_metric_row_create(body, "最近采样", "--");
    s_dev.rows[6] = vg_metric_row_create(body, "连接状态", "--");
    s_dev.rows[7] = vg_metric_row_create(body, "设备 ID", "--");

    btn = lv_button_create(parent);
    vg_style_apply_btn(btn, true);
    lv_obj_set_size(btn, lv_pct(100), VG_MIN_TOUCH_H);
    lv_obj_set_style_min_height(btn, VG_MIN_TOUCH_H, 0);
    lv_obj_set_style_max_height(btn, VG_MIN_TOUCH_H, 0);
    lv_obj_set_style_pad_ver(btn, 2, 0);
    lab = lv_label_create(btn);
    lv_label_set_text(lab, "查看实时趋势");
    lv_obj_center(lab);
    lv_obj_add_event_cb(btn, on_trend, LV_EVENT_CLICKED, NULL);

    vg_model_on_change(refresh_device, NULL);
    refresh_device(NULL);
}
