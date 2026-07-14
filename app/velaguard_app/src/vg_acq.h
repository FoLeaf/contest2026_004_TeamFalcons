/****************************************************************************
 * app/velaguard_app/src/vg_acq.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef APP_VELAGUARD_APP_SRC_VG_ACQ_H
#define APP_VELAGUARD_APP_SRC_VG_ACQ_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VG_ACQ_BACKEND_NONE  0
#define VG_ACQ_BACKEND_MOCK  1
#define VG_ACQ_BACKEND_UART  2

/****************************************************************************
 * Public Types
 ****************************************************************************/

enum vg_acq_quality
{
  VG_ACQ_Q_NONE = 0,
  VG_ACQ_Q_GOOD,
  VG_ACQ_Q_BAD
};

struct vg_acq_point
{
  FAR const char *name;
  FAR const char *unit;
  float value;
  enum vg_acq_quality quality;
  uint32_t age_ms;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int vg_acq_init(void);
void vg_acq_poll(void);
FAR const struct vg_acq_point *vg_acq_primary(void);
FAR const char *vg_acq_backend_name(void);
FAR const char *vg_acq_quality_string(enum vg_acq_quality quality);

#endif /* APP_VELAGUARD_APP_SRC_VG_ACQ_H */
