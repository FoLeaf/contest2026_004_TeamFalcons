/****************************************************************************
 * app/velaguard_app/src/vg_ui_perf.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <stdio.h>

#include <lvgl/lvgl.h>

#include "vg_ui_perf.h"

#ifdef CONFIG_VG_UI_PERF_DIAGNOSTICS

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct vg_ui_perf_state
{
  uint32_t window_start;
  uint32_t frame_start;
  uint32_t frames;
  uint32_t total_ms;
  uint32_t max_ms;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct vg_ui_perf_state g_perf;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void vg_ui_perf_report(void)
{
  uint32_t elapsed = lv_tick_elaps(g_perf.window_start);

  if (elapsed < 1000)
    {
      return;
    }

  printf("[velaguard][ui-perf] frames=%lu elapsed_ms=%lu "
         "avg_ms=%lu max_ms=%lu\n",
         (unsigned long)g_perf.frames,
         (unsigned long)elapsed,
         (unsigned long)(g_perf.frames == 0 ? 0 :
                         g_perf.total_ms / g_perf.frames),
         (unsigned long)g_perf.max_ms);
  g_perf.window_start = lv_tick_get();
  g_perf.frames = 0;
  g_perf.total_ms = 0;
  g_perf.max_ms = 0;
}

static void vg_ui_perf_event_cb(FAR lv_event_t *event)
{
  lv_event_code_t code = lv_event_get_code(event);
  uint32_t now = lv_tick_get();

  if (code == LV_EVENT_REFR_START)
    {
      g_perf.frame_start = now;
    }
  else if (code == LV_EVENT_REFR_READY)
    {
      uint32_t frame_ms = lv_tick_elaps(g_perf.frame_start);

      g_perf.frames++;
      g_perf.total_ms += frame_ms;
      if (frame_ms > g_perf.max_ms)
        {
          g_perf.max_ms = frame_ms;
        }

      vg_ui_perf_report();
    }
}

static void vg_ui_perf_anim_cb(FAR void *var, int32_t value)
{
  lv_obj_set_x((FAR lv_obj_t *)var, value);
}

#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void vg_ui_perf_init(FAR lv_display_t *display)
{
#ifdef CONFIG_VG_UI_PERF_DIAGNOSTICS
  FAR lv_obj_t *marker;
  lv_anim_t anim;

  if (display == NULL)
    {
      return;
    }

  g_perf.window_start = lv_tick_get();
  lv_display_add_event_cb(display, vg_ui_perf_event_cb, LV_EVENT_ALL, NULL);

  marker = lv_obj_create(lv_layer_sys());
  lv_obj_remove_style_all(marker);
  lv_obj_set_size(marker, 24, 4);
  lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(marker, lv_color_hex(0x39b6b2), LV_PART_MAIN);
  lv_obj_set_pos(marker, 0, 266);

  lv_anim_init(&anim);
  lv_anim_set_var(&anim, marker);
  lv_anim_set_exec_cb(&anim, vg_ui_perf_anim_cb);
  lv_anim_set_values(&anim, 0, 456);
  lv_anim_set_duration(&anim, 1000);
  lv_anim_set_playback_duration(&anim, 1000);
  lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&anim);
#else
  UNUSED(display);
#endif
}
