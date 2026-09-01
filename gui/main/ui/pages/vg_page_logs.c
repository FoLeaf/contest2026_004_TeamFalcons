#include "vg_pages.h"
#include "theme/vg_theme.h"
#include "model/vg_model.h"
#include "vg_display.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    lv_obj_t * root;
    lv_obj_t * list;
} logs_ctx_t;

static logs_ctx_t s_logs_ui;

static void clear_list_children(lv_obj_t * list)
{
    if(list == NULL) return;
    lv_obj_clean(list);
}

static lv_obj_t * make_log_row(lv_obj_t * parent, const vg_log_entry_t * e)
{
    char buf[128];
    lv_obj_t * row;
    lv_obj_t * lab;

    row = lv_obj_create(parent);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(row, vg_color_border(), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_pad_ver(row, 3, 0);
    lv_obj_set_style_pad_hor(row, 2, 0);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);

    lab = lv_label_create(row);
    lv_label_set_long_mode(lab, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lab, lv_pct(100));
    lv_snprintf(buf, sizeof(buf), "[%s] [%s] %s",
                e->time, vg_log_type_label_zh(e->type), e->text);
    lv_label_set_text(lab, buf);
    lv_obj_set_style_text_font(lab, vg_font_ui(), 0);
    lv_obj_set_style_text_color(lab, vg_color_severity(e->severity), 0);
    return row;
}

static void rebuild_logs(void)
{
    const vg_log_entry_t * logs;
    uint16_t n = 0;
    int i;

    if(s_logs_ui.list == NULL || !lv_obj_is_valid(s_logs_ui.list)) return;
    logs = vg_model_get_logs(&n);
    clear_list_children(s_logs_ui.list);

    if(logs == NULL || n == 0) {
        lv_obj_t * empty = lv_label_create(s_logs_ui.list);
        lv_label_set_text(empty, "暂无日志");
        vg_style_apply_label(empty, true);
        return;
    }

    /* Newest first for operator scan */
    for(i = (int)n - 1; i >= 0; i--) {
        make_log_row(s_logs_ui.list, &logs[i]);
    }
}

static void refresh_logs(void * user)
{
    LV_UNUSED(user);
    if(vg_nav_current() != VG_PAGE_LOGS) return;
    if(s_logs_ui.root == NULL || !lv_obj_is_valid(s_logs_ui.root)) return;
    rebuild_logs();
}

static void on_logs_delete(lv_event_t * e)
{
    LV_UNUSED(e);
    vg_model_off_change(refresh_logs, NULL);
    memset(&s_logs_ui, 0, sizeof(s_logs_ui));
}

void vg_page_logs_create(lv_obj_t * parent, const void * args)
{
    LV_UNUSED(args);
    memset(&s_logs_ui, 0, sizeof(s_logs_ui));
    s_logs_ui.root = parent;
    lv_obj_add_event_cb(parent, on_logs_delete, LV_EVENT_DELETE, NULL);

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, VG_GAP, 0);
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    s_logs_ui.list = lv_obj_create(parent);
    vg_style_apply_card(s_logs_ui.list);
    lv_obj_set_width(s_logs_ui.list, lv_pct(100));
    lv_obj_set_flex_grow(s_logs_ui.list, 1);
    lv_obj_set_style_min_height(s_logs_ui.list, 0, 0);
    lv_obj_set_flex_flow(s_logs_ui.list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_logs_ui.list, 0, 0);
    lv_obj_set_style_pad_all(s_logs_ui.list, 4, 0);
    lv_obj_add_flag(s_logs_ui.list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_logs_ui.list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_logs_ui.list, LV_SCROLLBAR_MODE_AUTO);

    vg_model_on_change(refresh_logs, NULL);
    rebuild_logs();
}
