#include "HostComm.h"
#include "oven_utils.h"

// #warning "HOST BUILD: compiling HostComm.cpp"

/**
 * @brief Construct a new HostComm communication handler.
 *
 * History:
 * 0.2 2026-01-07 stabilere, robuste Version, ausführlich über 16-Testcases überprüft
 * 0.1 2025-12-10 initiale Version
 *
 *
 *
 * This class is responsible for:
 * - Reading UART data from the client (ESP32-WROOM)
 * - Assembling complete text lines terminated by '\n'
 * - Parsing protocol messages using ProtocolCodec
 * - Maintaining a shadow copy of the remote status (ProtocolStatus)
 * - Exposing simple flags and getters for the application/UI layer
 *
 * It does NOT:
 * - Block the CPU (loop() is fully non-blocking)
 * - Know anything about LVGL or the display
 * - Directly touch any GPIO or hardware except the UART interface
 *
 * @param serial Reference to the HardwareSerial instance used for the host-client link.
 *               Example: Serial2 on ESP32-S3.
 */
HostComm::HostComm(HardwareSerial &serial)
    : _serial(serial),
      _rx(0),
      _tx(0),
      _rxBuffer(),
      _localOutputsMask(0) {
    // Initialize remote status with known defaults
    _remoteStatus.outputsMask = 0;
    _remoteStatus.adcRaw[0] = 0;
    _remoteStatus.adcRaw[1] = 0;
    _remoteStatus.adcRaw[2] = 0;
    _remoteStatus.adcRaw[3] = 0;
    _remoteStatus.tempRaw = 0;

    // Communication / state flags
    _newStatus = false;    // becomes true when a valid STATUS frame is received
    _lastSetAcked = false; // true when the last SET has been acknowledged by the client
    _commError = false;    // set if parsing or protocol errors occur
    _lastPong = false;
}

/**
 * @brief Initialize the UART interface.
 *
 * This must be called once from setup() before using any other methods.
 * It does not configure any pins here; pin routing (e.g. via Serial2.begin with specific pins)
 * should be handled outside, or by the platform's default configuration.
 *
 * @param baudrate UART baud rate (must match the client side).
 */
void HostComm::begin(uint32_t baudrate, uint8_t rx, uint8_t tx) {
    _rx = rx;
    _tx = tx;
    _serial.begin(baudrate, SERIAL_8N1, _rx, _tx);
}

/**
 * @brief Periodic processing function.
 *
 * This function should be called very frequently from the main loop(), e.g.:
 *   void loop() {
 *       hostComm.loop();
 *       lv_timer_handler(); // or your LVGL integration
 *   }
 *
 * It:
 * - Reads all available bytes from the UART receive buffer
 * - Accumulates characters into _rxBuffer until a '\n' is encountered
 * - Strips '\r' and treats '\n' as end-of-line marker
 * - For each complete line, calls handleIncomingLine()
 *
 * The implementation is fully non-blocking and uses no delays,
 * so it is safe to run alongside LVGL or any real-time GUI loop.
 */
void HostComm::loop() {
    while (_serial.available() > 0) {
        const char c = static_cast<char>(_serial.read());
        handleRxByte(c);
    }
}

/**
 * @brief Set the desired outputsMask and send a SET command to the client.
 *
 * The outputsMask is a 16-bit bitfield:
 * - Bit 0..11: digital channels CH0..CH11
 * - Bit 12..15: digital channels for CH12..CH15 (which also carry analog info)
 *
 * This function:
 * - Updates the local view of what the host *wants* (localOutputsMask)
 * - Sends the SET frame immediately over UART
 * - Resets the "lastSetAcked" flag to false, waiting for client's ACK
 *
 * @param mask 16-bit mask representing the desired digital states.
 */
void HostComm::setOutputsMask(uint16_t mask) {
    _localOutputsMask = mask;
    _lastSetAcked = false; // wait for a fresh ACK from the client

    // Build protocol message: H;SET;<mask>\r\n
    String msg = ProtocolCodec::buildHostSet(mask);
    _serial.print(msg);
}

/**
 * @brief Request a STATUS frame from the client.
 *
 * This sends:
 *   H;GET;STATUS\r\n
 *
 * The client should respond with:
 *   C;STATUS;<mask>;<a0>;<a1>;<a2>;<a3>;<temp>\r\n
 *
 * After reception and successful parsing, hasNewStatus() will become true
 * and getRemoteStatus() will return updated values.
 */
void HostComm::requestStatus() {
    _newStatus = false; // reset stale flag
    String msg = ProtocolCodec::buildHostGetStatus();
    _serial.print(msg);
}

/**
 * @brief Send a PING command to the client.
 *
 * This sends:
 *   H;PING\r\n
 *
 * The client responds with:
 *   C;PONG\r\n
 *
 * This is optional but can be used to check if the link is alive.
 */
void HostComm::sendPing() {
    String msg = ProtocolCodec::buildHostPing();
    static uint32_t n = 0;
    ++n;
    HOST_DBG("[HostComm] TX(#%lu): %s", n, msg.c_str()); // contains \r\n already
    // HEX dump (damit wir 100% sehen was wirklich gesendet wird)
    HOST_RAW("[HostComm] TX HEX:");
    for (size_t i = 0; i < msg.length(); ++i) {
        HOST_RAW(" %02x", (uint8_t)msg[i]);
    }
    HOST_RAW("\n");
    _serial.print(msg);
    _serial.flush(); // for debugging only
}

/**
 * @brief Get the last mask that was sent by the host (local desired state).
 *
 * This reflects what your UI or logic *requested*, not necessarily what the
 * client currently uses (for that, check getRemoteOutputsMask()).
 *
 * @return 16-bit local outputs mask.
 */
uint16_t HostComm::getLocalOutputsMask() const {
    return _localOutputsMask;
}

/**
 * @brief Get the remote outputs mask from the last received STATUS or ACK.
 *
 * This is updated when:
 * - A valid C;STATUS frame is received
 * - A valid C;ACK;SET frame is received (we sync outputsMask there)
 *
 * @return 16-bit outputs mask as reported by the client.
 */
uint16_t HostComm::getRemoteOutputsMask() const {
    return _remoteStatus.outputsMask;
}

/**
 * @brief Get a const reference to the last known remote status structure.
 *
 * The ProtocolStatus struct contains:
 * - outputsMask: digital state of all 16 channels
 * - adcRaw[0..3]: raw ADC values for CH12..CH15
 * - tempRaw: temperature in 0.25°C units
 *
 * Note: you should only read this after checking hasNewStatus()
 * and/or after knowing a STATUS frame was recently received.
 *
 * @return const ProtocolStatus& reference.
 */
const ProtocolStatus &HostComm::getRemoteStatus() const {
    return _remoteStatus;
}

/**
 * @brief Check whether a new STATUS frame has been received.
 *
 * This flag is set to true when:
 * - A valid C;STATUS;... message is received and parsed.
 *
 * Your application can use this flag to trigger UI updates,
 * processing, logging, etc.
 *
 * @return true if new status is available.
 * @return false otherwise.
 */
bool HostComm::hasNewStatus() const {
    return _newStatus;
}

/**
 * @brief Clear the "new status" flag after processing the data.
 */
void HostComm::clearNewStatusFlag() {
    _newStatus = false;
}

/**
 * @brief Check whether the last SET command has been acknowledged by the client.
 *
 * This flag becomes true when:
 * - A valid C;ACK;SET;<mask> message is received.
 *
 * This allows your application to verify that the requested outputs
 * have been accepted and applied on the client.
 *
 * @return true if the last SET has been ACKed.
 * @return false otherwise.
 */
bool HostComm::lastSetAcked() const {
    return _lastSetAcked;
}

/**
 * @brief Clear the "last SET acknowledged" flag.
 *
 * Use this if you want to detect a new ACK event after another SET.
 */
void HostComm::clearLastSetAckFlag() {
    _lastSetAcked = false;
}

/**
 * @brief Check whether a communication or parsing error has been detected.
 *
 * This flag is set when:
 * - ProtocolCodec::parseLine(...) returns false (invalid format)
 * - An unexpected message type is received on the host side
 *
 * It does not automatically reset; your code must call clearCommErrorFlag()
 * after handling/logging the error.
 *
 * @return true if an error was detected.
 * @return false otherwise.
 */
bool HostComm::hasCommError() const {
    return _commError;
}

/**
 * @brief Clear the communication error flag.
 */
void HostComm::clearCommErrorFlag() {
    _commError = false;
}

/**
 * @brief Handle one complete, CR/LF-stripped protocol line.
 *
 * This function:
 * - Uses ProtocolCodec::parseLine to decode the message.
 * - Updates flags and remote status according to the message type.
 *
 * It is intentionally kept small and focused on state handling;
 * all string parsing and formatting is delegated to ProtocolCodec.
 *
 * @param line A single complete protocol line, without trailing \r or \n.
// //  */
void HostComm::handleIncomingLine(const String &line) {
    ProtocolMessageType type;
    ProtocolStatus statusTmp;
    uint16_t mask = 0;
    int errorCode = 0;
    uint16_t maskB = 0;
    uint16_t maskC = 0;

    const bool ok = ProtocolCodec::parseLine(line, type, statusTmp, mask, errorCode, maskB, maskC);
    if (!ok) {
        _parseFailCount++;
        _lastBadLine = line;
        HOST_WARN("[HostComm] parse failed for line='%s' (failCount=%lu)\n",
                  line.c_str(), (unsigned long)_parseFailCount);

        // IMPORTANT:
        // Before we have a stable link, ignore junk (boot noise, partial frames, etc.)
        // After sync, treat parse failures as a comm error.
        if (_linkSynced) {
            _commError = true;
        }
        return;
    }

    switch (type) {
    case ProtocolMessageType::ClientAckSet:
        HOST_DBG("ACK SET received, mask=0x%04X (%10s)\n", mask, oven_outputs_mask_to_str(mask));
        _lastSetAcked = true;
        _remoteStatus.outputsMask = mask;
        //        _lastRxAnyMs = millis();
        break;

    case ProtocolMessageType::ClientAckUpd:
        HOST_DBG("ACK UPD received, mask=0x%04X (%10s)\n", mask, oven_outputs_mask_to_str(mask));
        _remoteStatus.outputsMask = mask;
        _lastUpdAcked = true;
        // _lastRxAnyMs = millis();
        break;

    case ProtocolMessageType::ClientAckTog:
        HOST_DBG("ACK TOG received, mask=0x%04X (%10s)\n", mask, oven_outputs_mask_to_str(mask));
        _remoteStatus.outputsMask = mask;
        // _lastTogAcked = true;
        break;

    case ProtocolMessageType::ClientErrSet:
        HOST_ERR("ERR SET received, code=%d\n", errorCode);
        _commError = true; // THIS is a real protocol-level error
        // _lastRxAnyMs = millis();
        break;

    case ProtocolMessageType::ClientStatus:
        _remoteStatus = statusTmp;
        _newStatus = true;
        _lastStatusMs = millis();
        // _lastRxAnyMs = millis();
        HOST_DBG("STATUS received, mask=0x%04X (%10s), adc=[%u,%u,%u,%u] tempRaw=%d\n",
                 statusTmp.outputsMask,
                 oven_outputs_mask_to_str(statusTmp.outputsMask),
                 statusTmp.adcRaw[0], statusTmp.adcRaw[1], statusTmp.adcRaw[2], statusTmp.adcRaw[3],
                 statusTmp.tempRaw);
        break;

    case ProtocolMessageType::ClientPong:
        HOST_DBG("PONG received\n");
        _lastPong = true;

        if (_pongStreak < 255) {
            _pongStreak++;
        }
        if (_pongStreak >= 2) {
            _linkSynced = true;
        }
        // _lastRxAnyMs = millis();
        break;

    case ProtocolMessageType::ClientRst:
        HOST_WARN("RST received from client\n");
        _linkSynced = false;
        _pongStreak = 0;
        // _lastRxAnyMs = millis();
        break;

    default:
        HOST_ERR("Unexpected message type=%u\n", (unsigned)type);
        _commError = true; // parse was OK but message is invalid for host
        break;
    }
    _lastRxAnyMs = millis();
    HOST_DBG("--------> _lastRxAnyMs = %lu\n", _lastRxAnyMs);
}

void HostComm::sendRst() {
    String msg = ProtocolCodec::buildHostRst();
    _serial.print(msg);
}

void HostComm::updOutputs(uint16_t setMask, uint16_t clrMask) {
    String msg = ProtocolCodec::buildHostUpd(setMask, clrMask);
    _serial.print(msg);
}

void HostComm::togOutputs(uint16_t togMask) {
    String msg = ProtocolCodec::buildHostTog(togMask);
    _serial.print(msg);
}

// In HostComm.cpp:
void HostComm::processLine(const String &line) {
    processCompletedLine(line);
}

bool HostComm::lastPongReceived() const {
    return _lastPong;
}

void HostComm::clearLastPongFlag() {
    _lastPong = false;
}

bool HostComm::linkSynced() const { return _linkSynced; }
void HostComm::clearLinkSync() {
    _linkSynced = false;
    _pongStreak = 0;
}
uint8_t HostComm::pongStreak() const { return _pongStreak; }

bool HostComm::lastUpdAcked() const { return _lastUpdAcked; }
void HostComm::clearLastUpdAckFlag() { _lastUpdAcked = false; }
bool HostComm::lastTogAcked() const { return _lastTogAcked; }
void HostComm::clearLastTogAckFlag() { _lastTogAcked = false; }

// --- NEW: shared RX byte handler (used by UART loop + tests) ---
void HostComm::handleRxByte(char c) {
    // Ignore CR
    if (c == '\r') {
        return;
    }

    if (c == '\n') {
        // End of line: process even if empty buffer (we sanitize below)
        String line = _rxBuffer;
        _rxBuffer = "";
        processCompletedLine(line);
        return;
    }

    _rxBuffer += c;

    if (_rxBuffer.length() > 120) {
        HOST_WARN("[HostComm] RX line overflow, dropping\n");
        _rxBuffer = "";
        // Optional: set _commError only after link is stable
        // if (_linkSynced) { _commError = true; }
    }
}

// --- NEW: same sanitize path as loop() should use ---
void HostComm::processCompletedLine(String line) {
    // 1) trim whitespace
    line.trim();
    if (line.isEmpty()) {
        return; // ignore empty lines
    }

    // 2) drop leading junk until we see 'C' or 'H'
    int start = -1;
    for (int i = 0; i < (int)line.length(); ++i) {
        const char ch = line[i];
        if (ch == 'C' || ch == 'H') {
            start = i;
            break;
        }
    }

    if (start < 0) {
        // treat as junk line (count as fail) but do not hard-fail the link
        _parseFailCount++;
        _lastBadLine = line;
        HOST_WARN("[HostComm] RX junk line ignored: '%s' (failCount=%lu)\n",
                  line.c_str(), (unsigned long)_parseFailCount);
        return;
    }

    if (start > 0) {
        HOST_WARN("[HostComm] RX leading junk (%d bytes) removed\n", start);
        line = line.substring(start);
    }

    // DBG("[HostComm] RX LINE: '%s'\n", line.c_str());

    handleIncomingLine(line);
}

// --- NEW: processRxBytes for TC5 (and future RX fragmentation tests) ---
void HostComm::processRxBytes(const uint8_t *data, size_t len) {
    if (!data || len == 0) {
        return;
    }
    for (size_t i = 0; i < len; ++i) {
        handleRxByte((char)data[i]);
    }
}

// END OF FILE
