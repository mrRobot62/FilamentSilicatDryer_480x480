//
// host_random_mask_test.cpp
// ESP32-S3 HOST test: Serial2 link to ESP32-WROOM CLIENT.
//
// - TX (HOST) = IO40
// - RX (HOST) = IO02
//
// Behaviour:
// - Every 500 ms: send H;GET;STATUS
// - Parse and print incoming C;STATUS lines
// - Every 5 seconds: randomly toggle 1..3 bits in range 0..8 and send H;SET;<mask>
//

#include <Arduino.h>
#include "protocol.h"

HardwareSerial &linkSerial = Serial2;

// Host UART pins on ESP32-S3
constexpr int HOST_RX_PIN = 2;  // IO02
constexpr int HOST_TX_PIN = 40; // IO40

constexpr uint32_t LINK_BAUDRATE = 115200;
constexpr unsigned long STATUS_INTERVAL_MS = 500;
constexpr unsigned long RANDOM_SET_INTERVAL_MS = 5000;

// RX line buffer for Serial2
static String rxBuffer;

// Local desired mask (what HOST wants to set)
static uint16_t desiredMask = 0x0000;

static unsigned long lastStatusMs = 0;
static unsigned long lastRandomSetMs = 0;

// --- Utility: 16-bit value -> bit string ("0101...") ---
// inline const char *bits16ToStr(uint16_t value)
// {
//     static char buf[17];
//     for (int i = 0; i < 16; ++i)
//     {
//         buf[i] = (value & (1u << (16 - i))) ? '1' : '0';
//     }
//     buf[16] = '\0';
//     return buf;
// }

// Toggle n random bits in [bitMin..bitMax]
static void toggleRandomBits(uint16_t &mask, int bitMin, int bitMax, int count)
{
    for (int i = 0; i < count; ++i)
    {
        int b = random(bitMin, bitMax + 1); // inclusive
        mask ^= (1u << b);
    }
}

void setup()
{
    Serial.begin(115200);
    while (!Serial)
    {
        ;
    }

    Serial.println();
    Serial.println("=== ESP32-S3 HOST Random Mask Test ===");
    Serial.println("Serial2 link to CLIENT.");
    Serial.print("HOST RX pin: IO");
    Serial.println(HOST_RX_PIN);
    Serial.print("HOST TX pin: IO");
    Serial.println(HOST_TX_PIN);
    Serial.print("Baudrate: ");
    Serial.println(LINK_BAUDRATE);
    Serial.println("Every 500ms: H;GET;STATUS");
    Serial.println("Every 5s: random toggle bits [0..8] and send H;SET");
    Serial.println();

    // Init Serial2 with explicit pins
    linkSerial.begin(LINK_BAUDRATE, SERIAL_8N1, HOST_RX_PIN, HOST_TX_PIN);

    // Seed RNG (ESP32 has hardware RNG available through randomSeed)
    randomSeed(esp_random());

    lastStatusMs = millis();
    lastRandomSetMs = millis();
}

void loop()
{
    const unsigned long now = millis();

    // 1) Periodic STATUS request
    if (now - lastStatusMs >= STATUS_INTERVAL_MS)
    {
        String msg = ProtocolCodec::buildHostGetStatus(); // "H;GET;STATUS\r\n"
        linkSerial.print(msg);

        // Log TX (trim CRLF for pretty output)
        Serial.print("[Host] TX: ");
        Serial.println("H;GET;STATUS");

        lastStatusMs = now;
    }

    // 2) Every 5s: random toggle bits 0..8 and send SET
    if (now - lastRandomSetMs >= RANDOM_SET_INTERVAL_MS)
    {
        int toggles = random(1, 4); // 1..3 bits
        uint16_t before = desiredMask;

        toggleRandomBits(desiredMask, 0, 3, toggles);

        String setMsg = ProtocolCodec::buildHostSet(desiredMask); // "H;SET;XXXX\r\n"
        linkSerial.print(setMsg);

        Serial.print("[Host] TX: H;SET;0x");
        Serial.print(desiredMask, HEX);
        Serial.print("  bits=");
        Serial.print(bits16ToStr(desiredMask));
        Serial.print("  (toggled ");
        Serial.print(toggles);
        Serial.print(" bit(s), before=0x");
        Serial.print(before, HEX);
        Serial.println(")");

        lastRandomSetMs = now;
    }

    // 3) Read incoming lines from CLIENT on Serial2
    while (linkSerial.available() > 0)
    {
        char c = static_cast<char>(linkSerial.read());

        if (c == '\r')
            continue;

        if (c == '\n')
        {
            if (rxBuffer.length() == 0)
                continue;

            String line = rxBuffer;
            rxBuffer = "";

            // Log raw RX line
            Serial.print("[Host] RX: ");
            Serial.println(line);

            // Parse
            ProtocolMessageType type;
            ProtocolStatus st;
            uint16_t mask = 0;
            int errorCode = 0;
            uint16_t maskB = 0;
            uint16_t maskC = 0;
            bool ok = ProtocolCodec::parseLine(line, type, st, mask, errorCode, maskB, maskC);
            if (!ok)
            {
                Serial.println("  [Host] Parse error.");
                continue;
            }

            if (type == ProtocolMessageType::ClientStatus)
            {
                Serial.println("  [Host] Parsed STATUS:");
                Serial.print("    outputsMask: 0x");
                Serial.print(st.outputsMask, HEX);
                Serial.print("  bits=");
                Serial.println(bits16ToStr(st.outputsMask));

                Serial.print("    adcRaw: ");
                Serial.print(st.adcRaw[0]);
                Serial.print(", ");
                Serial.print(st.adcRaw[1]);
                Serial.print(", ");
                Serial.print(st.adcRaw[2]);
                Serial.print(", ");
                Serial.println(st.adcRaw[3]);

                // NOTE: your current protocol uses tempRaw = °C * 4 (0.25°C steps)
                float tempC = st.tempRaw / 4.0f;
                Serial.print("    tempRaw: ");
                Serial.print(st.tempRaw);
                Serial.print(" => ");
                Serial.print(tempC);
                Serial.println(" °C");
            }
            else if (type == ProtocolMessageType::ClientAckSet)
            {
                Serial.print("  [Host] ACK SET mask=0x");
                Serial.println(mask, HEX);
            }
            else if (type == ProtocolMessageType::ClientErrSet)
            {
                Serial.print("  [Host] ERR SET code=");
                Serial.println(errorCode);
            }
            else if (type == ProtocolMessageType::ClientPong)
            {
                Serial.println("  [Host] PONG");
            }
            else if (type == ProtocolMessageType::ClientRst)
            {
                Serial.println("  [Host] RST ACK");
            }
        }
        else
        {
            rxBuffer += c;
            // Optional: prevent runaway if noise/no newline
            if (rxBuffer.length() > 200)
            {
                Serial.println("[Host] RX buffer overflow, clearing.");
                rxBuffer = "";
            }
        }
    }
}