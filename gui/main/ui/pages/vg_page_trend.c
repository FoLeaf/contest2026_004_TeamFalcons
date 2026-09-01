#include "vg_pages.h"
#include "theme/vg_theme.h"
#include "model/vg_model.h"
#include "vg_display.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define VG_TREND_MIN_COUNT 60u   /* 1 minute @1s */
#define VG_TREND_SERIES_N 4u     /* ser + thr_warn + thr_crit + anomaly */

typedef struct {
    lv_obj_t * root;
    lv_obj_t * chart;
    lv_chart_series_t * ser;
    lv_chart_series_t * thr_warn;
    lv_chart_series_t * thr_crit;
    lv_chart_series_t * anomaly;
    lv_obj_t * value_lab;
    lv_obj_t * meta_lab;
    lv_obj_t * win_btn[2];
    lv_obj_t * win_lab[2];
    int window_min;               /* 1 or 5 */
} trend_ctx_t;

static trend_ctx_t s_trend;

static void fmt_f1(char * buf, size_t n, float v)
{
    int vi = (int)v;
    int vf = (int)((v - (float)vi) * 10.0f);
    if(vf < 0) vf = -vf;
    lv_snprintf(buf, n, "%d.%d", vi, vf);
}

/*
 * LVGL 9 chart styles line width / point size per chart, not per series.
 * lv_chart.c draw_series() iterates the series list tail→head and assigns
 * base.id1 = (series_count - 1) down to 0 in draw order. Series are added
 * at the list tail, so the anomaly series (added last, drawn FIRST) carries
 * id1 == VG_TREND_SERIES_N - 1, and the main data series (added first,
 * drawn last) carries id1 == 0. Hide the anomaly series' connecting line
 * and the dot tasks of every other series so only red over-threshold dots
 * remain (manual 6.5).
 */
static void chart_draw_task_cb(lv_event_t * e)
{
    lv_draw_task_t * task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t * base = (lv_draw_dsc_base_t *)task->draw_dsc;

    if(task->type == LV_DRAW_TASK_TYPE_LINE && base->part == LV_PART_ITEMS) {
        if(base->id1 == (uint32_t)(VG_TREND_SERIES_N - 1)) {
            lv_draw_line_dsc_t * dsc = (lv_draw_line_dsc_t *)task->draw_dsc;
            dsc->width = 0;
            dsc->opa = LV_OPA_TRANSP;
        }
    }
    else if(task->type == LV_DRAW_TASK_TYPE_FILL && base->part == LV_PART_INDICATOR) {
        if(base->id1 != (uint32_t)(VG_TREND_SERIES_N - 1)) {
            lv_draw_rect_dsc_t * dsc = (lv_draw_rect_dsc_t *)task->draw_dsc;
            dsc->bg_opa = LV_OPA_TRANSP;
        }
    }
}

static uint32_t window_count(void)
{
    return s_trend.window_min == 1 ? VG_TREND_MIN_COUNT : (uint32_t)VG_HISTORY_LEN;
}

static uint32_t window_offset(uint32_t count)
{
    if(s_trend.window_min != 1) return 0;
    if((uint32_t)VG_HISTORY_LEN < count) return 0;
    return (uint32_t)VG_HISTORY_LEN - count;
}

static void refresh_trend(void * user)
{
    const vg_sensor_t * s;
    char buf[64];
    char a[16], b[16], c[16];
    uint32_t i;
    uint32_t count;
    uint32_t offset;
    LV_UNUSED(user);
    if(vg_nav_current() != VG_PAGE_TREND) return;
    if(s_trend.root == NULL || !lv_obj_is_valid(s_trend.root)) return;
    s = vg_model_get_selected_sensor();
    if(s == NULL || s_trend.chart == NULL) return;

    if(s->online) {
        fmt_f1(a, sizeof(a), s->value);
        lv_snprintf(buf, sizeof(buf), "%s %s", a, s->unit);
    }
    else {
        lv_snprintf(buf, sizeof(buf), "-- %s", s->unit);
    }
    lv_label_set_text(s_trend.value_lab, buf);
    lv_obj_set_style_text_color(s_trend.value_lab, vg_color_severity(s->severity), 0);

    fmt_f1(b, sizeof(b), s->thr_warn);
    fmt_f1(c, sizeof(c), s->thr_crit);
    lv_snprintf(buf, sizeof(buf), "阈值 预警%s / 严重%s · 最近 %d 分钟",
                b, c, s_trend.window_min);
    lv_label_set_text(s_trend.meta_lab, buf);

    {
        int32_t min_v = (int32_t)((s->thr_low - 5.0f) * 10.0f);
        int32_t max_v = (int32_t)((s->thr_crit + 10.0f) * 10.0f);
        if(max_v <= min_v) max_v = min_v + 200;
        lv_chart_set_range(s_trend.chart, LV_CHART_AXIS_PRIMARY_Y, min_v, max_v);
    }

    count = window_count();
    offset = window_offset(count);
    if(offset + count > (uint32_t)s->history_len) {
        offset = 0;
        count = (uint32_t)s->history_len;
    }
    lv_chart_set_point_count(s_trend.chart, count);
    for(i = 0; i < count; i++) {
        float v = s->history[offset + i];
        lv_chart_set_value_by_id(s_trend.chart, s_trend.ser, i, (int32_t)(v * 10.0f));
        lv_chart_set_value_by_id(s_trend.chart, s_trend.thr_warn, i, (int32_t)(s->thr_warn * 10.0f));
        lv_chart_set_value_by_id(s_trend.chart, s_trend.thr_crit, i, (int32_t)(s->thr_crit * 10.0f));
        /* red marker above warn threshold, otherwise no point */
        lv_chart_set_value_by_id(s_trend.chart, s_trend.anomaly, i,
                                 v > s->thr_warn ? (int32_t)(v * 10.0f) : LV_CHART_POINT_NONE);
    }
    lv_chart_refresh(s_trend.chart);
}

static void style_win_btn(lv_obj_t * btn, lv_obj_t * lab, bool on)
{
    if(on) {
        lv_obj_set_style_bg_color(btn, vg_color_accent(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn, vg_color_accent(), 0);
        lv_obj_set_style_text_color(lab, lv_color_hex(0xFFFFFF), 0);
    }
    else {
        lv_obj_set_style_bg_color(btn, vg_color_surface(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn, vg_color_border(), 0);
        lv_obj_set_style_text_color(lab, vg_color_text(), 0);
    }
}

static void refresh_win_btns(void)
{
    int i;
    for(i = 0; i < 2; i++) {
        if(s_trend.win_btn[i] && s_trend.win_lab[i])
            style_win_btn(s_trend.win_btn[i], s_trend.win_lab[i], s_trend.window_min == (i == 0 ? 1 : 5));
    }
}

static void on_window(lv_event_t * e)
{
    int min = (int)(intptr_t)lv_event_get_user_data(e);
    LV_UNUSED(e);
    if(s_trend.window_min == min) return;
    s_trend.window_min = min;
    refresh_win_btns();
    refresh_trend(NULL);
}

static void on_trend_delete(lv_event_t * e)
{
    LV_UNUSED(e);
    vg_model_off_change(refresh_trend, NULL);
    memset(&s_trend, 0, sizeof(s_trend));
}

void vg_page_trend_create(lv_obj_t * parent, const void * args)
{
    lv_obj_t * head;
    lv_obj_t * left;
    lv_obj_t * cur_lab;
    LV_UNUSED(args);
    memset(&s_trend, 0, sizeof(s_trend));
    s_trend.root = parent;
    lv_obj_add_event_cb(parent, on_trend_delete, LV_EVENT_DELETE, NULL);

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, VG_GAP, 0);
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    head = lv_obj_create(parent);
    lv_obj_remove_flag(head, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(head, 0, 0);
    lv_obj_set_style_pad_all(head, 0, 0);
    lv_obj_set_width(head, lv_pct(100));
    lv_obj_set_height(head, 36);
    lv_obj_set_style_min_height(head, 36, 0);
    lv_obj_set_style_max_height(head, 36, 0);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    left = lv_obj_create(head);
    lv_obj_remove_flag(left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(left, 6, 0);

    cur_lab = lv_label_create(left);
    lv_label_set_text(cur_lab, "当前");
    vg_style_apply_label(cur_lab, true);

    s_trend.value_lab = lv_label_create(left);
    /* Metric font (20) fits 36px head; avoid lg/24 clipping into chart */
    lv_obj_set_style_text_font(s_trend.value_lab, vg_font_metric(), 0);
    lv_obj_set_style_text_color(s_trend.value_lab, vg_color_text(), 0);
    lv_label_set_text(s_trend.value_lab, "--");

    /* 1min / 5min window toggles (manual 6.5) */
    {
        static const char * names[2] = {"1分钟", "5分钟"};
        int i;
        for(i = 0; i < 2; i++) {
            lv_obj_t * btn = lv_button_create(head);
            lv_obj_t * lab;
            lv_obj_set_size(btn, 54, VG_MIN_TOUCH_H);
            lv_obj_set_style_radius(btn, 3, 0);
            lv_obj_set_style_border_width(btn, 1, 0);
            lv_obj_set_style_shadow_width(btn, 0, 0);
            lab = lv_label_create(btn);
            lv_label_set_text(lab, names[i]);
            lv_obj_set_style_text_font(lab, vg_font_small(), 0);
            lv_obj_center(lab);
            lv_obj_add_event_cb(btn, on_window, LV_EVENT_CLICKED,
                                (void *)(intptr_t)(i == 0 ? 1 : 5));
            s_trend.win_btn[i] = btn;
            s_trend.win_lab[i] = lab;
        }
    }

    s_trend.meta_lab = lv_label_create(head);
    vg_style_apply_label(s_trend.meta_lab, true);
    lv_obj_set_style_text_font(s_trend.meta_lab, vg_font_small(), 0);
    lv_label_set_long_mode(s_trend.meta_lab, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(s_trend.meta_lab, 1);
    lv_obj_set_style_text_align(s_trend.meta_lab, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(s_trend.meta_lab, "");

    s_trend.chart = lv_chart_create(parent);
    lv_obj_set_width(s_trend.chart, lv_pct(100));
    lv_obj_set_flex_grow(s_trend.chart, 1);
    lv_obj_set_style_min_height(s_trend.chart, 0, 0);
    lv_obj_set_style_bg_color(s_trend.chart, vg_color_surface(), 0);
    lv_obj_set_style_bg_opa(s_trend.chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_trend.chart, vg_color_border(), 0);
    lv_obj_set_style_border_width(s_trend.chart, 1, 0);
    lv_obj_set_style_radius(s_trend.chart, VG_CARD_RADIUS, 0);
    lv_obj_set_style_pad_all(s_trend.chart, 4, 0);
    lv_obj_set_style_line_color(s_trend.chart, vg_color_border(), LV_PART_MAIN);
    lv_chart_set_type(s_trend.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_trend.chart, VG_HISTORY_LEN);
    lv_chart_set_div_line_count(s_trend.chart, 4, 6);
    /* 5px dots for the anomaly marker series; other series' dots are
     * hidden in chart_draw_task_cb. */
    lv_obj_set_style_size(s_trend.chart, 5, 0, LV_PART_INDICATOR);

    s_trend.ser = lv_chart_add_series(s_trend.chart, vg_color_accent(), LV_CHART_AXIS_PRIMARY_Y);
    s_trend.thr_warn = lv_chart_add_series(s_trend.chart, vg_color_warn(), LV_CHART_AXIS_PRIMARY_Y);
    s_trend.thr_crit = lv_chart_add_series(s_trend.chart, vg_color_crit(), LV_CHART_AXIS_PRIMARY_Y);
    /* must stay the 4th (last) series — see chart_draw_task_cb */
    s_trend.anomaly = lv_chart_add_series(s_trend.chart, vg_color_crit(), LV_CHART_AXIS_PRIMARY_Y);

    lv_obj_add_flag(s_trend.chart, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(s_trend.chart, chart_draw_task_cb, LV_EVENT_DRAW_TASK_ADDED, NULL);

    s_trend.window_min = 1;
    refresh_win_btns();
    vg_model_on_change(refresh_trend, NULL);
    refresh_trend(NULL);
}
