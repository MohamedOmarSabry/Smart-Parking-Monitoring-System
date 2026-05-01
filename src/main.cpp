#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "nvs_flash.h"

#include "esp_http_client.h"
#include "driver/gpio.h"
#include "esp_timer.h"

// Wifi credentials
#define WIFI_SSID ""
#define WIFI_PASS ""

// Server
#define SERVER_URL "http://192.168.1.67:3000/sensor"

// Pins
#define INTERNAL_LED_PIN GPIO_NUM_2
#define RED_LED_PIN GPIO_NUM_23
#define TRIG_PIN GPIO_NUM_5
#define ECHO_PIN GPIO_NUM_18

static void gpio_init(void);
static void wifi_init(void);
static void blink_led(void);
static float get_distance(void);
static void send_http_request(bool occupied);

// Entry point
extern "C" void app_main(void)
{
  wifi_init();
  gpio_init();
  blink_led();
  while (1) {
    float distance = get_distance();
    bool occupied = (distance != -1 && distance < 20);

    printf("Distance: %.2f cm | Occupied: %s\n",
      distance,
      occupied ? "YES" : "NO");

    gpio_set_level(RED_LED_PIN, occupied);

    send_http_request(occupied);

    // Take a new reading every second
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

static void gpio_init(void)
{
  gpio_set_direction(INTERNAL_LED_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(RED_LED_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(TRIG_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(ECHO_PIN, GPIO_MODE_INPUT);
}

static void wifi_init(void)
{
  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  wifi_config_t wifi_config = {
      .sta = {
          .ssid = WIFI_SSID,
          .password = WIFI_PASS,
      },
  };

  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  esp_wifi_start();
  esp_wifi_connect();

  ESP_LOGI("WIFI", "Connecting to WiFi...");
}

static void blink_led(void)
{
  for (int i = 0; i < 5; i++) {
    gpio_set_level(INTERNAL_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(250));
    gpio_set_level(INTERNAL_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

static float get_distance(void)
{
  gpio_set_level(TRIG_PIN, 0);
  esp_rom_delay_us(2);

  gpio_set_level(TRIG_PIN, 1);
  esp_rom_delay_us(10);
  gpio_set_level(TRIG_PIN, 0);

  int64_t start = esp_timer_get_time();

  // Wait for ECHO to changes from 0 to 1.
  while (gpio_get_level(ECHO_PIN) == 0) {
    if (esp_timer_get_time() - start > 50000) return -1;
  }

  int64_t echo_start = esp_timer_get_time();

  // When ECHO changes to 1, start measuring the time.
  while (gpio_get_level(ECHO_PIN) == 1) {
    if (esp_timer_get_time() - echo_start > 50000) return -1;
  }

  int64_t echo_end = esp_timer_get_time();

  float duration_us = (float)(echo_end - echo_start);
  float distance = duration_us / 58;

  if (distance < 1 || distance > 400)
    return -1;

  return distance;
}

static void send_http_request(bool occupied)
{
  char json[64];
  sprintf(json, "{\"occupied\":%s}", occupied ? "true" : "false");

  esp_http_client_config_t config = {
      .url = SERVER_URL,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);

  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, json, strlen(json));

  esp_http_client_perform(client);

  esp_http_client_cleanup(client);
}