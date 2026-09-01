#ifndef VG_CONFIRM_DIALOG_H
#define VG_CONFIRM_DIALOG_H

#include "lvgl/lvgl.h"

typedef void (*vg_confirm_cb_t)(void * user);

/* Full-screen modal confirm dialog (covers status bar).
 * risk: "low" | "medium" | "high" — medium and high require two taps on the
 * OK button (manual 16.8 confirmation grading).
 * Returns the modal root; auto-deletes on cancel/complete.
 * on_ok is invoked only after confirmation (single step for low,
 * double step for medium/high). */
lv_obj_t * vg_confirm_dialog_create(
    const char * title, const char * body, const char * risk,
    const char * ok_text, vg_confirm_cb_t on_ok, void * user);

bool vg_confirm_dialog_is_open(void);
void vg_confirm_dialog_close(void);

#endif
