#include <Arduino.h>

static constexpr uint32_t BAUD_USB  = 115200;
static constexpr uint32_t BAUD_LINK = 115200;

// HOST UART pins on ESP32-S3
static constexpr int HOST_RX_PIN = 2;
static constexpr int HOST_TX_PIN = 40;

HardwareSerial& LINK = Serial2;

static uint32_t g_nextTxMs = 0;
static uint8_t  g_counter = 0;

static void sendBurst() {
  // Send a recognizable pattern
  // Example: "U" is 0x55 (nice for UART scope/logic), plus counter and CRLF
  LINK.write('U');
  LINK.write('0' + (g_counter % 10));
  LINK.write('\r');
  LINK.write('\n');
  g_counter++;
}

void setup() {
  Serial.begin(BAUD_USB);
  delay(200);

  Serial.println();
  Serial.println("=== HOST UART SENDER ===");
  Serial.printf("Serial2 RX=%d TX=%d BAUD=%lu\n", HOST_RX_PIN, HOST_TX_PIN, (unsigned long)BAUD_LINK);

  LINK.begin(BAUD_LINK, SERIAL_8N1, HOST_RX_PIN, HOST_TX_PIN);
  g_nextTxMs = millis() + 500;
}

void loop() {
  const uint32_t now = millis();

  if ((int32_t)(now - g_nextTxMs) >= 0) {
    Serial.println("[TX] sending burst...");
    sendBurst();
    g_nextTxMs = now + 500;
  }

  while (LINK.available() > 0) {
    int b = LINK.read();
    Serial.printf("[RX] 0x%02X '%c'\n", (unsigned)b, (b >= 32 && b <= 126) ? (char)b : '.');
  }
}