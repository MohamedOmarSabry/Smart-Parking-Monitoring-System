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

// Server
#define SERVER_URL "http://192.168.4.2:3000/sensor"

// Pins
#define INTERNAL_LED_PIN GPIO_NUM_2
#define RED_LED_PIN GPIO_NUM_23
#define TRIG_PIN GPIO_NUM_5
#define ECHO_PIN GPIO_NUM_18
#define DATA_PIN GPIO_NUM_22
#define CLK_PIN GPIO_NUM_19  
#define CS_PIN GPIO_NUM_21   

static const uint8_t digit_0[8] = {
    0b00111100,
    0b01100110,
    0b01101110,
    0b01110110,
    0b01100110,
    0b01100110,
    0b00111100,
    0b00000000 };

static const uint8_t digit_1[8] = {
    0b00011000,
    0b00111000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00111100,
    0b00000000 };

static void gpio_init(void);
static void wifi_init(void);
static void blink_led(void);
static float get_distance(void);
static void send_http_request(bool occupied);
static void max7219_send(uint8_t reg, uint8_t data);
static void max7219_init(void);
static void max7219_clear(void);
static void max7219_display_digit(const uint8_t* digit);

// Entry point
extern "C" void app_main(void)
{
  wifi_init();
  gpio_init();
  blink_led();
  //max7219_init();
  //max7219_clear();

  while (1)
  {
    float distance = get_distance();
    bool occupied = (distance != -1 && distance < 20);

    printf("Distance: %.2f cm | Occupied: %s\n",
      distance,
      occupied ? "YES" : "NO");

    gpio_set_level(RED_LED_PIN, occupied);

    send_http_request(occupied);

    // if (occupied)
    // {
    //   max7219_display_digit(digit_0); // show 0
    // }
    // else
    // {
    //   max7219_display_digit(digit_1); // show 1
    // }

    // Take a new reading every second.
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
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  wifi_config_t wifi_config = {};
  strcpy((char*)wifi_config.ap.ssid, "ESP32-AP");
  strcpy((char*)wifi_config.ap.password, "12345678");
  wifi_config.ap.max_connection = 4;
  wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

  esp_wifi_set_mode(WIFI_MODE_AP);
  esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
  esp_wifi_start();

  ESP_LOGI("WIFI", "AP started. SSID: ESP32-AP");
}

static void max7219_init(void)
{
  gpio_set_direction(DATA_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(CLK_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(CS_PIN, GPIO_MODE_OUTPUT);

  max7219_send(0x0F, 0x00); // display test off
  max7219_send(0x0C, 0x01); // shutdown = normal operation
  max7219_send(0x0B, 0x07); // scan limit = 8 digits
  max7219_send(0x09, 0x00); // no decode
  max7219_send(0x0A, 0x05); // brightness (0-15)
}

static void max7219_clear(void)
{
  for (int row = 1; row <= 8; row++)
  {
    max7219_send(row, 0x00);
  }
}

static void blink_led(void)
{
  for (int i = 0; i < 5; i++)
  {
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
  while (gpio_get_level(ECHO_PIN) == 0)
  {
    if (esp_timer_get_time() - start > 50000)
      return -1;
  }

  int64_t echo_start = esp_timer_get_time();

  // When ECHO changes to 1, start measuring the time.
  while (gpio_get_level(ECHO_PIN) == 1)
  {
    if (esp_timer_get_time() - echo_start > 50000)
      return -1;
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

  esp_http_client_config_t config = {};
  config.url = SERVER_URL;

  esp_http_client_handle_t client = esp_http_client_init(&config);

  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, json, strlen(json));

  esp_http_client_perform(client);

  esp_http_client_cleanup(client);
}

static void max7219_send(uint8_t reg, uint8_t data)
{
  gpio_set_level(CS_PIN, 0);

  for (int i = 0; i < 3; i++) // 3 modules
  {
    for (int bit = 7; bit >= 0; bit--)
    {
      gpio_set_level(CLK_PIN, 0);
      gpio_set_level(DATA_PIN, (reg >> bit) & 1);
      gpio_set_level(CLK_PIN, 1);
    }

    for (int bit = 7; bit >= 0; bit--)
    {
      gpio_set_level(CLK_PIN, 0);
      gpio_set_level(DATA_PIN, (data >> bit) & 1);
      gpio_set_level(CLK_PIN, 1);
    }
  }

  gpio_set_level(CS_PIN, 1);
}

static void max7219_display_digit(const uint8_t* digit)
{
  for (int row = 0; row < 8; row++)
  {
    gpio_set_level(CS_PIN, 0);

    // Send same digit to all 3 modules
    for (int dev = 0; dev < 3; dev++)
    {
      // send row address
      for (int bit = 7; bit >= 0; bit--)
      {
        gpio_set_level(CLK_PIN, 0);
        gpio_set_level(DATA_PIN, ((row + 1) >> bit) & 1);
        gpio_set_level(CLK_PIN, 1);
      }

      // send row data
      for (int bit = 7; bit >= 0; bit--)
      {
        gpio_set_level(CLK_PIN, 0);
        gpio_set_level(DATA_PIN, (digit[row] >> bit) & 1);
        gpio_set_level(CLK_PIN, 1);
      }
    }

    gpio_set_level(CS_PIN, 1);
  }
}