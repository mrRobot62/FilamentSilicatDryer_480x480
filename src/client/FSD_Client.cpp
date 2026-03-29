//
// Client.cpp (client_main.cpp)
// ESP32-WROOM CLIENT (real hardware) using ClientComm + protocol callbacks.
//
// READY-TO-DROP-IN FILE
// ---------------------
// This file is functionally identical to your provided source, but with extensive
// English inline documentation added. No logic changes were made.
//
// Purpose:
// - Communicate with HOST over UART2 (Serial2) on GPIO16 (RX2) / GPIO17 (TX2)
// - Apply digital outputs CH0..CH7 based on outputsMask bits 0..7
// - Provide STATUS telemetry with:
//     - outputsMask (effective, after safety + PWM truth)
//     - adcRaw[] (raw diagnostic values)
//     - tempChamber_dC  (temperature value; source depends on build flags)
//
// Protocol (HOST -> CLIENT):
//   H;SET;XXXX
//   H;UPD;SSSS;CCCC
//   H;TOG;TTTT
//   H;GET;STATUS
//   H;PING
//
// Protocol (CLIENT -> HOST):
//   C;ACK;SET;MMMM
//   C;ACK;UPD;MMMM
//   C;ACK;TOG;MMMM
//   C;STATUS;mask;a0;a1;a2;a3;tempChamber_dC
//   C;PONG
//
// Design rules:
// - Client is hardware-authoritative: it enforces safety and reports real state.
// - Do NOT switch GPIO/PWM from UART RX/callback context.
//   Instead: mark "pending", apply deterministically from loop().
// - Door is input-only telemetry and must never be driven as an output.
//
// Hardware notes:
// - GPIO34/35/36/39 are input-only on ESP32.
// - Heater is driven via PWM (LEDC), not via plain digitalWrite.
//
// Sensor notes:
// - ENABLE_INTERNAL_NTC uses ADS1115 over I2C.
// - Your schematic says: internal NTC is connected to ADS1115 AIN0.
//   Ensure the ADS channel matches that wiring (see readTemperatureRawValue()).
//

#include "FSD_Client.h"
#include "client/heater_io.h"
#include "client/sensor_ntc.h"
#include "log_client.h"
#include "log_csv.h"
#include "ntc/ntc_convert.h"
#include "ntc/ntc_divider_config_chamber.h"
#include "ntc/ntc_divider_config_hotspot.h"
#include "ntc/ntc_table_10k_ioveo_036HS05201.h"
#include "sensors/ads1115_config.h"
// #include "pins_client.h"
#include "versions.h"
#include "wifi_net.h"
#include "wifi_secrets.h"
#include <Arduino.h>

#if defined(WIFI_LOGGING_ENABLE) && (WIFI_LOGGING_ENABLE == 1)
#include "udp/fsd_udp.h"
#endif

// T13 Dual-NTC ADS channel mapping
#ifndef ADS_NTC_PORT_HOTSPOT
#define ADS_NTC_PORT_HOTSPOT 0 // CH0
#endif
#ifndef ADS_NTC_PORT_CHAMBER
#define ADS_NTC_PORT_CHAMBER 1 // CH1
#endif

// -----------------------------------------------------------------------------
// Global state (client-side)
// -----------------------------------------------------------------------------
//
// g_effectiveMask:
//   The *effective* output mask that reflects what is actually applied on hardware
//   after safety gating (door/host-loss) and after reflecting PWM truth.
//
// g_applyPending / g_pendingMask:
//   T10.1.41 fix: do not apply outputs in callbacks (UART RX context).
//   We only store the new mask and set a flag. loop() performs applyOutputs().
// -----------------------------------------------------------------------------
static uint16_t g_effectiveMask = 0;
static volatile bool g_applyPending = false;
static volatile uint16_t g_pendingMask = 0;

static void heaterPwmEnable(bool enable);
static bool isDoorOpen();

// -------------------------
// Helpers
// -------------------------
//
// bitmask8_to_str():
//   Returns a human-readable 8-bit string for CH7..CH0.
//   Example: mask=0x0003 (bits 0 and 1 set) -> "00000011"
//
// IMPORTANT: The string order is CH7..CH0 (MSB..LSB), not CH0..CH7.
//
static const char *bitmask8_to_str(uint16_t mask) {
    static char buf[9];
    for (int i = 0; i < 8; ++i) {
        const uint8_t bit = 1u << (7 - i); // print CH7..CH0
        buf[i] = (mask & bit) ? '1' : '0';
    }
    buf[8] = '\0';
    return buf;
}

// Re-entrant version that writes into a provided buffer.
static void bitmask8_to_str_r(uint16_t mask, char out[9]) {
    for (int i = 0; i < 8; ++i) {
        const uint8_t bit = 1u << (7 - i);
        out[i] = (mask & bit) ? '1' : '0';
    }
    out[8] = '\0';
}

enum class ClientDiagState : uint8_t {
    DOOR_OPEN = 0,
    HEATER_ON = 1,
    OUTPUTS_ACTIVE = 2,
    IDLE = 3,
};

typedef struct {
    bool fan12V;
    bool fan230V;
    bool fan230V_SLOW;
    bool silica_motor;
    bool heater;
    bool lamp;
    bool door;
    uint8_t running_state; // ClientDiagState
} CLIENT_COMPLETE_STATE;

static ClientDiagState get_diag_state() {
    if (sensor_ntc::is_door_open()) {
        return ClientDiagState::DOOR_OPEN;
    }
    if (heater_io::is_running()) {
        return ClientDiagState::HEATER_ON;
    }
    if ((g_effectiveMask & 0x00FFu) != 0u) {
        return ClientDiagState::OUTPUTS_ACTIVE;
    }
    return ClientDiagState::IDLE;
}

static const char *diag_state_to_str() {
    switch (get_diag_state()) {
    case ClientDiagState::DOOR_OPEN:
        return "DOOR_OPEN";
    case ClientDiagState::HEATER_ON:
        return "HEATER_ON";
    case ClientDiagState::OUTPUTS_ACTIVE:
        return "OUTPUTS_ACTIVE";
    case ClientDiagState::IDLE:
        return "IDLE";
    }
    return "UNKNOWN";
}

static int diag_state_to_int() {
    return static_cast<int>(get_diag_state());
}

// ---------------------------------------------------------------------------
// @brief Return a integer value, which represents all current states of
// GPIO's as integer (bitmask)
//
// ---------------------------------------------------------------------------
static int diag_all_client_state() {
    uint16_t mask = 0;
    mask &= (get_diag_state() == ClientDiagState::DOOR_OPEN ? ~(1u << OUTPUT_BIT_MASK_8BIT::BIT_DOOR) : 0); // bit 5
    return mask;
}

static CLIENT_COMPLETE_STATE build_client_state(bool door_open) {
    CLIENT_COMPLETE_STATE s{};

    const uint16_t m = g_effectiveMask;

    s.fan12V = (m & (1 << 0)) != 0;
    s.fan230V = (m & (1 << 1)) != 0;
    s.fan230V_SLOW = (m & (1 << 2)) != 0;
    s.silica_motor = (m & (1 << 3)) != 0;
    s.heater = heater_io::is_running();
    s.lamp = (m & (1 << 5)) != 0;
    s.door = door_open;
    s.running_state = (uint8_t)diag_state_to_int();

    return s;
}

static void emit_csv_client_state_once_per_second() {
    static uint32_t last = 0;
    const uint32_t now = millis();

    if ((now - last) < 1000) {
        return;
    }
    last = now;

    const bool door_open = sensor_ntc::is_door_open();
    const CLIENT_COMPLETE_STATE s = build_client_state(door_open);

#if defined(CSV_OUT) && (CSV_OUT == 1)
    CSV_LOG_CLIENT_LOGIC(
        s.fan12V ? 1 : 0,
        s.fan230V ? 1 : 0,
        s.fan230V_SLOW ? 1 : 0,
        s.silica_motor ? 1 : 0,
        s.heater ? 1 : 0,
        s.lamp ? 1 : 0,
        s.door ? 1 : 0,
        s.running_state);

#endif
}

static void emit_diagnostic_log_once_per_second() {
    static uint32_t lastMs = 0;
    const uint32_t now = millis();
    if ((now - lastMs) < 1000u) {
        return;
    }
    lastMs = now;

    const sensor_ntc::Sample &s = sensor_ntc::get_sample();
    const bool door_open = sensor_ntc::is_door_open();
    const bool heater_on = heater_io::is_running();

#if defined(CSV_OUT) && (CSV_OUT == 1)
    // const long hot_dC = s.hotValid ? (long)s.hot_dC : -32768L;
    static constexpr int16_t TEMP_INVALID_DC = -32768;
    const long hot_dC = s.hotValid ? (long)s.hot_dC : TEMP_INVALID_DC;
    const long cha_dC = (long)s.cha_dC;

    CSV_LOG_CLIENT_TEMP(
        // Hotspot NTC
        (long)s.rawHotspot,
        (long)s.hot_mV,
        hot_dC,
        // Chamber NTC
        (long)s.rawChamber,
        (long)s.cha_mV,
        cha_dC,
        // Diagnostic state
        (long)diag_state_to_int(),
        heater_on ? 1L : 0L,
        door_open ? 1L : 0L);

#endif

    CLIENT_INFO(
        "[DIAG] Door=%s state=%s mask=0x%04X rawHot=%d hot_mV=%ld hot_valid=%d "
        "Thot=%.2fC (%s) rawCh=%d cha_mV=%ld cha_ohm=%ld Tch=%.2fC heater=%s\n",
        door_open ? "OPEN" : "CLOSED",
        diag_state_to_str(),
        (unsigned)g_effectiveMask,
        (int)s.rawHotspot,
        (long)s.hot_mV,
        s.hotValid ? 1 : 0,
        s.tempHotspotC,
        s.hotValid ? "valid" : "nanC",
        (int)s.rawChamber,
        (long)s.cha_mV,
        (long)s.cha_ohm,
        s.tempChamberC,
        heater_on ? "ON" : "OFF");
}

// parse_hex4_at():
//   Parse exactly 4 hex chars at a given position in an Arduino String.
//   Used to extract MMMM from outgoing ACK/STATUS lines for debug annotation.
static bool parse_hex4_at(const String &s, int start, uint16_t &out) {
    if (start < 0 || start + 4 > (int)s.length()) {
        return false;
    }
    uint16_t v = 0;
    for (int i = 0; i < 4; ++i) {
        char c = s[start + i];
        uint8_t n;
        if (c >= '0' && c <= '9') {
            n = (uint8_t)(c - '0');
        } else if (c >= 'A' && c <= 'F') {
            n = (uint8_t)(10 + (c - 'A'));
        } else if (c >= 'a' && c <= 'f') {
            n = (uint8_t)(10 + (c - 'a'));
        } else {
            return false;
        }
        v = (uint16_t)((v << 4) | n);
    }
    out = v;
    return true;
}

// -----------------------------------------------------------------------------
// Door safety gating (authoritative on client)
// -----------------------------------------------------------------------------
// Policy:
// - Door is input-only telemetry and must never be treated as an output.
// - If door is open, force a safe state:
//     - HEATER OFF
//     - SILICA MOTOR OFF
//     - FAN230V OFF (explicit requirement)
//   FAN230V_SLOW is intentionally not forced either way.
//
// This function converts a "requested mask" into a "safe effective mask".
// -----------------------------------------------------------------------------
static inline uint16_t applyDoorSafetyGating(uint16_t mask, bool doorOpen) {
    // Never treat DOOR as output (input-only telemetry bit)
    mask &= ~(1u << OUTPUT_BIT_MASK_8BIT::BIT_DOOR);

    if (doorOpen) {
        mask &= ~(1u << OUTPUT_BIT_MASK_8BIT::BIT_HEATER);
        mask &= ~(1u << OUTPUT_BIT_MASK_8BIT::BIT_SILICA_MOTOR);
        mask &= ~(1u << OUTPUT_BIT_MASK_8BIT::BIT_FAN230V); // forced OFF when door open
        // FAN230V_SLOW: intentionally not forced either way
    }
    return mask;
}

/**
 * @brief Read the logical door state from the door sensor input.
 *
 * Reads the door GPIO and converts the electrical level into a logical
 * OPEN / CLOSED state based on the configured polarity
 * (DOOR_OPEN_IS_HIGH).
 *
 * Notes:
 * - The door signal is input-only telemetry and is client-authoritative.
 * - The returned state is used for safety gating (heater, motor, fan).
 * - A log entry is emitted only on state changes to avoid log spam.
 *
 * @return true  Door is OPEN
 * @return false Door is CLOSED
 */
static bool isDoorOpen() {
    const bool now = sensor_ntc::is_door_open();

    static bool last = false;
    if (now != last) {
        CLIENT_INFO("[DOOR] state=%s\n", now ? "OPEN" : "CLOSED");
        last = now;
    }
    return now;
}

// safety_check_overtemp():
// Absolute safety: if tempCur >= CLIENT_ABS_MAX_TEMP_C, then:
// - latch g_heater_overtemp (log once)
// - hard-disable heater PWM (HARD OFF)
// - print an error message
//
// This is client-authoritative and does not depend on the host.
static void safety_check_overtemp(float tempCur) {
    if (tempCur >= CLIENT_ABS_MAX_TEMP_C) {
        if (!g_heater_overtemp) {
            g_heater_overtemp = true;

            heaterPwmEnable(false); // HARD OFF
            CLIENT_ERR("[SAFETY] ABS OVER-TEMP! cur=%.1f >= %.1f -> HEATER OFF\n",
                       tempCur, CLIENT_ABS_MAX_TEMP_C);
        }
    }
}

/**
 * @brief Apply a requested output bitmask to the physical hardware outputs.
 *
 * This function is the **single authoritative point** where logical output
 * requests are translated into actual GPIO and PWM actions on the ESP32.
 *
 * Key responsibilities:
 * - Enforce door safety rules before touching any hardware.
 * - Drive normal outputs via digital GPIO.
 * - Drive the heater exclusively via PWM (LEDC), never via digitalWrite.
 * - Maintain a shadow mask (`g_effectiveMask`) that reflects the *real*
 *   hardware state after safety gating and PWM truth.
 *
 * Important design rules:
 * - This function must only be called from the main loop(), never from
 *   UART RX callbacks or interrupt context.
 * - Door state is input-only telemetry and must never be driven as an output.
 *
 * @param requestedMask
 *        The logical output mask requested by the HOST (bits CH0..CH7).
 *        This mask may be modified by safety logic before being applied.
 */
static void applyOutputs(uint16_t requestedMask) {
    // Read current door state (client-authoritative safety input)
    const bool doorOpen = isDoorOpen();

    // Apply door-based safety gating:
    // - Clears DOOR bit (input-only)
    // - Forces HEATER, MOTOR and FAN230V OFF when door is open
    const uint16_t eff = applyDoorSafetyGating(requestedMask, doorOpen);

    // Apply the effective (safe) mask to physical outputs
    for (int i = 0; i < 8; ++i) {

        // Skip DOOR bit entirely:
        // DOOR is telemetry-only and must never be driven as an output.
        if (i == OUTPUT_BIT_MASK_8BIT::BIT_DOOR) {
            continue;
        }

        // Determine desired ON/OFF state for this channel
        const bool on = (eff & (1u << i)) != 0;

        // Heater handling:
        // The heater is controlled exclusively via PWM.
        // No direct GPIO writes are allowed on the heater pin.
        if (i == HEATER_BIT_INDEX) {
            heaterPwmEnable(on);
            continue;
        }

        // All other channels (motor, fans, lamp, etc.)
        // are plain digital GPIO outputs.
        digitalWrite(OUT_PINS[i], on ? HIGH : LOW);
    }

    // Store the effective mask after safety gating
    g_effectiveMask = eff;

    // Reflect the *actual* PWM state back into the mask:
    // This guarantees that STATUS reports what is really happening on hardware,
    // not just what was logically requested.
    if (heater_io::is_running()) {
        g_effectiveMask |= (1u << OUTPUT_BIT_MASK_8BIT::BIT_HEATER);
    } else {
        g_effectiveMask &= ~(1u << OUTPUT_BIT_MASK_8BIT::BIT_HEATER);
    }
}

// -----------------------------------------------------------------------------
// Temperature reading
// -----------------------------------------------------------------------------
//
// readTemperatureRawValue(mode):
// - mode=0: MAX6675 (if enabled). Returns 0.25°C steps (°C * 4), rounded.
// - mode=1: Internal NTC via ADS1115 (if enabled). Returns ADS raw code.
//
// IMPORTANT (schematic requirement):
// - You stated: internal NTC is connected to ADS1115 AIN0.
// - The code currently reads ads.readADC_SingleEnded(1).
//   That is ADS channel 1 (AIN1). If wiring is AIN0, channel should be 0.
//   Keep this aligned with your hardware.
//
int16_t readTemperatureRawValue(uint8_t mode = 1) {
    int16_t raw = 0;
    switch (mode) {
    case 0:
#if ENABLE_MAX6675
        const float c = g_thermocouple.readCelsius();
        // MAX6675 returns NAN if not connected / invalid; handle safely.
        if (isnan(c) || c < -100.0f || c > 1000.0f) {
            return 0;
        }

        // Convert to 0.25°C steps (°C * 4), rounded
        raw = (int16_t)lroundf(c * 4.0f);
#endif
        break;

    case 1:
    default:
#if ENABLE_INTERNAL_NTC
        // ADS1115 raw reading (single-ended).
        // NOTE: Ensure channel matches the actual NTC wiring (AIN0 vs AIN1).
        raw = ads.readADC_SingleEnded(ADS_NTC_PORT);

#endif
        break;
    }
    return raw;
}

static inline int32_t read_adcX_mv(uint8_t ch) {
    if (!ntc_available) {
        return -1;
    }
    if (ch < 0) {
        ch = 0;
    }
    if (ch > 3) {
        ch = 3;
    }

    const int32_t raw = ads.readADC_SingleEnded(ch); // 0..32767

    // GAIN_TWOTHIRDS: 0.1875 mV/LSB = 3/16 mV per count
    // Rounded: (raw*3 + 8) / 16
    return (raw * 3 + 8) / 16;
}

static int16_t readTemperatureValue() {
#if ENABLE_MAX6675
    // value in 0.25° steps
    const float c = g_thermocouple.readCelsius();
    (void)c;

    // Round to nearest 0.25°C step
    const int32_t raw = readTemperatureRawValue(0);

    if (raw < INT16_MIN) {
        return INT16_MIN;
    }
    if (raw > INT16_MAX) {
        return INT16_MAX;
    }

    return (int16_t)raw;
#endif

#if ENABLE_INTERNAL_NTC
    // uint16_t r = readTemperatureRawValue(1);
    int32_t r = read_adcX_mv(ADS_NTC_PORT);
    // // Convert ADS raw code to voltage (depends on configured gain)
    // // float v = ads.computeVolts(r);
    // // Convert voltage to temperature using NTC lookup/fit (verify unit!)

    int32_t celsius = ntc_adc_to_temp_dC(r);
    CLIENT_DBG("[I2C] DegFloat=%3.1fC (int32=%d) (raw=%dmV) \n", (celsius / 10.0f), celsius, r);
    return (int16_t)celsius;
#endif
}

/**
 * @brief Scan the I2C bus and report detected devices.
 *
 * Performs a simple address scan on the active I2C bus (1..126) and logs
 * all responding devices. Intended for startup diagnostics and wiring checks.
 *
 * Notes:
 * - Used mainly to verify presence of ADS1115 and other I2C peripherals.
 * - Does not modify bus configuration or device state.
 * - Output is informational only (no functional dependency).
 */
void i2cScan() {
    CLIENT_INFO("---------------------------\n");
    CLIENT_INFO("I2C scan started...\n");
    CLIENT_INFO("---------------------------\n");

    uint8_t count = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();

        if (err == 0) {
            CLIENT_INFO("   Found device at 0x%s\n", String(addr, HEX));
            if (addr < 16) {
                CLIENT_INFO("   addr < 16\n");
            }
            count++;
        }
    }

    if (count == 0) {
        CLIENT_ERR("   No I2C devices found!\n");
    } else {
        CLIENT_INFO("   Total devices found: %d\n", count);
    }
    CLIENT_INFO("---------------------------\n");
    CLIENT_INFO("I2C scan done\n\n");
    CLIENT_INFO("---------------------------\n");
}

// Fill STATUS via callback (raw units only; no assumptions about conversion)
// T10.1.28 Door bugfix
static void fillStatusCallback(ProtocolStatus &st) {
    st.outputsMask = g_effectiveMask;

    const bool door_open = sensor_ntc::is_door_open();
    if (door_open) {
        st.outputsMask |= (1u << OUTPUT_BIT_MASK_8BIT::BIT_DOOR);
    } else {
        st.outputsMask &= ~(1u << OUTPUT_BIT_MASK_8BIT::BIT_DOOR);
    }

    sensor_ntc::sample_temperatures();
    const sensor_ntc::Sample &sample = sensor_ntc::get_sample();

    st.adcRaw[0] = sample.rawHotspot;
    st.adcRaw[1] = sample.rawChamber;
    st.adcRaw[2] = 0;
    st.adcRaw[3] = 0;

    st.tempHotspot_dC = sample.hotValid ? sample.hot_dC : ntc::TEMP_INVALID_DC;
    st.tempChamber_dC =
        (sample.cha_dC == ntc::TEMP_INVALID_DC) ? ntc::TEMP_INVALID_DC : sample.cha_dC;
}

// Apply outputs when mask changes
// T10.1.41: Do NOT apply outputs inside the callback (RX context).
// Only mark pending; apply in main loop() deterministically.
static void outputsChangedCallback(uint16_t newMask) {
    g_pendingMask = newMask;
    g_applyPending = true;

    // Debug (USB serial only)
    CLIENT_RAW("[outputsChangedCallback] pending outputsMask=0x%04X; BitMask: %s\n",
               newMask, bitmask8_to_str(newMask));
}

// Debug outgoing frames (optional)
// Adds a human-readable BitMask for known frames carrying a 16-bit mask.
static void txLineCallback(const String &line, const String &dir) {
    uint16_t mask = 0;
    bool hasMask = false;

    // Expected formats:
    // - C;ACK;SET;MMMM
    // - C;ACK;UPD;MMMM
    // - C;ACK;TOG;MMMM
    // - C;STATUS;MMMM;A0;A1;A2;A3;TEMP

    const int idxAckSet = line.indexOf("C;ACK;SET;");
    if (idxAckSet >= 0) {
        const int hexPos = idxAckSet + (int)strlen("C;ACK;SET;");
        hasMask = parse_hex4_at(line, hexPos, mask);
    }

    const int idxAckUpd = (!hasMask) ? line.indexOf("C;ACK;UPD;") : -1;
    if (idxAckUpd >= 0) {
        const int hexPos = idxAckUpd + (int)strlen("C;ACK;UPD;");
        hasMask = parse_hex4_at(line, hexPos, mask);
    }

    const int idxAckTog = (!hasMask) ? line.indexOf("C;ACK;TOG;") : -1;
    if (idxAckTog >= 0) {
        const int hexPos = idxAckTog + (int)strlen("C;ACK;TOG;");
        hasMask = parse_hex4_at(line, hexPos, mask);
    }

    const int idxStatus = (!hasMask) ? line.indexOf("C;STATUS;") : -1;
    if (idxStatus >= 0) {
        const int hexPos = idxStatus + (int)strlen("C;STATUS;");
        hasMask = parse_hex4_at(line, hexPos, mask);
    }

#ifdef CLIENTNOPONGLOG
    // Optional log hygiene: suppress PONG logs
    const int idxPONG = line.indexOf("C;PONG");
    if (idxPONG >= 0) {
        return;
    }
#endif
    if (hasMask) {
        CLIENT_RAW("[CLIENT] [%s]: BitMask: (%s) => %s;",
                   dir.c_str(), bitmask8_to_str(mask), line.c_str());
    } else {
        if (dir.length() > 2) {
            CLIENT_RAW("[CLIENT] [%s]: %s", dir.c_str(), line.c_str());
        }
    }
}

/**
 * @brief Non-blocking heartbeat LED pulser (alive indicator).
 *
 * This function generates a periodic short LED pulse (blip) to indicate that:
 * - the firmware is running,
 * - the main loop is alive,
 * - timing (millis()) is advancing normally.
 *
 * Behavior:
 * - Every PERIOD_MS (default: 5000 ms), the LED turns ON for PULSE_MS (default: 350 ms),
 *   then turns OFF for the remainder of the period.
 * - The function is fully non-blocking: it uses timestamps (millis()) and does not delay().
 *
 * Log hygiene:
 * - To avoid constant log spam (e.g. printing '.' each heartbeat),
 *   this implementation only prints an occasional counter marker:
 *   after HB_LINE_BREAK pulses, it prints a line break marker "(00001)", "(00002)", ...
 *
 * LED polarity:
 * - Some boards have an "active-low" built-in LED (ON when GPIO=LOW).
 * - LED_ACTIVE_LOW controls whether the logic is inverted.
 *
 * Usage:
 * - Typically called from a periodic callback or from loop() at high frequency.
 * - Safe to call very often; it will only toggle state when the next timestamp is reached.
 */
void heartbeatLED_update() {
    // Tracks whether LED is currently ON (true) or OFF (false).
    static bool ledOn = false;

    // Next scheduled time (millis) when the LED state should toggle.
    static uint32_t nextToggleMs = 0;

    // Pulse counter within a "line". Used for controlled logging.
    static uint8_t hbCount = 0;

    // Counts how many full lines of HB_LINE_BREAK pulses have occurred.
    static uint32_t lastHBCount = 0;

    // Number of pulses before printing a compact marker in the log.
    const uint8_t HB_LINE_BREAK = 80;

    // Total heartbeat period. 5000 ms => 0.2 Hz pulse frequency.
    const uint32_t PERIOD_MS = 5000; // 5 seconds (0.2 Hz)

    // Duration the LED stays ON within each period (a short visible blip).
    const uint32_t PULSE_MS = 350; // LED ON time

    // Many built-in LEDs are active-low; keep configurable.
    // - false: ON  => GPIO HIGH
    // - true:  ON  => GPIO LOW
    const bool LED_ACTIVE_LOW = false;

    // Small helper lambda to write LED state with correct polarity.
    auto led_write = [&](bool on) {
        if (LED_ACTIVE_LOW) {
            digitalWrite(PIN_BOARD_LED, on ? LOW : HIGH);
        } else {
            digitalWrite(PIN_BOARD_LED, on ? HIGH : LOW);
        }
    };

    // Current time in ms since boot.
    uint32_t now = millis();

    // Turn LED ON when:
    // - it is currently OFF
    // - and the scheduled toggle time has been reached/passed.
    //
    // Note: (int32_t)(now - nextToggleMs) >= 0 is a standard rollover-safe check.
    if (!ledOn && (int32_t)(now - nextToggleMs) >= 0) {
        led_write(true);
        ledOn = true;

        // Schedule LED OFF after the ON pulse duration.
        nextToggleMs = now + PULSE_MS;

        // Controlled logging:
        // After HB_LINE_BREAK pulses, print a compact counter marker and newline.
        if (++hbCount >= HB_LINE_BREAK) {
            hbCount = 0;
            lastHBCount++;
            CLIENT_RAW(" (%05lu)\n", (unsigned long)lastHBCount);
        } else {
            // Intentionally disabled for log hygiene (avoid dot spam).
            // RAW(".");
        }
    }
    // Turn LED OFF when:
    // - it is currently ON
    // - and the scheduled toggle time has been reached/passed.
    else if (ledOn && (int32_t)(now - nextToggleMs) >= 0) {
        led_write(false);
        ledOn = false;

        // Schedule the next ON event at the end of the full period.
        // This creates: ON for PULSE_MS, then OFF for (PERIOD_MS - PULSE_MS).
        nextToggleMs = now + (PERIOD_MS - PULSE_MS);
    }
}

static void printStartupInfo() {
    Serial.println();
    CLIENT_INFO("----------------------------------------------\n");
    CLIENT_INFO("- ESP32-WROOM CLIENT Hardware connector.   ---\n");
    CLIENT_INFO("----------------------------------------------\n");
    CLIENT_INFO("%s\n", CLIENT_VERSION_NAME);
    CLIENT_INFO("%s\n\n", CLIENT_VERSION_DATE);

    CLIENT_INFO("UART link: Serial2 (RX2=GPIO16, TX2=GPIO17)\n");
    CLIENT_INFO("Supported HOST frames:\n");
    CLIENT_INFO("  H;SET;XXXX\n");
    CLIENT_INFO("  H;UPD;SSSS;CCCC\n");
    CLIENT_INFO("  H;TOG;TTTT\n");
    CLIENT_INFO("  H;GET;STATUS\n");
    CLIENT_INFO("  H;PING\n");
    CLIENT_INFO("\n");
    CLIENT_INFO("Outputs (CH0..CH7) are mapped to bits 0..7 of outputsMask.\n");
    CLIENT_INFO("ADC0 pin: %d\n", PIN_ADC0);
#if ENABLE_MAX6675
    CLIENT_INFO("MAX6675 enabled. Pins: SCK= %d, CS=%d, SO=%d\n",
                (int)MAX6675_SCK_PIN,
                (int)MAX6675_CS_PIN,
                (int)MAX6675_SO_PIN);
#else
    Serial.println("MAX6675 disabled.");
#endif
    Serial.println();
}

// static uint32_t heaterDutyFromPercent(int percent) {
//     // Convert duty percent [0..100] into PWM duty register value.
//     const uint32_t maxDuty = (1u << HEATER_PWM_RES_BITS) - 1u;
//     if (percent <= 0) {
//         return 0;
//     }
//     if (percent >= 100) {
//         return maxDuty;
//     }
//     // Round to nearest
//     return (maxDuty * (uint32_t)percent + 50u) / 100u;
// }

static void heaterPwmEnable(bool enable) {
    (void)heater_io::set_enabled(enable, heater_io::kDefaultDutyPercent);
}

//----------------------------------------------------------------------------
// WiFi & UDP
//----------------------------------------------------------------------------
static void udp_diag_print() {
    Serial.printf(
        "[UDP/DIAG] freeHeap=%u internal=%u largestInt=%u psram=%u wifi=%d rssi=%d\n",
        (unsigned)ESP.getFreeHeap(),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        (int)WiFi.status(),
        (int)WiFi.RSSI());
}

//----------------------------------------------------------------------------
// setup
//----------------------------------------------------------------------------

#ifdef T13_NTC_CHAMBER_TEST
static uint32_t g_lastNtcTestMs = 0;

static void t13_ntc_chamber_test_tick() {
    const uint32_t now = millis();
    if (now - g_lastNtcTestMs < 1000) {
        return;
    }
    g_lastNtcTestMs = now;

    const int16_t rawCh = (int16_t)ads.readADC_SingleEnded(ADS_NTC_PORT_CHAMBER);

    const int16_t chamber_dC = t13::calc_temp_from_ads_raw_dC(
        rawCh,
        t13::kNtc10k_Chamber_Table,
        t13::kNtc10k_Chamber_TableCount,
        t13::kNtcTableMode,
        t13::chamber::NTC_VREF_MV,
        t13::chamber::NTC_R_FIXED_OHM,
        t13::chamber::NTC_TO_GND);

    CLIENT_INFO("[T13][NTC_TEST] CH1 raw=%d chamber_dC=%d (%.1fC)\n",
                (int)rawCh, (int)chamber_dC, (float)chamber_dC / 10.0f);
}
#endif

void setup() {
    Serial.begin(115200);
    delay(2000);

#if defined(WIFI_LOGGING_ENABLE) && (WIFI_LOGGING_ENABLE == 1)
    const bool ok = udp::begin("CLIENT");
    if (ok) {
        udp::send_cstr("[UDP] selftest packet 1 (CLIENT)\n");
        delay(50);
        udp::send_cstr("[UDP] selftest packet 2 (CLIENT)\n");
        delay(50);
        udp::send_cstr("[UDP] selftest packet 3 (CLIENT)\n");
    }

    else {
        ERROR("UDP can't establish WIFI_LOGGING_ENABLE !!!!");
    }
#endif

    printStartupInfo();

    // Built-in LED pin
    pinMode(PIN_BOARD_LED, OUTPUT);
    digitalWrite(PIN_BOARD_LED, LOW);

    // Configure outputs + door input (door uses INPUT_PULLUP).
    for (int i = 0; i < 8; ++i) {
        if (OUT_PINS[i] == OVEN_DOOR_SENSOR) {
            pinMode(OUT_PINS[i], INPUT_PULLUP);
            CLIENT_INFO("[IO] INPUT_PULLUP init: GPIO=%d (DOOR)\n", OUT_PINS[i]);
        } else {
            pinMode(OUT_PINS[i], OUTPUT);
            digitalWrite(OUT_PINS[i], LOW);
            CLIENT_INFO("[IO] OUTPUT init: GPIO=%d\n", OUT_PINS[i]);
        }
    }

    heater_io::init_off();

    // ESP32 internal ADC pin (input-only), configured as INPUT for completeness.
    pinMode(PIN_ADC0, INPUT);

// ADS1x15
#if ENABLE_INTERNAL_NTC
    sensor_ntc::init_i2c_and_ads();
#endif

    // Initialize ClientComm UART (routes RX2/TX2 inside ClientComm as required)
    clientComm.begin(LINK_BAUDRATE);

    // Register callbacks
    clientComm.setOutputsChangedCallback(outputsChangedCallback);
    clientComm.setFillStatusCallback(fillStatusCallback);
    clientComm.setTxLineCallback(txLineCallback);
    clientComm.setHeartBeatCallback(heartbeatLED_update);

    // Ensure outputs are at known safe state
    outputsChangedCallback(0x0000);

    // Door init log (after pin configuration)
    // const bool door_open = (digitalRead(OVEN_DOOR_SENSOR) != 0);

    sensor_ntc::init_door();
}

//----------------------------------------------------------------------------
// loop
//----------------------------------------------------------------------------
void loop() {
#ifdef T13_NTC_CHAMBER_TEST
    // Standalone chamber NTC bring-up (no Host required)
    t13_ntc_chamber_test_tick();
    return;
#endif

    // Run UART protocol engine (RX/TX, parser, watchdog)
    clientComm.loop();

    // T10.1.41: Apply outputs deterministically from main loop
    if (g_applyPending) {
        const uint16_t m = g_pendingMask;
        g_applyPending = false;
        applyOutputs(m);
        CLIENT_RAW("[T10.1.41] applyOutputs() from loop: mask=0x%04X\n", m);
    }

    // ---------------------------------------------------------------------
    // SAFETY T10.1.40: HOST watchdog HARD-KILL
    //
    // If host is silent beyond CLIENT_HOST_TIMEOUT_MS, ClientComm forces
    // outputsMask=0x0000. We must ensure heater PWM is physically OFF and
    // motor is LOW, and reflect that in g_effectiveMask.
    // ---------------------------------------------------------------------
    if (clientComm.hasNewOutputsMask()) {
        const uint16_t m = clientComm.getOutputsMask();

        if (m == 0x0000) {
            // Absolute safe state
            if (heater_io::is_running()) {
                CLIENT_ERR("[SAFETY] HOST LOST -> HARD HEATER OFF\n");
                heaterPwmEnable(false);
            }

            // Kill motor explicitly as well
            digitalWrite(OUT_PINS[MOTOR_BIT_INDEX], LOW);

            // Keep shadow mask consistent
            g_effectiveMask = 0x0000;
        }

        clientComm.clearNewOutputsMaskFlag();
    }

    // Door transition watcher: on door OPEN, force safe outputs immediately.
    static bool lastDoorOpen = false;
    const bool doorOpenNow = isDoorOpen();

    if (doorOpenNow != lastDoorOpen) {
        lastDoorOpen = doorOpenNow;

        if (doorOpenNow) {
            uint16_t before = g_effectiveMask;

            // Ensure "before" reflects PWM truth for meaningful logging
            if (heater_io::is_running()) {
                before |= (1u << OUTPUT_BIT_MASK_8BIT::BIT_HEATER);
            } else {
                before &= ~(1u << OUTPUT_BIT_MASK_8BIT::BIT_HEATER);
            }

            // Gate the current effective state into a safe state for "door open"
            const uint16_t after = applyDoorSafetyGating(before, true);

            const bool motorPinHigh = (digitalRead(OUT_PINS[MOTOR_BIT_INDEX]) == HIGH);
            const bool needKill = heater_io::is_running() || motorPinHigh;

            if (after != before) {
                char b0[9], b1[9];
                bitmask8_to_str_r(before, b0);
                bitmask8_to_str_r(after, b1);

                CLIENT_WARN("[DOOR] OPEN -> forcing safe outputs: %s -> %s\n", b0, b1);
                applyOutputs(after);
            } else if (needKill) {
                CLIENT_WARN("[DOOR] OPEN -> forcing safe outputs (PWM/MOTOR still active)\n");

                heaterPwmEnable(false);
                digitalWrite(OUT_PINS[MOTOR_BIT_INDEX], LOW);

                // Keep shadow consistent
                g_effectiveMask &= ~(1u << OUTPUT_BIT_MASK_8BIT::BIT_HEATER);
                g_effectiveMask &= ~(1u << OUTPUT_BIT_MASK_8BIT::BIT_SILICA_MOTOR);
            }
        }
    }

    emit_diagnostic_log_once_per_second();
    emit_csv_client_state_once_per_second();
}

// EOF
