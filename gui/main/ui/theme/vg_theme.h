#ifndef VG_THEME_H
#define VG_THEME_H

#include "lvgl/lvgl.h"

#define VG_COLOR_BG       0x1A1D21
#define VG_COLOR_SURFACE  0x24292E
#define VG_COLOR_BORDER   0x3A424A
#define VG_COLOR_TEXT     0xE8ECF0
#define VG_COLOR_MUTED    0x9AA3AD
#define VG_COLOR_OK       0x2F9E44
#define VG_COLOR_WARN     0xF0A202
#define VG_COLOR_CRIT     0xE03131
#define VG_COLOR_OFFLINE  0x6C757D
#define VG_COLOR_INFO     0x1C7ED6
#define VG_COLOR_ACCENT   0x1C7ED6

typedef enum {
    VG_SEV_OK = 0,
    VG_SEV_WARN,
    VG_SEV_CRIT,
    VG_SEV_OFFLINE,
    VG_SEV_INFO
} vg_severity_t;

void vg_theme_init(void);

lv_color_t vg_color_bg(void);
lv_color_t vg_color_surface(void);
lv_color_t vg_color_border(void);
lv_color_t vg_color_text(void);
lv_color_t vg_color_muted(void);
lv_color_t vg_color_ok(void);
lv_color_t vg_color_warn(void);
lv_color_t vg_color_crit(void);
lv_color_t vg_color_offline(void);
lv_color_t vg_color_info(void);
lv_color_t vg_color_accent(void);
lv_color_t vg_color_severity(vg_severity_t sev);

const lv_font_t * vg_font_ui(void);
const lv_font_t * vg_font_small(void);
const lv_font_t * vg_font_metric(void);
const lv_font_t * vg_font_metric_lg(void);

void vg_style_apply_screen(lv_obj_t * obj);
void vg_style_apply_card(lv_obj_t * obj);
void vg_style_apply_btn(lv_obj_t * obj, bool primary);
void vg_style_apply_chip(lv_obj_t * obj, vg_severity_t sev);
void vg_style_apply_label(lv_obj_t * obj, bool muted);

#endif
