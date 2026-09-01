#include "vg_confirm_dialog.h"
#include "vg_widgets.h"
#include "theme/vg_theme.h"
#include "vg_display.h"
#include <string.h>

static lv_obj_t * s_modal;
static lv_obj_t * s_ok_btn;
static lv_obj_t * s_ok_lab;
static const char * s_risk;
static bool s_armed;
static vg_confirm_cb_t s_on_ok;
static void * s_user;

static void modal_delete_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    s_modal = NULL;
    s_ok_btn = NULL;
    s_ok_lab = NULL;
}

static void on_overlay_click(lv_event_t * e)
{
    /* Consume clicks on the mask so they never reach pages below. */
    LV_UNUSED(e);
}

void vg_confirm_dialog_close(void)
{
    if(s_modal && lv_obj_is_valid(s_modal)) {
        lv_obj_delete(s_modal);
    }
    s_modal = NULL;
    s_ok_btn = NULL;
    s_ok_lab = NULL;
    s_armed = false;
    s_on_ok = NULL;
    s_user = NULL;
}

static void on_cancel(lv_event_t * e)
{
    LV_UNUSED(e);
    vg_confirm_dialog_close();
}

static void ok_clicked(lv_event_t * e)
{
    vg_confirm_cb_t cb;
    void * user;
    LV_UNUSED(e);

    /* Manual 16.8: low = single tap; medium and high = two taps. */
    if(s_risk && (strcmp(s_risk, "high") == 0 || strcmp(s_risk, "medium") == 0) &&
       !s_armed) {
        s_armed = true;
        if(s_ok_lab) lv_label_set_text(s_ok_lab, "再次确认");
        if(s_ok_btn) {
            lv_obj_set_style_bg_color(s_ok_btn, vg_color_crit(), 0);
            lv_obj_set_style_border_color(s_ok_btn, vg_color_crit(), 0);
        }
        return;
    }

    cb = s_on_ok;
    user = s_user;
    vg_confirm_dialog_close();
    if(cb) cb(user);
}

static lv_obj_t * make_btn(lv_obj_t * parent, const char * text,
                           bool primary, lv_event_cb_t cb)
{
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_t * lab;
    lv_obj_set_size(btn, 104, VG_MIN_TOUCH_H);
    vg_style_apply_btn(btn, primary);
    lab = lv_label_create(btn);
    lv_label_set_text(lab, text);
    lv_obj_set_style_text_font(lab, vg_font_ui(), 0);
    lv_obj_center(lab);
    if(cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    return btn;
}

lv_obj_t * vg_confirm_dialog_create(
    const char * title, const char * body, const char * risk,
    const char * ok_text, vg_confirm_cb_t on_ok, void * user)
{
    lv_obj_t * card;
    lv_obj_t * row;
    lv_obj_t * lab;

    if(s_modal) vg_confirm_dialog_close();

    s_risk = risk ? risk : "low";
    s_on_ok = on_ok;
    s_user = user;
    s_armed = false;

    /* Mask: full screen incl. status bar; consumes all input. */
    s_modal = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_modal, VG_DISP_W, VG_DISP_H);
    lv_obj_set_pos(s_modal, 0, 0);
    lv_obj_set_style_bg_color(s_modal, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_modal, LV_OPA_50, 0);
    lv_obj_set_style_border_width(s_modal, 0, 0);
    lv_obj_set_style_pad_all(s_modal, 0, 0);
    lv_obj_remove_flag(s_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_modal, on_overlay_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_modal, modal_delete_cb, LV_EVENT_DELETE, NULL);

    card = lv_obj_create(s_modal);
    lv_obj_set_size(card, 320, 172);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    vg_style_apply_card(card);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_pad_row(card, 6, 0);

    /* Title + risk badge */
    row = lv_obj_create(card);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 22);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lab = lv_label_create(row);
    lv_label_set_text(lab, title ? title : "确认操作");
    vg_style_apply_label(lab, false);
    lv_obj_set_style_text_font(lab, vg_font_ui(), 0);

    vg_risk_badge_create(row, s_risk);

    /* Body */
    lab = lv_label_create(card);
    lv_label_set_long_mode(lab, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lab, lv_pct(100));
    vg_style_apply_label(lab, true);
    lv_label_set_text(lab, body ? body : "");
    lv_obj_set_flex_grow(lab, 1);

    /* Buttons */
    row = lv_obj_create(card);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, VG_MIN_TOUCH_H);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);

    make_btn(row, "取消", false, on_cancel);
    s_ok_btn = make_btn(row, ok_text ? ok_text : "确认", true, ok_clicked);
    s_ok_lab = (lv_obj_t *)lv_obj_get_child(s_ok_btn, 0);

    return s_modal;
}

bool vg_confirm_dialog_is_open(void)
{
    if(s_modal && !lv_obj_is_valid(s_modal)) {
        s_modal = NULL;
        s_ok_btn = NULL;
        s_ok_lab = NULL;
    }
    return s_modal != NULL;
}
