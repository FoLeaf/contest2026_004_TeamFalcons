############################################################################
# app/velaguard_app/Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = velaguard
PRIORITY  = $(CONFIG_LVX_VELAGUARD_PRIORITY)
STACKSIZE = $(CONFIG_LVX_VELAGUARD_STACKSIZE)
MODULE    = $(CONFIG_LVX_USE_VELAGUARD)

MAINSRC = velaguard_main.c

CSRCS = src/vg_identity.c \
        src/vg_startup.c \
        src/vg_acq.c \
        src/vg_alarm.c \
        src/vg_config.c \
        src/vg_log.c \
        src/vg_ui_home.c \
        src/fonts/vg_font_cn_16.c \
        src/fonts/vg_font_cn_20.c

include $(APPDIR)/Application.mk
