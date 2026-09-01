/****************************************************************************
 * Device-bound SHA256 stream + HMAC-SHA256 for provision blobs (v1).
 ****************************************************************************/

#include "vg_provision.h"

#include <errno.h>
#include <string.h>

#ifdef __NuttX__
#  include <mbedtls/sha256.h>
#endif

#define VGPR_MAGIC0 'V'
#define VGPR_MAGIC1 'G'
#define VGPR_MAGIC2 'P'
#define VGPR_MAGIC3 'R'
#define VGPR_VERSION 1

#define VGPR_HDR_SIZE 28
#define VGPR_TAG_SIZE 32
#define VGPR_NONCE_SIZE 16

static void derive_keys(const uint8_t uid[VG_PROVISION_UID_LEN],
                        uint8_t aes_key[32], uint8_t mac_key[32])
{
#ifdef __NuttX__
  mbedtls_sha256_context sha;
  uint8_t master[32];
  static const char info[] = "velaguard:provision:v1:";
  static const char mac_info[] = "mac";

  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  mbedtls_sha256_update(&sha, (const uint8_t *)info, sizeof(info) - 1);
  mbedtls_sha256_update(&sha, uid, VG_PROVISION_UID_LEN);
  mbedtls_sha256_finish(&sha, master);
  mbedtls_sha256_free(&sha);

  memcpy(aes_key, master, 32);

  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  mbedtls_sha256_update(&sha, master, sizeof(master));
  mbedtls_sha256_update(&sha, (const uint8_t *)mac_info, sizeof(mac_info) - 1);
  mbedtls_sha256_finish(&sha, mac_key);
  mbedtls_sha256_free(&sha);
#else
  (void)uid;
  memset(aes_key, 0x5a, 32);
  memset(mac_key, 0xa5, 32);
#endif
}

#ifndef __NuttX__

int vg_provision_crypto_seal(const uint8_t uid[VG_PROVISION_UID_LEN],
                             const uint8_t *plain, size_t plain_len,
                             uint8_t *out, size_t outsz, size_t *out_len)
{
  (void)uid;
  (void)plain;
  (void)plain_len;
  (void)out;
  (void)outsz;
  (void)out_len;
  return -ENOSYS;
}

int vg_provision_crypto_open(const uint8_t uid[VG_PROVISION_UID_LEN],
                             const uint8_t *blob, size_t blob_len,
                             uint8_t *plain, size_t plainsz, size_t *plain_len)
{
  (void)uid;
  (void)blob;
  (void)blob_len;
  (void)plain;
  (void)plainsz;
  (void)plain_len;
  return -ENOSYS;
}

#else

static int hmac_sha256(const uint8_t key[32], const uint8_t *msg, size_t msg_len,
                       uint8_t tag[32])
{
  mbedtls_sha256_context sha;
  uint8_t ipad[64];
  uint8_t opad[64];
  uint8_t inner[32];
  size_t i;

  memset(ipad, 0x36, sizeof(ipad));
  memset(opad, 0x5c, sizeof(opad));
  for (i = 0; i < 32; i++)
    {
      ipad[i] ^= key[i];
      opad[i] ^= key[i];
    }

  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  mbedtls_sha256_update(&sha, ipad, sizeof(ipad));
  mbedtls_sha256_update(&sha, msg, msg_len);
  mbedtls_sha256_finish(&sha, inner);
  mbedtls_sha256_free(&sha);

  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  mbedtls_sha256_update(&sha, opad, sizeof(opad));
  mbedtls_sha256_update(&sha, inner, sizeof(inner));
  mbedtls_sha256_finish(&sha, tag);
  mbedtls_sha256_free(&sha);
  return 0;
}

static void stream_xor(const uint8_t stream_key[32], const uint8_t nonce[16],
                       const uint8_t *in, uint8_t *out, size_t len)
{
  mbedtls_sha256_context sha;
  uint8_t block[32];
  uint32_t idx = 0;
  size_t off = 0;

  while (off < len)
    {
      uint8_t ctr[4];

      mbedtls_sha256_init(&sha);
      mbedtls_sha256_starts(&sha, 0);
      mbedtls_sha256_update(&sha, stream_key, 32);
      mbedtls_sha256_update(&sha, nonce, 16);
      ctr[0] = (uint8_t)(idx);
      ctr[1] = (uint8_t)(idx >> 8);
      ctr[2] = (uint8_t)(idx >> 16);
      ctr[3] = (uint8_t)(idx >> 24);
      mbedtls_sha256_update(&sha, ctr, sizeof(ctr));
      mbedtls_sha256_finish(&sha, block);
      mbedtls_sha256_free(&sha);

      for (size_t i = 0; i < 32 && off < len; i++, off++)
        {
          out[off] = in[off] ^ block[i];
        }

      idx++;
    }
}

int vg_provision_crypto_seal(const uint8_t uid[VG_PROVISION_UID_LEN],
                             const uint8_t *plain, size_t plain_len,
                             uint8_t *out, size_t outsz, size_t *out_len)
{
  uint8_t stream_key[32];
  uint8_t mac_key[32];
  uint8_t nonce[VGPR_NONCE_SIZE];
  size_t need;
  uint8_t tag[VGPR_TAG_SIZE];

  if (plain_len > 512 || plain == NULL || out == NULL || out_len == NULL)
    {
      return -EINVAL;
    }

  need = VGPR_HDR_SIZE + plain_len + VGPR_TAG_SIZE;
  if (outsz < need)
    {
      return -ENOSPC;
    }

  derive_keys(uid, stream_key, mac_key);

  for (size_t i = 0; i < sizeof(nonce); i++)
    {
      nonce[i] = (uint8_t)(plain_len ^ i ^ uid[i % VG_PROVISION_UID_LEN]);
    }

  out[0] = VGPR_MAGIC0;
  out[1] = VGPR_MAGIC1;
  out[2] = VGPR_MAGIC2;
  out[3] = VGPR_MAGIC3;
  out[4] = VGPR_VERSION;
  out[5] = out[6] = out[7] = 0;
  memcpy(out + 8, nonce, VGPR_NONCE_SIZE);
  out[24] = (uint8_t)(plain_len & 0xff);
  out[25] = (uint8_t)((plain_len >> 8) & 0xff);
  out[26] = out[27] = 0;

  stream_xor(stream_key, nonce, plain, out + VGPR_HDR_SIZE, plain_len);
  hmac_sha256(mac_key, out, VGPR_HDR_SIZE + plain_len, tag);
  memcpy(out + VGPR_HDR_SIZE + plain_len, tag, VGPR_TAG_SIZE);
  *out_len = need;
  return 0;
}

int vg_provision_crypto_open(const uint8_t uid[VG_PROVISION_UID_LEN],
                             const uint8_t *blob, size_t blob_len,
                             uint8_t *plain, size_t plainsz, size_t *plain_len)
{
  uint8_t stream_key[32];
  uint8_t mac_key[32];
  uint8_t tag[VGPR_TAG_SIZE];
  uint8_t expect[VGPR_TAG_SIZE];
  uint32_t ct_len;

  if (blob_len < VGPR_HDR_SIZE + VGPR_TAG_SIZE || blob == NULL ||
      plain == NULL || plain_len == NULL)
    {
      return -EINVAL;
    }

  if (blob[0] != VGPR_MAGIC0 || blob[1] != VGPR_MAGIC1 ||
      blob[2] != VGPR_MAGIC2 || blob[3] != VGPR_MAGIC3 ||
      blob[4] != VGPR_VERSION)
    {
      return -EINVAL;
    }

  ct_len = (uint32_t)blob[24] | ((uint32_t)blob[25] << 8);
  if (ct_len == 0 || ct_len > 512 ||
      blob_len != VGPR_HDR_SIZE + ct_len + VGPR_TAG_SIZE)
    {
      return -EINVAL;
    }

  if (plainsz < ct_len)
    {
      return -ENOSPC;
    }

  derive_keys(uid, stream_key, mac_key);
  memcpy(tag, blob + VGPR_HDR_SIZE + ct_len, VGPR_TAG_SIZE);
  hmac_sha256(mac_key, blob, VGPR_HDR_SIZE + ct_len, expect);
  if (memcmp(tag, expect, VGPR_TAG_SIZE) != 0)
    {
      return -EACCES;
    }

  stream_xor(stream_key, blob + 8, blob + VGPR_HDR_SIZE, plain, ct_len);
  *plain_len = ct_len;
  return 0;
}

#endif /* __NuttX__ */
