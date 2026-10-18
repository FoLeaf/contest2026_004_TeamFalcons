/* SPDX-License-Identifier: Apache-2.0 */
/****************************************************************************
 * The vgmqtt default-authentication entry point.
 *
 * vgmqtt accepts -u/-P for explicit debugging, but without both options it
 * must resolve credentials from the same sealed record as the resident
 * session.  This test exercises that source path with fake credentials and
 * verifies that a missing record fails before the network path starts.
 ****************************************************************************/

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define VG_PROVISION_DIR   "."
#define VG_PROVISION_FILE  "vgtest-vgmqtt-llm.v1"
#define VG_MQTT_CRED_FILE  "vgtest-vgmqtt-mqtt.v1"
#define CONFIG_VG_MQTT_CREDENTIALS 1

#include "mqtt_cred_fixture.h"
#include "../vg_provision.c"

#define main vgmqtt_main
#include "../velaguard_mqtt.c"
#undef main

static int g_network_calls;

int __wrap_getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints, struct addrinfo **res)
{
  (void)node;
  (void)service;
  (void)hints;
  (void)res;
  g_network_calls++;
  return EAI_FAIL;
}

int __wrap_socket(int domain, int type, int protocol)
{
  (void)domain;
  (void)type;
  (void)protocol;
  g_network_calls++;
  errno = ENETUNREACH;
  return -1;
}

int __wrap_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
  (void)sockfd;
  (void)addr;
  (void)addrlen;
  g_network_calls++;
  errno = ENETUNREACH;
  return -1;
}

const char *vg_device_id(void)
{
  return "test-device";
}

enum MQTTErrors mqtt_init(struct mqtt_client *client, mqtt_pal_socket_handle fd,
                          uint8_t *tx, size_t txsz, uint8_t *rx, size_t rxsz,
                          void (*callback)(void **,
                                           struct mqtt_response_publish *))
{
  (void)client;
  (void)fd;
  (void)tx;
  (void)txsz;
  (void)rx;
  (void)rxsz;
  (void)callback;
  return MQTT_OK;
}

enum MQTTErrors mqtt_connect(struct mqtt_client *client,
                             const char *client_id,
                             const char *will_topic,
                             const void *will_message,
                             size_t will_message_size,
                             const char *user_name,
                             const char *password,
                             uint8_t connect_flags,
                             uint16_t keep_alive)
{
  (void)client;
  (void)client_id;
  (void)will_topic;
  (void)will_message;
  (void)will_message_size;
  (void)user_name;
  (void)password;
  (void)connect_flags;
  (void)keep_alive;
  return MQTT_OK;
}

enum MQTTErrors mqtt_publish(struct mqtt_client *client, const char *topic,
                             const void *message, size_t message_size,
                             uint8_t publish_flags)
{
  (void)client;
  (void)topic;
  (void)message;
  (void)message_size;
  (void)publish_flags;
  return MQTT_OK;
}

enum MQTTErrors mqtt_sync(struct mqtt_client *client)
{
  client->event_connect = true;
  return MQTT_OK;
}

const char *mqtt_error_str(enum MQTTErrors error)
{
  (void)error;
  return "stub";
}

enum MQTTErrors mqtt_disconnect(struct mqtt_client *client)
{
  (void)client;
  return MQTT_OK;
}

struct mqtt_queued_message *mqtt_mq_find(struct mqtt_message_queue *mq,
                                         enum MQTTControlPacketType control_type,
                                         uint16_t *packet_id)
{
  (void)mq;
  (void)control_type;
  (void)packet_id;
  return NULL;
}

static void write_credential(const char *user, const char *pass)
{
  char json[256];

  snprintf(json, sizeof(json),
           "{\"kind\":\"mqtt\",\"username\":\"%s\",\"password\":\"%s\"}",
           user, pass);
  assert(fixture_put_record(VG_MQTT_CRED_FILE, json) == 0);
}

static void reset_cfg(struct vgmqtt_cfg_s *cfg)
{
  memset(cfg, 0, sizeof(*cfg));
  cfg->user = NULL;
  cfg->pass = NULL;
}

int main(void)
{
  char arg0[] = "vgmqtt";
  char arg1[] = "-h";
  char arg2[] = "broker.invalid";
  char *argv[] = { arg0, arg1, arg2, NULL };
  struct vgmqtt_cfg_s cfg;

  fixture_clean_records();
  g_network_calls = 0;

  reset_cfg(&cfg);
  assert(parse_args(3, argv, &cfg) == OK);
  assert(resolve_credentials(&cfg) == ERROR);
  assert(cfg.user == NULL && cfg.pass == NULL);

  /* The full entry point must reject the same missing record before trying
   * to resolve the broker address. */
  assert(vgmqtt_main(3, argv) == ERROR);
  assert(g_network_calls == 0);

  write_credential("default-user", "default-password");
  reset_cfg(&cfg);
  assert(parse_args(3, argv, &cfg) == OK);
  assert(resolve_credentials(&cfg) == OK);
  assert(strcmp(cfg.user, "default-user") == 0);
  assert(strcmp(cfg.pass, "default-password") == 0);

  drop_credentials();
  write_credential("replaced-user", "replaced-password");
  reset_cfg(&cfg);
  assert(parse_args(3, argv, &cfg) == OK);
  assert(resolve_credentials(&cfg) == OK);
  assert(strcmp(cfg.user, "replaced-user") == 0);
  assert(strcmp(cfg.pass, "replaced-password") == 0);

  /* A full entry-point failure after the record is loaded must not leave the
   * clear-text copy in the module's static credential buffer. */
  write_credential("wipe-user", "wipe-password");
  g_network_calls = 0;
  assert(vgmqtt_main(3, argv) == ERROR);
  assert(g_network_calls == 1);
  assert(g_cred.username[0] == '\0' && g_cred.password[0] == '\0');

  fixture_clean_records();
  puts("test_mqtt_vgmqtt_auth: OK (default record path and missing-record refusal)");
  return 0;
}
