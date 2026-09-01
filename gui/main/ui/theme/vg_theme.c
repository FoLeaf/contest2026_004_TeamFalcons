#include "vg_theme.h"
#include "vg_display.h"
#include "fonts/vg_fonts.h"

void vg_theme_init(void)
{
}

lv_color_t vg_color_bg(void)
{
    return lv_color_hex(VG_COLOR_BG);
}

lv_color_t vg_color_surface(void)
{
    return lv_color_hex(VG_COLOR_SURFACE);
}

lv_color_t vg_color_border(void)
{
    return lv_color_hex(VG_COLOR_BORDER);
}

lv_color_t vg_color_text(void)
{
    return lv_color_hex(VG_COLOR_TEXT);
}

lv_color_t vg_color_muted(void)
{
    return lv_color_hex(VG_COLOR_MUTED);
}

lv_color_t vg_color_ok(void)
{
    return lv_color_hex(VG_COLOR_OK);
}

lv_color_t vg_color_warn(void)
{
    return lv_color_hex(VG_COLOR_WARN);
}

lv_color_t vg_color_crit(void)
{
    return lv_color_hex(VG_COLOR_CRIT);
}

lv_color_t vg_color_offline(void)
{
    return lv_color_hex(VG_COLOR_OFFLINE);
}

lv_color_t vg_color_info(void)
{
    return lv_color_hex(VG_COLOR_INFO);
}

lv_color_t vg_color_accent(void)
{
    return lv_color_hex(VG_COLOR_ACCENT);
}

lv_color_t vg_color_severity(vg_severity_t sev)
{
    switch(sev) {
        case VG_SEV_WARN: return vg_color_warn();
        case VG_SEV_CRIT: return vg_color_crit();
        case VG_SEV_OFFLINE: return vg_color_offline();
        case VG_SEV_INFO: return vg_color_info();
        case VG_SEV_OK:
        default: return vg_color_ok();
    }
}

const lv_font_t * vg_font_ui(void)
{
    /* Prebuilt subset covers C1 Chinese labels + ASCII */
    return vg_fonts_cjk();
}

const lv_font_t * vg_font_small(void)
{
    /* Keep CJK for chip/status labels that mix Latin + Chinese */
    return vg_fonts_cjk();
}

const lv_font_t * vg_font_metric(void)
{
#if LV_FONT_MONTSERRAT_20
    return &lv_font_montserrat_20;
#elif LV_FONT_MONTSERRAT_16
    return &lv_font_montserrat_16;
#else
    return &lv_font_montserrat_14;
#endif
}

const lv_font_t * vg_font_metric_lg(void)
{
#if LV_FONT_MONTSERRAT_24
    return &lv_font_montserrat_24;
#else
    return vg_font_metric();
#endif
}

void vg_style_apply_screen(lv_obj_t * obj)
{
    lv_obj_set_style_bg_color(obj, vg_color_bg(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_text_color(obj, vg_color_text(), 0);
    lv_obj_set_style_text_font(obj, vg_font_ui(), 0);
}

void vg_style_apply_card(lv_obj_t * obj)
{
    lv_obj_set_style_bg_color(obj, vg_color_surface(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, vg_color_border(), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, VG_CARD_RADIUS, 0);
    lv_obj_set_style_pad_all(obj, 6, 0);
    lv_obj_set_style_text_color(obj, vg_color_text(), 0);
    lv_obj_set_style_text_font(obj, vg_font_ui(), 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void vg_style_apply_btn(lv_obj_t * obj, bool primary)
{
    lv_obj_set_style_radius(obj, VG_CARD_RADIUS, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_pad_hor(obj, 8, 0);
    lv_obj_set_style_pad_ver(obj, 4, 0);
    lv_obj_set_style_text_font(obj, vg_font_ui(), 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    if(primary) {
        lv_obj_set_style_bg_color(obj, vg_color_accent(), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(obj, vg_color_accent(), 0);
        lv_obj_set_style_text_color(obj, lv_color_hex(0xFFFFFF), 0);
    }
    else {
        lv_obj_set_style_bg_color(obj, vg_color_surface(), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(obj, vg_color_border(), 0);
        lv_obj_set_style_text_color(obj, vg_color_text(), 0);
    }
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x2A3138), LV_STATE_PRESSED);
}

void vg_style_apply_chip(lv_obj_t * obj, vg_severity_t sev)
{
    lv_color_t c = vg_color_severity(sev);
    lv_obj_set_style_bg_color(obj, c, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_30, 0);
    lv_obj_set_style_border_color(obj, c, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, 3, 0);
    lv_obj_set_style_pad_hor(obj, 4, 0);
    lv_obj_set_style_pad_ver(obj, 1, 0);
    lv_obj_set_style_text_color(obj, c, 0);
    lv_obj_set_style_text_font(obj, vg_font_small(), 0);
}

void vg_style_apply_label(lv_obj_t * obj, bool muted)
{
    lv_obj_set_style_text_font(obj, vg_font_ui(), 0);
    lv_obj_set_style_text_color(obj, muted ? vg_color_muted() : vg_color_text(), 0);
}
