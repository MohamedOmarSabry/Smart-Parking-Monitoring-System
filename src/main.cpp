#include <Arduino.h>

const int LED_PIN = 2;

// put function declarations here:
void blinkLED();

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("LED blink test starting...");
  blinkLED();
}

void loop() {
  // put your main code here, to run repeatedly:
}

// put function definitions here:
void blinkLED() {
  for (int i = 0; i < 10; i++) {
    digitalWrite(2, HIGH);
    delay(250);
    digitalWrite(2, LOW);
    delay(250);
  }
}