/****************************************************************************
 * VelaGuard eMMC provision store (encrypted LLM and MQTT credentials).
 ****************************************************************************/

#ifndef __VG_PROVISION_H
#define __VG_PROVISION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VG_PROVISION_UID_LEN     12
#define VG_PROVISION_HOST_MAX    96
#define VG_PROVISION_PATH_MAX    96
#define VG_PROVISION_PORT_MAX    8
#define VG_PROVISION_MODEL_MAX   32
#define VG_PROVISION_API_KEY_MAX 192

/* Largest plaintext the crypto layer will seal.  vg_provision_crypto_seal()
 * enforces the same bound; keeping it here lets the credential build report an
 * oversized blob itself instead of failing inside the seal.
 */

#define VG_PROVISION_PLAIN_MAX   512

/* Largest sealed blob the crypto layer produces for that plaintext: header,
 * ciphertext and tag.  Buffers that hold a whole blob are sized from it.
 */

#define VG_PROVISION_BLOB_MAX    768

/* Both file names are overridable so the host tests can point the store at
 * their own working directory instead of /data.
 */

#ifndef VG_PROVISION_FILE
#  define VG_PROVISION_FILE      "/data/velaguard/provision/llm_secrets.v1"
#endif

struct vg_llm_credentials
{
  char host[VG_PROVISION_HOST_MAX];
  char path[VG_PROVISION_PATH_MAX];
  char port[VG_PROVISION_PORT_MAX];
  char model[VG_PROVISION_MODEL_MAX];
  char api_key[VG_PROVISION_API_KEY_MAX];
};

int vg_provision_read_uid(uint8_t uid[VG_PROVISION_UID_LEN]);

bool vg_provision_is_present(void);

int vg_provision_seal(const struct vg_llm_credentials *cred);

int vg_provision_load(struct vg_llm_credentials *out);

int vg_provision_apply_llm_config(void);

void vg_provision_boot_apply_if_needed(void);

/* 0 when credits written to `path` would survive a reboot, -ENODEV when the
 * mount is RAM-backed (tmpfs or the pseudo filesystem), other negative on a
 * statfs failure.  Callers use this to refuse a write that only looks
 * successful because /data fell back to RAM.
 */

int vg_provision_store_is_persistent(const char *path);

/* True when /data/agent/config/config.json carries a non-empty host and key,
 * i.e. the agent has a backend it can reach.  False means every AI request
 * will end without a network call, and callers should say so rather than
 * blaming the feature that consumes the answer.
 */

bool vg_llm_credentials_ready(void);

/* The same test applied to JSON already in hand, so it can be exercised
 * without a board: the real predicate reads a fixed /data path.
 */

bool vg_provision_creds_ready_in(const char *json);

int vg_provision_parse_endpoint(const char *endpoint,
                                char *host, size_t hostsz,
                                char *path, size_t pathsz,
                                char *port, size_t portsz);

/* --- MQTT broker credentials ---------------------------------------------
 *
 * A second record in the same store, in its own sealed file.  It is kept
 * apart from the LLM record because the two have different lifetimes: the LLM
 * record is applied to /data/agent/config/config.json once per boot, while
 * the MQTT record is read every time the network manager opens a session.
 * Separate files also mean neither can be written over the other.
 *
 * The plaintext of this record carries a "kind" tag that the LLM record does
 * not have, so a blob moved between the two files is refused by both readers
 * instead of being accepted as the wrong credential.  The tag was added here
 * rather than to the LLM record so boards already provisioned keep working.
 */

#ifndef VG_MQTT_CRED_FILE
#  define VG_MQTT_CRED_FILE      "/data/velaguard/provision/mqtt_secrets.v1"
#endif

#define VG_MQTT_CRED_KIND        "mqtt"

/* Buffer sizes, terminator included.  The JSON scanner in vg_provision.c
 * needs one spare byte past the last character it accepts, so the longest
 * usable value is one byte shorter than the buffer.
 */

#define VG_MQTT_CRED_USER_MAX    64
#define VG_MQTT_CRED_PASS_MAX    128

struct vg_mqtt_credentials
{
  char username[VG_MQTT_CRED_USER_MAX];
  char password[VG_MQTT_CRED_PASS_MAX];
};

/* Parse decrypted MQTT credential JSON.  A record whose kind tag is absent or
 * wrong, or whose fields are empty, is refused: that is what stops an LLM
 * record from being read as an MQTT one.
 */

int vg_mqtt_cred_parse(const char *plain, struct vg_mqtt_credentials *out);

/* Decrypt a sealed blob through the same path the session uses, and only then
 * publish it as the MQTT record.  A truncated or foreign push is refused
 * before the working record is touched.
 */

int vg_mqtt_cred_install(const void *blob, size_t blob_len);

/* Read the record.  Negative on anything that leaves the caller without
 * usable credentials: -ENOENT when there is no record, -EACCES when the
 * integrity check fails, -EINVAL when the plaintext is not an MQTT record.
 * `out` is zeroed on failure, so a caller that ignores the return value still
 * cannot connect with half a credential.
 */

int vg_mqtt_cred_load(struct vg_mqtt_credentials *out);

bool vg_mqtt_cred_present(void);

/* One line describing the record without disclosing it: presence, a masked
 * user name and the password length.  The password itself is never written,
 * and there is no accessor that returns it other than vg_mqtt_cred_load(),
 * whose callers are the two MQTT entry points.
 */

int vg_mqtt_cred_status(char *out, size_t outsz);

void vg_mqtt_cred_mask_user(const char *user, char *out, size_t outsz);

int vg_provision_crypto_seal(const uint8_t uid[VG_PROVISION_UID_LEN],
                             const uint8_t *plain, size_t plain_len,
                             uint8_t *out, size_t outsz, size_t *out_len);

int vg_provision_crypto_open(const uint8_t uid[VG_PROVISION_UID_LEN],
                             const uint8_t *blob, size_t blob_len,
                             uint8_t *plain, size_t plainsz, size_t *plain_len);

#endif /* __VG_PROVISION_H */
