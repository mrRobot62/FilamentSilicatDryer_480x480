#include <Arduino.h>

// Host UART pins on ESP32-S3
static constexpr int HOST_RX_PIN = 2;
static constexpr int HOST_TX_PIN = 40;

static constexpr uint32_t BAUD_USB = 115200;
static constexpr uint32_t BAUD_LINK = 115200;

HardwareSerial &LINK = Serial2;

static String rxLine;

static void sendPing() {
    LINK.print("H;PING\r\n");
    Serial.println("[HOST TX] H;PING");
}

static void processRxByte(char c) {
    if (c == '\r') {
        return;
    }

    if (c == '\n') {
        if (rxLine.length() > 0) {
            Serial.print("[HOST RX] ");
            Serial.println(rxLine);

            // Minimal check
            if (rxLine == "C;PONG") {
                Serial.println("[HOST] OK: PONG");
            }

            rxLine = "";
        }
        return;
    }

    rxLine += c;

    // Safety: avoid runaway if garbage/no '\n'
    if (rxLine.length() > 120) {
        Serial.print("[HOST RX] (overflow) ");
        Serial.println(rxLine);
        rxLine = "";
    }
}

void setup() {
    Serial.begin(BAUD_USB);
    delay(200);

    Serial.println();
    Serial.println("=== HOST LOWLEVEL PING TEST ===");
    Serial.printf("LINK Serial2 RX=%d TX=%d BAUD=%lu\n", HOST_RX_PIN, HOST_TX_PIN, (unsigned long)BAUD_LINK);

    LINK.begin(BAUD_LINK, SERIAL_8N1, HOST_RX_PIN, HOST_TX_PIN);
}

void loop() {
    static uint32_t nextPingMs = 0;
    const uint32_t now = millis();

    // Send ping every 500ms
    if ((int32_t)(now - nextPingMs) >= 0) {
        sendPing();
        nextPingMs = now + 500;
    }

    // Read all available RX bytes
    while (LINK.available() > 0) {
        char c = (char)LINK.read();
        processRxByte(c);
    }
}