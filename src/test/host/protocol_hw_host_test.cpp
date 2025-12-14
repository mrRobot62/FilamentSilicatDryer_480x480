//
// protocol_hw_host_test.cpp
// External HOST test runner (ESP32-S3) for real CLIENT HW test.
//
// - Link UART: Serial2 (HOST_RX_PIN=IO02, HOST_TX_PIN=IO40)
// - CLIENT on WROOM listens on Serial2 RX=GPIO16 TX=GPIO17 (already in your client)
//
// Workflow:
//   help -> list tests
//   enter TestID -> run selected test
//   print PASS/FAIL and return to prompt
//

#include <Arduino.h>
#include "protocol.h"

// -------------------------
// Link UART config (HOST side / ESP32-S3)
// -------------------------
HardwareSerial &linkSerial = Serial2;

// Host UART pins on ESP32-S3
constexpr int HOST_RX_PIN = 2;  // IO02
constexpr int HOST_TX_PIN = 40; // IO40

constexpr uint32_t LINK_BAUDRATE = 115200;

// Timeouts
constexpr unsigned long RX_TIMEOUT_MS = 800;   // wait for a response line
constexpr unsigned long BETWEEN_STEPS_MS = 50; // small spacing
constexpr unsigned long LINE_MAX_LEN = 200;

// -------------------------
// Console input (USB Serial)
// -------------------------
static String usbLine;

// -------------------------
// Link RX line buffer
// -------------------------
static String linkLineBuf;

// -------------------------
// Utility: strip CR/LF
// -------------------------
static String stripCrlf(const String &s)
{
    String t = s;
    t.replace("\r", "");
    t.replace("\n", "");
    t.trim();
    return t;
}

// -------------------------
// Send one HOST frame (without CRLF) over link
// -------------------------
static void sendHostLine(const String &lineNoCrlf)
{
    // protocol expects CRLF on wire
    linkSerial.print(lineNoCrlf);
    linkSerial.print("\r\n");

    Serial.print("[HOST->LINK] ");
    Serial.println(lineNoCrlf);
}

// -------------------------
// Read one complete line from link (no CRLF returned). Returns true if line received.
// -------------------------
static bool readLinkLine(String &outLineNoCrlf, unsigned long timeoutMs)
{
    const unsigned long start = millis();
    while ((millis() - start) < timeoutMs)
    {
        while (linkSerial.available() > 0)
        {
            char c = static_cast<char>(linkSerial.read());
            if (c == '\r')
                continue;

            if (c == '\n')
            {
                if (linkLineBuf.length() == 0)
                    continue;
                outLineNoCrlf = stripCrlf(linkLineBuf);
                linkLineBuf = "";
                return true;
            }
            else
            {
                linkLineBuf += c;
                if (linkLineBuf.length() > LINE_MAX_LEN)
                {
                    // protect against noise
                    linkLineBuf = "";
                }
            }
        }
        delay(1);
    }
    return false;
}

// Flush any pending incoming link data
static void flushLinkRx()
{
    while (linkSerial.available() > 0)
        (void)linkSerial.read();
    linkLineBuf = "";
}

// -------------------------
// Expectation types
// -------------------------
enum class ExpectKind
{
    AnyLine, // just require a line
    Pong,    // C;PONG
    AckSet,  // C;ACK;SET;MMMM
    AckUpd,  // C;ACK;UPD;MMMM
    AckTog,  // C;ACK;TOG;MMMM
    Status   // C;STATUS;....
};

struct Expectation
{
    ExpectKind kind;
    bool checkMask;
    uint16_t expectedMask;
};

// One test step = send one HOST line and validate next CLIENT response line
struct TestStep
{
    const char *hostTx; // line without CRLF
    Expectation expect;
};

// A test case consists of multiple steps
using TestFn = bool (*)();
struct TestCase
{
    int id;
    const char *name;
    const char *description;

    // Step-based tests (existing)
    const TestStep *steps;
    size_t stepCount;

    // Function-based meta tests (new)
    TestFn fn;
};

// -------------------------
// Parse and validate a single client response line
// -------------------------
static bool validateResponse(const String &clientLine, const Expectation &exp, String &failReason)
{
    Serial.print("[LINK->HOST] ");
    Serial.println(clientLine);

    ProtocolMessageType type = ProtocolMessageType::Unknown;
    ProtocolStatus st{};
    uint16_t mask = 0;
    int errorCode = 0;
    uint16_t maskB = 0;
    uint16_t maskC = 0;

    if (!ProtocolCodec::parseLine(clientLine, type, st, mask, errorCode, maskB, maskC))
    {
        failReason = "Host parser could not parse client line";
        return false;
    }

    switch (exp.kind)
    {
    case ExpectKind::AnyLine:
        return true;

    case ExpectKind::Pong:
        if (type != ProtocolMessageType::ClientPong)
        {
            failReason = "Expected C;PONG";
            return false;
        }
        return true;

    case ExpectKind::AckSet:
        if (type != ProtocolMessageType::ClientAckSet)
        {
            failReason = "Expected C;ACK;SET";
            return false;
        }
        if (exp.checkMask && mask != exp.expectedMask)
        {
            failReason = "ACK SET mask mismatch";
            return false;
        }
        return true;

    case ExpectKind::AckUpd:
        if (type != ProtocolMessageType::ClientAckUpd)
        {
            failReason = "Expected C;ACK;UPD";
            return false;
        }
        if (exp.checkMask && mask != exp.expectedMask)
        {
            failReason = "ACK UPD mask mismatch";
            return false;
        }
        return true;

    case ExpectKind::AckTog:
        if (type != ProtocolMessageType::ClientAckTog)
        {
            failReason = "Expected C;ACK;TOG";
            return false;
        }
        if (exp.checkMask && mask != exp.expectedMask)
        {
            failReason = "ACK TOG mask mismatch";
            return false;
        }
        return true;

    case ExpectKind::Status:
        if (type != ProtocolMessageType::ClientStatus)
        {
            failReason = "Expected C;STATUS";
            return false;
        }
        if (exp.checkMask && st.outputsMask != exp.expectedMask)
        {
            failReason = "STATUS outputsMask mismatch";
            return false;
        }
        // Minimal check only; HW-test can later add ADC/temp plausibility checks
        Serial.print("  outputsMask=0x");
        Serial.println(st.outputsMask, HEX);
        Serial.print("  adcRaw0=");
        Serial.println(st.adcRaw[0]);
        Serial.print("  tempRaw=");
        Serial.println(st.tempRaw);
        return true;
    }

    failReason = "Unknown expectation kind";
    return false;
}

// -------------------------
// Run a test case
// -------------------------
static bool runTestCase(const TestCase &tc)
{
    // Function-based meta test
    if (tc.fn)
    {
        bool ok = tc.fn();
        return ok;
    }

    Serial.println();
    Serial.print("[TEST] #");
    Serial.print(tc.id);
    Serial.print(" ");
    Serial.println(tc.name);
    Serial.print("       ");
    Serial.println(tc.description);
    Serial.println();

    flushLinkRx();

    for (size_t i = 0; i < tc.stepCount; ++i)
    {
        const TestStep &step = tc.steps[i];

        sendHostLine(step.hostTx);

        String rxLine;
        if (!readLinkLine(rxLine, RX_TIMEOUT_MS))
        {
            Serial.println("[FAIL] Timeout waiting for client response");
            Serial.print("       After command: ");
            Serial.println(step.hostTx);
            return false;
        }

        String reason;
        if (!validateResponse(rxLine, step.expect, reason))
        {
            Serial.println("[FAIL] Validation failed");
            Serial.print("       Command: ");
            Serial.println(step.hostTx);
            Serial.print("       Reason: ");
            Serial.println(reason);
            Serial.print("       Result: ");
            Serial.println(rxLine);
            return false;
        }

        delay(BETWEEN_STEPS_MS);
    }

    return true;
}

static bool sendAndExpect(const char *hostLine, const Expectation &exp, String &failReason)
{
    flushLinkRx();
    sendHostLine(hostLine);
    delay(5);

    String rxLine;
    if (!readLinkLine(rxLine, RX_TIMEOUT_MS))
    {
        failReason = String("Timeout waiting for response after: ") + hostLine;
        return false;
    }

    if (!validateResponse(rxLine, exp, failReason))
    {
        failReason = String("Validation failed after: ") + hostLine + " | " + failReason;
        return false;
    }

    delay(BETWEEN_STEPS_MS);
    return true;
}

static String hx4(uint16_t v)
{
    char b[8];
    snprintf(b, sizeof(b), "%04X", v);
    return String(b);
}

// -------------------------
// Test Definitions (minimal starter set)
// Extend later to more permutations.
// -------------------------

// Test 1: Ping
static const TestStep STEPS_PING[] = {
    {"H;PING", {ExpectKind::Pong, false, 0}},
};

// Test 2: SET 0 then STATUS must report 0
static const TestStep STEPS_SET0_STATUS[] = {
    {"H;SET;0000", {ExpectKind::AckSet, true, 0x0000}},
    {"H;GET;STATUS", {ExpectKind::Status, true, 0x0000}},
};

// Test 3: SET lower 8 bits then STATUS (CH0..CH7 should be ON)
static const TestStep STEPS_SETFF_STATUS[] = {
    {"H;SET;00FF", {ExpectKind::AckSet, true, 0x00FF}},
    {"H;GET;STATUS", {ExpectKind::Status, true, 0x00FF}},
};

// Test 4: UPD set bit3 clear bit1 starting from 0x000F
static const TestStep STEPS_UPD_MIX[] = {
    {"H;SET;000F", {ExpectKind::AckSet, true, 0x000F}},
    {"H;UPD;0008;0002", {ExpectKind::AckUpd, true, 0x000D}}, // (0xF | 0x8) & ~0x2 = 0xD
    {"H;GET;STATUS", {ExpectKind::Status, true, 0x000D}},
};

// Test 5: TOG bit0 twice returns original
static const TestStep STEPS_TOG_TWICE[] = {
    {"H;SET;0000", {ExpectKind::AckSet, true, 0x0000}},
    {"H;TOG;0001", {ExpectKind::AckTog, true, 0x0001}},
    {"H;TOG;0001", {ExpectKind::AckTog, true, 0x0000}},
    {"H;GET;STATUS", {ExpectKind::Status, true, 0x0000}},
};

// Test 10 – SET single-bit sweep (Bit0..Bit7)
static bool test_set_single_bit_sweep()
{
    Serial.println("[META] SET single-bit sweep (bits 0..7)");

    flushLinkRx();
    String reason;

    // Start from known state
    if (!sendAndExpect("H;SET;0000", {ExpectKind::AckSet, true, 0x0000}, reason))
    {
        Serial.println(reason);
        return false;
    }

    for (int i = 0; i < 8; ++i)
    {
        uint16_t m = (uint16_t)(1u << i);

        String cmd = String("H;SET;") + hx4(m);
        if (!sendAndExpect(cmd.c_str(), {ExpectKind::AckSet, true, m}, reason))
        {
            Serial.println(reason);
            return false;
        }

        if (!sendAndExpect("H;GET;STATUS", {ExpectKind::Status, true, m}, reason))
        {
            Serial.println(reason);
            return false;
        }

        Serial.print("  [OK] bit ");
        Serial.print(i);
        Serial.print(" mask=0x");
        Serial.println(m, HEX);
    }

    return true;
}

// Test 11 – SET patterns
static bool test_set_patterns()
{
    Serial.println("[META] SET patterns");

    const uint16_t patterns[] = {0x0000, 0x00FF, 0x00AA, 0x0055, 0x000F, 0x00F0, 0x0033, 0x00CC};

    flushLinkRx();
    String reason;

    for (uint16_t p : patterns)
    {
        String cmd = String("H;SET;") + hx4(p);
        if (!sendAndExpect(cmd.c_str(), {ExpectKind::AckSet, true, p}, reason))
        {
            Serial.println(reason);
            return false;
        }
        if (!sendAndExpect("H;GET;STATUS", {ExpectKind::Status, true, p}, reason))
        {
            Serial.println(reason);
            return false;
        }

        Serial.print("  [OK] pattern 0x");
        Serial.println(p, HEX);
    }
    return true;
}

// Test 20 – TOG single-bit sweep
static bool test_tog_single_bit_sweep()
{
    Serial.println("[META] TOG single-bit sweep (bits 0..7)");

    flushLinkRx();
    String reason;

    uint16_t expected = 0x0000;
    if (!sendAndExpect("H;SET;0000", {ExpectKind::AckSet, true, expected}, reason))
    {
        Serial.println(reason);
        return false;
    }

    for (int i = 0; i < 8; ++i)
    {
        uint16_t t = (uint16_t)(1u << i);
        expected ^= t;

        String cmd = String("H;TOG;") + hx4(t);
        if (!sendAndExpect(cmd.c_str(), {ExpectKind::AckTog, true, expected}, reason))
        {
            Serial.println(reason);
            return false;
        }
        if (!sendAndExpect("H;GET;STATUS", {ExpectKind::Status, true, expected}, reason))
        {
            Serial.println(reason);
            return false;
        }

        Serial.print("  [OK] toggle bit ");
        Serial.print(i);
        Serial.print(" expected=0x");
        Serial.println(expected, HEX);
    }

    return true;
}

// TestID 21 – TOG Patterns
static bool test_tog_patterns()
{
    Serial.println("[META] TOG patterns (AA, 55, FF)");

    String reason;
    flushLinkRx();

    // Start known
    uint16_t expected = 0x0000;
    if (!sendAndExpect("H;SET;0000", {ExpectKind::AckSet, true, expected}, reason))
    {
        Serial.println(reason);
        return false;
    }

    // Toggle 0x00AA -> expected = 0x00AA
    expected ^= 0x00AA;
    if (!sendAndExpect("H;TOG;00AA", {ExpectKind::AckTog, true, expected}, reason))
    {
        Serial.println(reason);
        return false;
    }
    if (!sendAndExpect("H;GET;STATUS", {ExpectKind::Status, true, expected}, reason))
    {
        Serial.println(reason);
        return false;
    }

    // Toggle 0x0055 -> expected = 0x00FF
    expected ^= 0x0055;
    if (!sendAndExpect("H;TOG;0055", {ExpectKind::AckTog, true, expected}, reason))
    {
        Serial.println(reason);
        return false;
    }
    if (!sendAndExpect("H;GET;STATUS", {ExpectKind::Status, true, expected}, reason))
    {
        Serial.println(reason);
        return false;
    }

    // Toggle 0x00FF -> expected = 0x0000
    expected ^= 0x00FF;
    if (!sendAndExpect("H;TOG;00FF", {ExpectKind::AckTog, true, expected}, reason))
    {
        Serial.println(reason);
        return false;
    }
    if (!sendAndExpect("H;GET;STATUS", {ExpectKind::Status, true, expected}, reason))
    {
        Serial.println(reason);
        return false;
    }

    Serial.println("  [OK] TOG patterns verified (AA -> FF -> 00)");
    return true;
}

// Test 30 – UPD set-only sweep
static bool test_upd_set_only_sweep()
{
    Serial.println("[META] UPD set-only sweep (bits 0..7)");

    flushLinkRx();
    String reason;

    uint16_t expected = 0x0000;
    if (!sendAndExpect("H;SET;0000", {ExpectKind::AckSet, true, expected}, reason))
    {
        Serial.println(reason);
        return false;
    }

    for (int i = 0; i < 8; ++i)
    {
        uint16_t setM = (uint16_t)(1u << i);
        uint16_t clrM = 0;

        expected = (expected | setM) & (uint16_t)(~clrM);

        String cmd = String("H;UPD;") + hx4(setM) + ";" + hx4(clrM);
        if (!sendAndExpect(cmd.c_str(), {ExpectKind::AckUpd, true, expected}, reason))
        {
            Serial.println(reason);
            return false;
        }
        if (!sendAndExpect("H;GET;STATUS", {ExpectKind::Status, true, expected}, reason))
        {
            Serial.println(reason);
            return false;
        }

        Serial.print("  [OK] upd set bit ");
        Serial.print(i);
        Serial.print(" expected=0x");
        Serial.println(expected, HEX);
    }

    return true;
}

// Test 31 – UPD clear-only sweep
static bool test_upd_clear_only_sweep()
{
    Serial.println("[META] UPD clear-only sweep (bits 0..7)");

    flushLinkRx();
    String reason;

    uint16_t expected = 0x00FF; // start with all 8 bits set
    if (!sendAndExpect("H;SET;00FF", {ExpectKind::AckSet, true, expected}, reason))
    {
        Serial.println(reason);
        return false;
    }

    for (int i = 0; i < 8; ++i)
    {
        uint16_t setM = 0;
        uint16_t clrM = (uint16_t)(1u << i);

        expected = (expected | setM) & (uint16_t)(~clrM);

        String cmd = String("H;UPD;") + hx4(setM) + ";" + hx4(clrM);
        if (!sendAndExpect(cmd.c_str(), {ExpectKind::AckUpd, true, expected}, reason))
        {
            Serial.println(reason);
            return false;
        }
        if (!sendAndExpect("H;GET;STATUS", {ExpectKind::Status, true, expected}, reason))
        {
            Serial.println(reason);
            return false;
        }

        Serial.print("  [OK] upd clear bit ");
        Serial.print(i);
        Serial.print(" expected=0x");
        Serial.println(expected, HEX);
    }

    return true;
}

// TestID 40 UPD overlap
static bool test_upd_overlap_priority()
{
    Serial.println("[META] UPD overlap priority (clear wins)");

    flushLinkRx();
    String reason;

    // Start: all 8 bits set
    if (!sendAndExpect("H;SET;00FF", {ExpectKind::AckSet, true, 0x00FF}, reason))
    {
        Serial.println(reason);
        return false;
    }

    // Overlap: set bit3 and clear bit3 simultaneously -> clear must win => bit3 becomes 0
    // 0x00FF with bit3 cleared => 0x00F7
    if (!sendAndExpect("H;UPD;0008;0008", {ExpectKind::AckUpd, true, 0x00F7}, reason))
    {
        Serial.println(reason);
        return false;
    }

    if (!sendAndExpect("H;GET;STATUS", {ExpectKind::Status, true, 0x00F7}, reason))
    {
        Serial.println(reason);
        return false;
    }

    Serial.println("  [OK] overlap priority verified (clear wins)");
    return true;
}

// Test 50 – STATUS sensor sanity (plausibility)
static bool test_status_sensor_sanity()
{
    Serial.println("[META] STATUS sensor sanity (range checks only)");

    flushLinkRx();
    String reason;

    // Request status and parse it manually here for extra checks
    sendHostLine("H;GET;STATUS");

    String rxLine;
    if (!readLinkLine(rxLine, RX_TIMEOUT_MS))
    {
        Serial.println("Timeout waiting for STATUS");
        return false;
    }

    ProtocolMessageType type = ProtocolMessageType::Unknown;
    ProtocolStatus st{};
    uint16_t mask = 0;
    int errorCode = 0;
    uint16_t maskB = 0;
    uint16_t maskC = 0;

    if (!ProtocolCodec::parseLine(rxLine, type, st, mask, errorCode, maskB, maskC))
    {
        Serial.println("Could not parse STATUS");
        return false;
    }
    if (type != ProtocolMessageType::ClientStatus)
    {
        Serial.println("Expected C;STATUS");
        return false;
    }

    // adcRaw[0] sanity (ESP32 typical 0..4095)
    if (st.adcRaw[0] > 4095)
    {
        Serial.print("adcRaw0 out of range: ");
        Serial.println(st.adcRaw[0]);
        return false;
    }

    // tempRaw sanity (quarter °C steps): allow wide range (no physical assumptions)
    // Example range: -400..4000 (=-100..1000°C)
    if (st.tempRaw < -400 || st.tempRaw > 4000)
    {
        Serial.print("tempRaw out of range: ");
        Serial.println(st.tempRaw);
        return false;
    }

    Serial.print("  [OK] adcRaw0=");
    Serial.println(st.adcRaw[0]);
    Serial.print("  [OK] tempRaw=");
    Serial.println(st.tempRaw);
    return true;
}

//

//

// Register tests here
static const TestCase TESTS[] = {
    {1, "PING", "Verify link is alive: HOST PING -> CLIENT PONG", STEPS_PING, sizeof(STEPS_PING) / sizeof(STEPS_PING[0])},
    {2, "SET 0000", "Set all outputs low and verify STATUS outputsMask=0x0000", STEPS_SET0_STATUS, sizeof(STEPS_SET0_STATUS) / sizeof(STEPS_SET0_STATUS[0])},
    {3, "SET 00FF", "Set CH0..CH7 high and verify STATUS outputsMask=0x00FF", STEPS_SETFF_STATUS, sizeof(STEPS_SETFF_STATUS) / sizeof(STEPS_SETFF_STATUS[0])},
    {4, "UPD mixed", "Verify UPD set/clear works and clearMask has priority", STEPS_UPD_MIX, sizeof(STEPS_UPD_MIX) / sizeof(STEPS_UPD_MIX[0])},
    {5, "TOG twice", "Verify toggling the same mask twice restores previous state", STEPS_TOG_TWICE, sizeof(STEPS_TOG_TWICE) / sizeof(STEPS_TOG_TWICE[0])},

    // new meta tests:
    {10, "SET sweep", "Set each single bit (0..7) and verify STATUS", nullptr, 0, test_set_single_bit_sweep},
    {11, "SET patterns", "Set common patterns and verify STATUS", nullptr, 0, test_set_patterns},
    {20, "TOG sweep", "Toggle each single bit (0..7) and verify", nullptr, 0, test_tog_single_bit_sweep},
    {30, "UPD set sweep", "UPD with setMask only for bits 0..7", nullptr, 0, test_upd_set_only_sweep},
    {31, "UPD clr sweep", "UPD with clearMask only for bits 0..7", nullptr, 0, test_upd_clear_only_sweep},
    {40, "UPD overlap", "Verify clearMask priority when setMask and clearMask overlap", nullptr, 0, test_upd_overlap_priority},
    {50, "STATUS sanity", "Range checks for adcRaw0 and tempRaw", nullptr, 0, test_status_sensor_sanity},

};

static constexpr size_t TEST_COUNT = sizeof(TESTS) / sizeof(TESTS[0]);

// -------------------------
// Console UI
// -------------------------
static void printHelp()
{
    Serial.println();
    Serial.println("=== HW HOST Test Runner ===");
    Serial.println("Commands:");
    Serial.println("  help          - list tests");
    Serial.println("  <id>          - run test by id (e.g. 1)");
    Serial.println();

    Serial.println("Available tests:");
    for (size_t i = 0; i < TEST_COUNT; ++i)
    {
        Serial.print("  ");
        Serial.print(TESTS[i].id);
        Serial.print(") ");
        Serial.print(TESTS[i].name);
        Serial.print(" - ");
        Serial.println(TESTS[i].description);
    }
    Serial.println();
}

static const TestCase *findTestById(int id)
{
    for (size_t i = 0; i < TEST_COUNT; ++i)
    {
        if (TESTS[i].id == id)
            return &TESTS[i];
    }
    return nullptr;
}

static void prompt()
{
    Serial.println();
    Serial.print("Please Enter TestID: ");
}

static void handleUsbConsole()
{
    while (Serial.available() > 0)
    {
        char c = static_cast<char>(Serial.read());
        if (c == '\r')
            continue;

        if (c == '\n')
        {
            String cmd = usbLine;
            usbLine = "";
            cmd.trim();

            if (cmd.length() == 0)
            {
                prompt();
                return;
            }

            String lower = cmd;
            lower.toLowerCase();

            if (lower == "help")
            {
                printHelp();
                prompt();
                return;
            }

            // Try parse integer test id
            int id = cmd.toInt();
            if (id <= 0)
            {
                Serial.print("Unknown command: ");
                Serial.println(cmd);
                Serial.println("Type 'help' to list tests.");
                prompt();
                return;
            }

            const TestCase *tc = findTestById(id);
            if (!tc)
            {
                Serial.print("Unknown TestID: ");
                Serial.println(id);
                Serial.println("Type 'help' to list tests.");
                prompt();
                return;
            }

            bool ok = runTestCase(*tc);
            if (ok)
            {
                Serial.println("----------- finished successful -----------");
            }
            else
            {
                Serial.println("------------ test failed ----------");
            }

            prompt();
            return;
        }
        else
        {
            usbLine += c;
            if (usbLine.length() > 80)
                usbLine = "";
        }
    }
}

// -------------------------
// Setup / Loop
// -------------------------
void setup()
{
    Serial.begin(115200);
    delay(200);

    Serial.println();
    Serial.println("=== protocol_hw_host_test START ===");

    // Start link UART (HOST side)
    linkSerial.begin(LINK_BAUDRATE, SERIAL_8N1, HOST_RX_PIN, HOST_TX_PIN);

    Serial.print("[INFO] Link UART started @ ");
    Serial.print(LINK_BAUDRATE);
    Serial.print(" baud, RX=IO");
    Serial.print(HOST_RX_PIN);
    Serial.print(" TX=IO");
    Serial.println(HOST_TX_PIN);

    printHelp();
    prompt();
}

void loop()
{
    handleUsbConsole();

    // Note: link RX is handled during test execution via readLinkLine().
    // Here we do nothing.
}