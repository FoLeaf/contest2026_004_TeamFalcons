/****************************************************************************
 * app/velaguard/vg_config_store.c
 *
 * Dual-slot config on a POSIX filesystem (NuttX or host).
 ****************************************************************************/

#include "vg_config_store.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __NuttX__
#  include <syslog.h>
#  define VG_CFG_LOG(...) syslog(LOG_WARNING, __VA_ARGS__)
#else
#  define VG_CFG_LOG(...) ((void)0)
#endif

static char g_basedir[256];
static char g_last_err[96];

/* CRC-32 (ISO 3309 / Ethernet), bit-wise — matches common lib_crc32. */

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
  size_t i;
  int bit;

  crc = ~crc;
  for (i = 0; i < len; i++)
    {
      crc ^= data[i];
      for (bit = 0; bit < 8; bit++)
        {
          if (crc & 1u)
            {
              crc = (crc >> 1) ^ 0xedb88320u;
            }
          else
            {
              crc >>= 1;
            }
        }
    }

  return ~crc;
}

static uint32_t payload_crc(const struct vg_config *cfg)
{
  char buf[128];
  int n;

  n = snprintf(buf, sizeof(buf), "%u|%u|%d|%s",
               (unsigned)cfg->schema_version,
               (unsigned)cfg->seq,
               cfg->committed ? 1 : 0,
               cfg->device_name);
  if (n <= 0 || (size_t)n >= sizeof(buf))
    {
      return 0;
    }

  return crc32_update(0, (const uint8_t *)buf, (size_t)n);
}

static void set_err(const char *stage, int ret)
{
  snprintf(g_last_err, sizeof(g_last_err), "%s ret=%d errno=%d",
           stage, ret, errno);
  VG_CFG_LOG("vgcfg: %s\n", g_last_err);
}

const char *vg_config_last_error(void)
{
  return g_last_err[0] != '\0' ? g_last_err : "none";
}

void vg_config_set_basedir(const char *dir)
{
  if (dir == NULL || dir[0] == '\0')
    {
      g_basedir[0] = '\0';
      return;
    }

  snprintf(g_basedir, sizeof(g_basedir), "%s", dir);
}

const char *vg_config_get_basedir(void)
{
  return g_basedir;
}

void vg_config_factory_default(struct vg_config *out)
{
  memset(out, 0, sizeof(*out));
  out->schema_version = VG_CFG_SCHEMA_VERSION;
  out->seq = 0;
  out->committed = true;
  snprintf(out->device_name, sizeof(out->device_name), "factory");
}

static int slot_path(int slot, char *out, size_t outlen)
{
  const char *name = (slot == 0) ? VG_CFG_SLOT_A_NAME : VG_CFG_SLOT_B_NAME;

  if (g_basedir[0] == '\0')
    {
      return -EINVAL;
    }

  snprintf(out, outlen, "%s/%s", g_basedir, name);
  return 0;
}

static int ensure_basedir(void)
{
  struct stat st;
  char tmp[256];
  char *p;

  if (g_basedir[0] == '\0')
    {
      return -EINVAL;
    }

  if (stat(g_basedir, &st) == 0)
    {
      if (S_ISDIR(st.st_mode))
        {
          return 0;
        }

      /* NuttX FAT occasionally omits S_IFDIR; try using the path anyway if
       * it is not a regular file we know we created by mistake.
       */

      if (S_ISREG(st.st_mode))
        {
          return -ENOTDIR;
        }

      return 0;
    }

  /* mkdir -p: create each component of g_basedir. */

  snprintf(tmp, sizeof(tmp), "%s", g_basedir);
  for (p = tmp + 1; *p != '\0'; p++)
    {
      if (*p != '/')
        {
          continue;
        }

      *p = '\0';
      if (stat(tmp, &st) != 0)
        {
          if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
            {
              return -errno;
            }
        }
      else if (S_ISREG(st.st_mode))
        {
          return -ENOTDIR;
        }

      *p = '/';
    }

  if (mkdir(g_basedir, 0755) != 0 && errno != EEXIST)
    {
      return -errno;
    }

  if (stat(g_basedir, &st) != 0)
    {
      return -errno;
    }

  if (S_ISREG(st.st_mode))
    {
      return -ENOTDIR;
    }

  return 0;
}

static int parse_slot_line(const char *line, struct vg_config *out,
                           unsigned *crc_field)
{
  unsigned schema = 0;
  unsigned seq = 0;
  unsigned committed = 0;
  unsigned crc = 0;
  const char *p;
  const char *end;
  size_t n;
  int got;

  /* Avoid sscanf scanset — CONFIG_LIBC_SCANSET is often off on NuttX. */

  got = sscanf(line,
               "{\"schema_version\":%u,\"seq\":%u,\"committed\":%u,"
               "\"crc32\":%u,",
               &schema, &seq, &committed, &crc);
  if (got != 4)
    {
      return -EINVAL;
    }

  p = strstr(line, "\"device_name\":\"");
  if (p == NULL)
    {
      return -EINVAL;
    }

  p += sizeof("\"device_name\":\"") - 1;
  end = strchr(p, '"');
  if (end == NULL || end == p)
    {
      return -EINVAL;
    }

  n = (size_t)(end - p);
  if (n > VG_CFG_DEVICE_NAME_MAX)
    {
      return -EINVAL;
    }

  memset(out, 0, sizeof(*out));
  out->schema_version = schema;
  out->seq = seq;
  out->committed = (committed != 0);
  memcpy(out->device_name, p, n);
  out->device_name[n] = '\0';
  *crc_field = crc;
  return 0;
}

static int read_slot(int slot, struct vg_config *out, uint32_t *crc_out)
{
  char path[300];
  char line[256];
  int fd;
  ssize_t nread;
  unsigned crc = 0;
  uint32_t expect;
  int ret;

  if (slot_path(slot, path, sizeof(path)) != 0)
    {
      return -EINVAL;
    }

  fd = open(path, O_RDONLY);
  if (fd < 0)
    {
      return -errno;
    }

  nread = read(fd, line, sizeof(line) - 1);
  close(fd);
  if (nread < 0)
    {
      return -errno;
    }

  if (nread == 0)
    {
      return -EIO;
    }

  line[nread] = '\0';

  ret = parse_slot_line(line, out, &crc);
  if (ret != 0)
    {
      return ret;
    }

  expect = payload_crc(out);
  if (crc_out)
    {
      *crc_out = expect;
    }

  if (crc != expect)
    {
      return -EINVAL;
    }

  if (out->schema_version != VG_CFG_SCHEMA_VERSION)
    {
      return -EINVAL;
    }

  if (!out->committed)
    {
      return -EINVAL;
    }

  return 0;
}

static int write_slot_raw(int slot, const struct vg_config *cfg)
{
  char path[300];
  char body[256];
  uint32_t crc;
  int fd;
  int n;
  ssize_t nw;
  int ret;

  ret = slot_path(slot, path, sizeof(path));
  if (ret != 0)
    {
      return ret;
    }

  crc = payload_crc(cfg);
  n = snprintf(body, sizeof(body),
               "{\"schema_version\":%u,\"seq\":%u,\"committed\":%u,"
               "\"crc32\":%u,\"device_name\":\"%s\"}\n",
               (unsigned)cfg->schema_version,
               (unsigned)cfg->seq,
               cfg->committed ? 1u : 0u,
               (unsigned)crc,
               cfg->device_name);
  if (n <= 0 || (size_t)n >= sizeof(body))
    {
      return -EINVAL;
    }

  /* Single-shot POSIX write — avoids stdio/FAT fclose quirks. */

  fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0)
    {
      return -errno;
    }

  nw = write(fd, body, (size_t)n);
  if (nw != n)
    {
      ret = (nw < 0) ? -errno : -EIO;
      close(fd);
      return ret;
    }

  if (fsync(fd) != 0)
    {
      ret = -errno;
      close(fd);
      return ret;
    }

  if (close(fd) != 0)
    {
      return -errno;
    }

  return 0;
}

int vg_config_load(struct vg_config *out)
{
  struct vg_config a;
  struct vg_config b;
  int ra;
  int rb;
  bool a_ok;
  bool b_ok;

  if (out == NULL)
    {
      return -EINVAL;
    }

  ra = read_slot(0, &a, NULL);
  rb = read_slot(1, &b, NULL);
  a_ok = (ra == 0);
  b_ok = (rb == 0);

  if (a_ok && b_ok)
    {
      *out = (a.seq >= b.seq) ? a : b;
      return 0;
    }

  if (a_ok)
    {
      *out = a;
      return 0;
    }

  if (b_ok)
    {
      *out = b;
      return 0;
    }

  vg_config_factory_default(out);
  VG_CFG_LOG("vgcfg: both slots invalid; factory default\n");
  return 1;
}

int vg_config_commit(const struct vg_config *in)
{
  struct vg_config next;
  struct vg_config a;
  struct vg_config b;
  int inactive;
  uint32_t max_seq = 0;
  int ra;
  int rb;
  int ret;

  g_last_err[0] = '\0';

  if (in == NULL)
    {
      set_err("null", -EINVAL);
      return -EINVAL;
    }

  ret = ensure_basedir();
  if (ret != 0)
    {
      set_err("mkdir", ret);
      return ret;
    }

  ra = read_slot(0, &a, NULL);
  rb = read_slot(1, &b, NULL);
  if (ra == 0 && a.seq > max_seq)
    {
      max_seq = a.seq;
    }

  if (rb == 0 && b.seq > max_seq)
    {
      max_seq = b.seq;
    }

  if (ra == 0 && rb == 0)
    {
      inactive = (a.seq >= b.seq) ? 1 : 0;
    }
  else if (ra == 0)
    {
      inactive = 1;
    }
  else if (rb == 0)
    {
      inactive = 0;
    }
  else
    {
      inactive = 0;
    }

  next = *in;
  next.schema_version = VG_CFG_SCHEMA_VERSION;
  next.seq = max_seq + 1;
  next.committed = false;

  ret = write_slot_raw(inactive, &next);
  if (ret != 0)
    {
      set_err("write0", ret);
      return ret;
    }

  next.committed = true;
  ret = write_slot_raw(inactive, &next);
  if (ret != 0)
    {
      set_err("write1", ret);
      return ret;
    }

  {
    struct vg_config verify;
    int vr = read_slot(inactive, &verify, NULL);
    if (vr != 0 || verify.seq != next.seq ||
        strcmp(verify.device_name, next.device_name) != 0)
      {
        set_err("verify", vr != 0 ? vr : -EIO);
        return -EIO;
      }
  }

  return 0;
}

int vg_config_damage_slot(int slot, int mode)
{
  char path[300];
  int fd;
  int ret;

  if (slot != 0 && slot != 1)
    {
      return -EINVAL;
    }

  ret = slot_path(slot, path, sizeof(path));
  if (ret != 0)
    {
      return ret;
    }

  if (mode == 0)
    {
      fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0)
        {
          return -errno;
        }

      close(fd);
      return 0;
    }

  if (mode == 1)
    {
      char line[256];
      ssize_t nread;
      size_t len;

      fd = open(path, O_RDWR);
      if (fd < 0)
        {
          return -errno;
        }

      nread = read(fd, line, sizeof(line) - 1);
      if (nread <= 0)
        {
          close(fd);
          return nread < 0 ? -errno : -EIO;
        }

      line[nread] = '\0';
      len = (size_t)nread;
      if (len > 10)
        {
          line[len / 2] ^= 0x55;
        }

      if (lseek(fd, 0, SEEK_SET) < 0)
        {
          ret = -errno;
          close(fd);
          return ret;
        }

      if (write(fd, line, len) != (ssize_t)len)
        {
          ret = -errno;
          close(fd);
          return ret;
        }

      (void)fsync(fd);
      close(fd);
      return 0;
    }

  return -EINVAL;
}

int vg_config_probe(char *msg, size_t msglen)
{
  char path[300];
  char body[] = "vgcfg-probe\n";
  char got[32];
  struct stat st;
  int fd;
  int ret;
  ssize_t n;

  if (msg == NULL || msglen == 0)
    {
      return -EINVAL;
    }

  msg[0] = '\0';

  if (g_basedir[0] == '\0')
    {
      snprintf(msg, msglen, "basedir empty");
      return -EINVAL;
    }

  if (stat("/data", &st) != 0 && stat("/mnt/emmc", &st) != 0)
    {
      snprintf(msg, msglen, "data/emmc mount missing errno=%d", errno);
      return -errno;
    }

  ret = ensure_basedir();
  if (ret != 0)
    {
      snprintf(msg, msglen, "ensure_basedir %d errno=%d dir=%s",
               ret, errno, g_basedir);
      return ret;
    }

  snprintf(path, sizeof(path), "%s/probe.txt", g_basedir);
  fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0)
    {
      snprintf(msg, msglen, "open %s fail errno=%d", path, errno);
      return -errno;
    }

  n = write(fd, body, sizeof(body) - 1);
  if (n != (ssize_t)(sizeof(body) - 1))
    {
      snprintf(msg, msglen, "write probe fail n=%d errno=%d", (int)n, errno);
      close(fd);
      return -EIO;
    }

  if (fsync(fd) != 0)
    {
      snprintf(msg, msglen, "fsync probe fail errno=%d", errno);
      close(fd);
      return -errno;
    }

  close(fd);

  fd = open(path, O_RDONLY);
  if (fd < 0)
    {
      snprintf(msg, msglen, "reopen %s fail errno=%d", path, errno);
      return -errno;
    }

  memset(got, 0, sizeof(got));
  n = read(fd, got, sizeof(got) - 1);
  close(fd);
  if (n != (ssize_t)(sizeof(body) - 1) || strcmp(got, "vgcfg-probe\n") != 0)
    {
      snprintf(msg, msglen, "readback mismatch n=%d got='%.16s'", (int)n, got);
      return -EIO;
    }

  snprintf(msg, msglen, "ok dir=%s", g_basedir);
  return 0;
}
