#include <Arduino.h>
#include "HostComm.h"
#include "log_core.h"

// Host UART pins on ESP32-S3
static constexpr int HOST_RX_PIN = 2;
static constexpr int HOST_TX_PIN = 40;

static constexpr uint32_t BAUD_USB  = 115200;
static constexpr uint32_t BAUD_LINK = 115200;

static constexpr uint32_t PING_PERIOD_MS = 3000;

HardwareSerial &LINK = Serial2;
HostComm g_comm(LINK);

static String rxLine;

static void processCompleteLine(const String &line) {
    // 1) Keep your proven raw log
    Serial.print("[HOST RX] ");
    Serial.println(line);

    // 2) Feed the same line into HostComm parser (NO UART reads inside HostComm!)
    g_comm.processLine(line);

    // 3) Optional: show HostComm interpretation
    if (g_comm.lastPongReceived()) {
        Serial.println("[HOST] HostComm OK: PONG flag set");
        g_comm.clearLastPongFlag();
    }
    if (g_comm.hasCommError()) {
        Serial.println("[HOST] HostComm ERROR flag set");
        g_comm.clearCommErrorFlag();
    }
}

static void processRxByte(char c) {
    if (c == '\r') return;

    if (c == '\n') {
        if (rxLine.length() > 0) {
            processCompleteLine(rxLine);
            rxLine = "";
        }
        return;
    }

    rxLine += c;

    if (rxLine.length() > 120) {
        Serial.print("[HOST RX] (overflow) ");
        Serial.println(rxLine);
        rxLine = "";
    }
}

static void sendPingViaHostComm() {
    // Fresh cycle (so we don't pass on stale PONG)
    g_comm.clearLastPongFlag();
    g_comm.clearCommErrorFlag();

    // Send ping using HostComm (so we test its TX path too)
    g_comm.sendPing();

    // Optional raw debug
    Serial.println("[HOST TX] HostComm.sendPing()");
}

void setup() {
    Serial.begin(BAUD_USB);
    delay(200);

    Serial.println();
    Serial.println("=== HOST LOWLEVEL + HostComm PING/PONG ===");
    Serial.printf("LINK Serial2 RX=%d TX=%d BAUD=%lu\n", HOST_RX_PIN, HOST_TX_PIN, (unsigned long)BAUD_LINK);

    // IMPORTANT: Only ONE begin() call for the LINK UART
    g_comm.begin(BAUD_LINK, HOST_RX_PIN, HOST_TX_PIN);
}

void loop() {
    static uint32_t nextPingMs = 0;
    const uint32_t now = millis();

    if ((int32_t)(now - nextPingMs) >= 0) {
        sendPingViaHostComm();
        nextPingMs = now + PING_PERIOD_MS;
    }

    // Single UART consumer: lowlevel reads bytes
    while (LINK.available() > 0) {
        const char c = (char)LINK.read();
        processRxByte(c);
    }
}