#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Wifi credentials
const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";

// Server
const char* SERVER_URL = "http://192.168.1.67:3000/sensor";

// Pins
const int LED_PIN = 2;
const int RED_LED_PIN = 23;
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;

void blinkLED();
void connectWiFi();
float getDistance();
void sendHTTPRequest(bool);

void setup() {
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  blinkLED();
  connectWiFi();
}

void loop() {
  float distance = getDistance();
  bool occupied = (distance != -1 && distance < 10);

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.print(" cm  |  Occupied: ");
  Serial.println(occupied ? "YES" : "NO");

  digitalWrite(RED_LED_PIN, occupied ? HIGH : LOW);

  sendHTTPRequest(occupied);

  // Take a new every reading every second
  delay(1000);
}

void blinkLED() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(250);
    digitalWrite(LED_PIN, LOW);
    delay(250);
  }
}

void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected — IP: ");
  Serial.println(WiFi.localIP());
}

float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = (duration * 0.0343) / 2;

  if (distance < 1 || distance > 400) return -1;
  return distance;
}

void sendHTTPRequest(bool occupied) {
  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");

  String body = String("{\"occupied\":") + (occupied ? "true" : "false") + "}";

  int httpCode = http.POST(body);

  if (httpCode != 200) {
    Serial.print("[HTTP] Error: ");
    Serial.println(httpCode);
  }

  http.end();
}