/****************************************************************************
 * app/velaguard_app/src/vg_network.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Async network status + apply orchestration for VelaGuard UI.
 * UI only reads snapshots; this module owns file I/O and netinit policy.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include <netutils/netinit.h>
#include <netutils/netlib.h>

#include "vg_log.h"
#include "vg_network.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_NETINIT_NETLOCAL
#  define VG_NET_IFNAME "eth0"
#else
#  define VG_NET_IFNAME "eth0"
#endif

#define VG_NETWORK_POLL_MS 500

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* g_network_lock protects only snapshot/config copies visible to UI.
 * Network I/O never runs while this lock is held.
 */

static pthread_mutex_t g_network_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_network_cond = PTHREAD_COND_INITIALIZER;
static struct vg_network_config g_saved_config;
static struct vg_network_config g_pending_config;
static struct vg_network_status g_status;
static bool g_apply_pending;
static bool g_boot_policy_pending;
static bool g_started;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void vg_network_set_message_locked(FAR const char *message)
{
  if (message == NULL)
    {
      g_status.message[0] = '\0';
      return;
    }

  strlcpy(g_status.message, message, sizeof(g_status.message));
}

static void vg_network_set_feedback_locked(FAR const char *feedback)
{
  if (feedback == NULL)
    {
      g_status.feedback[0] = '\0';
      return;
    }

  strlcpy(g_status.feedback, feedback, sizeof(g_status.feedback));
}

static bool vg_network_query_carrier(void)
{
  struct ifreq ifr;
  int sd;
  int ret;

  sd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sd < 0)
    {
      return false;
    }

  memset(&ifr, 0, sizeof(ifr));
  strlcpy(ifr.ifr_name, VG_NET_IFNAME, IFNAMSIZ);
  ret = ioctl(sd, SIOCGIFFLAGS, (unsigned long)&ifr);
  close(sd);
  if (ret < 0)
    {
      return false;
    }

  return (ifr.ifr_flags & IFF_RUNNING) != 0;
}

static void vg_network_query_ipv4(FAR char *out, size_t out_size)
{
  struct in_addr addr;

  out[0] = '\0';
  addr.s_addr = INADDR_ANY;
  if (netlib_get_ipv4addr(VG_NET_IFNAME, &addr) < 0)
    {
      return;
    }

  if (addr.s_addr == INADDR_ANY)
    {
      return;
    }

  inet_ntop(AF_INET, &addr, out, out_size);
}

/* Publish a runtime snapshot.  Queries run without g_network_lock; only the
 * short memcpy into g_status is locked so UI never waits on network I/O.
 */

static void vg_network_publish_runtime(void)
{
  bool carrier;
  char ipv4[INET_ADDRSTRLEN];
  enum vg_network_mode mode;
  enum vg_network_state state;

  carrier = vg_network_query_carrier();
  vg_network_query_ipv4(ipv4, sizeof(ipv4));

  pthread_mutex_lock(&g_network_lock);
  if (g_status.state == VG_NETWORK_APPLYING)
    {
      /* Still refresh carrier/IP while apply is in flight. */

      g_status.carrier = carrier;
      strlcpy(g_status.ipv4, ipv4, sizeof(g_status.ipv4));
      pthread_mutex_unlock(&g_network_lock);
      return;
    }

  mode = g_saved_config.mode;
  g_status.carrier = carrier;
  g_status.mode = mode;
  strlcpy(g_status.ipv4, ipv4, sizeof(g_status.ipv4));

  /* Unrecoverable apply failure keeps ERROR; still refresh carrier/IP. */

  if (g_status.state == VG_NETWORK_ERROR)
    {
      pthread_mutex_unlock(&g_network_lock);
      return;
    }

  if (!carrier)
    {
      state = VG_NETWORK_DOWN;
      vg_network_set_message_locked("无链路");
    }
  else if (ipv4[0] == '\0')
    {
      state = VG_NETWORK_ADDRESSING;
      vg_network_set_message_locked(
        mode == VG_NETWORK_MODE_DHCP ? "正在获取地址" : "等待地址");
    }
  else
    {
      state = VG_NETWORK_ONLINE;
      vg_network_set_message_locked("已联网");
    }

  g_status.state = state;
  pthread_mutex_unlock(&g_network_lock);
}

static int vg_network_to_netinit(FAR const struct vg_network_config *cfg,
                                 FAR struct netinit_ipv4_config *out)
{
  memset(out, 0, sizeof(*out));

  if (cfg->mode == VG_NETWORK_MODE_DHCP)
    {
      out->mode = NETINIT_IPV4_DHCP;
      out->address.s_addr = INADDR_ANY;
      out->netmask.s_addr = INADDR_ANY;
      out->router.s_addr = INADDR_ANY;
      out->dns.s_addr = INADDR_ANY;
      return 0;
    }

  out->mode = NETINIT_IPV4_STATIC;
  if (inet_pton(AF_INET, cfg->ipv4, &out->address) != 1 ||
      inet_pton(AF_INET, cfg->netmask, &out->netmask) != 1 ||
      inet_pton(AF_INET, cfg->gateway, &out->router) != 1 ||
      inet_pton(AF_INET, cfg->dns, &out->dns) != 1)
    {
      return -EINVAL;
    }

  return 0;
}

static int vg_network_apply_runtime(FAR const struct vg_network_config *cfg)
{
  struct netinit_ipv4_config policy;
  int result;

  result = vg_network_to_netinit(cfg, &policy);
  if (result < 0)
    {
      return result;
    }

  return netinit_set_ipv4_config(&policy);
}

static void vg_network_handle_boot_policy(void)
{
  struct vg_network_config cfg;
  int result;

  pthread_mutex_lock(&g_network_lock);
  if (!g_boot_policy_pending)
    {
      pthread_mutex_unlock(&g_network_lock);
      return;
    }

  cfg = g_saved_config;
  g_boot_policy_pending = false;
  pthread_mutex_unlock(&g_network_lock);

  result = vg_network_apply_runtime(&cfg);
  if (result < 0)
    {
      vg_log_human("WARN", "initial network policy apply deferred");
    }
  else
    {
      vg_log_human("INFO", "initial network policy published");
    }

  vg_network_publish_runtime();
}

static void vg_network_handle_apply(void)
{
  struct vg_network_config previous;
  struct vg_network_config requested;
  int result;
  int rollback;

  pthread_mutex_lock(&g_network_lock);
  if (!g_apply_pending)
    {
      pthread_mutex_unlock(&g_network_lock);
      return;
    }

  previous = g_saved_config;
  requested = g_pending_config;
  g_apply_pending = false;
  g_status.state = VG_NETWORK_APPLYING;
  g_status.last_error = 0;
  vg_network_set_message_locked("正在应用");
  vg_network_set_feedback_locked("正在应用");
  pthread_mutex_unlock(&g_network_lock);

  result = vg_config_network_validate(&requested);
  if (result < 0)
    {
      goto fail_validate;
    }

  /* Transaction order: tmp -> runtime -> rename. */

  result = vg_config_network_write_tmp(&requested);
  if (result < 0)
    {
      goto fail_tmp;
    }

  result = vg_network_apply_runtime(&requested);
  if (result < 0)
    {
      goto fail_runtime;
    }

  result = vg_config_network_commit();
  if (result < 0)
    {
      goto fail_commit;
    }

  pthread_mutex_lock(&g_network_lock);
  g_saved_config = requested;
  g_saved_config.loaded_from_file = true;
  g_status.last_error = 0;
  g_status.state = VG_NETWORK_ADDRESSING;
  vg_network_set_feedback_locked("应用成功");
  pthread_mutex_unlock(&g_network_lock);

  vg_network_publish_runtime();

  pthread_mutex_lock(&g_network_lock);
  if (g_status.state == VG_NETWORK_ONLINE && g_status.ipv4[0] != '\0')
    {
      char ok[64];

      snprintf(ok, sizeof(ok), "应用成功 %s", g_status.ipv4);
      vg_network_set_feedback_locked(ok);
    }
  else if (g_status.feedback[0] == '\0')
    {
      vg_network_set_feedback_locked("应用成功");
    }

  pthread_mutex_unlock(&g_network_lock);

  vg_log_human("INFO", "network config applied");
  vg_log_event("network_apply", "network config applied",
               "\"result\":\"ok\"");
  return;

fail_commit:
  rollback = vg_network_apply_runtime(&previous);
  vg_config_network_abort_tmp();
  pthread_mutex_lock(&g_network_lock);
  g_status.last_error = result;
  if (rollback < 0)
    {
      /* Formal file unchanged; runtime may be inconsistent. */

      g_status.state = VG_NETWORK_ERROR;
      vg_network_set_message_locked("网络异常");
      vg_network_set_feedback_locked("应用失败,请从NSH检查网络");
      pthread_mutex_unlock(&g_network_lock);
    }
  else
    {
      /* Runtime restored; formal file never replaced. */

      g_status.state = VG_NETWORK_DOWN;
      vg_network_set_feedback_locked("提交失败,已恢复运行配置");
      pthread_mutex_unlock(&g_network_lock);
      vg_network_publish_runtime();
    }

  vg_log_human("ERROR", "network config commit failed");
  return;

fail_runtime:
  rollback = vg_network_apply_runtime(&previous);
  vg_config_network_abort_tmp();
  pthread_mutex_lock(&g_network_lock);
  g_status.last_error = result;
  if (rollback < 0)
    {
      g_status.state = VG_NETWORK_ERROR;
      vg_network_set_message_locked("网络异常");
      vg_network_set_feedback_locked("应用失败,请从NSH检查网络");
      pthread_mutex_unlock(&g_network_lock);
    }
  else
    {
      g_status.state = VG_NETWORK_DOWN;
      vg_network_set_feedback_locked("应用失败,已恢复原配置");
      pthread_mutex_unlock(&g_network_lock);
      vg_network_publish_runtime();
    }

  vg_log_human("ERROR", "network runtime apply failed");
  return;

fail_tmp:
fail_validate:
  vg_config_network_abort_tmp();
  pthread_mutex_lock(&g_network_lock);
  g_status.last_error = result;
  g_status.state = VG_NETWORK_DOWN;
  vg_network_set_feedback_locked(
    result == -EINVAL ? "配置无效" : "写入失败");
  pthread_mutex_unlock(&g_network_lock);
  vg_network_publish_runtime();
  vg_log_human("ERROR", "network config rejected before apply");
}

static FAR void *vg_network_thread(FAR void *arg)
{
  struct timespec ts;

  UNUSED(arg);

  /* Publish boot policy asynchronously so UI is never blocked. */

  vg_network_handle_boot_policy();

  for (; ; )
    {
      bool apply_now;
      bool boot_now;

      pthread_mutex_lock(&g_network_lock);
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_nsec += VG_NETWORK_POLL_MS * 1000000L;
      if (ts.tv_nsec >= 1000000000L)
        {
          ts.tv_sec += ts.tv_nsec / 1000000000L;
          ts.tv_nsec %= 1000000000L;
        }

      while (!g_apply_pending && !g_boot_policy_pending)
        {
          int wait_result = pthread_cond_timedwait(&g_network_cond,
                                                   &g_network_lock, &ts);
          if (wait_result == ETIMEDOUT ||
              g_apply_pending || g_boot_policy_pending)
            {
              break;
            }
        }

      apply_now = g_apply_pending;
      boot_now = g_boot_policy_pending;
      pthread_mutex_unlock(&g_network_lock);

      if (boot_now)
        {
          vg_network_handle_boot_policy();
        }

      if (apply_now)
        {
          vg_network_handle_apply();
        }
      else
        {
          vg_network_publish_runtime();
        }
    }

  return NULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int vg_network_start(void)
{
  pthread_t tid;
  pthread_attr_t attr;
  int result;

  if (g_started)
    {
      return 0;
    }

  memset(&g_status, 0, sizeof(g_status));
  vg_config_network_defaults(&g_saved_config);

  /* Load config here is file I/O on the caller thread, but must not call
   * netinit DHCP/static apply (that can block for seconds).  Policy publish
   * is deferred to the worker via g_boot_policy_pending.
   */

  result = vg_config_network_load(&g_saved_config);
  if (result < 0)
    {
      vg_config_network_defaults(&g_saved_config);
      vg_log_human("WARN", "network config load failed; using DHCP default");
    }

  pthread_mutex_lock(&g_network_lock);
  g_status.mode = g_saved_config.mode;
  g_status.state = VG_NETWORK_DOWN;
  vg_network_set_message_locked("初始化");
  g_boot_policy_pending = true;
  pthread_mutex_unlock(&g_network_lock);

  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 4096);
  result = pthread_create(&tid, &attr, vg_network_thread, NULL);
  pthread_attr_destroy(&attr);
  if (result != 0)
    {
      return -result;
    }

  pthread_detach(tid);
  pthread_setname_np(tid, "vg_network");
  g_started = true;
  return 0;
}

void vg_network_get_status(FAR struct vg_network_status *status)
{
  if (status == NULL)
    {
      return;
    }

  pthread_mutex_lock(&g_network_lock);
  *status = g_status;
  pthread_mutex_unlock(&g_network_lock);
}

void vg_network_get_config(FAR struct vg_network_config *config)
{
  if (config == NULL)
    {
      return;
    }

  pthread_mutex_lock(&g_network_lock);
  *config = g_saved_config;
  pthread_mutex_unlock(&g_network_lock);
}

int vg_network_request_apply(FAR const struct vg_network_config *config)
{
  int result;

  if (config == NULL)
    {
      return -EINVAL;
    }

  result = vg_config_network_validate(config);
  if (result < 0)
    {
      return result;
    }

  pthread_mutex_lock(&g_network_lock);
  if (g_apply_pending || g_status.state == VG_NETWORK_APPLYING)
    {
      pthread_mutex_unlock(&g_network_lock);
      return -EBUSY;
    }

  g_pending_config = *config;
  g_pending_config.version = VG_NETWORK_CFG_VERSION;
  g_apply_pending = true;
  g_status.state = VG_NETWORK_APPLYING;
  g_status.last_error = 0;
  vg_network_set_message_locked("正在应用");
  vg_network_set_feedback_locked("正在应用");
  pthread_cond_signal(&g_network_cond);
  pthread_mutex_unlock(&g_network_lock);
  return 0;
}
