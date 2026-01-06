#include <Arduino.h>
#include "HostComm.h"

constexpr int HOST_RX_PIN = 2;
constexpr int HOST_TX_PIN = 40;
constexpr int HOST_USB_SERIAL_BAUD = 115200;
constexpr int HOST_LINK_SERIAL_BAUD = 115200;
HostComm host(Serial2);

void setup() {
  Serial.begin(HOST_USB_SERIAL_BAUD);
  delay(200);
  Serial.println("=== HOST HostComm PING ONLY ===");
  host.begin(HOST_LINK_SERIAL_BAUD, HOST_RX_PIN, HOST_TX_PIN);
}

void loop() {
  host.loop();

  static uint32_t nextPing = 0;
  uint32_t now = millis();

  if ((int32_t)(now - nextPing) >= 0) {
    host.sendPing();
    Serial.println("[HOST] sendPing()");
    nextPing = now + 500;
  }

  if (host.lastPongReceived()) {
    Serial.println("[HOST] got PONG");
    host.clearLastPongFlag();
  }

  if (host.hasCommError()) {
    Serial.println("[HOST] COMM ERROR!");
    host.clearCommErrorFlag();
  }
}