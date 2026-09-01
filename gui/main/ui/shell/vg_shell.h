#ifndef VG_SHELL_H
#define VG_SHELL_H

#include "lvgl/lvgl.h"
#include "model/vg_model.h"

typedef enum {
    VG_PAGE_HOME = 0,
    VG_PAGE_DEVICE,
    VG_PAGE_TREND,
    VG_PAGE_ALARM,
    VG_PAGE_DIAGNOSIS,
    VG_PAGE_ADD_SENSOR,
    VG_PAGE_LOGS,
    VG_PAGE_SYSTEM,
    VG_PAGE_OTA,
    VG_PAGE_REPORT,
    VG_PAGE_DISCOVER,
    VG_PAGE_COUNT
} vg_page_id_t;

void vg_shell_create(void);
lv_obj_t * vg_shell_get_content(void);
void vg_shell_set_title(const char * title);
void vg_shell_set_status(const vg_net_status_t * net);
void vg_shell_show_back(bool show);
void vg_shell_toast(const char * msg);
void vg_shell_refresh_status(void);
/* Close any open confirm modal (Esc/back priority); no-op when none open. */
void vg_shell_modal_close(void);

void vg_nav_goto(vg_page_id_t id, const void * args);
void vg_nav_back(void);
vg_page_id_t vg_nav_current(void);

#endif
