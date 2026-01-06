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
constexpr int HOST_RX_PIN = 2;  // IO02 = relay2
constexpr int HOST_TX_PIN = 40; // IO40 = relay1

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

        INFO("[Host:%06d] TX: %s", millis(), msg.c_str());

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
                INFO("[Host] RX: %s\n", line.c_str());

                // 3) Try to parse the line using ProtocolCodec
                ProtocolMessageType type;
                ProtocolStatus status;
                uint16_t mask = 0;
                int errorCode = 0;

                bool ok = ProtocolCodec::parseLine(line, type, status, mask, errorCode);
                if (!ok)
                {
                    ERR("\t[Host] Parse error on received line.\n");
                }
                else
                {
                    switch (type)
                    {
                    case ProtocolMessageType::ClientStatus:
                    {
                        INFO("\t[HOST] parsed STTUS frame:\n");
                        INFO("\t>>>outputsMask: 0x%04d (0b%s)\n", status.outputsMask, status.outputsMask);
                        INFO("\t>>>adcRaw[0..3]\n");
                        INFO("\t>>>[0]: %d\n", status.adcRaw[0]);
                        INFO("\t>>>[1]: %d\n", status.adcRaw[1]);
                        INFO("\t>>>[2]: %d\n", status.adcRaw[2]);
                        INFO("\t>>>[3]: %d\n", status.adcRaw[3]);

                        INFO("\t>>>Temp (MAX6675): %d °C\n", status.tempRaw);
                        break;
                    }

                    case ProtocolMessageType::ClientAckSet:
                        INFO("\t>>>[Host] Parsed ACK for SET, mask = 0x%04d (0b%s)\n", status.outputsMask, status.outputsMask);
                        break;

                    case ProtocolMessageType::ClientErrSet:
                        INFO("\t>>>[Host] Parsed ERR for SET, errorCode=%d", errorCode);
                        break;

                    case ProtocolMessageType::ClientPong:
                        INFO("\t>>>[Host] Parsed PONG (reply to PING");
                        break;

                    default:
                        Serial.println("  [Host] Parsed unexpected message type from CLIENT.");
                        INFO("\t>>>[Host] Parsed unexpected message type from CLIENT.");
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