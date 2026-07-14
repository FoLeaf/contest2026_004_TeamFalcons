/****************************************************************************
 * app/velaguard_app/src/vg_acq.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Acquisition seam: mock now, UART/Modbus later (issue #02).
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "vg_acq.h"
#include "vg_startup.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool g_acq_ready;
static struct vg_acq_point g_primary;
static uint32_t g_last_poll_ms;
static uint32_t g_mock_tick;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int vg_acq_backend(void)
{
#ifdef CONFIG_VG_ACQ_BACKEND
  return CONFIG_VG_ACQ_BACKEND;
#else
  return VG_ACQ_BACKEND_MOCK;
#endif
}

static void vg_acq_set_none(void)
{
  g_primary.name = "Acquisition";
  g_primary.unit = "--";
  g_primary.value = 0.0f;
  g_primary.quality = VG_ACQ_Q_NONE;
  g_primary.age_ms = 0;
}

static void vg_acq_mock_update(uint32_t now_ms)
{
  /* Triangle 40..85 C over 120 steps (~2 minutes at 1 Hz). No libm. */

  uint32_t step = g_mock_tick % 120u;
  float value;

  if (step <= 60u)
    {
      value = 40.0f + (45.0f * (float)step) / 60.0f;
    }
  else
    {
      value = 85.0f - (45.0f * (float)(step - 60u)) / 60.0f;
    }

  g_primary.name = "Mock Temperature";
  g_primary.unit = "C";
  g_primary.value = value;
  g_primary.quality = VG_ACQ_Q_GOOD;
  g_primary.age_ms = 0;
  g_last_poll_ms = now_ms;
  g_mock_tick++;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int vg_acq_init(void)
{
  g_acq_ready = true;
  g_last_poll_ms = 0;
  g_mock_tick = 0;
  vg_acq_set_none();

  if (vg_acq_backend() == VG_ACQ_BACKEND_MOCK)
    {
      vg_acq_mock_update(0);
    }

  return 0;
}

void vg_acq_poll(void)
{
  uint32_t now_ms;

  if (!g_acq_ready)
    {
      return;
    }

  now_ms = (uint32_t)vg_uptime_ms();

  if (vg_acq_backend() == VG_ACQ_BACKEND_MOCK)
    {
      if (g_last_poll_ms == 0 || (now_ms - g_last_poll_ms) >= 1000u)
        {
          vg_acq_mock_update(now_ms);
        }
      else
        {
          g_primary.age_ms = now_ms - g_last_poll_ms;
        }

      return;
    }

  if (vg_acq_backend() == VG_ACQ_BACKEND_UART)
    {
      /* Issue #02: open /dev/rs485 and nanoMODBUS. */

      vg_acq_set_none();
      g_primary.name = "UART pending";
      return;
    }

  vg_acq_set_none();
}

FAR const struct vg_acq_point *vg_acq_primary(void)
{
  return &g_primary;
}

FAR const char *vg_acq_backend_name(void)
{
  switch (vg_acq_backend())
    {
      case VG_ACQ_BACKEND_MOCK:
        return "MOCK";

      case VG_ACQ_BACKEND_UART:
        return "UART";

      default:
        return "NONE";
    }
}

FAR const char *vg_acq_quality_string(enum vg_acq_quality quality)
{
  switch (quality)
    {
      case VG_ACQ_Q_GOOD:
        return "GOOD";

      case VG_ACQ_Q_BAD:
        return "BAD";

      default:
        return "NONE";
    }
}
