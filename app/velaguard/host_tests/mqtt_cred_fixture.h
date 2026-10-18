/* SPDX-License-Identifier: Apache-2.0 */
/****************************************************************************
 * Shared fake crypto and file helpers for MQTT authentication tests.
 *
 * These tests use the real credential loader and the real MQTT entry points.
 * Only the device UID and the board crypto implementation are replaced, so
 * the record format, path handling, and unlock/wipe sequence remain the ones
 * used by the firmware.
 ****************************************************************************/

#ifndef VG_HOST_TEST_MQTT_CRED_FIXTURE_H
#define VG_HOST_TEST_MQTT_CRED_FIXTURE_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../vg_provision.h"

static int fixture_put_record(const char *path, const char *json)
{
  uint8_t blob[1024];
  size_t blob_len = 0;
  FILE *fp;

  if (vg_provision_crypto_seal(NULL, (const uint8_t *)json, strlen(json),
                               blob, sizeof(blob), &blob_len) != 0)
    {
      return -1;
    }

  fp = fopen(path, "wb");
  if (fp == NULL)
    {
      return -1;
    }

  if (fwrite(blob, 1, blob_len, fp) != blob_len)
    {
      fclose(fp);
      return -1;
    }

  return fclose(fp) == 0 ? 0 : -1;
}

static void fixture_clean_records(void)
{
  remove(VG_PROVISION_FILE);
  remove(VG_PROVISION_FILE ".tmp");
  remove(VG_PROVISION_FILE ".bak");
  remove(VG_PROVISION_FILE ".btmp");
  remove(VG_MQTT_CRED_FILE);
  remove(VG_MQTT_CRED_FILE ".tmp");
  remove(VG_MQTT_CRED_FILE ".bak");
  remove(VG_MQTT_CRED_FILE ".btmp");
}

int vg_device_id_read_uid(uint8_t uid[VG_PROVISION_UID_LEN])
{
  memset(uid, 0x11, VG_PROVISION_UID_LEN);
  return 0;
}

int vg_provision_crypto_seal(const uint8_t uid[VG_PROVISION_UID_LEN],
                             const uint8_t *plain, size_t plain_len,
                             uint8_t *out, size_t outsz, size_t *out_len)
{
  size_t i;

  (void)uid;
  if (plain == NULL || out == NULL || out_len == NULL ||
      plain_len > 255 || outsz < plain_len + 4)
    {
      return -1;
    }

  out[0] = 'T';
  out[1] = 'S';
  out[2] = 'T';
  out[3] = (uint8_t)plain_len;
  for (i = 0; i < plain_len; i++)
    {
      out[4 + i] = (uint8_t)(plain[i] ^ 0xa5);
    }

  *out_len = plain_len + 4;
  return 0;
}

int vg_provision_crypto_open(const uint8_t uid[VG_PROVISION_UID_LEN],
                             const uint8_t *blob, size_t blob_len,
                             uint8_t *plain, size_t plainsz,
                             size_t *plain_len)
{
  size_t n;
  size_t i;

  (void)uid;
  if (blob == NULL || plain == NULL || plain_len == NULL || blob_len < 4 ||
      blob[0] != 'T' || blob[1] != 'S' || blob[2] != 'T')
    {
      return -1;
    }

  n = blob[3];
  if (blob_len != n + 4 || plainsz <= n)
    {
      return -1;
    }

  for (i = 0; i < n; i++)
    {
      plain[i] = (uint8_t)(blob[4 + i] ^ 0xa5);
    }

  plain[n] = '\0';
  *plain_len = n;
  return 0;
}

#endif
