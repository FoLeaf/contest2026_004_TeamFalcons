#include "vg_pages.h"
#include "widgets/vg_widgets.h"
#include "theme/vg_theme.h"
#include "model/vg_model.h"
#include "vg_display.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    lv_obj_t * root;
    lv_obj_t * loading_box;
    lv_obj_t * error_box;
    lv_obj_t * error_lab;
    lv_obj_t * ok_box;
    lv_obj_t * summary;
    lv_obj_t * risk_badge;
    lv_obj_t * conf_lab;
    lv_obj_t * causes_title;
    lv_obj_t * causes[3];
    lv_obj_t * actions_title;
    lv_obj_t * actions[3];
    lv_obj_t * note_lab;
    lv_obj_t * btn_retry;
    lv_obj_t * btn_ack;
} diag_ctx_t;

static diag_ctx_t s_diag_ui;

static void show_only(lv_obj_t * show)
{
    if(s_diag_ui.loading_box) {
        if(show == s_diag_ui.loading_box) lv_obj_remove_flag(s_diag_ui.loading_box, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_diag_ui.loading_box, LV_OBJ_FLAG_HIDDEN);
    }
    if(s_diag_ui.error_box) {
        if(show == s_diag_ui.error_box) lv_obj_remove_flag(s_diag_ui.error_box, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_diag_ui.error_box, LV_OBJ_FLAG_HIDDEN);
    }
    if(s_diag_ui.ok_box) {
        if(show == s_diag_ui.ok_box) lv_obj_remove_flag(s_diag_ui.ok_box, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_diag_ui.ok_box, LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh_diagnosis(void * user)
{
    const vg_diagnosis_t * d;
    char buf[48];
    int i;
    LV_UNUSED(user);
    if(vg_nav_current() != VG_PAGE_DIAGNOSIS) return;
    if(s_diag_ui.root == NULL || !lv_obj_is_valid(s_diag_ui.root)) return;

    d = vg_model_get_diagnosis();
    if(d == NULL) return;

    switch(d->state) {
        case VG_DIAG_LOADING:
            show_only(s_diag_ui.loading_box);
            break;

        case VG_DIAG_ERROR:
            show_only(s_diag_ui.error_box);
            lv_label_set_text(s_diag_ui.error_lab,
                              d->error_msg[0] ? d->error_msg : "诊断失败");
            break;

        case VG_DIAG_OK:
            show_only(s_diag_ui.ok_box);
            lv_label_set_text(s_diag_ui.summary, d->summary);
            vg_risk_badge_set(s_diag_ui.risk_badge, d->risk);
            lv_snprintf(buf, sizeof(buf), "置信 %d%%", (int)d->confidence_pct);
            lv_label_set_text(s_diag_ui.conf_lab, buf);

            for(i = 0; i < 3; i++) {
                if(i < d->cause_n) {
                    char line[80];
                    lv_snprintf(line, sizeof(line), "· %s", d->causes[i]);
                    lv_label_set_text(s_diag_ui.causes[i], line);
                    lv_obj_remove_flag(s_diag_ui.causes[i], LV_OBJ_FLAG_HIDDEN);
                }
                else {
                    lv_obj_add_flag(s_diag_ui.causes[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
            for(i = 0; i < 3; i++) {
                if(i < d->action_n) {
                    char line[80];
                    lv_snprintf(line, sizeof(line), "· %s", d->actions[i]);
                    lv_label_set_text(s_diag_ui.actions[i], line);
                    lv_obj_remove_flag(s_diag_ui.actions[i], LV_OBJ_FLAG_HIDDEN);
                }
                else {
                    lv_obj_add_flag(s_diag_ui.actions[i], LV_OBJ_FLAG_HIDDEN);
                }
            }

            /* "确认已处理" enabled only while an unacked alarm is active */
            if(s_diag_ui.btn_ack) {
                const vg_alarm_t * a = vg_model_get_active_alarm();
                if(a && a->active && !a->acked)
                    lv_obj_remove_state(s_diag_ui.btn_ack, LV_STATE_DISABLED);
                else
                    lv_obj_add_state(s_diag_ui.btn_ack, LV_STATE_DISABLED);
            }
            break;

        case VG_DIAG_IDLE:
        default:
            show_only(s_diag_ui.loading_box);
            break;
    }
}

static void on_retry(lv_event_t * e)
{
    LV_UNUSED(e);
    vg_model_request_diagnosis();
}

/* Manual 6.7 OK-state actions: save report / play voice (mock) / ack alarm */
static void on_save_report(lv_event_t * e)
{
    LV_UNUSED(e);
    vg_model_append_log(VG_LOG_DIAG, VG_SEV_OK, "AI 诊断报告已保存");
    vg_shell_toast("报告已保存");
}

static void on_play_voice(lv_event_t * e)
{
    const vg_net_status_t * net = vg_model_get_net();
    LV_UNUSED(e);
    if(net->mimo_ok && vg_model_get_scenario() != VG_SCENARIO_OFFLINE) {
        vg_shell_toast("语音播报中…(mock)");
    }
    else {
        vg_shell_toast("TTS 不可用");
    }
}

static void on_ack_alarm(lv_event_t * e)
{
    LV_UNUSED(e);
    vg_model_ack_alarm();
    vg_shell_toast("已标记处理");
}

static void on_diag_delete(lv_event_t * e)
{
    LV_UNUSED(e);
    vg_model_off_change(refresh_diagnosis, NULL);
    memset(&s_diag_ui, 0, sizeof(s_diag_ui));
}

void vg_page_diagnosis_create(lv_obj_t * parent, const void * args)
{
    lv_obj_t * lab;
    lv_obj_t * head_row;
    int i;
    LV_UNUSED(args);
    memset(&s_diag_ui, 0, sizeof(s_diag_ui));
    s_diag_ui.root = parent;
    lv_obj_add_event_cb(parent, on_diag_delete, LV_EVENT_DELETE, NULL);

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, 2, 0);
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* LOADING */
    s_diag_ui.loading_box = lv_obj_create(parent);
    vg_style_apply_card(s_diag_ui.loading_box);
    lv_obj_set_width(s_diag_ui.loading_box, lv_pct(100));
    lv_obj_set_flex_grow(s_diag_ui.loading_box, 1);
    lv_obj_set_style_min_height(s_diag_ui.loading_box, 0, 0);
    lab = lv_label_create(s_diag_ui.loading_box);
    lv_label_set_text(lab, "诊断中...");
    vg_style_apply_label(lab, false);
    lv_obj_center(lab);

    /* ERROR */
    s_diag_ui.error_box = lv_obj_create(parent);
    vg_style_apply_card(s_diag_ui.error_box);
    lv_obj_set_width(s_diag_ui.error_box, lv_pct(100));
    lv_obj_set_flex_grow(s_diag_ui.error_box, 1);
    lv_obj_set_style_min_height(s_diag_ui.error_box, 0, 0);
    lv_obj_set_flex_flow(s_diag_ui.error_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_diag_ui.error_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_diag_ui.error_box, 8, 0);

    s_diag_ui.error_lab = lv_label_create(s_diag_ui.error_box);
    lv_label_set_text(s_diag_ui.error_lab, "诊断失败");
    lv_obj_set_style_text_color(s_diag_ui.error_lab, vg_color_warn(), 0);
    lv_obj_set_style_text_font(s_diag_ui.error_lab, vg_font_ui(), 0);

    s_diag_ui.btn_retry = lv_button_create(s_diag_ui.error_box);
    vg_style_apply_btn(s_diag_ui.btn_retry, true);
    lv_obj_set_size(s_diag_ui.btn_retry, 120, VG_MIN_TOUCH_H);
    lab = lv_label_create(s_diag_ui.btn_retry);
    lv_label_set_text(lab, "重试");
    lv_obj_center(lab);
    lv_obj_add_event_cb(s_diag_ui.btn_retry, on_retry, LV_EVENT_CLICKED, NULL);

    /* OK */
    s_diag_ui.ok_box = lv_obj_create(parent);
    vg_style_apply_card(s_diag_ui.ok_box);
    lv_obj_set_width(s_diag_ui.ok_box, lv_pct(100));
    lv_obj_set_flex_grow(s_diag_ui.ok_box, 1);
    lv_obj_set_style_min_height(s_diag_ui.ok_box, 0, 0);
    lv_obj_set_flex_flow(s_diag_ui.ok_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_diag_ui.ok_box, 2, 0);
    lv_obj_set_style_pad_all(s_diag_ui.ok_box, 6, 0);
    lv_obj_add_flag(s_diag_ui.ok_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_diag_ui.ok_box, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_diag_ui.ok_box, LV_SCROLLBAR_MODE_AUTO);

    s_diag_ui.summary = lv_label_create(s_diag_ui.ok_box);
    lv_label_set_long_mode(s_diag_ui.summary, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_diag_ui.summary, lv_pct(100));
    vg_style_apply_label(s_diag_ui.summary, false);
    lv_label_set_text(s_diag_ui.summary, "");

    head_row = lv_obj_create(s_diag_ui.ok_box);
    lv_obj_remove_flag(head_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(head_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(head_row, 0, 0);
    lv_obj_set_style_pad_all(head_row, 0, 0);
    lv_obj_set_width(head_row, lv_pct(100));
    lv_obj_set_height(head_row, 22);
    lv_obj_set_flex_flow(head_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(head_row, 8, 0);

    s_diag_ui.risk_badge = vg_risk_badge_create(head_row, "low");
    s_diag_ui.conf_lab = lv_label_create(head_row);
    vg_style_apply_label(s_diag_ui.conf_lab, true);
    lv_label_set_text(s_diag_ui.conf_lab, "置信 --");

    s_diag_ui.causes_title = lv_label_create(s_diag_ui.ok_box);
    lv_label_set_text(s_diag_ui.causes_title, "可能原因");
    vg_style_apply_label(s_diag_ui.causes_title, true);

    for(i = 0; i < 3; i++) {
        s_diag_ui.causes[i] = lv_label_create(s_diag_ui.ok_box);
        lv_label_set_long_mode(s_diag_ui.causes[i], LV_LABEL_LONG_DOT);
        lv_obj_set_width(s_diag_ui.causes[i], lv_pct(100));
        vg_style_apply_label(s_diag_ui.causes[i], false);
        lv_label_set_text(s_diag_ui.causes[i], "");
    }

    s_diag_ui.actions_title = lv_label_create(s_diag_ui.ok_box);
    lv_label_set_text(s_diag_ui.actions_title, "建议动作");
    vg_style_apply_label(s_diag_ui.actions_title, true);

    for(i = 0; i < 3; i++) {
        s_diag_ui.actions[i] = lv_label_create(s_diag_ui.ok_box);
        lv_label_set_long_mode(s_diag_ui.actions[i], LV_LABEL_LONG_DOT);
        lv_obj_set_width(s_diag_ui.actions[i], lv_pct(100));
        vg_style_apply_label(s_diag_ui.actions[i], false);
        lv_label_set_text(s_diag_ui.actions[i], "");
    }

    s_diag_ui.note_lab = lv_label_create(s_diag_ui.ok_box);
    lv_label_set_text(s_diag_ui.note_lab, "注: AI 只建议不执行");
    lv_obj_set_style_text_color(s_diag_ui.note_lab, vg_color_muted(), 0);
    lv_obj_set_style_text_font(s_diag_ui.note_lab, vg_font_small(), 0);

    /* Action row (manual 6.7): save report / play voice / ack handled */
    lv_obj_t * btn_row = lv_obj_create(s_diag_ui.ok_box);
    lv_obj_remove_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, VG_MIN_TOUCH_H);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_row, 4, 0);
    {
        static const char * names[3] = {"保存报告", "播放语音", "确认已处理"};
        static const lv_event_cb_t cbs[3] = {on_save_report, on_play_voice, on_ack_alarm};
        int k;
        for(k = 0; k < 3; k++) {
            lv_obj_t * btn = lv_button_create(btn_row);
            lv_obj_t * lab;
            lv_obj_set_size(btn, 140, VG_MIN_TOUCH_H);
            vg_style_apply_btn(btn, k == 0);
            lab = lv_label_create(btn);
            lv_label_set_text(lab, names[k]);
            lv_obj_set_style_text_font(lab, vg_font_ui(), 0);
            lv_obj_center(lab);
            lv_obj_add_event_cb(btn, cbs[k], LV_EVENT_CLICKED, NULL);
            if(k == 2) s_diag_ui.btn_ack = btn;
        }
    }

    show_only(s_diag_ui.loading_box);

    vg_model_on_change(refresh_diagnosis, NULL);
    /* Auto-start diagnosis when entering the page */
    if(vg_model_get_diagnosis()->state != VG_DIAG_LOADING) {
        vg_model_request_diagnosis();
    }
    else {
        refresh_diagnosis(NULL);
    }
}
