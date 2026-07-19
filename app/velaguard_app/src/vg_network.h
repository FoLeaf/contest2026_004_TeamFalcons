/****************************************************************************
 * app/velaguard_app/src/vg_network.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef APP_VELAGUARD_APP_SRC_VG_NETWORK_H
#define APP_VELAGUARD_APP_SRC_VG_NETWORK_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <netinet/in.h>
#include <stdbool.h>

#include "vg_config.h"

/****************************************************************************
 * Public Types
 ****************************************************************************/

enum vg_network_state
{
  VG_NETWORK_DOWN = 0,
  VG_NETWORK_ADDRESSING,
  VG_NETWORK_ONLINE,
  VG_NETWORK_APPLYING,
  VG_NETWORK_ERROR
};

struct vg_network_status
{
  enum vg_network_state state;
  enum vg_network_mode mode;
  bool carrier;
  char ipv4[INET_ADDRSTRLEN];
  char message[64];      /* Operational summary for home/network status row. */
  char feedback[64];     /* Sticky apply result until next apply attempt. */
  int last_error;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int vg_network_start(void);
void vg_network_get_status(FAR struct vg_network_status *status);
void vg_network_get_config(FAR struct vg_network_config *config);
int vg_network_request_apply(FAR const struct vg_network_config *config);

#endif /* APP_VELAGUARD_APP_SRC_VG_NETWORK_H */
