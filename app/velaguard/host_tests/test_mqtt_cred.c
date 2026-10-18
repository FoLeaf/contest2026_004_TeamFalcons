/* SPDX-License-Identifier: Apache-2.0 */
/****************************************************************************
 * app/velaguard/host_tests/test_mqtt_cred.c
 *
 * The MQTT credential record: round trip, length and format rejection,
 * isolation from the LLM record, a failed write that must leave the previous
 * record alone, and a status line that must not disclose what it describes.
 *
 * The store is compiled in rather than linked so the record paths can be
 * pointed at this directory; the record paths and the directory they live in
 * are the only things that tie the board behaviour to /data.  The crypto
 * translation unit is replaced by a reversible stand-in, because its desktop
 * branch is a stub and because these cases are about the record's shape and
 * the store's failure behaviour, not about the cipher.
 ****************************************************************************/

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define VG_PROVISION_DIR  "."
#define VG_PROVISION_FILE "vgtest-llm_secrets.v1"
#define VG_MQTT_CRED_FILE "vgtest-mqtt_secrets.v1"

#include "../vg_provision.c"

/****************************************************************************
 * Stand-ins for the parts that only exist on the board
 ****************************************************************************/

int vg_device_id_read_uid(uint8_t uid[VG_PROVISION_UID_LEN])
{
  memset(uid, 0x11, VG_PROVISION_UID_LEN);
  return 0;
}

/* Magic "TST", one length byte, then the plaintext exclusive-ored with 0xa5.
 * Reversible, so a record written by the test can be read by the real
 * loader; the real seal is device-bound and cannot run here.
 */

int vg_provision_crypto_seal(const uint8_t uid[VG_PROVISION_UID_LEN],
                             const uint8_t *plain, size_t plain_len,
                             uint8_t *out, size_t outsz, size_t *out_len)
{
  size_t i;

  (void)uid;

  if (plain == NULL || out == NULL || out_len == NULL || plain_len > 255 ||
      outsz < plain_len + 4)
    {
      return -EINVAL;
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
      return -EACCES;
    }

  n = blob[3];
  if (blob_len != n + 4 || plainsz <= n)
    {
      return -EINVAL;
    }

  for (i = 0; i < n; i++)
    {
      plain[i] = (uint8_t)(blob[4 + i] ^ 0xa5);
    }

  plain[n] = '\0';
  *plain_len = n;
  return 0;
}

/* A rename that can be made to fail for one destination, so the store's
 * publication step can be interrupted the way a dying eMMC write would.
 */

static const char *g_block_rename_to;

int __real_rename(const char *from, const char *to);

int __wrap_rename(const char *from, const char *to)
{
  if (g_block_rename_to != NULL && strcmp(to, g_block_rename_to) == 0)
    {
      errno = EIO;
      return -1;
    }

  return __real_rename(from, to);
}

/****************************************************************************
 * Deterministic interleaving of two loads
 *
 * The store's working buffers are module scope, so two concurrent loads share
 * them.  The case that matters is a second load writing into the sealed-bytes
 * buffer between the first load's read() and its integrity check: the first
 * load then authenticates bytes that were never on the card.
 *
 * The rendezvous below forces exactly that window.  Loader A is stopped inside
 * its read of the shared buffer, the record on the card is replaced, and
 * loader B is released to try to enter the same buffer.  A then continues and
 * must still see its own record.  No other read runs after t_armed is set, so
 * the wrapper does not need to know the name of the module-scope buffer.  That
 * keeps the same test compilable against both the pre-fix and fixed source.
 *
 * The handshake up to "B has been released" is deterministic.  The last step,
 * "B has started a read of the shared buffer", is waited for with a bounded
 * deadline, because with the lock held end to end B never gets there and there
 * is no event to wait for.  That deadline can only make the test miss a
 * regression, never report one that is not there.
 ****************************************************************************/

static pthread_mutex_t t_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  t_cv   = PTHREAD_COND_INITIALIZER;
static volatile int    t_armed;
static int             t_first_inside;
static int             t_first_done;
static int             t_second_started;
static int             t_second_reached;

ssize_t __real_read(int fd, void *buf, size_t n);

ssize_t __wrap_read(int fd, void *buf, size_t n)
{
  ssize_t got;

  if (!t_armed)
    {
      return __real_read(fd, buf, n);
    }

  /* Fill the buffer first: the window under test opens once the first loader
   * holds bytes it has not authenticated yet. */

  got = __real_read(fd, buf, n);

  pthread_mutex_lock(&t_lock);
  if (!t_first_inside)
    {
      struct timespec deadline;

      t_first_inside = 1;
      pthread_cond_broadcast(&t_cv);

      while (!t_second_started)
        {
          pthread_cond_wait(&t_cv, &t_lock);
        }

      clock_gettime(CLOCK_REALTIME, &deadline);
      deadline.tv_nsec += 200000000L;
      if (deadline.tv_nsec >= 1000000000L)
        {
          deadline.tv_sec++;
          deadline.tv_nsec -= 1000000000L;
        }

      while (!t_second_reached)
        {
          if (pthread_cond_timedwait(&t_cv, &t_lock, &deadline) == ETIMEDOUT)
            {
              break;
            }
        }

      t_first_done = 1;
    }
  else if (!t_first_done)
    {
      /* A second read landed on the shared buffer while the first loader was
       * still between its read and its integrity check. */

      t_second_reached = 1;
      pthread_cond_broadcast(&t_cv);
    }

  pthread_mutex_unlock(&t_lock);
  return got;
}

static void *loader_a(void *arg)
{
  return (void *)(intptr_t)vg_mqtt_cred_load((struct vg_mqtt_credentials *)arg);
}

static void *loader_b(void *arg)
{
  pthread_mutex_lock(&t_lock);
  t_second_started = 1;
  pthread_cond_broadcast(&t_cv);
  pthread_mutex_unlock(&t_lock);

  return (void *)(intptr_t)vg_mqtt_cred_load((struct vg_mqtt_credentials *)arg);
}

/****************************************************************************
 * Helpers
 ****************************************************************************/

static int fails;

static void expect(int cond, const char *msg)
{
  if (!cond)
    {
      fprintf(stderr, "FAIL: %s\n", msg);
      fails++;
    }
}

static void seal_into(const char *json, uint8_t *blob, size_t blobsz,
                      size_t *blob_len)
{
  if (vg_provision_crypto_seal(NULL, (const uint8_t *)json, strlen(json),
                               blob, blobsz, blob_len) != 0)
    {
      fprintf(stderr, "FAIL: the test could not seal a record\n");
      fails++;
      *blob_len = 0;
    }
}

/* Write a sealed record the way the host packer does: seal the JSON, then put
 * the blob where the board expects it.
 */

static void put_record(const char *path, const char *json)
{
  uint8_t blob[1024];
  size_t blob_len = 0;
  FILE *fp;

  seal_into(json, blob, sizeof(blob), &blob_len);
  if (blob_len == 0)
    {
      return;
    }

  fp = fopen(path, "wb");
  if (fp == NULL)
    {
      fprintf(stderr, "FAIL: cannot write %s\n", path);
      fails++;
      return;
    }

  if (fwrite(blob, 1, blob_len, fp) != blob_len)
    {
      fprintf(stderr, "FAIL: short write to %s\n", path);
      fails++;
    }

  fclose(fp);
}

/* vg_file_replace keeps a .bak and may leave a .tmp / .btmp behind, so a
 * clean run has to remove the whole family, not just the record. */

static void clean_store(void)
{
  static const char *const suffixes[] = { "", ".tmp", ".bak", ".btmp" };
  char path[128];
  size_t i;

  for (i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++)
    {
      snprintf(path, sizeof(path), "%s%s", VG_PROVISION_FILE, suffixes[i]);
      remove(path);
      snprintf(path, sizeof(path), "%s%s", VG_MQTT_CRED_FILE, suffixes[i]);
      remove(path);
    }
}

static size_t read_record(const char *path, uint8_t *out, size_t outsz)
{
  FILE *fp = fopen(path, "rb");
  size_t n;

  if (fp == NULL)
    {
      return 0;
    }

  n = fread(out, 1, outsz, fp);
  fclose(fp);
  return n;
}

#define MQTT_JSON(user, pass)                                                \
  "{\"kind\":\"mqtt\",\"username\":\"" user "\",\"password\":\"" pass "\"}"

#define LLM_JSON                                                             \
  "{\"host\":\"llm.example.invalid\",\"path\":\"/v1/chat/completions\","     \
  "\"port\":\"443\",\"model\":\"test-model\",\"api_key\":\"tp-test\"}"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(void)
{
  static const char *password = "test-only-not-a-real-password";
  struct vg_mqtt_credentials cred;
  struct vg_llm_credentials llm;
  uint8_t before[1024];
  uint8_t after[1024];
  uint8_t blob[1024];
  char json[512];
  char status[192];
  size_t before_len;
  size_t after_len;
  size_t blob_len;
  int ret;

  clean_store();

  /* A record written the way the host packer writes one reads back whole.
   * Both fields are checked: a loader that stopped after the user name would
   * pass a "loads" test and still leave the session without a password. */

  put_record(VG_MQTT_CRED_FILE, MQTT_JSON("velaguard", "pw-round-trip"));
  memset(&cred, 0, sizeof(cred));
  expect(vg_mqtt_cred_load(&cred) == 0, "a sealed mqtt record loads");
  expect(strcmp(cred.username, "velaguard") == 0,
         "the user name round-trips");
  expect(strcmp(cred.password, "pw-round-trip") == 0,
         "the password round-trips");
  expect(vg_mqtt_cred_present(), "a written record reports present");

  /* Length.  The scanner accepts one character less than the buffer, so a
   * name that exactly fills the buffer must still survive the round trip and
   * one byte more must be refused rather than truncated into a shorter name
   * that would authenticate as somebody else. */

  {
    char user[64];
    char longuser[100];

    memset(user, 'u', sizeof(user) - 1);
    user[sizeof(user) - 1] = '\0';
    snprintf(json, sizeof(json), MQTT_JSON("%s", "p"), user);
    put_record(VG_MQTT_CRED_FILE, json);
    expect(vg_mqtt_cred_load(&cred) == 0 &&
           strlen(cred.username) == sizeof(user) - 1,
           "a user name that fills the buffer round-trips");

    memset(longuser, 'u', sizeof(longuser) - 1);
    longuser[sizeof(longuser) - 1] = '\0';
    snprintf(json, sizeof(json), MQTT_JSON("%s", "p"), longuser);
    put_record(VG_MQTT_CRED_FILE, json);
    expect(vg_mqtt_cred_load(&cred) == -ENOSPC,
           "an over-long user name is refused");
  }

  /* Empty fields.  A masked or blanked record is the failure that looks like
   * a broker rejecting the board, so it has to be caught here. */

  put_record(VG_MQTT_CRED_FILE, MQTT_JSON("", "p"));
  memset(&cred, 0xff, sizeof(cred));
  expect(vg_mqtt_cred_load(&cred) == -EINVAL, "an empty user name is refused");
  expect(cred.username[0] == '\0' && cred.password[0] == '\0',
         "a refused record leaves no field behind for a caller to use");

  put_record(VG_MQTT_CRED_FILE, MQTT_JSON("u", ""));
  expect(vg_mqtt_cred_load(&cred) == -EINVAL, "an empty password is refused");

  put_record(VG_MQTT_CRED_FILE, "{\"kind\":\"llm\",\"username\":\"u\","
                                "\"password\":\"p\"}");
  expect(vg_mqtt_cred_load(&cred) == -EINVAL, "a foreign kind tag is refused");

  /* Purpose isolation, both directions.  The two records live in separate
   * files, but a file copied by hand must not be readable as the other kind. */

  put_record(VG_MQTT_CRED_FILE, LLM_JSON);
  expect(vg_mqtt_cred_load(&cred) == -EINVAL,
         "an LLM record is not accepted as an MQTT credential");

  put_record(VG_PROVISION_FILE, MQTT_JSON("u", "p"));
  memset(&llm, 0, sizeof(llm));
  expect(vg_provision_load(&llm) < 0,
         "an MQTT record is not accepted as an LLM credential");

  /* A missing record, and one that fails its integrity check. */

  remove(VG_MQTT_CRED_FILE);
  expect(vg_mqtt_cred_load(&cred) == -ENOENT, "a missing record reports -ENOENT");
  expect(!vg_mqtt_cred_present(), "a missing record is not present");
  ret = vg_mqtt_cred_status(status, sizeof(status));
  expect(ret == -ENOENT && strstr(status, "MISSING") != NULL,
         "status names a missing record");

  put_record(VG_MQTT_CRED_FILE, MQTT_JSON("u", "p"));
  {
    FILE *fp = fopen(VG_MQTT_CRED_FILE, "wb");

    if (fp != NULL)
      {
        fputs("not a sealed record at all", fp);
        fclose(fp);
      }
  }

  expect(vg_mqtt_cred_load(&cred) == -EACCES,
         "a record that fails its integrity check is refused");
  ret = vg_mqtt_cred_status(status, sizeof(status));
  expect(ret == -EACCES && strstr(status, "UNREADABLE") != NULL,
         "status names an unreadable record");

  /* The status line is the only thing an operator reads back, so it has to
   * carry the length without carrying the value. */

  snprintf(json, sizeof(json), MQTT_JSON("velaguard", "%s"), password);
  put_record(VG_MQTT_CRED_FILE, json);
  ret = vg_mqtt_cred_status(status, sizeof(status));
  expect(ret == 0, "status reports a usable record");
  expect(strstr(status, password) == NULL, "status does not contain the password");
  expect(strstr(status, "velaguard") == NULL,
         "status does not contain the whole user name");
  expect(strstr(status, "ve****rd") != NULL, "status masks the user name");
  expect(strstr(status, "password=set(") != NULL,
         "status reports that a password is set");

  vg_mqtt_cred_mask_user("abc", status, sizeof(status));
  expect(strcmp(status, "****") == 0, "a short user name is masked whole");

  /* Publication.  The record on the board has to survive a write that dies
   * part way: losing the credentials means losing the dashboard link, and
   * losing them silently is worse than refusing the new ones. */

  put_record(VG_MQTT_CRED_FILE, MQTT_JSON("first", "pw-one"));
  before_len = read_record(VG_MQTT_CRED_FILE, before, sizeof(before));
  expect(before_len > 0, "the first record reached the disk");

  seal_into(MQTT_JSON("second", "pw-two"), blob, sizeof(blob), &blob_len);
  g_block_rename_to = VG_MQTT_CRED_FILE;
  ret = vg_mqtt_cred_install(blob, blob_len);
  g_block_rename_to = NULL;
  expect(ret < 0, "a publication that fails is reported");

  after_len = read_record(VG_MQTT_CRED_FILE, after, sizeof(after));
  expect(after_len == before_len && memcmp(before, after, before_len) == 0,
         "a failed publication leaves the previous record in place");
  expect(vg_mqtt_cred_load(&cred) == 0 && strcmp(cred.username, "first") == 0,
         "the surviving record is still the previous one");

  /* A record the loader would refuse must be refused at the store, before
   * the working record is touched. */

  seal_into(LLM_JSON, blob, sizeof(blob), &blob_len);
  expect(vg_mqtt_cred_install(blob, blob_len) == -EINVAL,
         "a foreign record is refused before it is published");
  after_len = read_record(VG_MQTT_CRED_FILE, after, sizeof(after));
  expect(after_len == before_len && memcmp(before, after, before_len) == 0,
         "refusing a foreign record leaves the previous record in place");

  expect(vg_mqtt_cred_install(NULL, 0) == -EINVAL,
         "an empty blob is refused");
  {
    uint8_t huge[VG_PROVISION_BLOB_MAX + 1];

    memset(huge, 0, sizeof(huge));
    expect(vg_mqtt_cred_install(huge, sizeof(huge)) == -EINVAL,
           "an over-sized blob is refused");
  }

  /* And a good one does replace it. */

  seal_into(MQTT_JSON("second", "pw-two"), blob, sizeof(blob), &blob_len);
  expect(vg_mqtt_cred_install(blob, blob_len) == 0, "a good record publishes");
  expect(vg_mqtt_cred_load(&cred) == 0 && strcmp(cred.username, "second") == 0 &&
         strcmp(cred.password, "pw-two") == 0,
         "the published record is the one that reads back");

  /* Interleaving.  Loader A is held inside its read of the shared buffer while
   * the record on the card is replaced and loader B is released to try to
   * enter the same buffer.  A must still come back with the record it read. */

  {
    struct vg_mqtt_credentials a;
    struct vg_mqtt_credentials b;
    pthread_t ta;
    pthread_t tb;
    void *res;
    int ra = -1;
    int rb = -1;

    put_record(VG_MQTT_CRED_FILE, MQTT_JSON("interleave-a", "pw-aaaa"));
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));

    pthread_mutex_lock(&t_lock);
    t_first_inside = 0;
    t_first_done = 0;
    t_second_started = 0;
    t_second_reached = 0;
    pthread_mutex_unlock(&t_lock);
    t_armed = 1;

    if (pthread_create(&ta, NULL, loader_a, &a) != 0)
      {
        expect(0, "loader A starts");
        t_armed = 0;
      }
    else
      {
        pthread_mutex_lock(&t_lock);
        while (!t_first_inside)
          {
            pthread_cond_wait(&t_cv, &t_lock);
          }
        pthread_mutex_unlock(&t_lock);

        /* Replace the record while A is still inside the buffer, then let B
         * in.  A read its bytes before the replacement, so only a clobbered
         * buffer can change what A ends up authenticating. */

        put_record(VG_MQTT_CRED_FILE, MQTT_JSON("interleave-b", "pw-bbbb"));

        if (pthread_create(&tb, NULL, loader_b, &b) != 0)
          {
            expect(0, "loader B starts");
            pthread_mutex_lock(&t_lock);
            t_second_started = 1;
            t_second_reached = 1;
            pthread_cond_broadcast(&t_cv);
            pthread_mutex_unlock(&t_lock);
          }
        else
          {
            pthread_join(tb, &res);
            rb = (int)(intptr_t)res;
          }

        pthread_join(ta, &res);
        ra = (int)(intptr_t)res;
        t_armed = 0;

        expect(!t_second_reached,
               "a second load cannot read the shared buffer while the first "
               "is still between its read and its check");
        expect(ra == 0 && strcmp(a.username, "interleave-a") == 0 &&
               strcmp(a.password, "pw-aaaa") == 0,
               "the first loader still gets the record it read");
        expect(rb == 0 && strcmp(b.username, "interleave-b") == 0 &&
               strcmp(b.password, "pw-bbbb") == 0,
               "the second loader gets the record it read");
      }
  }

  /* And the ordinary case: several loaders at once all succeed with the
   * record on the card. */

  {
    struct vg_mqtt_credentials many[4];
    pthread_t threads[4];
    void *res;
    int started = 0;
    size_t i;

    put_record(VG_MQTT_CRED_FILE, MQTT_JSON("concurrent", "pw-shared"));

    for (i = 0; i < 4; i++)
      {
        memset(&many[i], 0, sizeof(many[i]));
        if (pthread_create(&threads[i], NULL, loader_a, &many[i]) == 0)
          {
            started++;
          }
      }

    for (i = 0; i < (size_t)started; i++)
      {
        pthread_join(threads[i], &res);
        expect((int)(intptr_t)res == 0 &&
               strcmp(many[i].username, "concurrent") == 0 &&
               strcmp(many[i].password, "pw-shared") == 0,
               "a concurrent load gets the record on the card");
      }
  }

  clean_store();

  if (fails == 0)
    {
      printf("test_mqtt_cred: OK\n");
      return 0;
    }

  printf("test_mqtt_cred: %d failure(s)\n", fails);
  return 1;
}
