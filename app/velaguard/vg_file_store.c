/* SPDX-License-Identifier: Apache-2.0 */
#include "vg_file_store.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define VG_STORE_PATH_MAX 256
static pthread_mutex_t g_store_lock = PTHREAD_MUTEX_INITIALIZER;

static int write_all(int fd, const unsigned char *data, size_t len)
{
  while (len > 0)
    {
      ssize_t n = write(fd, data, len);
      if (n < 0 && errno == EINTR) continue;
      if (n <= 0) return n < 0 ? -errno : -EIO;
      data += n;
      len -= (size_t)n;
    }
  return 0;
}

static int write_verified(const char *path, const void *data, size_t len,
                          mode_t mode)
{
  unsigned char check[256];
  const unsigned char *bytes = data;
  size_t off = 0;
  int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, mode);
  int rc;

  if (fd < 0) return -errno;
  rc = write_all(fd, data, len);
  if (rc == 0 && fsync(fd) != 0) rc = -errno;
  if (rc == 0 && lseek(fd, 0, SEEK_SET) < 0) rc = -errno;
  while (rc == 0 && off < len)
    {
      size_t want = len - off < sizeof(check) ? len - off : sizeof(check);
      ssize_t n = read(fd, check, want);
      if (n < 0 && errno == EINTR) continue;
      if (n <= 0 || memcmp(check, bytes + off, (size_t)n) != 0)
        { rc = -EIO; break; }
      off += (size_t)n;
    }
  if (close(fd) != 0 && rc == 0) rc = -errno;
  if (rc != 0) unlink(path);
  return rc;
}

static int verify_copy(int source, int copy)
{
  unsigned char expected[256];
  unsigned char actual[256];
  if (lseek(source, 0, SEEK_SET) < 0 || lseek(copy, 0, SEEK_SET) < 0)
    return -errno;
  for (;;)
    {
      ssize_t n = read(source, expected, sizeof(expected));
      if (n < 0 && errno == EINTR) continue;
      if (n < 0) return -errno;
      if (n == 0)
        {
          do { n = read(copy, actual, 1); } while (n < 0 && errno == EINTR);
          return n < 0 ? -errno : n == 0 ? 0 : -EIO;
        }
      size_t off = 0;
      while (off < (size_t)n)
        {
          ssize_t got = read(copy, actual + off, (size_t)n - off);
          if (got < 0 && errno == EINTR) continue;
          if (got <= 0) return got < 0 ? -errno : -EIO;
          off += (size_t)got;
        }
      if (memcmp(expected, actual, (size_t)n) != 0) return -EIO;
    }
}

static int backup_file(const char *path, const char *backup, mode_t mode)
{
  unsigned char buf[256];
  int in = open(path, O_RDONLY);
  int out;
  int rc = 0;

  if (in < 0) return errno == ENOENT ? 0 : -errno;
  out = open(backup, O_RDWR | O_CREAT | O_TRUNC, mode);
  if (out < 0) { rc = -errno; close(in); return rc; }
  for (;;)
    {
      ssize_t n = read(in, buf, sizeof(buf));
      if (n < 0 && errno == EINTR) continue;
      if (n < 0) { rc = -errno; break; }
      if (n == 0) break;
      rc = write_all(out, buf, (size_t)n);
      if (rc != 0) break;
    }
  if (rc == 0 && fsync(out) != 0) rc = -errno;
  if (rc == 0) rc = verify_copy(in, out);
  if (close(out) != 0 && rc == 0) rc = -errno;
  close(in);
  return rc;
}

static int store_file(const char *path, const void *data, size_t len, mode_t mode,
                      int create_only)
{
  char tmp[VG_STORE_PATH_MAX];
  char bak[VG_STORE_PATH_MAX];
  char bak_tmp[VG_STORE_PATH_MAX];
  int rc;

  if (path == NULL || path[0] == '\0' || (data == NULL && len != 0))
    return -EINVAL;
  if (snprintf(bak_tmp, sizeof(bak_tmp), "%s.bak.tmp", path) >=
      (int)sizeof(bak_tmp)) return -ENAMETOOLONG;
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  snprintf(bak, sizeof(bak), "%s.bak", path);

  pthread_mutex_lock(&g_store_lock);
  if (create_only && access(path, F_OK) == 0)
    { pthread_mutex_unlock(&g_store_lock); return -EEXIST; }
  unlink(bak_tmp);
  rc = write_verified(tmp, data, len, mode);
  if (rc == 0)
    {
      int had_old = access(path, F_OK) == 0;
      rc = backup_file(path, bak_tmp, mode);
      if (rc == 0 && access(bak_tmp, F_OK) == 0 && rename(bak_tmp, bak) != 0)
        rc = -errno;
      if (rc == 0 && rename(tmp, path) != 0)
        {
          rc = -errno;
          /* NuttX VFS may unlink the destination before FAT rename fails.
           * Keep the backup and restore the old path when that happened. */
          if (had_old && access(path, F_OK) != 0 && errno == ENOENT &&
              access(bak, F_OK) == 0)
            {
              if (backup_file(bak, path, mode) != 0) unlink(path);
            }
        }
    }
  if (rc != 0) unlink(tmp);
  unlink(bak_tmp);
  pthread_mutex_unlock(&g_store_lock);
  return rc;
}

int vg_file_replace(const char *path, const void *data, size_t len, mode_t mode)
{
  return store_file(path, data, len, mode, 0);
}

int vg_file_create(const char *path, const void *data, size_t len, mode_t mode)
{
  return store_file(path, data, len, mode, 1);
}
