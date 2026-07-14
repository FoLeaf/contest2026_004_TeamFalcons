# VelaGuard ISSUE1 application

if(CONFIG_LVX_USE_VELAGUARD)
  nuttx_add_application(
    NAME velaguard
    PRIORITY ${CONFIG_LVX_VELAGUARD_PRIORITY}
    STACKSIZE ${CONFIG_LVX_VELAGUARD_STACKSIZE}
    MODULE ${CONFIG_LVX_USE_VELAGUARD}
    DEPENDS lvgl
    SRCS velaguard_main.c
         src/vg_identity.c
         src/vg_startup.c
         src/vg_acq.c
         src/vg_alarm.c
         src/vg_config.c
         src/vg_log.c
         src/vg_ui_home.c
         src/fonts/vg_font_cn_16.c
         src/fonts/vg_font_cn_20.c)
endif()
