/****************************************************************************
 * app/velaguard_app/src/vg_config.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef APP_VELAGUARD_APP_SRC_VG_CONFIG_H
#define APP_VELAGUARD_APP_SRC_VG_CONFIG_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <netinet/in.h>
#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VG_NETWORK_CFG_PATH     "/data/velaguard/configs/network.json"
#define VG_NETWORK_CFG_TMP_PATH "/data/velaguard/configs/network.json.tmp"
#define VG_NETWORK_CFG_VERSION  1

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct vg_alarm_config
{
  float warn_c;
  float crit_c;
  float restore_c;
  unsigned int trigger_ms;
  unsigned int restore_ms;
  bool loaded_from_file;
};

enum vg_network_mode
{
  VG_NETWORK_MODE_DHCP = 0,
  VG_NETWORK_MODE_STATIC
};

struct vg_network_config
{
  int version;
  enum vg_network_mode mode;
  char ipv4[INET_ADDRSTRLEN];
  char netmask[INET_ADDRSTRLEN];
  char gateway[INET_ADDRSTRLEN];
  char dns[INET_ADDRSTRLEN];
  bool loaded_from_file;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Load /data/velaguard/configs/alarm.json or write defaults. */

int vg_config_load(void);
FAR const struct vg_alarm_config *vg_config_alarm(void);

/* Network configuration (network.json).  UI must not parse JSON itself. */

void vg_config_network_defaults(FAR struct vg_network_config *cfg);
int vg_config_network_load(FAR struct vg_network_config *cfg);
int vg_config_network_validate(FAR const struct vg_network_config *cfg);
int vg_config_network_write_tmp(FAR const struct vg_network_config *cfg);
int vg_config_network_commit(void);
int vg_config_network_abort_tmp(void);
int vg_config_network_save(FAR const struct vg_network_config *cfg);

#endif /* APP_VELAGUARD_APP_SRC_VG_CONFIG_H */
