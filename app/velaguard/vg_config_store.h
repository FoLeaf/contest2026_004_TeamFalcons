/****************************************************************************
 * app/velaguard/vg_config_store.h
 *
 * Dual-slot + CRC32 + monotonic seq config store (FAT-safe commit).
 * Host-testable without NuttX.
 ****************************************************************************/

#ifndef __APP_VELAGUARD_VG_CONFIG_STORE_H
#define __APP_VELAGUARD_VG_CONFIG_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VG_CFG_SCHEMA_VERSION 1
#define VG_CFG_DEVICE_NAME_MAX 31

/* Default relative layout under mount (bring-up: /mnt/emmc/...). */
#define VG_CFG_DEFAULT_RELDIR "velaguard/config"
#define VG_CFG_SLOT_A_NAME    "point_table_a.json"
#define VG_CFG_SLOT_B_NAME    "point_table_b.json"

struct vg_config
{
  uint32_t schema_version;
  uint32_t seq;
  bool committed;
  char device_name[VG_CFG_DEVICE_NAME_MAX + 1];
};

/****************************************************************************
 * Name: vg_config_set_basedir
 *
 * Description:
 *   Set absolute directory for the two slot files (created on commit if
 *   needed).  Pass NULL to clear (tests).
 ****************************************************************************/

void vg_config_set_basedir(const char *dir);
const char *vg_config_get_basedir(void);
const char *vg_config_last_error(void);

/****************************************************************************
 * Name: vg_config_factory_default
 ****************************************************************************/

void vg_config_factory_default(struct vg_config *out);

/****************************************************************************
 * Name: vg_config_probe
 *
 * Description:
 *   Board bring-up: mkdir basedir, write/read probe.txt.  Fills msg.
 ****************************************************************************/

int vg_config_probe(char *msg, size_t msglen);

/****************************************************************************
 * Name: vg_config_load
 *
 * Description:
 *   Pick the valid slot with the largest seq.  If none valid, fill factory
 *   default and return 1.  On success return 0.  Negative = I/O error.
 ****************************************************************************/

int vg_config_load(struct vg_config *out);

/****************************************************************************
 * Name: vg_config_commit
 *
 * Description:
 *   Atomic dual-slot commit into the inactive slot with seq = max_valid+1.
 *   Returns 0 on success.
 ****************************************************************************/

int vg_config_commit(const struct vg_config *in);

/****************************************************************************
 * Name: vg_config_damage_slot
 *
 * Description:
 *   Test hook: slot 0 = A, 1 = B.  mode 0 truncate, 1 corrupt CRC.
 ****************************************************************************/

int vg_config_damage_slot(int slot, int mode);

#endif /* __APP_VELAGUARD_VG_CONFIG_STORE_H */
