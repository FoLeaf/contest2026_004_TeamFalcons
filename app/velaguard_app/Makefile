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
        src/vg_ui_home.c

include $(APPDIR)/Application.mk
