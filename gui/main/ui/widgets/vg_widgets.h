#ifndef VG_WIDGETS_H
#define VG_WIDGETS_H

#include "lvgl/lvgl.h"
#include "model/vg_model.h"

lv_obj_t * vg_status_chip_create(lv_obj_t * parent, const char * text, vg_severity_t sev);
void vg_status_chip_set(lv_obj_t * chip, const char * text, vg_severity_t sev);

lv_obj_t * vg_metric_row_create(lv_obj_t * parent, const char * label, const char * value);
void vg_metric_row_set_value(lv_obj_t * row, const char * value);

/* risk: "low" | "medium" | "high" → ok/warn/crit colors */
lv_obj_t * vg_risk_badge_create(lv_obj_t * parent, const char * risk);
void vg_risk_badge_set(lv_obj_t * badge, const char * risk);

#endif
