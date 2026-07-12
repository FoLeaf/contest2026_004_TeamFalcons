/****************************************************************************
 * apps/app/hello_app/vscode_lab_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <lvgl/lvgl.h>
#include <nshlib/nshlib.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VSCODE_LAB_VERSION        "1.0.0"
#define VSCODE_LAB_BG_COLOR       0x10151f
#define VSCODE_LAB_PANEL_COLOR    0x1b2433
#define VSCODE_LAB_ACCENT_COLOR   0x35c2ff
#define VSCODE_LAB_TEXT_COLOR     0xf4f7fb
#define VSCODE_LAB_MUTED_COLOR    0x9ba9bc

/****************************************************************************
 * Public Type Declarations
 ****************************************************************************/

struct vscode_lab_debug_state
{
  volatile uint32_t button_count;
  volatile int32_t slider_value;
  volatile uint32_t uptime_seconds;
};

/****************************************************************************
 * Public Data
 ****************************************************************************/

struct vscode_lab_debug_state g_vscode_lab_debug_state =
{
  .button_count = 0,
  .slider_value = 50,
  .uptime_seconds = 0,
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool g_vscode_lab_started;
static FAR lv_obj_t *g_count_label;
static FAR lv_obj_t *g_slider_label;
static FAR lv_obj_t *g_uptime_label;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: vscode_lab_debug_checkpoint
 *
 * Description:
 *   Stable function breakpoint for the Windows VS Code debug experiment.
 ****************************************************************************/

__attribute__((noinline))
void vscode_lab_debug_checkpoint(uint32_t button_count,
                                 int32_t slider_value)
{
  printf("[vscode_lab] checkpoint: count=%lu slider=%ld\n",
         (unsigned long)button_count, (long)slider_value);
}

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static FAR lv_obj_t *vscode_lab_label(FAR lv_obj_t *parent,
                                      FAR const char *text,
                                      uint32_t color)
{
  FAR lv_obj_t *label = lv_label_create(parent);

  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
  return label;
}

static void vscode_lab_update_count(void)
{
  char text[40];

  snprintf(text, sizeof(text), "Touch count: %lu",
           (unsigned long)g_vscode_lab_debug_state.button_count);
  lv_label_set_text(g_count_label, text);
}

static void vscode_lab_button_event(FAR lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
    {
      return;
    }

  g_vscode_lab_debug_state.button_count++;
  vscode_lab_update_count();
  vscode_lab_debug_checkpoint(g_vscode_lab_debug_state.button_count,
                              g_vscode_lab_debug_state.slider_value);
}

static void vscode_lab_slider_event(FAR lv_event_t *event)
{
  FAR lv_obj_t *slider = lv_event_get_target(event);
  lv_event_code_t code = lv_event_get_code(event);
  char text[32];

  if (code == LV_EVENT_VALUE_CHANGED)
    {
      g_vscode_lab_debug_state.slider_value = lv_slider_get_value(slider);
      snprintf(text, sizeof(text), "Slider: %ld",
               (long)g_vscode_lab_debug_state.slider_value);
      lv_label_set_text(g_slider_label, text);
    }
  else if (code == LV_EVENT_RELEASED)
    {
      printf("[vscode_lab] slider released: %ld\n",
             (long)g_vscode_lab_debug_state.slider_value);
    }
}

static void vscode_lab_uptime_timer(FAR lv_timer_t *timer)
{
  char text[40];

  UNUSED(timer);
  g_vscode_lab_debug_state.uptime_seconds++;
  snprintf(text, sizeof(text), "Running: %lu s",
           (unsigned long)g_vscode_lab_debug_state.uptime_seconds);
  lv_label_set_text(g_uptime_label, text);
}

static void vscode_lab_create_ui(void)
{
  FAR lv_obj_t *screen = lv_screen_active();
  FAR lv_obj_t *panel;
  FAR lv_obj_t *title;
  FAR lv_obj_t *subtitle;
  FAR lv_obj_t *button;
  FAR lv_obj_t *button_label;
  FAR lv_obj_t *slider;
  FAR lv_obj_t *hint;

  lv_obj_set_style_bg_color(screen, lv_color_hex(VSCODE_LAB_BG_COLOR),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

  title = vscode_lab_label(screen, "Team Falcons  |  VS Code Lab",
                           VSCODE_LAB_TEXT_COLOR);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

  subtitle = vscode_lab_label(screen,
                              "STM32H750B-DK  QSPI-XIP  v"
                              VSCODE_LAB_VERSION,
                              VSCODE_LAB_ACCENT_COLOR);
  lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 34);

  panel = lv_obj_create(screen);
  lv_obj_set_size(panel, 440, 62);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 58);
  lv_obj_set_style_bg_color(panel, lv_color_hex(VSCODE_LAB_PANEL_COLOR),
                            LV_PART_MAIN);
  lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(panel, 10, LV_PART_MAIN);

  g_uptime_label = vscode_lab_label(panel, "Running: 0 s",
                                    VSCODE_LAB_TEXT_COLOR);
  lv_obj_align(g_uptime_label, LV_ALIGN_LEFT_MID, 8, -12);

  g_count_label = vscode_lab_label(panel, "Touch count: 0",
                                   VSCODE_LAB_TEXT_COLOR);
  lv_obj_align(g_count_label, LV_ALIGN_LEFT_MID, 8, 12);

  hint = vscode_lab_label(panel, "NSH: COM7 / 115200",
                          VSCODE_LAB_MUTED_COLOR);
  lv_obj_align(hint, LV_ALIGN_RIGHT_MID, -8, 0);

  button = lv_button_create(screen);
  lv_obj_set_size(button, 200, 64);
  lv_obj_align(button, LV_ALIGN_BOTTOM_LEFT, 20, -45);
  lv_obj_set_style_bg_color(button, lv_color_hex(VSCODE_LAB_ACCENT_COLOR),
                            LV_PART_MAIN);
  lv_obj_add_event_cb(button, vscode_lab_button_event, LV_EVENT_CLICKED,
                      NULL);

  button_label = vscode_lab_label(button, "Breakpoint +1",
                                  VSCODE_LAB_BG_COLOR);
  lv_obj_center(button_label);

  g_slider_label = vscode_lab_label(screen, "Slider: 50",
                                    VSCODE_LAB_TEXT_COLOR);
  lv_obj_align(g_slider_label, LV_ALIGN_BOTTOM_RIGHT, -88, -92);

  slider = lv_slider_create(screen);
  lv_obj_set_size(slider, 200, 20);
  lv_obj_align(slider, LV_ALIGN_BOTTOM_RIGHT, -20, -65);
  lv_slider_set_range(slider, 0, 100);
  lv_slider_set_value(slider, 50, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(slider, lv_color_hex(VSCODE_LAB_ACCENT_COLOR),
                            LV_PART_INDICATOR);
  lv_obj_add_event_cb(slider, vscode_lab_slider_event, LV_EVENT_ALL, NULL);

  hint = vscode_lab_label(screen,
                          "F5 attach, set breakpoint, then touch the button",
                          VSCODE_LAB_MUTED_COLOR);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -12);

  lv_timer_create(vscode_lab_uptime_timer, 1000, NULL);
}

static int vscode_lab_ui_main(int argc, FAR char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;

  UNUSED(argc);
  UNUSED(argv);

  lv_init();
  lv_nuttx_dsc_init(&info);
  info.input_path =
    CONFIG_LVX_USE_DEMO_CONTEST2026_004_VSCODE_LAB_INPUT_PATH;
  lv_nuttx_init(&info, &result);

  if (result.disp == NULL)
    {
      printf("[vscode_lab] ERROR: LVGL display initialization failed\n");
      lv_nuttx_deinit(&result);
      lv_deinit();
      return 1;
    }

  vscode_lab_create_ui();
  printf("[vscode_lab] UI ready; touch and debug experiment started\n");

  while (true)
    {
      uint32_t idle = lv_timer_handler();

      usleep((idle > 0 ? idle : 1) * 1000);
    }

  return 0;
}

/****************************************************************************
 * Name: main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int pid;

  if (g_vscode_lab_started)
    {
      printf("[vscode_lab] already running\n");
      return 0;
    }

  g_vscode_lab_started = true;

  /* nsh_initialize() performs BOARDIOC_INIT on this configuration.  The
   * LCD and touchscreen must exist before the UI task starts.
   */

  nsh_initialize();

  pid = task_create("vscode_lab_ui",
                    CONFIG_LVX_USE_DEMO_CONTEST2026_004_VSCODE_LAB_PRIORITY,
                    CONFIG_LVX_USE_DEMO_CONTEST2026_004_VSCODE_LAB_STACKSIZE,
                    vscode_lab_ui_main, NULL);
  if (pid < 0)
    {
      printf("[vscode_lab] ERROR: UI task creation failed: %d\n", errno);
    }
  else
    {
      printf("[vscode_lab] UI task started with pid %d\n", pid);
    }

  return nsh_consolemain(argc, argv);
}
