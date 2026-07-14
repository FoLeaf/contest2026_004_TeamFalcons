/****************************************************************************
 * app/velaguard_app/src/vg_log.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef APP_VELAGUARD_APP_SRC_VG_LOG_H
#define APP_VELAGUARD_APP_SRC_VG_LOG_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Append one human line to /data/velaguard/logs/latest.log */

int vg_log_human(FAR const char *level, FAR const char *message);

/* Append one compact JSON line to events.jsonl (type + summary + fields). */

int vg_log_event(FAR const char *type, FAR const char *summary,
                 FAR const char *extra_json_object_fields);

#endif /* APP_VELAGUARD_APP_SRC_VG_LOG_H */
