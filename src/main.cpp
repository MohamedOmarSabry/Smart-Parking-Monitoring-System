#include <Arduino.h>

const int LED_PIN = 2;
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;

void blinkLED();
float getDistance();

void setup() {
  Serial.begin(9600);

  pinMode(LED_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  blinkLED();
}

void loop() {
  float distance = getDistance();

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  delay(1000);
}

void blinkLED() {
  Serial.println("LED blink test starting...");
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(250);
    digitalWrite(LED_PIN, LOW);
    delay(250);
  }
}

float getDistance() {
  // Send a 10 microsecond pulse on TRIG to trigger the sensor
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // Set TRIG pin to high for 10 microseconds
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Measure how long the ECHO pin stays HIGH (in microseconds)
  long duration = pulseIn(ECHO_PIN, HIGH);

  // Convert m/s to cm/microsecond
  float distance = (duration * 0.0343) / 2;

  // Filter out readings outside the sensor's valid range
  if (distance < 1 || distance > 400) {
    return -1; // invalid reading
  }
  return distance;
}