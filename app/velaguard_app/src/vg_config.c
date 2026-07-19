/****************************************************************************
 * app/velaguard_app/src/vg_config.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Alarm and network configuration load/save for VelaGuard.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netutils/cJSON.h>

#include "vg_config.h"
#include "vg_log.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VG_ALARM_CFG_PATH "/data/velaguard/configs/alarm.json"

#define VG_DEFAULT_WARN_C     70.0f
#define VG_DEFAULT_CRIT_C     80.0f
#define VG_DEFAULT_RESTORE_C  68.0f
#define VG_DEFAULT_TRIGGER_MS 2000u
#define VG_DEFAULT_RESTORE_MS 2000u

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct vg_alarm_config g_alarm_cfg;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void vg_config_set_defaults(void)
{
  g_alarm_cfg.warn_c = VG_DEFAULT_WARN_C;
  g_alarm_cfg.crit_c = VG_DEFAULT_CRIT_C;
  g_alarm_cfg.restore_c = VG_DEFAULT_RESTORE_C;
  g_alarm_cfg.trigger_ms = VG_DEFAULT_TRIGGER_MS;
  g_alarm_cfg.restore_ms = VG_DEFAULT_RESTORE_MS;
  g_alarm_cfg.loaded_from_file = false;
}

static bool vg_parse_float_field(FAR const char *json, FAR const char *key,
                                 FAR float *out)
{
  FAR char *pos;
  char pattern[48];

  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  pos = strstr(json, pattern);
  if (pos == NULL)
    {
      return false;
    }

  pos = strchr(pos, ':');
  if (pos == NULL)
    {
      return false;
    }

  *out = strtof(pos + 1, NULL);
  return true;
}

static bool vg_parse_uint_field(FAR const char *json, FAR const char *key,
                                FAR unsigned int *out)
{
  float value;

  if (!vg_parse_float_field(json, key, &value))
    {
      return false;
    }

  if (value < 0.0f)
    {
      return false;
    }

  *out = (unsigned int)value;
  return true;
}

static int vg_config_write_defaults(void)
{
  FAR FILE *stream = fopen(VG_ALARM_CFG_PATH, "w");
  int result = 0;

  if (stream == NULL)
    {
      return -errno;
    }

  if (fprintf(stream,
              "{\n"
              "  \"warn_c\": %.1f,\n"
              "  \"crit_c\": %.1f,\n"
              "  \"restore_c\": %.1f,\n"
              "  \"trigger_ms\": %u,\n"
              "  \"restore_ms\": %u\n"
              "}\n",
              (double)g_alarm_cfg.warn_c,
              (double)g_alarm_cfg.crit_c,
              (double)g_alarm_cfg.restore_c,
              g_alarm_cfg.trigger_ms,
              g_alarm_cfg.restore_ms) < 0)
    {
      result = -EIO;
    }

  if (fclose(stream) != 0 && result == 0)
    {
      result = -errno;
    }

  return result;
}

static int vg_config_read_file(void)
{
  char buffer[512];
  size_t nread;
  FAR FILE *stream = fopen(VG_ALARM_CFG_PATH, "r");
  float warn_c;
  float crit_c;
  float restore_c;
  unsigned int trigger_ms;
  unsigned int restore_ms;
  bool ok;

  if (stream == NULL)
    {
      return -errno;
    }

  nread = fread(buffer, 1, sizeof(buffer) - 1, stream);
  buffer[nread] = '\0';
  fclose(stream);

  if (nread == 0)
    {
      return -EINVAL;
    }

  ok = vg_parse_float_field(buffer, "warn_c", &warn_c) &&
       vg_parse_float_field(buffer, "crit_c", &crit_c) &&
       vg_parse_float_field(buffer, "restore_c", &restore_c) &&
       vg_parse_uint_field(buffer, "trigger_ms", &trigger_ms) &&
       vg_parse_uint_field(buffer, "restore_ms", &restore_ms);

  if (!ok || warn_c >= crit_c || restore_c > warn_c ||
      trigger_ms == 0 || restore_ms == 0)
    {
      return -EINVAL;
    }

  g_alarm_cfg.warn_c = warn_c;
  g_alarm_cfg.crit_c = crit_c;
  g_alarm_cfg.restore_c = restore_c;
  g_alarm_cfg.trigger_ms = trigger_ms;
  g_alarm_cfg.restore_ms = restore_ms;
  g_alarm_cfg.loaded_from_file = true;
  return 0;
}

static int vg_copy_json_string(FAR cJSON *item, FAR char *out, size_t out_size)
{
  FAR const char *value;

  if (item == NULL || !cJSON_IsString(item) || item->valuestring == NULL)
    {
      return -EINVAL;
    }

  value = item->valuestring;
  if (strlen(value) >= out_size)
    {
      return -EINVAL;
    }

  strlcpy(out, value, out_size);
  return 0;
}

static bool vg_ipv4_parse(FAR const char *text, FAR struct in_addr *addr)
{
  if (text == NULL || text[0] == '\0')
    {
      return false;
    }

  return inet_pton(AF_INET, text, addr) == 1;
}

static bool vg_netmask_is_contiguous(uint32_t mask_host)
{
  uint32_t inverted;

  if (mask_host == 0)
    {
      return false;
    }

  inverted = ~mask_host;
  return (inverted & (inverted + 1u)) == 0;
}

static int vg_fsync_path(FAR const char *path)
{
  int fd = open(path, O_RDONLY);
  int result = 0;

  if (fd < 0)
    {
      return -errno;
    }

  if (fsync(fd) < 0)
    {
      result = -errno;
    }

  close(fd);
  return result;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int vg_config_load(void)
{
  int result;

  vg_config_set_defaults();
  result = vg_config_read_file();
  if (result == 0)
    {
      vg_log_human("INFO", "alarm config loaded from file");
      return 0;
    }

  result = vg_config_write_defaults();
  if (result == 0)
    {
      vg_log_human("INFO", "alarm config defaults written");
      g_alarm_cfg.loaded_from_file = true;
    }
  else
    {
      vg_log_human("WARN", "alarm config using built-in defaults");
      g_alarm_cfg.loaded_from_file = false;
    }

  return result;
}

FAR const struct vg_alarm_config *vg_config_alarm(void)
{
  return &g_alarm_cfg;
}

void vg_config_network_defaults(FAR struct vg_network_config *cfg)
{
  if (cfg == NULL)
    {
      return;
    }

  memset(cfg, 0, sizeof(*cfg));
  cfg->version = VG_NETWORK_CFG_VERSION;
  cfg->mode = VG_NETWORK_MODE_DHCP;
  cfg->ipv4[0] = '\0';
  cfg->netmask[0] = '\0';
  cfg->gateway[0] = '\0';
  cfg->dns[0] = '\0';
  cfg->loaded_from_file = false;
}

int vg_config_network_validate(FAR const struct vg_network_config *cfg)
{
  struct in_addr address;
  struct in_addr netmask;
  struct in_addr gateway;
  struct in_addr dns;
  uint32_t addr_h;
  uint32_t mask_h;
  uint32_t gw_h;
  uint32_t network;
  uint32_t broadcast;

  if (cfg == NULL || cfg->version != VG_NETWORK_CFG_VERSION)
    {
      return -EINVAL;
    }

  if (cfg->mode == VG_NETWORK_MODE_DHCP)
    {
      /* DHCP may retain previous static fields, but each non-empty field
       * must still be syntactically valid.
       */

      if (cfg->ipv4[0] != '\0' && !vg_ipv4_parse(cfg->ipv4, &address))
        {
          return -EINVAL;
        }

      if (cfg->netmask[0] != '\0' && !vg_ipv4_parse(cfg->netmask, &netmask))
        {
          return -EINVAL;
        }

      if (cfg->gateway[0] != '\0' && !vg_ipv4_parse(cfg->gateway, &gateway))
        {
          return -EINVAL;
        }

      if (cfg->dns[0] != '\0' && !vg_ipv4_parse(cfg->dns, &dns))
        {
          return -EINVAL;
        }

      return 0;
    }

  if (cfg->mode != VG_NETWORK_MODE_STATIC)
    {
      return -EINVAL;
    }

  if (!vg_ipv4_parse(cfg->ipv4, &address) ||
      !vg_ipv4_parse(cfg->netmask, &netmask) ||
      !vg_ipv4_parse(cfg->gateway, &gateway) ||
      !vg_ipv4_parse(cfg->dns, &dns))
    {
      return -EINVAL;
    }

  addr_h = ntohl(address.s_addr);
  mask_h = ntohl(netmask.s_addr);
  gw_h = ntohl(gateway.s_addr);

  if (!vg_netmask_is_contiguous(mask_h))
    {
      return -EINVAL;
    }

  /* Reject unspecified, loopback, multicast, and link-local host addresses. */

  if (addr_h == 0 ||
      (addr_h & 0xff000000u) == 0x7f000000u ||
      (addr_h & 0xf0000000u) == 0xe0000000u ||
      (addr_h & 0xffff0000u) == 0xa9fe0000u)
    {
      return -EINVAL;
    }

  network = addr_h & mask_h;
  broadcast = network | ~mask_h;
  if (addr_h == network || addr_h == broadcast)
    {
      return -EINVAL;
    }

  if ((gw_h & mask_h) != network || gw_h == network || gw_h == broadcast)
    {
      return -EINVAL;
    }

  if (dns.s_addr == INADDR_ANY ||
      (ntohl(dns.s_addr) & 0xf0000000u) == 0xe0000000u)
    {
      return -EINVAL;
    }

  return 0;
}

int vg_config_network_load(FAR struct vg_network_config *cfg)
{
  FAR FILE *stream;
  FAR char *buffer = NULL;
  FAR cJSON *root = NULL;
  FAR cJSON *item;
  long length;
  size_t nread;
  int result = 0;
  struct vg_network_config local;

  if (cfg == NULL)
    {
      return -EINVAL;
    }

  vg_config_network_defaults(&local);

  stream = fopen(VG_NETWORK_CFG_PATH, "r");
  if (stream == NULL)
    {
      result = vg_config_network_save(&local);
      if (result == 0)
        {
          local.loaded_from_file = true;
          *cfg = local;
          vg_log_human("INFO", "network config defaults written");
        }
      else
        {
          *cfg = local;
          vg_log_human("WARN", "network config using built-in DHCP default");
        }

      return result;
    }

  if (fseek(stream, 0, SEEK_END) != 0)
    {
      result = -errno;
      fclose(stream);
      return result;
    }

  length = ftell(stream);
  if (length < 0 || length > 4096)
    {
      fclose(stream);
      return -EINVAL;
    }

  if (fseek(stream, 0, SEEK_SET) != 0)
    {
      result = -errno;
      fclose(stream);
      return result;
    }

  buffer = (FAR char *)malloc((size_t)length + 1u);
  if (buffer == NULL)
    {
      fclose(stream);
      return -ENOMEM;
    }

  nread = fread(buffer, 1, (size_t)length, stream);
  buffer[nread] = '\0';
  fclose(stream);

  root = cJSON_Parse(buffer);
  free(buffer);
  if (root == NULL || !cJSON_IsObject(root))
    {
      cJSON_Delete(root);
      return -EINVAL;
    }

  item = cJSON_GetObjectItemCaseSensitive(root, "version");
  if (item == NULL || !cJSON_IsNumber(item))
    {
      cJSON_Delete(root);
      return -EINVAL;
    }

  local.version = item->valueint;

  item = cJSON_GetObjectItemCaseSensitive(root, "mode");
  if (item == NULL || !cJSON_IsString(item) || item->valuestring == NULL)
    {
      cJSON_Delete(root);
      return -EINVAL;
    }

  if (strcmp(item->valuestring, "dhcp") == 0)
    {
      local.mode = VG_NETWORK_MODE_DHCP;
    }
  else if (strcmp(item->valuestring, "static") == 0)
    {
      local.mode = VG_NETWORK_MODE_STATIC;
    }
  else
    {
      cJSON_Delete(root);
      return -EINVAL;
    }

  result = vg_copy_json_string(cJSON_GetObjectItemCaseSensitive(root, "ipv4"),
                               local.ipv4, sizeof(local.ipv4));
  if (result == 0)
    {
      result = vg_copy_json_string(
                 cJSON_GetObjectItemCaseSensitive(root, "netmask"),
                 local.netmask, sizeof(local.netmask));
    }

  if (result == 0)
    {
      result = vg_copy_json_string(
                 cJSON_GetObjectItemCaseSensitive(root, "gateway"),
                 local.gateway, sizeof(local.gateway));
    }

  if (result == 0)
    {
      result = vg_copy_json_string(
                 cJSON_GetObjectItemCaseSensitive(root, "dns"),
                 local.dns, sizeof(local.dns));
    }

  cJSON_Delete(root);
  if (result < 0)
    {
      return result;
    }

  result = vg_config_network_validate(&local);
  if (result < 0)
    {
      return result;
    }

  local.loaded_from_file = true;
  *cfg = local;
  vg_log_human("INFO", "network config loaded from file");
  return 0;
}

int vg_config_network_write_tmp(FAR const struct vg_network_config *cfg)
{
  FAR FILE *stream;
  FAR const char *mode;
  int result = 0;

  if (cfg == NULL)
    {
      return -EINVAL;
    }

  result = vg_config_network_validate(cfg);
  if (result < 0)
    {
      return result;
    }

  stream = fopen(VG_NETWORK_CFG_TMP_PATH, "w");
  if (stream == NULL)
    {
      return -errno;
    }

  mode = cfg->mode == VG_NETWORK_MODE_STATIC ? "static" : "dhcp";
  if (fprintf(stream,
              "{\n"
              "  \"version\": %d,\n"
              "  \"mode\": \"%s\",\n"
              "  \"ipv4\": \"%s\",\n"
              "  \"netmask\": \"%s\",\n"
              "  \"gateway\": \"%s\",\n"
              "  \"dns\": \"%s\"\n"
              "}\n",
              cfg->version,
              mode,
              cfg->ipv4,
              cfg->netmask,
              cfg->gateway,
              cfg->dns) < 0)
    {
      result = -EIO;
    }

  if (fflush(stream) != 0 && result == 0)
    {
      result = -errno;
    }

  if (fsync(fileno(stream)) < 0 && result == 0)
    {
      result = -errno;
    }

  if (fclose(stream) != 0 && result == 0)
    {
      result = -errno;
    }

  if (result < 0)
    {
      unlink(VG_NETWORK_CFG_TMP_PATH);
    }

  return result;
}

int vg_config_network_commit(void)
{
  int result;

  result = rename(VG_NETWORK_CFG_TMP_PATH, VG_NETWORK_CFG_PATH);
  if (result < 0)
    {
      result = -errno;
      unlink(VG_NETWORK_CFG_TMP_PATH);
      return result;
    }

  /* Best-effort durability for the final path. */

  vg_fsync_path(VG_NETWORK_CFG_PATH);
  return 0;
}

int vg_config_network_abort_tmp(void)
{
  if (unlink(VG_NETWORK_CFG_TMP_PATH) < 0 && errno != ENOENT)
    {
      return -errno;
    }

  return 0;
}

int vg_config_network_save(FAR const struct vg_network_config *cfg)
{
  int result;

  result = vg_config_network_write_tmp(cfg);
  if (result < 0)
    {
      return result;
    }

  result = vg_config_network_commit();
  if (result < 0)
    {
      vg_config_network_abort_tmp();
    }

  return result;
}
