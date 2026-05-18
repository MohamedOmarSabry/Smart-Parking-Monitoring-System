#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#include "config.h"

// =============================================================================
// SLOT CONTROLLER
// =============================================================================
#if defined(ROLE_SLOT)

static volatile bool occupied_flag = false;

/* Function Prototypes */
float get_distance(void);
void send_to_lane_controller(bool occupied);
void sensor_task(void* arg);
void http_task(void* arg);
void wifi_init(void);
void gpio_init(void);

extern "C" void app_main(void)
{
  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();

  wifi_init();
  gpio_init();

  xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 2, NULL);
  xTaskCreate(http_task, "http_task", 4096, NULL, 1, NULL);
}

float get_distance(void)
{
  // Stabilizes the sensor before triggering.
  gpio_set_level(TRIG_PIN, 0);
  esp_rom_delay_us(2);

  // Emit waves.
  gpio_set_level(TRIG_PIN, 1);
  esp_rom_delay_us(10);
  gpio_set_level(TRIG_PIN, 0);

  // Wait until ECHO becomes HIGH.
  int64_t start = esp_timer_get_time();
  while (gpio_get_level(ECHO_PIN) == 0) {
    if (esp_timer_get_time() - start > 50000) return -1;
  }

  // Wait until ECHO becomes LOW.
  int64_t echo_start = esp_timer_get_time();
  while (gpio_get_level(ECHO_PIN) == 1) {
    if (esp_timer_get_time() - echo_start > 50000) return -1;
  }

  float distance = (float)(esp_timer_get_time() - echo_start) / 58.0f;
  if (distance < 1 || distance > 400) return -1;
  return distance;
}

void send_to_lane_controller(bool occupied)
{
  char json[64];
  sprintf(json, "{\"slot_id\":\"%s\",\"occupied\":%s}",
    SLOT_ID, occupied ? "true" : "false");

  esp_http_client_config_t config = {};
  config.url = LANE_SERVER_URL;
  config.timeout_ms = 1000;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, json, strlen(json));
  esp_http_client_perform(client);
  esp_http_client_cleanup(client);
}

void sensor_task(void* arg)
{
  while (1) {
    float distance = get_distance();
    bool occupied = (distance != -1 && distance < 20);

    printf("Distance: %.2f cm | Occupied: %s\n", distance, occupied ? "YES" : "NO");

    // Active low: 0 = LED on, 1 = LED off
    gpio_set_level(RED_PIN, occupied ? 0 : 1);
    gpio_set_level(GREEN_PIN, occupied ? 1 : 0);

    occupied_flag = occupied;

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void http_task(void* arg)
{
  while (1) {
    send_to_lane_controller(occupied_flag);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void wifi_init(void)
{
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  wifi_config_t wifi_config = {};
  strcpy((char*)wifi_config.sta.ssid, LANE_AP_SSID);
  strcpy((char*)wifi_config.sta.password, LANE_AP_PASS);

  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  esp_wifi_start();
  esp_wifi_connect();

  printf("Slot connecting to lane AP: %s\n", LANE_AP_SSID);
  vTaskDelay(pdMS_TO_TICKS(3000));
}

void gpio_init(void)
{
  gpio_set_direction(TRIG_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(ECHO_PIN, GPIO_MODE_INPUT);
  gpio_set_direction(GREEN_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(RED_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(RGB_VCC_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(RGB_VCC_PIN, 1);
}

// =============================================================================
// LANE CONTROLLER
// =============================================================================
#elif defined(ROLE_LANE)

#define MAX_SLOTS 4

/* Function Prototypes */
int find_or_add_slot(const char* slot_id);
bool json_get_string(const char* json, const char* key, char* out, int out_len);
bool json_get_bool(const char* json, const char* key, bool* out);
esp_err_t slot_post_handler(httpd_req_t* req);
void start_http_server(void);
void send_to_main_controller(void);
void http_task(void* arg);
void wifi_init(void);

struct {
  char id[32];
  bool occupied;
  bool valid;
} slots[MAX_SLOTS];

extern "C" void app_main(void)
{
  memset(slots, 0, sizeof(slots));

  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();

  wifi_init();
  start_http_server();

  xTaskCreate(http_task, "http_task", 4096, NULL, 1, NULL);
}

int find_or_add_slot(const char* slot_id)
{
  for (int i = 0; i < MAX_SLOTS; i++) {
    if (slots[i].valid && strcmp(slots[i].id, slot_id) == 0) return i;
  }
  for (int i = 0; i < MAX_SLOTS; i++) {
    if (!slots[i].valid) {
      strncpy(slots[i].id, slot_id, sizeof(slots[i].id) - 1);
      slots[i].occupied = false;
      slots[i].valid = true;
      printf("Lane: registered slot %s\n", slot_id);
      return i;
    }
  }
  return -1;
}

bool json_get_string(const char* json, const char* key, char* out, int out_len)
{
  char search[64];
  snprintf(search, sizeof(search), "\"%s\":", key);
  const char* p = strstr(json, search);
  if (!p) return false;
  p += strlen(search);
  while (*p == ' ') p++;
  if (*p != '"') return false;
  p++;
  int i = 0;
  while (*p && *p != '"' && i < out_len - 1) out[i++] = *p++;
  out[i] = '\0';
  return true;
}

bool json_get_bool(const char* json, const char* key, bool* out)
{
  char search[64];
  snprintf(search, sizeof(search), "\"%s\":", key);
  const char* p = strstr(json, search);
  if (!p) return false;
  p += strlen(search);
  while (*p == ' ') p++;
  if (strncmp(p, "true", 4) == 0) { *out = true;  return true; }
  if (strncmp(p, "false", 5) == 0) { *out = false; return true; }
  return false;
}

esp_err_t slot_post_handler(httpd_req_t* req)
{
  char body[128] = {};
  httpd_req_recv(req, body, sizeof(body) - 1);

  char slot_id[32] = {};
  bool occupied = false;

  if (!json_get_string(body, "slot_id", slot_id, sizeof(slot_id)) ||
    !json_get_bool(body, "occupied", &occupied))
  {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON");
    return ESP_FAIL;
  }

  int idx = find_or_add_slot(slot_id);
  if (idx >= 0) {
    slots[idx].occupied = occupied;
    printf("Slot %s → %s\n", slot_id, occupied ? "OCCUPIED" : "FREE");
  }

  httpd_resp_sendstr(req, "OK");
  return ESP_OK;
}

void start_http_server(void)
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 3000;

  httpd_handle_t server = NULL;
  httpd_start(&server, &config);

  httpd_uri_t slot_uri = {
      .uri = "/slot",
      .method = HTTP_POST,
      .handler = slot_post_handler,
  };
  httpd_register_uri_handler(server, &slot_uri);
  printf("Lane HTTP server started on port 3000\n");
}

void send_to_main_controller(void)
{
  char json[512];
  int  pos = 0;

  pos += sprintf(json + pos, "{\"lane_id\":\"%s\",\"slots\":[", LANE_ID);

  bool first = true;
  for (int i = 0; i < MAX_SLOTS; i++) {
    if (!slots[i].valid) continue;
    if (!first) pos += sprintf(json + pos, ",");
    pos += sprintf(json + pos,
      "{\"slot_id\":\"%s\",\"occupied\":%s}",
      slots[i].id,
      slots[i].occupied ? "true" : "false");
    first = false;
  }
  pos += sprintf(json + pos, "]}");

  esp_http_client_config_t config = {};
  config.url = MAIN_SERVER_URL;
  config.timeout_ms = 1000;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, json, strlen(json));
  esp_http_client_perform(client);
  esp_http_client_cleanup(client);
}

void http_task(void* arg)
{
  while (1) {
    send_to_main_controller();
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void wifi_init(void)
{
  esp_netif_create_default_wifi_ap();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  wifi_config_t ap_config = {};
  strcpy((char*)ap_config.ap.ssid, LANE_AP_SSID);
  strcpy((char*)ap_config.ap.password, LANE_AP_PASS);
  ap_config.ap.max_connection = 4;
  ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

  wifi_config_t sta_config = {};
  strcpy((char*)sta_config.sta.ssid, MAIN_AP_SSID);
  strcpy((char*)sta_config.sta.password, MAIN_AP_PASS);

  esp_wifi_set_mode(WIFI_MODE_APSTA);
  esp_wifi_set_config(WIFI_IF_AP, &ap_config);
  esp_wifi_set_config(WIFI_IF_STA, &sta_config);
  esp_wifi_start();
  esp_wifi_connect();

  printf("Lane AP: %s | Connecting to main: %s\n", LANE_AP_SSID, MAIN_AP_SSID);
  vTaskDelay(pdMS_TO_TICKS(3000));
}

// =============================================================================
// MAIN CONTROLLER
// =============================================================================
#elif defined(ROLE_MAIN)

#define MAX_LANES          4
#define MAX_SLOTS_PER_LANE 4

struct {
  char id[32];
  bool occupied;
  bool valid;
} slots[MAX_LANES][MAX_SLOTS_PER_LANE];

static char lane_ids[MAX_LANES][32];
static bool lane_valid[MAX_LANES];

int find_or_add_lane(const char* lane_id)
{
  for (int i = 0; i < MAX_LANES; i++) {
    if (lane_valid[i] && strcmp(lane_ids[i], lane_id) == 0) return i;
  }
  for (int i = 0; i < MAX_LANES; i++) {
    if (!lane_valid[i]) {
      strncpy(lane_ids[i], lane_id, sizeof(lane_ids[i]) - 1);
      lane_valid[i] = true;
      printf("Main: registered lane %s\n", lane_id);
      return i;
    }
  }
  return -1;
}

int find_or_add_slot(int li, const char* slot_id)
{
  for (int i = 0; i < MAX_SLOTS_PER_LANE; i++) {
    if (slots[li][i].valid && strcmp(slots[li][i].id, slot_id) == 0) return i;
  }
  for (int i = 0; i < MAX_SLOTS_PER_LANE; i++) {
    if (!slots[li][i].valid) {
      strncpy(slots[li][i].id, slot_id, sizeof(slots[li][i].id) - 1);
      slots[li][i].valid = true;
      return i;
    }
  }
  return -1;
}

bool json_get_string(const char* json, const char* key, char* out, int out_len)
{
  char search[64];
  snprintf(search, sizeof(search), "\"%s\":", key);
  const char* p = strstr(json, search);
  if (!p) return false;
  p += strlen(search);
  while (*p == ' ') p++;
  if (*p != '"') return false;
  p++;
  int i = 0;
  while (*p && *p != '"' && i < out_len - 1) out[i++] = *p++;
  out[i] = '\0';
  return true;
}

bool json_get_bool(const char* json, const char* key, bool* out)
{
  char search[64];
  snprintf(search, sizeof(search), "\"%s\":", key);
  const char* p = strstr(json, search);
  if (!p) return false;
  p += strlen(search);
  while (*p == ' ') p++;
  if (strncmp(p, "true", 4) == 0) { *out = true;  return true; }
  if (strncmp(p, "false", 5) == 0) { *out = false; return true; }
  return false;
}

void parse_slots_array(int li, const char* json)
{
  const char* p = strstr(json, "\"slots\":");
  if (!p) return;
  p = strchr(p, '[');
  if (!p) return;
  p++;

  while (*p && *p != ']') {
    p = strchr(p, '{');
    if (!p || *p == ']') break;
    const char* end = strchr(p, '}');
    if (!end) break;

    int  len = (int)(end - p + 1);
    char slot_json[128] = {};
    if (len < (int)sizeof(slot_json)) {
      strncpy(slot_json, p, len);
      char slot_id[32] = {};
      bool occupied = false;
      if (json_get_string(slot_json, "slot_id", slot_id, sizeof(slot_id)) &&
        json_get_bool(slot_json, "occupied", &occupied))
      {
        int si = find_or_add_slot(li, slot_id);
        if (si >= 0) slots[li][si].occupied = occupied;
      }
    }
    p = end + 1;
  }
}

static esp_err_t lane_post_handler(httpd_req_t* req)
{
  char body[512] = {};
  int received = httpd_req_recv(req, body, sizeof(body) - 1);
  printf("DEBUG handler hit, received %d bytes\n", received);
  printf("DEBUG body: %s\n", body);

  char lane_id[32] = {};
  if (!json_get_string(body, "lane_id", lane_id, sizeof(lane_id))) {
    printf("DEBUG failed to parse lane_id\n");
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON"), ESP_FAIL;
  }
  printf("DEBUG lane_id: %s\n", lane_id);

  int li = find_or_add_lane(lane_id);
  if (li < 0) { httpd_resp_sendstr(req, "OK"); return ESP_OK; }

  // Parse slots array
  const char* slots_start = strstr(body, "\"slots\":");
  const char* p = slots_start ? strchr(slots_start, '[') : NULL;
  printf("DEBUG slots array found: %s\n", p ? "yes" : "no");
  if (p) for (p++; *p && *p != ']';) {
    p = strchr(p, '{'); if (!p) break;
    const char* end = strchr(p, '}'); if (!end) break;
    char s[128] = {};
    strncpy(s, p, end - p + 1);
    printf("DEBUG slot json: %s\n", s);
    char slot_id[32] = {}; bool occupied = false;
    if (json_get_string(s, "slot_id", slot_id, sizeof(slot_id)) &&
      json_get_bool(s, "occupied", &occupied)) {
      printf("DEBUG slot_id=%s occupied=%d\n", slot_id, occupied);
      int si = find_or_add_slot(li, slot_id);
      if (si >= 0) slots[li][si].occupied = occupied;
    }
    else {
      printf("DEBUG failed to parse slot fields\n");
    }
    p = end + 1;
  }

  httpd_resp_sendstr(req, "OK");
  return ESP_OK;
}

void start_http_server(void)
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 3000;

  httpd_handle_t server = NULL;
  httpd_start(&server, &config);

  httpd_uri_t lane_uri = {
      .uri = "/lane",
      .method = HTTP_POST,
      .handler = lane_post_handler,
  };
  httpd_register_uri_handler(server, &lane_uri);
  printf("Main HTTP server started on port 3000\n");
}

void send_to_backend(void)
{
  char json[1024];
  int  pos = 0;

  pos += sprintf(json + pos, "{\"lanes\":[");

  bool first_lane = true;
  for (int li = 0; li < MAX_LANES; li++) {
    if (!lane_valid[li]) continue;
    if (!first_lane) pos += sprintf(json + pos, ",");
    pos += sprintf(json + pos, "{\"lane_id\":\"%s\",\"slots\":[", lane_ids[li]);

    bool first_slot = true;
    for (int si = 0; si < MAX_SLOTS_PER_LANE; si++) {
      if (!slots[li][si].valid) continue;
      if (!first_slot) pos += sprintf(json + pos, ",");
      pos += sprintf(json + pos,
        "{\"slot_id\":\"%s\",\"occupied\":%s}",
        slots[li][si].id,
        slots[li][si].occupied ? "true" : "false");
      first_slot = false;
    }

    pos += sprintf(json + pos, "]}");
    first_lane = false;
  }
  pos += sprintf(json + pos, "]}");

  esp_http_client_config_t config = {};
  config.url = BACKEND_URL;
  config.timeout_ms = 1000;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, json, strlen(json));
  esp_http_client_perform(client);
  printf("DEBUG: Backend status: %d\n", esp_http_client_get_status_code(client));
  esp_http_client_cleanup(client);
}

void backend_task(void* arg)
{
  while (1) {
    send_to_backend();
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void wifi_init(void)
{
  esp_netif_t* netif = esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  wifi_config_t ap_config = {};
  strcpy((char*)ap_config.ap.ssid, MAIN_AP_SSID);
  strcpy((char*)ap_config.ap.password, MAIN_AP_PASS);
  ap_config.ap.max_connection = 4;
  ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

  // Set IP before starting wifi so DHCP starts on the right subnet
  esp_netif_dhcps_stop(netif);
  esp_netif_ip_info_t ip_info;
  IP4_ADDR(&ip_info.ip, 192, 168, 5, 1);
  IP4_ADDR(&ip_info.gw, 192, 168, 5, 1);
  IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
  esp_netif_set_ip_info(netif, &ip_info);

  esp_wifi_set_mode(WIFI_MODE_AP);
  esp_wifi_set_config(WIFI_IF_AP, &ap_config);
  esp_wifi_start();

  esp_netif_dhcps_start(netif);

  printf("Main AP started: %s | IP: 192.168.5.1\n", MAIN_AP_SSID);
}

extern "C" void app_main(void)
{
  memset(slots, 0, sizeof(slots));
  memset(lane_ids, 0, sizeof(lane_ids));
  memset(lane_valid, 0, sizeof(lane_valid));

  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();

  wifi_init();
  start_http_server();

  xTaskCreate(backend_task, "backend_task", 8192, NULL, 1, NULL);
}

#endif