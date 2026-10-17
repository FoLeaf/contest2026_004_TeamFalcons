/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VG_FILE_STORE_H
#define VG_FILE_STORE_H

#include <stddef.h>
#include <sys/types.h>

/* Serialize writers, verify the complete temporary file before publishing,
 * and retain the previous bytes in path.bak. This is application-level
 * recovery, not a claim that FAT directory updates are power-fail atomic. */
int vg_file_replace(const char *path, const void *data, size_t len, mode_t mode);

/* Same transaction, but reject an existing destination under the writer lock. */
int vg_file_create(const char *path, const void *data, size_t len, mode_t mode);

#endif
