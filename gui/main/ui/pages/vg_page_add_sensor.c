#include "vg_pages.h"
#include "widgets/vg_widgets.h"
#include "widgets/vg_confirm_dialog.h"
#include "theme/vg_theme.h"
#include "model/vg_model.h"
#include "vg_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define VG_ADD_FIELD_H 36
#define VG_ADD_CHIP_H 36
#define VG_ADD_TMPL_N 6


typedef struct {
    lv_obj_t * root;
    lv_obj_t * input_box;
    lv_obj_t * type_chips;
    lv_obj_t * name;
    lv_obj_t * id;
    lv_obj_t * type;
    lv_obj_t * unit;
    lv_obj_t * slave_addr;
    lv_obj_t * function_code;
    lv_obj_t * reg_addr;
    lv_obj_t * length;
    lv_obj_t * period;
    lv_obj_t * data_format;
    lv_obj_t * word_order;
    lv_obj_t * formula;
    lv_obj_t * thr_low;
    lv_obj_t * thr_warn;
    lv_obj_t * thr_crit;
    lv_obj_t * err_lab;
    lv_obj_t * preview_box;
    lv_obj_t * pv_id;
    lv_obj_t * pv_name;
    lv_obj_t * pv_type;
    lv_obj_t * pv_transport;
    lv_obj_t * pv_decode;
    lv_obj_t * pv_formula;
    lv_obj_t * pv_thr;
    lv_obj_t * pv_period;
    lv_obj_t * testing_box;
    lv_obj_t * ok_box;
    lv_obj_t * ok_value;
    lv_obj_t * ok_quality;
    lv_obj_t * ok_msg;
    lv_obj_t * fail_box;
    lv_obj_t * fail_lab;
} add_ctx_t;

static add_ctx_t s_add_ui;

static void show_only(lv_obj_t * show)
{
    lv_obj_t * boxes[] = {
        s_add_ui.input_box, s_add_ui.preview_box, s_add_ui.testing_box,
        s_add_ui.ok_box, s_add_ui.fail_box
    };
    size_t i;
    for(i = 0; i < sizeof(boxes) / sizeof(boxes[0]); i++) {
        if(boxes[i] == NULL) continue;
        if(show == boxes[i]) {
            if(lv_obj_has_flag(boxes[i], LV_OBJ_FLAG_HIDDEN))
                lv_obj_remove_flag(boxes[i], LV_OBJ_FLAG_HIDDEN);
        }
        else if(!lv_obj_has_flag(boxes[i], LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(boxes[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void set_text(lv_obj_t * obj, const char * text)
{
    if(obj) lv_textarea_set_text(obj, text ? text : "");
}

static int parse_int_field(lv_obj_t * obj, int * out)
{
    char * end;
    long value;
    const char * text = obj ? lv_textarea_get_text(obj) : "";
    if(text == NULL || text[0] == '\0') return false;
    value = strtol(text, &end, 10);
    while(*end == ' ' || *end == '\t') end++;
    if(end == text || *end != '\0') return false;
    if(out) *out = (int)value;
    return true;
}

static int parse_float_field(lv_obj_t * obj, float * out)
{
    char * end;
    float value;
    const char * text = obj ? lv_textarea_get_text(obj) : "";
    if(text == NULL || text[0] == '\0') return false;
    value = strtof(text, &end);
    while(*end == ' ' || *end == '\t') end++;
    if(end == text || *end != '\0' || !isfinite(value)) return false;
    if(out) *out = value;
    return true;
}

static lv_obj_t * make_btn(lv_obj_t * parent, const char * text,
                           bool primary, lv_event_cb_t cb)
{
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_t * lab;
    lv_obj_set_height(btn, VG_MIN_TOUCH_H);
    lv_obj_set_width(btn, LV_SIZE_CONTENT);
    lv_obj_set_style_min_width(btn, 72, 0);
    vg_style_apply_btn(btn, primary);
    lab = lv_label_create(btn);
    lv_label_set_text(lab, text ? text : "");
    lv_label_set_long_mode(lab, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(lab, vg_font_ui(), 0);
    lv_obj_set_style_max_width(lab, 100, 0);
    lv_obj_center(lab);
    if(cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    return btn;
}

static lv_obj_t * make_field(lv_obj_t * parent, const char * label,
                             const char * placeholder, int max_len)
{
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_t * lab = lv_label_create(row);
    lv_obj_t * input = lv_textarea_create(row);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, VG_ADD_FIELD_H);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);

    lv_label_set_text(lab, label ? label : "");
    vg_style_apply_label(lab, true);
    lv_obj_set_width(lab, 92);
    lv_obj_set_style_text_font(lab, vg_font_small(), 0);

    lv_obj_set_height(input, VG_ADD_FIELD_H);
    lv_obj_set_flex_grow(input, 1);
    lv_obj_set_style_bg_color(input, vg_color_surface(), 0);
    lv_obj_set_style_bg_opa(input, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(input, vg_color_border(), 0);
    lv_obj_set_style_border_width(input, 1, 0);
    lv_obj_set_style_radius(input, VG_CARD_RADIUS, 0);
    lv_obj_set_style_pad_left(input, 6, 0);
    lv_obj_set_style_pad_right(input, 6, 0);
    lv_obj_set_style_text_font(input, vg_font_small(), 0);
    lv_obj_set_style_text_color(input, vg_color_text(), 0);
    lv_textarea_set_one_line(input, true);
    lv_textarea_set_placeholder_text(input, placeholder ? placeholder : "");
    lv_textarea_set_max_length(input, max_len > 0 ? max_len : VG_SENSOR_NAME_MAX);
    return input;
}

static lv_obj_t * make_dropdown_field(lv_obj_t * parent, const char * label,
                                       const char * options)
{
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_t * lab = lv_label_create(row);
    lv_obj_t * dropdown = lv_dropdown_create(row);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, VG_ADD_FIELD_H);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);

    lv_label_set_text(lab, label ? label : "");
    vg_style_apply_label(lab, true);
    lv_obj_set_width(lab, 92);
    lv_obj_set_style_text_font(lab, vg_font_small(), 0);

    lv_obj_set_height(dropdown, VG_ADD_FIELD_H);
    lv_obj_set_flex_grow(dropdown, 1);
    lv_dropdown_set_options(dropdown, options ? options : "");
    lv_dropdown_set_selected(dropdown, 0);
    lv_obj_set_style_text_font(dropdown, vg_font_small(), LV_PART_MAIN);
    lv_obj_set_style_text_font(dropdown, vg_font_small(), LV_PART_INDICATOR);
    return dropdown;
}

static void sync_decode_fields(void)
{
    uint32_t format;
    bool is_32;
    if(s_add_ui.data_format == NULL) return;
    format = lv_dropdown_get_selected(s_add_ui.data_format);
    is_32 = format >= (uint32_t)VG_SENSOR_FMT_UINT32;
    if(s_add_ui.word_order) {
        lv_obj_t * row = lv_obj_get_parent(s_add_ui.word_order);
        if(is_32) lv_obj_remove_flag(row, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
    }
    if(s_add_ui.length) set_text(s_add_ui.length, is_32 ? "2" : "1");
}

static void on_data_format_changed(lv_event_t * e)
{
    LV_UNUSED(e);
    sync_decode_fields();
}

static void apply_type_template(const vg_sensor_type_template_t * type)
{
    char buf[32];
    if(type == NULL) return;
    set_text(s_add_ui.type, type->type);
    set_text(s_add_ui.unit, type->unit);
    set_text(s_add_ui.formula, type->formula);
    lv_snprintf(buf, sizeof(buf), "%.3g", type->thr_low);
    set_text(s_add_ui.thr_low, buf);
    lv_snprintf(buf, sizeof(buf), "%.3g", type->thr_warn);
    set_text(s_add_ui.thr_warn, buf);
    lv_snprintf(buf, sizeof(buf), "%.3g", type->thr_crit);
    set_text(s_add_ui.thr_crit, buf);
    lv_dropdown_set_selected(s_add_ui.data_format, (uint32_t)type->data_format);
    lv_dropdown_set_selected(s_add_ui.word_order, (uint32_t)type->word_order);
    sync_decode_fields();
}

static void on_type_template(lv_event_t * e)
{
    uintptr_t index = (uintptr_t)lv_event_get_user_data(e);
    apply_type_template(vg_model_sensor_type_at((uint8_t)index));
}

static void rebuild_type_chips(void)
{
    uint8_t i;
    if(s_add_ui.type_chips == NULL || !lv_obj_is_valid(s_add_ui.type_chips)) return;
    lv_obj_clean(s_add_ui.type_chips);
    for(i = 0; i < vg_model_sensor_type_count(); i++) {
        const vg_sensor_type_template_t * type = vg_model_sensor_type_at(i);
        lv_obj_t * chip;
        lv_obj_t * lab;
        if(type == NULL) continue;
        chip = lv_button_create(s_add_ui.type_chips);
        lv_obj_set_size(chip, 68, VG_ADD_CHIP_H);
        vg_style_apply_btn(chip, false);
        lab = lv_label_create(chip);
        lv_label_set_text(lab, type->type);
        lv_label_set_long_mode(lab, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(lab, vg_font_small(), 0);
        lv_obj_set_style_max_width(lab, 60, 0);
        lv_obj_center(lab);
        lv_obj_add_event_cb(chip, on_type_template, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);
    }
}

static void on_random_id(lv_event_t * e)
{
    char id[VG_SENSOR_ID_MAX];
    LV_UNUSED(e);
    vg_model_generate_sensor_id(id, sizeof(id));
    set_text(s_add_ui.id, id);
}

static void on_save_type(lv_event_t * e)
{
    vg_sensor_type_template_t type;
    float low, warn, crit;
    LV_UNUSED(e);
    memset(&type, 0, sizeof(type));
    type.used = true;
    strncpy(type.type, lv_textarea_get_text(s_add_ui.type), sizeof(type.type) - 1);
    strncpy(type.unit, lv_textarea_get_text(s_add_ui.unit), sizeof(type.unit) - 1);
    strncpy(type.formula, lv_textarea_get_text(s_add_ui.formula), sizeof(type.formula) - 1);
    if(!parse_float_field(s_add_ui.thr_low, &low) ||
       !parse_float_field(s_add_ui.thr_warn, &warn) ||
       !parse_float_field(s_add_ui.thr_crit, &crit)) {
        lv_label_set_text(s_add_ui.err_lab, "自定义类型阈值必须是数字");
        lv_obj_remove_flag(s_add_ui.err_lab, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    type.thr_low = low;
    type.thr_warn = warn;
    type.thr_crit = crit;
    type.data_format = (vg_sensor_data_format_t)lv_dropdown_get_selected(s_add_ui.data_format);
    type.word_order = (vg_sensor_word_order_t)lv_dropdown_get_selected(s_add_ui.word_order);
    if(!vg_model_save_sensor_type(&type)) {
        lv_label_set_text(s_add_ui.err_lab, "自定义类型无效，请检查名称、公式和阈值");
        lv_obj_remove_flag(s_add_ui.err_lab, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_add_flag(s_add_ui.err_lab, LV_OBJ_FLAG_HIDDEN);
    rebuild_type_chips();
    vg_shell_toast("类型模板已保存");
}

static void on_generate(lv_event_t * e)
{
    vg_sensor_candidate_t candidate;
    float low, warn, crit;
    int length;
    int slave;
    float period_sec;
    LV_UNUSED(e);
    memset(&candidate, 0, sizeof(candidate));
    strncpy(candidate.name, lv_textarea_get_text(s_add_ui.name), sizeof(candidate.name) - 1);
    strncpy(candidate.id, lv_textarea_get_text(s_add_ui.id), sizeof(candidate.id) - 1);
    strncpy(candidate.type, lv_textarea_get_text(s_add_ui.type), sizeof(candidate.type) - 1);
    strncpy(candidate.unit, lv_textarea_get_text(s_add_ui.unit), sizeof(candidate.unit) - 1);
    strncpy(candidate.reg_addr_hex, lv_textarea_get_text(s_add_ui.reg_addr), sizeof(candidate.reg_addr_hex) - 1);
    strncpy(candidate.formula, lv_textarea_get_text(s_add_ui.formula), sizeof(candidate.formula) - 1);
    candidate.function_code = (uint8_t)(lv_dropdown_get_selected(s_add_ui.function_code) == 0 ? 3 : 4);
    candidate.data_format = (vg_sensor_data_format_t)lv_dropdown_get_selected(s_add_ui.data_format);
    candidate.word_order = (vg_sensor_word_order_t)lv_dropdown_get_selected(s_add_ui.word_order);
    strncpy(candidate.source, "manual", sizeof(candidate.source) - 1);
    if(!parse_int_field(s_add_ui.slave_addr, &slave) || slave < 1 || slave > 247) slave = 0;
    candidate.slave_addr = (uint8_t)slave;
    if(!parse_int_field(s_add_ui.length, &length)) length = 0;
    if(!parse_float_field(s_add_ui.period, &period_sec)) period_sec = 0.0f;
    if(!parse_float_field(s_add_ui.thr_low, &low)) low = NAN;
    if(!parse_float_field(s_add_ui.thr_warn, &warn)) warn = NAN;
    if(!parse_float_field(s_add_ui.thr_crit, &crit)) crit = NAN;
    candidate.length = length;
    candidate.period_ms = period_sec > 0.0f ? (int)(period_sec * 1000.0f) : 0;
    candidate.thr_low = low;
    candidate.thr_warn = warn;
    candidate.thr_crit = crit;
    if(!vg_model_add_sensor_generate(&candidate)) {
        lv_label_set_text(s_add_ui.err_lab, vg_model_add_sensor_test_msg());
        lv_obj_remove_flag(s_add_ui.err_lab, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_add_flag(s_add_ui.err_lab, LV_OBJ_FLAG_HIDDEN);
}

static void on_test(lv_event_t * e)
{
    LV_UNUSED(e);
    vg_model_add_sensor_test();
}

static void on_edit(lv_event_t * e)
{
    LV_UNUSED(e);
    vg_model_add_sensor_begin();
}

static void on_abort(lv_event_t * e)
{
    LV_UNUSED(e);
    vg_model_add_sensor_abort();
}

static void on_confirm_ok(void * user)
{
    LV_UNUSED(user);
    if(!vg_model_add_sensor_confirm()) {
        vg_shell_toast("添加失败，请检查配置");
        return;
    }
    vg_shell_toast("已添加");
    vg_nav_back();
}

static void on_confirm(lv_event_t * e)
{
    char body[128];
    const vg_sensor_candidate_t * c = vg_model_get_candidate();
    bool failed = vg_model_add_sensor_state() == VG_ADD_TEST_FAIL;
    LV_UNUSED(e);
    if(c == NULL) return;
    if(failed) {
        lv_snprintf(body, sizeof(body), "测试读取失败，添加后将显示离线。仍要添加「%s」吗？", c->name);
        vg_confirm_dialog_create("测试失败仍然添加", body, "medium", "仍然添加",
                                 on_confirm_ok, NULL);
    }
    else {
        lv_snprintf(body, sizeof(body), "确认添加传感器「%s」并启用采集？", c->name);
        vg_confirm_dialog_create("添加传感器", body, "low", "确认添加",
                                 on_confirm_ok, NULL);
    }
}

static void update_preview(const vg_sensor_candidate_t * c)
{
    char buf[128];
    if(c == NULL) return;
    lv_label_set_text(s_add_ui.pv_id, c->id);
    lv_label_set_text(s_add_ui.pv_name, c->name);
    lv_snprintf(buf, sizeof(buf), "%s / %s", c->type, c->unit);
    lv_label_set_text(s_add_ui.pv_type, buf);
    /* Manual 6.8: preview must show protocol / slave / baud besides registers */
    lv_snprintf(buf, sizeof(buf), "Modbus RTU · 从站 %u · 9600 8N1 · %02u / 0x%04X / 长度 %ld",
                (unsigned)c->slave_addr, (unsigned)c->function_code,
                (unsigned)c->reg_addr, (long)c->length);
    lv_label_set_text(s_add_ui.pv_transport, buf);
    lv_snprintf(buf, sizeof(buf), "%s / %s", c->data_format == VG_SENSOR_FMT_UINT16 ? "uint16" :
                c->data_format == VG_SENSOR_FMT_INT16 ? "int16" :
                c->data_format == VG_SENSOR_FMT_UINT32 ? "uint32" :
                c->data_format == VG_SENSOR_FMT_INT32 ? "int32" : "float32",
                c->word_order == VG_SENSOR_ORDER_ABCD ? "ABCD" : "CDAB");
    lv_label_set_text(s_add_ui.pv_decode, buf);
    lv_snprintf(buf, sizeof(buf), "%s", c->formula);
    lv_label_set_text(s_add_ui.pv_formula, buf);
    lv_snprintf(buf, sizeof(buf), "低 %.3g / 预警 %.3g / 严重 %.3g",
                c->thr_low, c->thr_warn, c->thr_crit);
    lv_label_set_text(s_add_ui.pv_thr, buf);
    if(c->period_ms >= 3600000) lv_snprintf(buf, sizeof(buf), "%ld 小时", (long)(c->period_ms / 3600000));
    else if(c->period_ms >= 60000) lv_snprintf(buf, sizeof(buf), "%ld 分钟", (long)(c->period_ms / 60000));
    else lv_snprintf(buf, sizeof(buf), "%ld 秒", (long)(c->period_ms / 1000));
    lv_label_set_text(s_add_ui.pv_period, buf);
}

static void refresh_add_sensor(void * user)
{
    const vg_sensor_candidate_t * c;
    vg_add_sensor_state_t state;
    LV_UNUSED(user);
    if(vg_nav_current() != VG_PAGE_ADD_SENSOR) return;
    if(s_add_ui.root == NULL || !lv_obj_is_valid(s_add_ui.root)) return;
    c = vg_model_get_candidate();
    state = vg_model_add_sensor_state();
    switch(state) {
        case VG_ADD_PREVIEW:
            update_preview(c);
            show_only(s_add_ui.preview_box);
            break;
        case VG_ADD_TESTING:
            show_only(s_add_ui.testing_box);
            break;
        case VG_ADD_TEST_OK:
            lv_label_set_text(s_add_ui.ok_value, "读取成功");
            lv_label_set_text(s_add_ui.ok_quality, "");
            {
                char buf[96];
                lv_snprintf(buf, sizeof(buf), "%.3g %s", vg_model_add_sensor_test_value(),
                            c ? c->unit : "");
                lv_label_set_text(s_add_ui.ok_msg, buf);
                lv_snprintf(buf, sizeof(buf), "质量 %ld%%，可以确认添加",
                            (long)vg_model_add_sensor_test_quality());
                lv_label_set_text(s_add_ui.ok_quality, buf);
            }
            show_only(s_add_ui.ok_box);
            break;
        case VG_ADD_TEST_FAIL:
            lv_label_set_text(s_add_ui.fail_lab,
                              vg_model_add_sensor_test_msg()[0] ?
                              vg_model_add_sensor_test_msg() : "测试读取失败");
            show_only(s_add_ui.fail_box);
            break;
        case VG_ADD_IDLE:
        default:
            show_only(s_add_ui.input_box);
            if(vg_model_add_sensor_test_msg()[0]) {
                lv_label_set_text(s_add_ui.err_lab, vg_model_add_sensor_test_msg());
                lv_obj_remove_flag(s_add_ui.err_lab, LV_OBJ_FLAG_HIDDEN);
            }
            break;
    }
}

static void on_add_delete(lv_event_t * e)
{
    LV_UNUSED(e);
    vg_confirm_dialog_close();
    vg_model_off_change(refresh_add_sensor, NULL);
    memset(&s_add_ui, 0, sizeof(s_add_ui));
}

static lv_obj_t * make_step_card(lv_obj_t * parent)
{
    lv_obj_t * card = lv_obj_create(parent);
    vg_style_apply_card(card);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_style_min_height(card, 0, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, 4, 0);
    return card;
}

static void build_input_step(lv_obj_t * parent)
{
    lv_obj_t * box = make_step_card(parent);
    lv_obj_t * chip_title;
    lv_obj_t * chip_row;
    lv_obj_t * btn_row;
    s_add_ui.input_box = box;
    /* 卡片样式默认移除 SCROLLABLE，长表单必须显式恢复才能拖拽/滚轮滚动 */
    lv_obj_add_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(box, LV_DIR_VER);

    chip_title = lv_label_create(box);
    lv_label_set_text(chip_title, "传感器类型（快捷项仅用于预填）");
    vg_style_apply_label(chip_title, true);
    chip_row = lv_obj_create(box);
    lv_obj_remove_flag(chip_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(chip_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chip_row, 0, 0);
    lv_obj_set_style_pad_all(chip_row, 0, 0);
    lv_obj_set_width(chip_row, lv_pct(100));
    lv_obj_set_height(chip_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(chip_row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(chip_row, 2, 0);
    lv_obj_set_style_pad_column(chip_row, 3, 0);
    s_add_ui.type_chips = chip_row;

    s_add_ui.name = make_field(box, "传感器名 *", "例如：仓库温度-A", VG_SENSOR_NAME_MAX - 1);
    s_add_ui.id = make_field(box, "传感器 ID", "留空自动随机，例如 s_7F3A21", VG_SENSOR_ID_MAX - 1);
    {
        /* 随机 ID 与 ID 输入同行，避免独占一整行 */
        lv_obj_t * id_row = lv_obj_get_parent(s_add_ui.id);
        lv_obj_t * rnd = lv_button_create(id_row);
        lv_obj_t * rnd_lab;
        lv_obj_set_size(rnd, 64, VG_ADD_FIELD_H - 4);
        vg_style_apply_btn(rnd, false);
        rnd_lab = lv_label_create(rnd);
        lv_label_set_text(rnd_lab, "随机 ID");
        lv_obj_set_style_text_font(rnd_lab, vg_font_small(), 0);
        lv_obj_center(rnd_lab);
        lv_obj_add_event_cb(rnd, on_random_id, LV_EVENT_CLICKED, NULL);
    }

    s_add_ui.type = make_field(box, "类型 *", "内置或自定义类型", VG_SENSOR_TYPE_MAX - 1);
    s_add_ui.unit = make_field(box, "单位 *", "例如：C、ppm、m3/h", VG_UNIT_MAX - 1);
    s_add_ui.slave_addr = make_field(box, "从站地址 *", "1–247，例如 1", 3);
    s_add_ui.function_code = make_dropdown_field(box, "功能码 *", "03\n04");
    s_add_ui.reg_addr = make_field(box, "寄存器 *", "仅十六进制，例如 0100", VG_SENSOR_REG_HEX_MAX - 1);
    s_add_ui.length = make_field(box, "读取长度 *", "十进制寄存器数，例如 1", 3);
    s_add_ui.period = make_field(box, "采样周期(s) *", "例如 0.1、1、5、60", 8);
    s_add_ui.data_format = make_dropdown_field(box, "数据格式 *", "uint16\nint16\nuint32\nint32\nfloat32");
    lv_obj_add_event_cb(s_add_ui.data_format, on_data_format_changed, LV_EVENT_VALUE_CHANGED, NULL);
    s_add_ui.word_order = make_dropdown_field(box, "字序", "ABCD\nCDAB");
    s_add_ui.formula = make_field(box, "换算公式 *", "例如 R0/10、R0*1.27", VG_SENSOR_FORMULA_MAX - 1);
    s_add_ui.thr_low = make_field(box, "低限阈值 *", "数字", 16);
    s_add_ui.thr_warn = make_field(box, "预警阈值 *", "数字", 16);
    s_add_ui.thr_crit = make_field(box, "严重阈值 *", "数字", 16);

    btn_row = lv_obj_create(box);
    lv_obj_remove_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, VG_MIN_TOUCH_H);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    make_btn(btn_row, "保存为类型", false, on_save_type);
    make_btn(btn_row, "生成配置", true, on_generate);

    s_add_ui.err_lab = lv_label_create(box);
    lv_label_set_text(s_add_ui.err_lab, "");
    lv_obj_set_style_text_color(s_add_ui.err_lab, vg_color_crit(), 0);
    lv_obj_set_style_text_font(s_add_ui.err_lab, vg_font_small(), 0);
    lv_label_set_long_mode(s_add_ui.err_lab, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_add_ui.err_lab, lv_pct(100));
    lv_obj_add_flag(s_add_ui.err_lab, LV_OBJ_FLAG_HIDDEN);
}

static void build_preview_step(lv_obj_t * parent)
{
    lv_obj_t * box = make_step_card(parent);
    lv_obj_t * btn_row;
    lv_obj_t * title = lv_label_create(box);
    s_add_ui.preview_box = box;
    lv_obj_add_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(box, LV_DIR_VER);
    lv_label_set_text(title, "配置预览（确认前不生效）");
    vg_style_apply_label(title, true);
    s_add_ui.pv_id = lv_obj_get_child(vg_metric_row_create(box, "ID", "--"), 1);
    s_add_ui.pv_name = lv_obj_get_child(vg_metric_row_create(box, "名称", "--"), 1);
    s_add_ui.pv_type = lv_obj_get_child(vg_metric_row_create(box, "类型/单位", "--"), 1);
    s_add_ui.pv_transport = lv_obj_get_child(vg_metric_row_create(box, "通信", "--"), 1);
    s_add_ui.pv_decode = lv_obj_get_child(vg_metric_row_create(box, "解析", "--"), 1);
    s_add_ui.pv_formula = lv_obj_get_child(vg_metric_row_create(box, "公式", "--"), 1);
    s_add_ui.pv_thr = lv_obj_get_child(vg_metric_row_create(box, "阈值", "--"), 1);
    s_add_ui.pv_period = lv_obj_get_child(vg_metric_row_create(box, "周期", "--"), 1);

    btn_row = lv_obj_create(box);
    lv_obj_remove_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, VG_MIN_TOUCH_H);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    make_btn(btn_row, "测试读取", true, on_test);
    make_btn(btn_row, "重新编辑", false, on_edit);
    make_btn(btn_row, "放弃", false, on_abort);
}

static void build_testing_step(lv_obj_t * parent)
{
    lv_obj_t * box = make_step_card(parent);
    lv_obj_t * lab;
    lv_obj_t * spin;
    s_add_ui.testing_box = box;
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    spin = lv_spinner_create(box);
    lv_obj_set_size(spin, 36, 36);
    lv_obj_set_style_arc_color(spin, vg_color_accent(), LV_PART_MAIN);
    lv_obj_set_style_arc_color(spin, vg_color_border(), LV_PART_INDICATOR);
    lab = lv_label_create(box);
    lv_label_set_text(lab, "测试读取中…");
    vg_style_apply_label(lab, false);
}

static void build_ok_step(lv_obj_t * parent)
{
    lv_obj_t * box = make_step_card(parent);
    lv_obj_t * btn_row;
    s_add_ui.ok_box = box;
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    s_add_ui.ok_value = lv_label_create(box);
    lv_obj_set_style_text_font(s_add_ui.ok_value, vg_font_metric_lg(), 0);
    lv_obj_set_style_text_color(s_add_ui.ok_value, vg_color_ok(), 0);
    s_add_ui.ok_msg = lv_label_create(box);
    vg_style_apply_label(s_add_ui.ok_msg, false);
    s_add_ui.ok_quality = lv_label_create(box);
    vg_style_apply_label(s_add_ui.ok_quality, true);
    btn_row = lv_obj_create(box);
    lv_obj_remove_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, VG_MIN_TOUCH_H);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    make_btn(btn_row, "确认添加", true, on_confirm);
    make_btn(btn_row, "重试", false, on_test);
    make_btn(btn_row, "放弃", false, on_abort);
}

static void build_fail_step(lv_obj_t * parent)
{
    lv_obj_t * box = make_step_card(parent);
    lv_obj_t * btn_row;
    lv_obj_t * hint;
    s_add_ui.fail_box = box;
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    s_add_ui.fail_lab = lv_label_create(box);
    lv_obj_set_style_text_color(s_add_ui.fail_lab, vg_color_warn(), 0);
    lv_obj_set_style_text_font(s_add_ui.fail_lab, vg_font_ui(), 0);
    hint = lv_label_create(box);
    lv_label_set_text(hint, "可重试，也可以风险确认后直接添加");
    vg_style_apply_label(hint, true);
    btn_row = lv_obj_create(box);
    lv_obj_remove_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, VG_MIN_TOUCH_H);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    make_btn(btn_row, "重试", true, on_test);
    make_btn(btn_row, "仍然添加", false, on_confirm);
    make_btn(btn_row, "放弃", false, on_abort);
}

void vg_page_add_sensor_create(lv_obj_t * parent, const void * args)
{
    LV_UNUSED(args);
    memset(&s_add_ui, 0, sizeof(s_add_ui));
    s_add_ui.root = parent;
    lv_obj_add_event_cb(parent, on_add_delete, LV_EVENT_DELETE, NULL);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(parent, VG_GAP, 0);
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    build_input_step(parent);
    build_preview_step(parent);
    build_testing_step(parent);
    build_ok_step(parent);
    build_fail_step(parent);
    vg_model_on_change(refresh_add_sensor, NULL);
    vg_model_add_sensor_begin();
    rebuild_type_chips();
    {
        const vg_sensor_candidate_t * c = vg_model_get_candidate();
        set_text(s_add_ui.name, c->name);
        set_text(s_add_ui.id, c->id);
        apply_type_template(vg_model_sensor_type_at(0));
        set_text(s_add_ui.slave_addr, "1");
        set_text(s_add_ui.reg_addr, c->reg_addr_hex);
        set_text(s_add_ui.period, "1");
    }
    refresh_add_sensor(NULL);
}
