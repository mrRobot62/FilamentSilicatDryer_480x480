//
// host_serial2_status_test.cpp
// ESP32-S3 HOST test: talk to ESP32-WROOM CLIENT via Serial2.
//
// - TX (HOST) = IO40
// - RX (HOST) = IO2
// - Baudrate  = 115200
//
// Every 500 ms the HOST sends: H;GET;STATUS
// The CLIENT (running client_main.cpp) should respond with:
//   C;STATUS;<mask>;<a0>;<a1>;<a2>;<a3>;<temp>
//
// This program:
//   - Logs TX frames: "[Host] TX: H;GET;STATUS"
//   - Logs RX frames: "[Host] RX: C;STATUS;..."
//   - Parses STATUS frames via ProtocolCodec and prints decoded values.
//

#include <Arduino.h>
#include "protocol.h"

// Serial2 on ESP32-S3 used for HOST <-> CLIENT link
HardwareSerial &linkSerial = Serial2;

// Host UART pins on ESP32-S3
constexpr int HOST_RX_PIN = 2;  // IO02
constexpr int HOST_TX_PIN = 40; // IO40

constexpr uint32_t LINK_BAUDRATE = 115200;
constexpr unsigned long STATUS_INTERVAL_MS = 500;

// Receive buffer for assembling lines from CLIENT
String rxBuffer;
unsigned long lastStatusRequest = 0;

void setup()
{
    // USB-Serial for debug output
    Serial.begin(115200);
    while (!Serial)
    {
        ; // wait for USB CDC connection on some boards
    }
    delay(3000);
    Serial.println();
    Serial.println("=== ESP32-S3 HOST Serial2 STATUS TEST ===");
    Serial.println("This device acts as HOST and talks to CLIENT via Serial2.");
    Serial.println();
    Serial.println("Pins:");
    Serial.print("  HOST TX (to CLIENT RX) = IO");
    Serial.println(HOST_TX_PIN);
    Serial.print("  HOST RX (from CLIENT TX) = IO");
    Serial.println(HOST_RX_PIN);
    Serial.println();
    Serial.println("Every 500 ms: H;GET;STATUS is sent.");
    Serial.println("Expected CLIENT answer: C;STATUS;... line(s).");
    Serial.println();

    // Configure Serial2 with explicit RX/TX pins
    // Note: no need to call any HostComm.begin() here, we talk directly over linkSerial.
    linkSerial.begin(LINK_BAUDRATE, SERIAL_8N1, HOST_RX_PIN, HOST_TX_PIN);

    lastStatusRequest = millis();
}

void loop()
{
    unsigned long now = millis();

    // 1) Periodically send H;GET;STATUS
    if (now - lastStatusRequest >= STATUS_INTERVAL_MS)
    {
        String msg = ProtocolCodec::buildHostGetStatus(); // "H;GET;STATUS\r\n"
        linkSerial.print(msg);

        INFO("[ESP32-S3:%06d] TX: %s", millis(), msg.c_str());

        lastStatusRequest = now;
    }

    // 2) Read incoming bytes from CLIENT on Serial2 and assemble lines
    while (linkSerial.available() > 0)
    {
        char c = static_cast<char>(linkSerial.read());

        if (c == '\r')
        {
            // ignore CR, wait for LF
            continue;
        }

        if (c == '\n')
        {
            if (rxBuffer.length() > 0)
            {
                String line = rxBuffer;
                rxBuffer = "";

                // Log raw RX line
                Serial.print("[Host] RX: ");
                Serial.println(line);

                // 3) Try to parse the line using ProtocolCodec
                ProtocolMessageType type;
                ProtocolStatus status;
                uint16_t mask = 0;
                int errorCode = 0;

                bool ok = ProtocolCodec::parseLine(line, type, status, mask, errorCode);
                if (!ok)
                {
                    Serial.println("  [Host] Parse error on received line.");
                }
                else
                {
                    switch (type)
                    {
                    case ProtocolMessageType::ClientStatus:
                    {
                        Serial.println("  [Host] Parsed STATUS frame:");
                        Serial.print("    outputsMask: 0x");
                        Serial.println(status.outputsMask, HEX);

                        Serial.print("    adcRaw[0..3]: ");
                        Serial.print(status.adcRaw[0]);
                        Serial.print(", ");
                        Serial.print(status.adcRaw[1]);
                        Serial.print(", ");
                        Serial.print(status.adcRaw[2]);
                        Serial.print(", ");
                        Serial.println(status.adcRaw[3]);

                        // Temperature interpretation:
                        // If you use tempRaw = °C * 4  -> tempC = tempRaw / 4.0f;
                        // If you use tempRaw = °C * 10 -> tempC = tempRaw / 10.0f;
                        //
                        // Adjust this according to your CURRENT protocol definition.
                        float tempC = status.tempRaw / 10.0f; // <-- change to /10.0f if you switch to x10 format

                        Serial.print("    tempRaw: ");
                        Serial.print(status.tempRaw);
                        Serial.print("  => ");
                        Serial.print(tempC);
                        Serial.println(" °C");
                        break;
                    }

                    case ProtocolMessageType::ClientAckSet:
                        Serial.print("  [Host] Parsed ACK for SET, mask = 0x");
                        Serial.println(mask, HEX);
                        break;

                    case ProtocolMessageType::ClientErrSet:
                        Serial.print("  [Host] Parsed ERR for SET, errorCode = ");
                        Serial.println(errorCode);
                        break;

                    case ProtocolMessageType::ClientPong:
                        Serial.println("  [Host] Parsed PONG (reply to PING).");
                        break;

                    default:
                        Serial.println("  [Host] Parsed unexpected message type from CLIENT.");
                        break;
                    }
                }
            }
        }
        else
        {
            rxBuffer += c;
        }
    }

    // No delay needed: loop is light; you can add small delay if you want
    // delay(1);
}