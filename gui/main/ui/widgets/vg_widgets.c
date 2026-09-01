#include "vg_widgets.h"
#include "theme/vg_theme.h"
#include "vg_display.h"
#include <stdio.h>
#include <string.h>

lv_obj_t * vg_status_chip_create(lv_obj_t * parent, const char * text, vg_severity_t sev)
{
    lv_obj_t * chip = lv_obj_create(parent);
    lv_obj_remove_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(chip, LV_SIZE_CONTENT, 18);
    vg_style_apply_chip(chip, sev);

    lv_obj_t * lab = lv_label_create(chip);
    lv_label_set_text(lab, text ? text : "");
    lv_obj_center(lab);
    lv_obj_set_user_data(chip, lab);
    return chip;
}

void vg_status_chip_set(lv_obj_t * chip, const char * text, vg_severity_t sev)
{
    if(chip == NULL) return;
    vg_style_apply_chip(chip, sev);
    lv_obj_t * lab = (lv_obj_t *)lv_obj_get_user_data(chip);
    if(lab) {
        lv_label_set_text(lab, text ? text : "");
    }
}

lv_obj_t * vg_metric_row_create(lv_obj_t * parent, const char * label, const char * value)
{
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_ver(row, 0, 0);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 17);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * l = lv_label_create(row);
    lv_label_set_text(l, label ? label : "");
    vg_style_apply_label(l, true);
    lv_obj_set_style_text_font(l, vg_font_ui(), 0);

    lv_obj_t * v = lv_label_create(row);
    lv_label_set_text(v, value ? value : "");
    vg_style_apply_label(v, false);
    lv_obj_set_style_text_font(v, vg_font_ui(), 0);

    return row;
}

void vg_metric_row_set_value(lv_obj_t * row, const char * value)
{
    if(row == NULL) return;
    lv_obj_t * v = lv_obj_get_child(row, 1);
    if(v) lv_label_set_text(v, value ? value : "");
}

static vg_severity_t risk_to_sev(const char * risk)
{
    if(risk == NULL) return VG_SEV_INFO;
    if(strcmp(risk, "high") == 0) return VG_SEV_CRIT;
    if(strcmp(risk, "medium") == 0) return VG_SEV_WARN;
    if(strcmp(risk, "low") == 0) return VG_SEV_OK;
    return VG_SEV_INFO;
}

lv_obj_t * vg_risk_badge_create(lv_obj_t * parent, const char * risk)
{
    char buf[24];
    vg_severity_t sev = risk_to_sev(risk);
    lv_snprintf(buf, sizeof(buf), "风险 %s", vg_risk_label_zh(risk));
    return vg_status_chip_create(parent, buf, sev);
}

void vg_risk_badge_set(lv_obj_t * badge, const char * risk)
{
    char buf[24];
    if(badge == NULL) return;
    lv_snprintf(buf, sizeof(buf), "风险 %s", vg_risk_label_zh(risk));
    vg_status_chip_set(badge, buf, risk_to_sev(risk));
}
