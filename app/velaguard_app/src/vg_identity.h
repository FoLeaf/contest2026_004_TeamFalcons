/****************************************************************************
 * app/velaguard_app/src/vg_identity.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef APP_VELAGUARD_APP_SRC_VG_IDENTITY_H
#define APP_VELAGUARD_APP_SRC_VG_IDENTITY_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VG_DEVICE_ID_MAX 40

/****************************************************************************
 * Public Types
 ****************************************************************************/

enum vg_build_mode
{
  VG_BUILD_TEST = 0,
  VG_BUILD_PRODUCTION = 1
};

struct vg_identity
{
  char device_id[VG_DEVICE_ID_MAX];
  enum vg_build_mode build_mode;
  FAR const char *firmware_version;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int vg_identity_init(void);
FAR const struct vg_identity *vg_identity_get(void);
bool vg_identity_valid(void);
FAR const char *vg_build_mode_string(enum vg_build_mode mode);

#endif /* APP_VELAGUARD_APP_SRC_VG_IDENTITY_H */
