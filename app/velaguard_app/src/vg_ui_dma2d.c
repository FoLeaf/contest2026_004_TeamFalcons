/****************************************************************************
 * app/velaguard_app/src/vg_ui_dma2d.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <nuttx/arch.h>
#include <nuttx/clock.h>
#include <nuttx/irq.h>
#include <nuttx/semaphore.h>

#include <arch/irq.h>

#include <lvgl/lvgl.h>
#include <lvgl/src/draw/lv_draw.h>
#include <lvgl/src/draw/lv_draw_rect.h>
#include <lvgl/src/draw/sw/lv_draw_sw.h>

#include "vg_ui_dma2d.h"

#ifdef CONFIG_VG_STM32H7_DMA2D

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VG_DMA2D_BASE             0x52001000u
#define VG_DMA2D_CR               (VG_DMA2D_BASE + 0x0000u)
#define VG_DMA2D_ISR              (VG_DMA2D_BASE + 0x0004u)
#define VG_DMA2D_IFCR             (VG_DMA2D_BASE + 0x0008u)
#define VG_DMA2D_FGMAR            (VG_DMA2D_BASE + 0x000cu)
#define VG_DMA2D_FGOR             (VG_DMA2D_BASE + 0x0010u)
#define VG_DMA2D_FGPFCCR          (VG_DMA2D_BASE + 0x001cu)
#define VG_DMA2D_OPFCCR           (VG_DMA2D_BASE + 0x0034u)
#define VG_DMA2D_OCOLR            (VG_DMA2D_BASE + 0x0038u)
#define VG_DMA2D_OMAR             (VG_DMA2D_BASE + 0x003cu)
#define VG_DMA2D_OOR              (VG_DMA2D_BASE + 0x0040u)
#define VG_DMA2D_NLR              (VG_DMA2D_BASE + 0x0044u)

#define VG_RCC_BASE               0x58024400u
#define VG_RCC_AHB3RSTR           (VG_RCC_BASE + 0x007cu)
#define VG_RCC_AHB3ENR            (VG_RCC_BASE + 0x00d4u)
#define VG_RCC_DMA2D              (1u << 4)

#define VG_DMA2D_CR_START         (1u << 0)
#define VG_DMA2D_CR_ABORT         (1u << 2)
#define VG_DMA2D_CR_TEIE          (1u << 8)
#define VG_DMA2D_CR_TCIE          (1u << 9)
#define VG_DMA2D_CR_CEIE          (1u << 13)
#define VG_DMA2D_CR_M2M           (0u << 16)
#define VG_DMA2D_CR_R2M           (3u << 16)

#define VG_DMA2D_ISR_ERROR        ((1u << 0) | (1u << 3) | (1u << 5))
#define VG_DMA2D_ISR_COMPLETE     (1u << 1)
#define VG_DMA2D_IFCR_ALL         0x3fu
#define VG_DMA2D_PF_RGB565        2u

#define VG_DMA2D_DRAW_UNIT_ID     7u
#define VG_DMA2D_MIN_PIXELS       256u
#define VG_DMA2D_COPY_MIN_PIXELS  1024u
#define VG_DMA2D_TIMEOUT_MS       20u
#define VG_DMA2D_REPORT_US        5000000u

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct vg_dma2d_unit_s
{
  lv_draw_unit_t base;
  FAR lv_draw_task_t *task;
  uint64_t frame_run_us;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static sem_t g_dma2d_sem;
static volatile int g_dma2d_result;
static bool g_dma2d_ready;
static uint32_t g_dma2d_hw_tasks;
static uint32_t g_dma2d_fallbacks;
static uint32_t g_dma2d_errors;
static uint32_t g_dma2d_fill_pixels;
static uint32_t g_dma2d_copy_tasks;
static uint32_t g_dma2d_copy_pixels;
static uint64_t g_dma2d_total_us;
static uint64_t g_dma2d_last_report_us;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t vg_dma2d_getreg(uintptr_t address)
{
  return *(FAR volatile uint32_t *)address;
}

static void vg_dma2d_putreg(uint32_t value, uintptr_t address)
{
  *(FAR volatile uint32_t *)address = value;
}

static uint64_t vg_dma2d_time_us(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

static void vg_dma2d_reset(void)
{
  uint32_t regval = vg_dma2d_getreg(VG_RCC_AHB3RSTR);

  vg_dma2d_putreg(regval | VG_RCC_DMA2D, VG_RCC_AHB3RSTR);
  vg_dma2d_putreg(regval & ~VG_RCC_DMA2D, VG_RCC_AHB3RSTR);
  vg_dma2d_putreg(VG_DMA2D_IFCR_ALL, VG_DMA2D_IFCR);
}

static int vg_dma2d_interrupt(int irq, FAR void *context, FAR void *arg)
{
  uint32_t status = vg_dma2d_getreg(VG_DMA2D_ISR);

  UNUSED(irq);
  UNUSED(context);
  UNUSED(arg);

  vg_dma2d_putreg(status & VG_DMA2D_IFCR_ALL, VG_DMA2D_IFCR);
  g_dma2d_result = (status & VG_DMA2D_ISR_ERROR) != 0 ? -EIO : OK;
  nxsem_post(&g_dma2d_sem);
  return OK;
}

static int vg_dma2d_wait(void)
{
  int ret = nxsem_tickwait_uninterruptible(&g_dma2d_sem,
                                           MSEC2TICK(VG_DMA2D_TIMEOUT_MS));

  if (ret < 0)
    {
      vg_dma2d_putreg(vg_dma2d_getreg(VG_DMA2D_CR) | VG_DMA2D_CR_ABORT,
                      VG_DMA2D_CR);
      vg_dma2d_reset();
      return ret;
    }

  return g_dma2d_result;
}

static void vg_dma2d_prepare(void)
{
  while (nxsem_trywait(&g_dma2d_sem) == OK)
    {
    }

  vg_dma2d_putreg(VG_DMA2D_IFCR_ALL, VG_DMA2D_IFCR);
  g_dma2d_result = -EINPROGRESS;
}

static int vg_dma2d_fill(FAR lv_draw_unit_t *draw_unit,
                         FAR const lv_draw_fill_dsc_t *dsc,
                         FAR const lv_area_t *coords)
{
  FAR lv_layer_t *layer = draw_unit->target_layer;
  FAR lv_draw_buf_t *draw_buf = layer->draw_buf;
  lv_area_t area;
  uint32_t stride_pixels;
  uintptr_t address;
  uintptr_t cache_start;
  uintptr_t cache_end;
  uint32_t width;
  uint32_t height;
  int ret;

  if (!_lv_area_intersect(&area, coords, draw_unit->clip_area))
    {
      return OK;
    }

  lv_area_move(&area, -layer->buf_area.x1, -layer->buf_area.y1);
  width = lv_area_get_width(&area);
  height = lv_area_get_height(&area);
  stride_pixels = draw_buf->header.stride / sizeof(uint16_t);
  address = (uintptr_t)draw_buf->data +
            area.y1 * draw_buf->header.stride +
            area.x1 * sizeof(uint16_t);
  cache_start = address;
  cache_end = address + (height - 1) * draw_buf->header.stride +
              width * sizeof(uint16_t);

#ifndef CONFIG_ARMV7M_DCACHE_WRITETHROUGH
  up_flush_dcache(cache_start, cache_end);
#endif
  vg_dma2d_prepare();
  vg_dma2d_putreg(VG_DMA2D_PF_RGB565, VG_DMA2D_OPFCCR);
  vg_dma2d_putreg(lv_color_to_u16(dsc->color), VG_DMA2D_OCOLR);
  vg_dma2d_putreg(address, VG_DMA2D_OMAR);
  vg_dma2d_putreg(stride_pixels - width, VG_DMA2D_OOR);
  vg_dma2d_putreg((width << 16) | height, VG_DMA2D_NLR);
  vg_dma2d_putreg(VG_DMA2D_CR_R2M | VG_DMA2D_CR_TEIE |
                  VG_DMA2D_CR_TCIE | VG_DMA2D_CR_CEIE |
                  VG_DMA2D_CR_START, VG_DMA2D_CR);

  ret = vg_dma2d_wait();
  if (ret >= 0)
    {
      up_invalidate_dcache(cache_start, cache_end);
    }

  return ret;
}

static bool vg_dma2d_copy(FAR lv_draw_buf_t *dest,
                          FAR const lv_area_t *dest_area,
                          FAR const lv_draw_buf_t *src,
                          FAR const lv_area_t *src_area)
{
  uint32_t width = dest_area == NULL ? dest->header.w :
                   lv_area_get_width(dest_area);
  uint32_t height = dest_area == NULL ? dest->header.h :
                    lv_area_get_height(dest_area);
  uint32_t src_x = src_area == NULL ? 0 : src_area->x1;
  uint32_t src_y = src_area == NULL ? 0 : src_area->y1;
  uint32_t dest_x = dest_area == NULL ? 0 : dest_area->x1;
  uint32_t dest_y = dest_area == NULL ? 0 : dest_area->y1;
  uint32_t src_stride = src->header.stride / sizeof(uint16_t);
  uint32_t dest_stride = dest->header.stride / sizeof(uint16_t);
  uintptr_t src_addr;
  uintptr_t dest_addr;
  uintptr_t src_end;
  uintptr_t dest_end;
  uint64_t start;
  int ret;

  if (!g_dma2d_ready || dest->header.cf != LV_COLOR_FORMAT_RGB565 ||
      src->header.cf != LV_COLOR_FORMAT_RGB565 ||
      width * height < VG_DMA2D_COPY_MIN_PIXELS)
    {
      return false;
    }

  src_addr = (uintptr_t)src->data + src_y * src->header.stride +
             src_x * sizeof(uint16_t);
  dest_addr = (uintptr_t)dest->data + dest_y * dest->header.stride +
              dest_x * sizeof(uint16_t);
  src_end = src_addr + (height - 1) * src->header.stride +
            width * sizeof(uint16_t);
  dest_end = dest_addr + (height - 1) * dest->header.stride +
             width * sizeof(uint16_t);

#ifndef CONFIG_ARMV7M_DCACHE_WRITETHROUGH
  up_flush_dcache(src_addr, src_end);
  up_flush_dcache(dest_addr, dest_end);
#endif
  vg_dma2d_prepare();
  vg_dma2d_putreg(src_addr, VG_DMA2D_FGMAR);
  vg_dma2d_putreg(src_stride - width, VG_DMA2D_FGOR);
  vg_dma2d_putreg(VG_DMA2D_PF_RGB565, VG_DMA2D_FGPFCCR);
  vg_dma2d_putreg(VG_DMA2D_PF_RGB565, VG_DMA2D_OPFCCR);
  vg_dma2d_putreg(dest_addr, VG_DMA2D_OMAR);
  vg_dma2d_putreg(dest_stride - width, VG_DMA2D_OOR);
  vg_dma2d_putreg((width << 16) | height, VG_DMA2D_NLR);

  start = vg_dma2d_time_us();
  vg_dma2d_putreg(VG_DMA2D_CR_M2M | VG_DMA2D_CR_TEIE |
                  VG_DMA2D_CR_TCIE | VG_DMA2D_CR_CEIE |
                  VG_DMA2D_CR_START, VG_DMA2D_CR);
  ret = vg_dma2d_wait();
  g_dma2d_total_us += vg_dma2d_time_us() - start;
  if (ret < 0)
    {
      g_dma2d_errors++;
      g_dma2d_fallbacks++;
      return false;
    }

  up_invalidate_dcache(dest_addr, dest_end);
  g_dma2d_copy_tasks++;
  g_dma2d_copy_pixels += width * height;
  return true;
}

static int32_t vg_dma2d_evaluate(FAR lv_draw_unit_t *draw_unit,
                                 FAR lv_draw_task_t *task)
{
  FAR const lv_draw_dsc_base_t *base = task->draw_dsc;
  FAR const lv_draw_fill_dsc_t *fill;
  uint32_t pixels;

  UNUSED(draw_unit);

  if (task->type != LV_DRAW_TASK_TYPE_FILL ||
      base->layer->color_format != LV_COLOR_FORMAT_RGB565)
    {
      return 0;
    }

  fill = task->draw_dsc;
  pixels = lv_area_get_size(&task->area);
  if (pixels < VG_DMA2D_MIN_PIXELS || fill->opa < LV_OPA_MAX ||
      fill->radius != 0 || fill->grad.dir != LV_GRAD_DIR_NONE)
    {
      return 0;
    }

  if (task->preference_score > 70)
    {
      task->preference_score = 70;
      task->preferred_draw_unit_id = VG_DMA2D_DRAW_UNIT_ID;
    }

  return 1;
}

static int32_t vg_dma2d_dispatch(FAR lv_draw_unit_t *draw_unit,
                                 FAR lv_layer_t *layer)
{
  FAR struct vg_dma2d_unit_s *unit =
    (FAR struct vg_dma2d_unit_s *)draw_unit;
  FAR lv_draw_task_t *task;
  uint64_t start;
  int ret;

  if (unit->task != NULL)
    {
      return 0;
    }

  task = lv_draw_get_next_available_task(layer, NULL,
                                         VG_DMA2D_DRAW_UNIT_ID);
  if (task == NULL ||
      task->preferred_draw_unit_id != VG_DMA2D_DRAW_UNIT_ID)
    {
      return -1;
    }

  if (lv_draw_layer_alloc_buf(layer) == NULL)
    {
      return -1;
    }

  task->state = LV_DRAW_TASK_STATE_IN_PROGRESS;
  draw_unit->target_layer = layer;
  draw_unit->clip_area = &task->clip_area;
  unit->task = task;
  start = vg_dma2d_time_us();
  ret = vg_dma2d_fill(draw_unit, task->draw_dsc, &task->area);
  start = vg_dma2d_time_us() - start;
  unit->frame_run_us += start;
  g_dma2d_total_us += start;

  if (ret < 0)
    {
      g_dma2d_errors++;
      g_dma2d_fallbacks++;
      lv_draw_sw_fill(draw_unit, task->draw_dsc, &task->area);
    }
  else
    {
      g_dma2d_hw_tasks++;
      g_dma2d_fill_pixels += lv_area_get_size(&task->area);
    }

  task->state = LV_DRAW_TASK_STATE_READY;
  unit->task = NULL;
  lv_draw_dispatch_request();
  return 1;
}

static void vg_dma2d_event(FAR lv_event_t *event)
{
  FAR struct vg_dma2d_unit_s *unit = lv_event_get_current_target(event);

  if (lv_event_get_code(event) == LV_EVENT_REFR_START)
    {
      unit->frame_run_us = 0;
    }
  else if (lv_event_get_code(event) == LV_EVENT_RENDER_READY &&
           lv_event_get_param(event) != NULL)
    {
      FAR lv_value_precise_t *run_time = lv_event_get_param(event);
      uint64_t now = vg_dma2d_time_us();

      *run_time += (lv_value_precise_t)((unit->frame_run_us + 500u) / 1000u);
      if (now - g_dma2d_last_report_us >= VG_DMA2D_REPORT_US)
        {
          printf("[velaguard][dma2d] fill=%" PRIu32
                 " fill_px=%" PRIu32 " copy=%" PRIu32
                 " copy_px=%" PRIu32 " fallback=%" PRIu32
                 " errors=%" PRIu32 " frame_gpu_us=%" PRIu64
                 " total_us=%" PRIu64 "\n",
                 g_dma2d_hw_tasks, g_dma2d_fill_pixels,
                 g_dma2d_copy_tasks, g_dma2d_copy_pixels,
                 g_dma2d_fallbacks, g_dma2d_errors,
                 unit->frame_run_us, g_dma2d_total_us);
          g_dma2d_last_report_us = now;
        }
    }
}

#endif /* CONFIG_VG_STM32H7_DMA2D */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int vg_ui_dma2d_init(void)
{
#ifdef CONFIG_VG_STM32H7_DMA2D
  FAR struct vg_dma2d_unit_s *unit;
  uint32_t regval;
  int ret;

  if (g_dma2d_ready)
    {
      return OK;
    }

  regval = vg_dma2d_getreg(VG_RCC_AHB3ENR);
  vg_dma2d_putreg(regval | VG_RCC_DMA2D, VG_RCC_AHB3ENR);
  vg_dma2d_reset();
  nxsem_init(&g_dma2d_sem, 0, 0);
  nxsem_set_protocol(&g_dma2d_sem, SEM_PRIO_NONE);
  ret = irq_attach(STM32_IRQ_DMA2D, vg_dma2d_interrupt, NULL);
  if (ret < 0)
    {
      fprintf(stderr, "[velaguard][dma2d] irq attach failed: %d\n", ret);
      return ret;
    }

  up_enable_irq(STM32_IRQ_DMA2D);
  unit = lv_draw_create_unit(sizeof(*unit));
  if (unit == NULL)
    {
      up_disable_irq(STM32_IRQ_DMA2D);
      irq_detach(STM32_IRQ_DMA2D);
      return -ENOMEM;
    }

  unit->base.evaluate_cb = vg_dma2d_evaluate;
  unit->base.dispatch_cb = vg_dma2d_dispatch;
  unit->base.event_cb = vg_dma2d_event;
  unit->base.name = "STM32_DMA2D";
  g_dma2d_ready = true;
  lv_draw_buf_get_handlers()->copy_cb = vg_dma2d_copy;
  printf("[velaguard][dma2d] RGB565 fill and buffer-copy acceleration enabled\n");
#endif
  return OK;
}
