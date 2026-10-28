/****************************************************************************
 * VelaGuard eMMC provision store (encrypted LLM credentials).
 ****************************************************************************/

#include "vg_provision.h"
#include "vg_device_id.h"
#include "vg_file_store.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <unistd.h>

#ifdef __NuttX__
#  include <nuttx/config.h>
#else
#  include <sys/vfs.h>
#endif

/* Overridable for the same reason as the record paths: the host tests run
 * against their own directory rather than /data. */

#ifndef VG_PROVISION_DIR
#  define VG_PROVISION_DIR "/data/velaguard/provision"
#endif

/* NuttX declares these in <sys/statfs.h>; the desktop libc used by the host
 * tests does not, and the values are the same on both.
 */

#ifndef PROC_SUPER_MAGIC
#  define PROC_SUPER_MAGIC 0x9fa0
#endif

#ifndef TMPFS_MAGIC
#  define TMPFS_MAGIC 0x01021994
#endif

/* The credentials only survive a reboot if /data is the eMMC volume.  The
 * agent mounts tmpfs on /data when stat("/data") fails, so a boot order or
 * eMMC fault silently turns every later write into a RAM write.  Writing
 * credentials there looks successful, reports a healthy status, and is gone
 * after the next restart - exactly the state the board was found in on
 * 2026-09-17, where provision/ and config.json were both absent while the
 * sealed blob had been "written" days earlier.
 */

int vg_provision_store_is_persistent(const char *path)
{
  struct statfs st;

  if (path == NULL || path[0] == '\0')
    {
      return -EINVAL;
    }

  if (statfs(path, &st) != 0)
    {
      return (errno != 0) ? -errno : -EIO;
    }

  if (st.f_type == TMPFS_MAGIC || st.f_type == PROC_SUPER_MAGIC)
    {
      return -ENODEV;
    }

  return 0;
}

static int json_escape(const char *in, char *out, size_t outsz)
{
  size_t o = 0;

  if (outsz == 0)
    {
      return -EINVAL;
    }

  for (; *in != '\0'; in++)
    {
      char c = *in;

      if (c == '"' || c == '\\')
        {
          if (o + 2 >= outsz)
            {
              return -ENOSPC;
            }

          out[o++] = '\\';
        }

      if (o + 1 >= outsz)
        {
          return -ENOSPC;
        }

      out[o++] = c;
    }

  out[o] = '\0';
  return 0;
}

static int json_get_string(const char *json, const char *key,
                           char *out, size_t outsz)
{
  char pattern[48];
  const char *p;
  const char *start;
  size_t i;

  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  p = strstr(json, pattern);
  if (p == NULL)
    {
      return -ENOENT;
    }

  p = strchr(p, ':');
  if (p == NULL)
    {
      return -EINVAL;
    }

  p++;
  while (*p == ' ' || *p == '\t')
    {
      p++;
    }

  if (*p != '"')
    {
      return -EINVAL;
    }

  start = p + 1;
  for (i = 0; start[i] != '\0'; i++)
    {
      if (start[i] == '"' && (i == 0 || start[i - 1] != '\\'))
        {
          /* i characters plus the terminator.  The bound used to be i + 1,
           * which refused a value that exactly filled the buffer and left
           * every caller with an effective limit one byte below its array. */

          if (i >= outsz)
            {
              return -ENOSPC;
            }

          memcpy(out, start, i);
          out[i] = '\0';
          return 0;
        }
    }

  return -EINVAL;
}

int vg_provision_read_uid(uint8_t uid[VG_PROVISION_UID_LEN])
{
  return vg_device_id_read_uid(uid);
}

int vg_provision_parse_endpoint(const char *endpoint,
                                char *host, size_t hostsz,
                                char *path, size_t pathsz,
                                char *port, size_t portsz)
{
  const char *p;
  const char *slash;
  const char *host_start;
  size_t host_len;

  if (endpoint == NULL || host == NULL || path == NULL || port == NULL)
    {
      return -EINVAL;
    }

  snprintf(port, portsz, "443");

  p = endpoint;
  if (strncmp(p, "https://", 8) == 0)
    {
      p += 8;
    }
  else if (strncmp(p, "http://", 7) == 0)
    {
      p += 7;
      snprintf(port, portsz, "80");
    }

  host_start = p;
  slash = strchr(p, '/');
  if (slash == NULL)
    {
      host_len = strlen(host_start);
      if (host_len + 1 > hostsz)
        {
          return -ENOSPC;
        }

      memcpy(host, host_start, host_len);
      host[host_len] = '\0';
      snprintf(path, pathsz, "/v1/chat/completions");
      return 0;
    }

  host_len = (size_t)(slash - host_start);
  if (host_len + 1 > hostsz)
    {
      return -ENOSPC;
    }

  memcpy(host, host_start, host_len);
  host[host_len] = '\0';

  if (strcmp(slash, "/v1") == 0 || strcmp(slash, "/v1/") == 0)
    {
      snprintf(path, pathsz, "/v1/chat/completions");
    }
  else
    {
      snprintf(path, pathsz, "%s", slash);
      if (strstr(path, "chat/completions") == NULL)
        {
          size_t n = strlen(path);

          if (n + 1 < pathsz && path[n - 1] != '/')
            {
              strcat(path, "/");
              n++;
            }

          if (n + 20 < pathsz)
            {
              strcat(path, "chat/completions");
            }
        }
    }

  return 0;
}

/* Append with an explicit bound, and use it instead of snprintf for the
 * credential JSON.  Two reasons: the formatter cannot be proven safe here (the
 * field types allow a worst case of about 860 bytes while the crypto layer
 * caps the plaintext at 512), and a silently truncated document still passes
 * for a credential blob to every later step.  Building it by hand makes the
 * overflow impossible and reports it at the point of the copy.
 */

static int json_append(char *out, size_t outsz, size_t *pos,
                       const char *s, size_t n)
{
  if (*pos + n >= outsz)
    {
      return -ENOSPC;
    }

  memcpy(out + *pos, s, n);
  *pos += n;
  out[*pos] = '\0';
  return 0;
}

static int cred_to_json(const struct vg_llm_credentials *cred,
                        char *out, size_t outsz)
{
  char key_esc[VG_PROVISION_API_KEY_MAX * 2];
  char host_esc[VG_PROVISION_HOST_MAX * 2];
  char path_esc[VG_PROVISION_PATH_MAX * 2];
  size_t pos = 0;
  int ret;

  ret = json_escape(cred->api_key, key_esc, sizeof(key_esc));
  if (ret < 0)
    {
      return ret;
    }

  ret = json_escape(cred->host, host_esc, sizeof(host_esc));
  if (ret < 0)
    {
      return ret;
    }

  ret = json_escape(cred->path, path_esc, sizeof(path_esc));
  if (ret < 0)
    {
      return ret;
    }

  if (outsz > 0)
    {
      out[0] = '\0';
    }

#define VG_JSON_PUT(lit)                                                     \
  do                                                                         \
    {                                                                        \
      ret = json_append(out, outsz, &pos, lit, sizeof(lit) - 1);             \
      if (ret < 0)                                                           \
        {                                                                    \
          return ret;                                                        \
        }                                                                    \
    }                                                                        \
  while (0)

#define VG_JSON_PUT_STR(s)                                                   \
  do                                                                         \
    {                                                                        \
      ret = json_append(out, outsz, &pos, s, strlen(s));                     \
      if (ret < 0)                                                           \
        {                                                                    \
          return ret;                                                        \
        }                                                                    \
    }                                                                        \
  while (0)

  VG_JSON_PUT("{");
  VG_JSON_PUT("\"host\":\"");
  VG_JSON_PUT_STR(host_esc);
  VG_JSON_PUT("\",\"path\":\"");
  VG_JSON_PUT_STR(path_esc);
  VG_JSON_PUT("\",\"port\":\"");
  VG_JSON_PUT_STR(cred->port);
  VG_JSON_PUT("\",\"model\":\"");
  VG_JSON_PUT_STR(cred->model);
  VG_JSON_PUT("\",\"api_key\":\"");
  VG_JSON_PUT_STR(key_esc);
  VG_JSON_PUT("\"}");

#undef VG_JSON_PUT
#undef VG_JSON_PUT_STR

  return 0;
}

static int json_to_cred(const char *json, struct vg_llm_credentials *out)
{
  int ret;

  memset(out, 0, sizeof(*out));
  ret = json_get_string(json, "host", out->host, sizeof(out->host));
  if (ret < 0)
    {
      return ret;
    }

  ret = json_get_string(json, "path", out->path, sizeof(out->path));
  if (ret < 0)
    {
      return ret;
    }

  ret = json_get_string(json, "port", out->port, sizeof(out->port));
  if (ret < 0)
    {
      return ret;
    }

  ret = json_get_string(json, "model", out->model, sizeof(out->model));
  if (ret < 0)
    {
      return ret;
    }

  return json_get_string(json, "api_key", out->api_key, sizeof(out->api_key));
}

bool vg_provision_is_present(void)
{
  struct stat st;

  return stat(VG_PROVISION_FILE, &st) == 0 && st.st_size > 0;
}

int vg_provision_seal(const struct vg_llm_credentials *cred)
{
  uint8_t uid[VG_PROVISION_UID_LEN];
  char plain[640];
  uint8_t blob[768];
  size_t blob_len;
  int ret;

  if (cred == NULL || cred->api_key[0] == '\0' || cred->host[0] == '\0')
    {
      return -EINVAL;
    }

  ret = mkdir(VG_PROVISION_DIR, 0700);
  if (ret < 0 && errno != EEXIST)
    {
      return -errno;
    }

  ret = cred_to_json(cred, plain, sizeof(plain));
  if (ret < 0)
    {
      return ret;
    }

  /* The crypto layer refuses anything past this bound.  Checking here keeps
   * the two limits in one place and reports a credential that is simply too
   * large, rather than a seal error the operator has to trace back. */

  if (strlen(plain) > VG_PROVISION_PLAIN_MAX)
    {
      return -E2BIG;
    }

  ret = vg_provision_read_uid(uid);
  if (ret < 0)
    {
      return ret;
    }

  ret = vg_provision_crypto_seal(uid, (const uint8_t *)plain, strlen(plain),
                                 blob, sizeof(blob), &blob_len);
  if (ret < 0)
    {
      return ret;
    }

  return vg_file_replace(VG_PROVISION_FILE, blob, blob_len, 0600);
}

int vg_provision_load(struct vg_llm_credentials *out)
{
  static uint8_t uid[VG_PROVISION_UID_LEN];
  static uint8_t blob[768];
  static char plain[640];
  size_t plain_len;
  ssize_t n;
  int fd;
  int ret;

  if (out == NULL)
    {
      return -EINVAL;
    }

  fd = open(VG_PROVISION_FILE, O_RDONLY);
  if (fd < 0)
    {
      return -errno;
    }

  n = read(fd, blob, sizeof(blob));
  close(fd);
  if (n <= 0)
    {
      return (n == 0) ? -EINVAL : -EIO;
    }

  ret = vg_provision_read_uid(uid);
  if (ret < 0)
    {
      return ret;
    }

  ret = vg_provision_crypto_open(uid, blob, (size_t)n,
                                 (uint8_t *)plain, sizeof(plain), &plain_len);
  if (ret < 0)
    {
      return ret;
    }

  plain[plain_len] = '\0';
  return json_to_cred(plain, out);
}

int vg_provision_apply_llm_config(void)
{
  struct vg_llm_credentials cred;
  char key_esc[VG_PROVISION_API_KEY_MAX * 2];
  static char backend[768];
  static char body[1536];
  int ret;

  if (!vg_provision_is_present())
    {
      return -ENOENT;
    }

  /* Refuse before decrypting anything: on a RAM-backed /data the file below
   * would be written and then discarded at the next reboot, leaving the board
   * with no backend and no trace of why. */

  ret = vg_provision_store_is_persistent("/data/agent/config");
  if (ret == -ENOENT)
    {
      ret = vg_provision_store_is_persistent("/data");
    }

  if (ret != 0)
    {
      return ret;
    }

  ret = vg_provision_load(&cred);
  if (ret < 0)
    {
      return ret;
    }

  ret = mkdir("/data/agent/config", 0700);
  if (ret < 0 && errno != EEXIST)
    {
      return -errno;
    }

  ret = json_escape(cred.api_key, key_esc, sizeof(key_esc));
  if (ret < 0)
    {
      return ret;
    }

  snprintf(backend, sizeof(backend),
           "{\\\"host\\\":\\\"%s\\\",\\\"path\\\":\\\"%s\\\","
           "\\\"port\\\":\\\"%s\\\",\\\"api_key\\\":\\\"%s\\\","
           "\\\"model\\\":\\\"%s\\\",\\\"priority\\\":0,\\\"cost_tier\\\":1}",
           cred.host, cred.path, cred.port, key_esc, cred.model);

  snprintf(body, sizeof(body),
           "{"
           "\"llm_host\":\"%s\","
           "\"llm_path\":\"%s\","
           "\"llm_port\":\"%s\","
           "\"model\":\"%s\","
           "\"api_key\":\"%s\","
           "\"llm_backend_0\":\"%s\""
           "}",
           cred.host, cred.path, cred.port, cred.model, key_esc, backend);

  return vg_file_replace("/data/agent/config/config.json", body,
                         strlen(body), 0600);
}

void vg_provision_boot_apply_if_needed(void)
{
  if (access("/data/velaguard/provision/.apply_on_boot", F_OK) != 0)
    {
      return;
    }

  (void)unlink("/data/velaguard/provision/.apply_on_boot");
  (void)vg_provision_apply_llm_config();
}

/* Whether the agent has something to call out with.  This mirrors the router's
 * own condition: it reports "No available backend" exactly when no slot carries
 * a host, and the router is fed only from the config.json written below.  The
 * check reads that file rather than the sealed blob, because a sealed blob
 * whose apply step never ran is precisely the failure this is meant to catch.
 *
 * The parse is separate from the read so the host tests can cover it without a
 * board: the file lives at a fixed /data path the desktop cannot write.
 */

bool vg_provision_creds_ready_in(const char *json)
{
  if (json == NULL || json[0] == '\0')
    {
      return false;
    }

  /* The value must be non-empty, not merely present: a masked or blanked key
   * leaves the router with a host but makes every call fail instantly. */

  if (strstr(json, "\"llm_host\":\"") == NULL ||
      strstr(json, "\"llm_host\":\"\"") != NULL)
    {
      return false;
    }

  if (strstr(json, "\"api_key\":\"") == NULL ||
      strstr(json, "\"api_key\":\"\"") != NULL)
    {
      return false;
    }

  return true;
}

bool vg_llm_credentials_ready(void)
{
  static char buf[1024];
  ssize_t n;
  int fd;

  fd = open("/data/agent/config/config.json", O_RDONLY);
  if (fd < 0)
    {
      return false;
    }

  n = read(fd, buf, sizeof(buf) - 1);
  close(fd);

  if (n <= 0)
    {
      return false;
    }

  buf[n] = '\0';
  return vg_provision_creds_ready_in(buf);
}

/* --- MQTT broker credentials ---------------------------------------------
 *
 * The record layout and the reasoning behind a separate file and a kind tag
 * are in vg_provision.h.  What matters here is that this module never reads a
 * password from anywhere but the sealed record, and that a record it cannot
 * decrypt or parse leaves the caller with no credentials at all rather than
 * with a partial or stale one.
 */

int vg_mqtt_cred_parse(const char *plain, struct vg_mqtt_credentials *out)
{
  char kind[16];
  int ret;

  if (plain == NULL || out == NULL)
    {
      return -EINVAL;
    }

  memset(out, 0, sizeof(*out));

  /* The kind tag is the whole isolation mechanism.  A record without it is
   * not an MQTT record, whatever else it contains, so the LLM blob dropped at
   * this path is refused here instead of being read as a user name. */

  ret = json_get_string(plain, "kind", kind, sizeof(kind));
  if (ret < 0)
    {
      return -EINVAL;
    }

  if (strcmp(kind, VG_MQTT_CRED_KIND) != 0)
    {
      return -EINVAL;
    }

  ret = json_get_string(plain, "username", out->username,
                        sizeof(out->username));
  if (ret < 0)
    {
      goto fail;
    }

  ret = json_get_string(plain, "password", out->password,
                        sizeof(out->password));
  if (ret < 0)
    {
      goto fail;
    }

  if (out->username[0] == '\0' || out->password[0] == '\0')
    {
      ret = -EINVAL;
      goto fail;
    }

  return 0;

fail:
  memset(out, 0, sizeof(*out));
  return ret;
}

int vg_mqtt_cred_install(const void *blob, size_t blob_len)
{
  uint8_t uid[VG_PROVISION_UID_LEN];
  char plain[640];
  struct vg_mqtt_credentials cred;
  size_t plain_len;
  int ret;

  if (blob == NULL || blob_len == 0 || blob_len > VG_PROVISION_BLOB_MAX)
    {
      return -EINVAL;
    }

  ret = vg_provision_read_uid(uid);
  if (ret < 0)
    {
      return ret;
    }

  ret = vg_provision_crypto_open(uid, blob, blob_len,
                                 (uint8_t *)plain, sizeof(plain), &plain_len);
  if (ret < 0)
    {
      return ret;
    }

  plain[plain_len] = '\0';
  ret = vg_mqtt_cred_parse(plain, &cred);
  memset(plain, 0, sizeof(plain));
  memset(&cred, 0, sizeof(cred));
  if (ret < 0)
    {
      return ret;
    }

  ret = mkdir(VG_PROVISION_DIR, 0700);
  if (ret < 0 && errno != EEXIST)
    {
      return -errno;
    }

  /* vg_file_replace keeps the previous bytes and restores them if the
   * publication step fails, so a board whose eMMC write dies part way through
   * still has the credentials it booted with. */

  return vg_file_replace(VG_MQTT_CRED_FILE, blob, blob_len, 0600);
}

/* The load working set is module scope rather than function scope so that the
 * lock below can be seen to cover every one of the four: the sealed bytes, the
 * decrypted text, the derived UID and the mutex itself.  Both the MQTT session
 * and the vgmqtt tool call this, and in a flat build they are tasks in one
 * address space, so the race is reachable in the product and not only in a
 * test.
 */

static uint8_t g_mqtt_uid[VG_PROVISION_UID_LEN];
static uint8_t g_mqtt_blob[VG_PROVISION_BLOB_MAX];
static char g_mqtt_plain[640];
static pthread_mutex_t g_mqtt_lock = PTHREAD_MUTEX_INITIALIZER;

int vg_mqtt_cred_load(struct vg_mqtt_credentials *out)
{
  size_t plain_len = 0;
  ssize_t n;
  int fd;
  int ret;

  if (out == NULL)
    {
      return -EINVAL;
    }

  memset(out, 0, sizeof(*out));

  /* The lock covers the read() as well as the decrypt.  Holding it only
   * around the decrypt left a window in which a second loader overwrote
   * g_mqtt_blob between the first loader's read() and its integrity check:
   * the first loader would then authenticate bytes that were never on the
   * card, and could hand back the other task's record - or fail with an
   * integrity error for a record that is perfectly good. */

  pthread_mutex_lock(&g_mqtt_lock);

  fd = open(VG_MQTT_CRED_FILE, O_RDONLY);
  if (fd < 0)
    {
      ret = -errno;
    }
  else
    {
      n = read(fd, g_mqtt_blob, sizeof(g_mqtt_blob));
      close(fd);
      if (n <= 0)
        {
          ret = (n == 0) ? -EINVAL : -EIO;
        }
      else
        {
          ret = vg_provision_read_uid(g_mqtt_uid);
          if (ret == 0)
            {
              ret = vg_provision_crypto_open(g_mqtt_uid, g_mqtt_blob, (size_t)n,
                                             (uint8_t *)g_mqtt_plain,
                                             sizeof(g_mqtt_plain), &plain_len);
            }

          if (ret == 0)
            {
              g_mqtt_plain[plain_len] = '\0';
              ret = vg_mqtt_cred_parse(g_mqtt_plain, out);
            }
        }
    }

  /* The decrypted text is the password in the clear; drop it before the lock
   * is released so the next loader cannot observe it. */

  memset(g_mqtt_plain, 0, sizeof(g_mqtt_plain));
  pthread_mutex_unlock(&g_mqtt_lock);

  if (ret < 0)
    {
      memset(out, 0, sizeof(*out));
    }

  return ret;
}

bool vg_mqtt_cred_present(void)
{
  struct stat st;

  return stat(VG_MQTT_CRED_FILE, &st) == 0 && st.st_size > 0;
}

void vg_mqtt_cred_mask_user(const char *user, char *out, size_t outsz)
{
  size_t n;

  if (out == NULL || outsz == 0)
    {
      return;
    }

  if (user == NULL)
    {
      user = "";
    }

  /* Short names are masked whole: keeping two characters of a five character
   * name would leave most of it on the console. */

  n = strlen(user);
  if (n <= 4)
    {
      snprintf(out, outsz, "****");
      return;
    }

  snprintf(out, outsz, "%.2s****%.2s", user, user + n - 2);
}

int vg_mqtt_cred_status(char *out, size_t outsz)
{
  struct vg_mqtt_credentials cred;
  char masked[VG_MQTT_CRED_USER_MAX + 8];
  const char *label;
  int ret;

  if (out == NULL || outsz == 0)
    {
      return -EINVAL;
    }

  ret = vg_mqtt_cred_load(&cred);
  if (ret < 0)
    {
      switch (ret)
        {
          case -ENOENT:
            label = "MISSING";
            break;

          case -EACCES:
            label = "UNREADABLE";
            break;

          case -EINVAL:
            label = "INVALID";
            break;

          default:
            label = "UNAVAILABLE";
            break;
        }

      snprintf(out, outsz, "mqtt credentials: %s (err %d)", label, ret);
      return ret;
    }

  vg_mqtt_cred_mask_user(cred.username, masked, sizeof(masked));
  snprintf(out, outsz, "mqtt credentials: OK user=%s password=set(%u)",
           masked, (unsigned)strlen(cred.password));

  memset(&cred, 0, sizeof(cred));
  memset(masked, 0, sizeof(masked));
  return 0;
}
