/****************************************************************************
 * NSH: vgprovision — install / inspect encrypted credentials on eMMC.
 *
 * Two records live in the same store: the LLM one the agent boots from, and
 * the MQTT one the network manager reads whenever it opens a session.  The
 * 'mqtt' subcommands handle the second.
 ****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#include <nuttx/config.h>

#include "vg_provision.h"

static void usage(void)
{
  printf("Usage:\n");
  printf("  vgprovision uid\n");
  printf("  vgprovision status\n");
  printf("  vgprovision wipe\n");
  printf("  vgprovision put <hex>\n");
  printf("  vgprovision commit\n");
  printf("  vgprovision apply\n");
  printf("  vgprovision mqtt put <hex>\n");
  printf("  vgprovision mqtt commit\n");
  printf("  vgprovision mqtt status\n");
  printf("  vgprovision mqtt wipe\n");
  printf("\n");
  printf("  <hex> is the sealed blob, not the secret.  Pack it on the host:\n");
  printf("    vg-provision-pack.py <uid> <plain.json>\n");
}

#define VG_STAGING_FILE "/data/velaguard/provision/.staging"
#define VG_PROVISION_DIR  "/data/velaguard/provision"
#define VG_APPLY_FLAG     "/data/velaguard/provision/.apply_on_boot"

/* A separate staging name, so a half-pushed MQTT record can never be picked
 * up by the LLM commit, or the other way round. */

#define VG_MQTT_STAGING_FILE "/data/velaguard/provision/.staging.mqtt"

static int hex_nibble(char c)
{
  if (c >= '0' && c <= '9')
    {
      return c - '0';
    }

  if (c >= 'a' && c <= 'f')
    {
      return c - 'a' + 10;
    }

  if (c >= 'A' && c <= 'F')
    {
      return c - 'A' + 10;
    }

  return -1;
}

static int cmd_put_to(const char *staging, const char *hex)
{
  uint8_t buf[32];
  size_t n = 0;
  size_t len;
  int fd;
  ssize_t w;

  if (staging == NULL || hex == NULL)
    {
      return -EINVAL;
    }

  len = strlen(hex);
  if (len == 0 || (len % 2) != 0 || len > sizeof(buf) * 2)
    {
      return -EINVAL;
    }

  for (size_t i = 0; i < len; i += 2)
    {
      int hi = hex_nibble(hex[i]);
      int lo = hex_nibble(hex[i + 1]);

      if (hi < 0 || lo < 0)
        {
          return -EINVAL;
        }

      buf[n++] = (uint8_t)((hi << 4) | lo);
    }

  (void)mkdir(VG_PROVISION_DIR, 0700);
  fd = open(staging, O_WRONLY | O_CREAT | O_APPEND, 0600);
  if (fd < 0)
    {
      return -errno;
    }

  w = write(fd, buf, n);
  close(fd);
  return (w == (ssize_t)n) ? 0 : -EIO;
}

static int cmd_put(const char *hex)
{
  return cmd_put_to(VG_STAGING_FILE, hex);
}

/* Report a store that would not survive a restart.  A commit onto RAM-backed
 * /data prints no other symptom at the time and only shows up later as a
 * board with no credentials.
 */

static int store_persistence(void)
{
  int ret;

  ret = vg_provision_store_is_persistent(VG_PROVISION_DIR);
  if (ret == -ENOENT)
    {
      ret = vg_provision_store_is_persistent("/data");
    }

  return ret;
}

static void warn_if_not_persistent(void)
{
  int ret = store_persistence();

  if (ret != 0)
    {
      printf("vgprovision: WARNING store is not persistent (%d): "
             "credentials will be lost on reboot\n", ret);
    }
}

static int cmd_commit(void)
{
  struct stat st;
  int fd;
  int ret;

  if (stat(VG_STAGING_FILE, &st) != 0 || st.st_size <= 0)
    {
      return -EINVAL;
    }

  ret = rename(VG_STAGING_FILE, VG_PROVISION_FILE);
  if (ret < 0)
    {
      return -errno;
    }

  printf("vgprovision: committed %s (%ld bytes)\n",
         VG_PROVISION_FILE, (long)st.st_size);

  /* Say plainly whether the blob will still be there after a restart.  A
   * commit onto RAM-backed /data prints no other symptom at the time and only
   * shows up later as an agent with no backend. */

  warn_if_not_persistent();

  fd = open(VG_APPLY_FLAG, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd >= 0)
    {
      close(fd);
    }

  printf("vgprovision: reboot required to apply LLM config "
         "(this build has no reboot command; reset the board)\n");
  return 0;
}

/* MQTT record.  Unlike the LLM one there is no boot step: the network manager
 * reads the record every time it opens a session, so a committed record takes
 * effect at the next connect without a restart. */

static int cmd_mqtt_commit(void)
{
  struct stat st;
  uint8_t blob[VG_PROVISION_BLOB_MAX];
  ssize_t n;
  int fd;
  int ret;

  if (stat(VG_MQTT_STAGING_FILE, &st) != 0 || st.st_size <= 0)
    {
      printf("vgprovision: nothing staged for mqtt\n");
      return -EINVAL;
    }

  if (st.st_size > (off_t)sizeof(blob))
    {
      printf("vgprovision: staged mqtt record too large (%ld bytes)\n",
             (long)st.st_size);
      return -E2BIG;
    }

  fd = open(VG_MQTT_STAGING_FILE, O_RDONLY);
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

  /* The staged bytes are decrypted and parsed before the working record is
   * touched, so a push that was cut short, or one carrying the LLM blob,
   * leaves the board with the credentials it already had. */

  ret = vg_mqtt_cred_install(blob, (size_t)n);
  memset(blob, 0, sizeof(blob));

  /* The staged bytes are spent either way: a refused push must not be left
   * behind for the next one to append to, which would produce a record that
   * is neither push.  The normal sequence is put... put, commit, with no
   * wipe in between, so a refused commit leaves the operator with an empty
   * staging file to push into again. */

  (void)unlink(VG_MQTT_STAGING_FILE);
  if (ret < 0)
    {
      printf("vgprovision: mqtt commit refused (%d); previous record kept\n",
             ret);
      return ret;
    }

  printf("vgprovision: mqtt credentials committed to %s (%ld bytes)\n",
         VG_MQTT_CRED_FILE, (long)n);
  warn_if_not_persistent();
  return 0;
}

static int cmd_mqtt_status(void)
{
  char line[160];
  int ret;

  ret = vg_mqtt_cred_status(line, sizeof(line));
  printf("vgprovision: %s\n", line);

  if (ret == 0)
    {
      printf("vgprovision: file=%s persistent=%s\n", VG_MQTT_CRED_FILE,
             (store_persistence() == 0) ? "yes" : "no");
    }

  return ret;
}

static int cmd_mqtt_wipe(void)
{
  (void)unlink(VG_MQTT_STAGING_FILE);
  (void)unlink(VG_MQTT_CRED_FILE);
  printf("vgprovision: mqtt credentials wiped\n");
  return 0;
}

static void *apply_worker(FAR void *arg)
{
  int ret;

  (void)arg;
  ret = vg_provision_apply_llm_config();
  printf("vgprovision: apply %d\n", ret);
  return NULL;
}

static int cmd_apply_async(void)
{
  pthread_t tid;
  pthread_attr_t attr;
  int ret;

  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 8192);
  ret = pthread_create(&tid, &attr, apply_worker, NULL);
  pthread_attr_destroy(&attr);
  if (ret != 0)
    {
      return -ret;
    }

  pthread_detach(tid);
  return 0;
}

static void print_uid(void)
{
  uint8_t uid[VG_PROVISION_UID_LEN];
  int ret;

  ret = vg_provision_read_uid(uid);
  if (ret < 0)
    {
      printf("vgprovision: uid error %d\n", ret);
      return;
    }

  printf("vgprovision: uid=");
  for (int i = 0; i < VG_PROVISION_UID_LEN; i++)
    {
      printf("%02x", uid[i]);
    }

  printf("\n");
}

int main(int argc, char *argv[])
{
  int ret;

  if (argc < 2)
    {
      usage();
      return 1;
    }

  if (strcmp(argv[1], "uid") == 0)
    {
      print_uid();
      return 0;
    }

  if (strcmp(argv[1], "status") == 0)
    {
      if (!vg_provision_is_present())
        {
          printf("vgprovision: not provisioned\n");
          return 1;
        }

      printf("vgprovision: OK file=%s\n", VG_PROVISION_FILE);
      return 0;
    }

  if (strcmp(argv[1], "wipe") == 0)
    {
      unlink(VG_STAGING_FILE);
      unlink(VG_PROVISION_FILE);
      printf("vgprovision: wiped\n");
      return 0;
    }

  if (strcmp(argv[1], "put") == 0)
    {
      if (argc < 3)
        {
          usage();
          return 1;
        }

      ret = cmd_put(argv[2]);
      if (ret < 0)
        {
          printf("vgprovision: put failed %d\n", ret);
          return 1;
        }

      return 0;
    }

  if (strcmp(argv[1], "commit") == 0)
    {
      ret = cmd_commit();
      return (ret < 0) ? 1 : 0;
    }

  if (strcmp(argv[1], "apply") == 0)
    {
      ret = cmd_apply_async();
      if (ret < 0)
        {
          printf("vgprovision: apply start failed %d\n", ret);
          return 1;
        }

      sleep(2);
      return 0;
    }

  if (strcmp(argv[1], "mqtt") == 0)
    {
      if (argc < 3)
        {
          usage();
          return 1;
        }

      if (strcmp(argv[2], "put") == 0)
        {
          if (argc < 4)
            {
              usage();
              return 1;
            }

          ret = cmd_put_to(VG_MQTT_STAGING_FILE, argv[3]);
          if (ret < 0)
            {
              printf("vgprovision: mqtt put failed %d\n", ret);
              return 1;
            }

          return 0;
        }

      if (strcmp(argv[2], "commit") == 0)
        {
          ret = cmd_mqtt_commit();
          return (ret < 0) ? 1 : 0;
        }

      if (strcmp(argv[2], "status") == 0)
        {
          ret = cmd_mqtt_status();
          return (ret < 0) ? 1 : 0;
        }

      if (strcmp(argv[2], "wipe") == 0)
        {
          ret = cmd_mqtt_wipe();
          return (ret < 0) ? 1 : 0;
        }

      usage();
      return 1;
    }

  usage();
  return 1;
}
