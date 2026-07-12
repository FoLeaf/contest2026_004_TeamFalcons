/****************************************************************************
 * app/velaguard_app/src/vg_identity.c
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
#include <string.h>

#include "vg_identity.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define STM32H750_UID_ADDRESS 0x1ff1e800u
#define STM32H750_UID_SIZE    12

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct vg_identity g_identity;
static bool g_identity_valid;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static bool vg_device_id_character_valid(char value)
{
  return (value >= 'a' && value <= 'z') ||
         (value >= 'A' && value <= 'Z') ||
         (value >= '0' && value <= '9') ||
         value == '-' || value == '_';
}

static bool vg_device_id_valid(FAR const char *device_id)
{
  size_t length;
  size_t index;

  if (device_id == NULL)
    {
      return false;
    }

  length = strlen(device_id);
  if (length == 0 || length >= VG_DEVICE_ID_MAX)
    {
      return false;
    }

  for (index = 0; index < length; index++)
    {
      if (!vg_device_id_character_valid(device_id[index]))
        {
          return false;
        }
    }

  return true;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int vg_identity_init(void)
{
  memset(&g_identity, 0, sizeof(g_identity));
  g_identity.firmware_version = CONFIG_VG_FIRMWARE_VERSION;

#if CONFIG_VG_BUILD_MODE == 0
  g_identity.build_mode = VG_BUILD_TEST;
  if (!vg_device_id_valid(CONFIG_VG_DEVICE_ID_OVERRIDE))
    {
      snprintf(g_identity.device_id, sizeof(g_identity.device_id),
               "identity-error");
      g_identity_valid = false;
      return -EINVAL;
    }

  snprintf(g_identity.device_id, sizeof(g_identity.device_id), "%s",
           CONFIG_VG_DEVICE_ID_OVERRIDE);
#else
  FAR const volatile uint8_t *uid =
    (FAR const volatile uint8_t *)STM32H750_UID_ADDRESS;
  size_t index;

  g_identity.build_mode = VG_BUILD_PRODUCTION;
  snprintf(g_identity.device_id, sizeof(g_identity.device_id), "vg-");
  for (index = 0; index < STM32H750_UID_SIZE; index++)
    {
      snprintf(&g_identity.device_id[3 + index * 2], 3, "%02x", uid[index]);
    }
#endif

  g_identity.device_id[VG_DEVICE_ID_MAX - 1] = '\0';
  g_identity_valid = true;
  return 0;
}

FAR const struct vg_identity *vg_identity_get(void)
{
  return &g_identity;
}

bool vg_identity_valid(void)
{
  return g_identity_valid;
}

FAR const char *vg_build_mode_string(enum vg_build_mode mode)
{
  return mode == VG_BUILD_PRODUCTION ? "PRODUCTION" : "TEST";
}
