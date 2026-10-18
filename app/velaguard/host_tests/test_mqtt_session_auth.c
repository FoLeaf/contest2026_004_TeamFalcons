/* SPDX-License-Identifier: Apache-2.0 */
/****************************************************************************
 * The resident MQTT session's credential entry point.
 *
 * This test uses the real vg_mqtt_session_open() and the real sealed-record
 * loader.  The transport and MQTT-C calls are stubs, which lets the test
 * prove two things without a broker:
 *
 *   - a missing record returns before vg_tcp_open() is called;
 *   - every new session open reloads the record, including after a
 *     reconnect or a record replacement.
 ****************************************************************************/

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define VG_PROVISION_DIR   "."
#define VG_PROVISION_FILE  "vgtest-session-llm.v1"
#define VG_MQTT_CRED_FILE  "vgtest-session-mqtt.v1"
#define CONFIG_VG_MQTT_CREDENTIALS 1
#define CONFIG_VG_DISCOVER_POINTS_PATH "vgtest-session-points.json"

#include "mqtt_cred_fixture.h"
#include "../vg_provision.c"
#include "../vg_mqtt_session.c"

static int g_tcp_open_calls;
static int g_tcp_close_calls;
static int g_connect_calls;
static char g_connect_user[VG_MQTT_CRED_USER_MAX];
static char g_connect_pass[VG_MQTT_CRED_PASS_MAX];

const char *vg_device_id(void)
{
  return "test-device";
}

int vg_tcp_open(vg_tcp_backend_t backend, const char *host, const char *port)
{
  (void)backend;
  (void)host;
  (void)port;
  g_tcp_open_calls++;
  return 42;
}

void vg_tcp_close(int *fd)
{
  g_tcp_close_calls++;
  if (fd != NULL)
    {
      *fd = -1;
    }
}

const char *vg_tcp_backend_str(vg_tcp_backend_t backend)
{
  (void)backend;
  return "test";
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
  (void)connect_flags;
  (void)keep_alive;
  g_connect_calls++;
  snprintf(g_connect_user, sizeof(g_connect_user), "%s",
           user_name == NULL ? "" : user_name);
  snprintf(g_connect_pass, sizeof(g_connect_pass), "%s",
           password == NULL ? "" : password);
  client->error = MQTT_OK;
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
  client->error = MQTT_OK;
  return MQTT_OK;
}

const char *mqtt_error_str(enum MQTTErrors error)
{
  (void)error;
  return "stub";
}

int vg_live_snapshot_read(const char *path, struct vg_live_snapshot *snap)
{
  (void)path;
  (void)snap;
  return -ENOENT;
}

uint32_t vg_live_now_ms(void)
{
  return 1;
}

uint32_t vg_live_age_ms(uint32_t tick, uint32_t now)
{
  return now - tick;
}

static void reset_capture(void)
{
  g_tcp_open_calls = 0;
  g_tcp_close_calls = 0;
  g_connect_calls = 0;
  memset(g_connect_user, 0, sizeof(g_connect_user));
  memset(g_connect_pass, 0, sizeof(g_connect_pass));
}

static void write_credential(const char *user, const char *pass)
{
  char json[256];

  snprintf(json, sizeof(json),
           "{\"kind\":\"mqtt\",\"username\":\"%s\",\"password\":\"%s\"}",
           user, pass);
  assert(fixture_put_record(VG_MQTT_CRED_FILE, json) == 0);
}

int main(void)
{
  fixture_clean_records();
  reset_capture();

  /* No record: the session must not open a TCP connection. */
  assert(vg_mqtt_session_open(VG_TCP_POSIX, "eth0") == -1);
  assert(g_tcp_open_calls == 0);
  assert(g_connect_calls == 0);

  /* First session open reads the record and passes it to MQTT-C. */
  write_credential("first", "password-one");
  assert(vg_mqtt_session_open(VG_TCP_POSIX, "eth0") == 0);
  assert(g_tcp_open_calls == 1);
  assert(g_connect_calls == 1);
  assert(strcmp(g_connect_user, "first") == 0);
  assert(strcmp(g_connect_pass, "password-one") == 0);
  vg_mqtt_session_close();

  /* A reconnect is a new open and must read the replaced record, not reuse
   * the clear-text copy from the previous session. */
  write_credential("second", "password-two");
  reset_capture();
  assert(vg_mqtt_session_open(VG_TCP_POSIX, "eth0") == 0);
  assert(g_tcp_open_calls == 1);
  assert(g_connect_calls == 1);
  assert(strcmp(g_connect_user, "second") == 0);
  assert(strcmp(g_connect_pass, "password-two") == 0);
  vg_mqtt_session_close();
  assert(g_tcp_close_calls >= 1);

  /* Removing the record after a successful session prevents the next open
   * before any transport operation. */
  remove(VG_MQTT_CRED_FILE);
  reset_capture();
  assert(vg_mqtt_session_open(VG_TCP_POSIX, "eth0") == -1);
  assert(g_tcp_open_calls == 0);
  assert(g_connect_calls == 0);

  fixture_clean_records();
  remove(CONFIG_VG_DISCOVER_POINTS_PATH);
  puts("test_mqtt_session_auth: OK (missing record blocks connect, reconnect reloads)");
  return 0;
}
