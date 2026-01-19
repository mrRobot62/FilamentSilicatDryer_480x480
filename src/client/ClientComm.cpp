#include "ClientComm.h"

/**
 * Version 0.3 (2026-01-07)
 *  Verbesserung in der Verarbeitung von eingehenden Zeilen, umfangreiche Tests durch die TesteCases zeigen eine stabile Client-Kommunikation.
 * 
 * Version 0.2 (2026-01-06)
 *  Fixes in line processing and added heartbeat callback.
 * 
 * @brief Construct a new ClientComm communication handler.
 *
 * This class handles:
 * - UART reads from the host (ESP32-S3)
 * - Line assembly terminated by '\n'
 * - Protocol parsing using ProtocolCodec
 * - Signaling events to the application:
 *     - hasNewOutputsMask(): host sent a SET command
 *     - statusRequested(): host sent a GET STATUS
 *
 * It does NOT:
 * - Directly manipulate GPIOs
 * - Read ADC or temperature sensors
 *
 * Instead, your application:
 * - Reacts to hasNewOutputsMask() by applying the mask to GPIO
 * - Reacts to statusRequested() by preparing a ProtocolStatus
 *   and calling sendStatus(...)
 *
 * @param serial Reference to HardwareSerial used for the host link, e.g. Serial2.
 */
ClientComm::ClientComm(HardwareSerial &serial, uint8_t rx, uint8_t tx)
    : _linkSerial(serial),
      _rx(rx),
      _tx(tx),
      _rxBuffer(),
      _outputsMask(0),
      _newOutputsMask(false),
      _statusRequested(false) {
}

/**
 * @brief Initialize UART for communication with the host.
 *
 * Must be called once in setup(), e.g.:
 *   clientComm.begin(115200);
 *
 * Pin routing can be set globally (e.g. via Serial2.begin with pins),
 * or configured elsewhere depending on your board setup.
 *
 * @param baudrate UART baud rate, must match the host side.
 */
void ClientComm::begin(uint32_t baudrate) {
    _linkSerial.begin(baudrate, SERIAL_8N1, _rx, _tx);
}

/**
 * @brief Periodic communication handling.
 *
 * Call this very frequently from loop(), e.g.:
 *   void loop() {
 *       clientComm.loop();
 *       // other tasks...
 *   }
 *
 * It:
 * - Reads all available bytes from the UART
 * - Assembles lines until '\n'
 * - Strips '\r', uses '\n' as end-of-line
 * - For each complete line, calls handleIncomingLine()
 *
 * This implementation is fully non-blocking and uses no delays.
 */
void ClientComm::loop() {
    bool hadActivity = false;

    while (_linkSerial.available() > 0) {
        hadActivity = true;
        char c = static_cast<char>(_linkSerial.read());

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            String line = _rxBuffer;
            _rxBuffer = "";

            // Trim and ignore empty
            line.trim();
            if (line.isEmpty()) {
                continue;
            }

            // Drop leading junk until 'H' or 'C'
            int start = -1;
            for (int i = 0; i < (int)line.length(); ++i) {
                char ch = line[i];
                if (ch == 'H' || ch == 'C') {
                    start = i;
                    break;
                }
            }

            if (start < 0) {
                // Junk-only line; ignore
                continue;
            }

            if (start > 0) {
                line = line.substring(start);
            }

            handleIncomingLine(line);
            continue; // IMPORTANT: do NOT return; keep consuming RX buffer
        }

        _rxBuffer += c;

        // Optional safety against runaway garbage without '\n'
        if (_rxBuffer.length() > 120) {
            _rxBuffer = "";
            // optionally count/log overflow
        }
    }

    if (hadActivity && _heartBeatCb) {
        _heartBeatCb();
    }
}

/**
 * @brief Check if a new outputsMask was received from the host (SET/UPD/TOG).
 *
 * This is set to true when a valid:

 * message is received and parsed.
 *
 * After your application applies the mask to GPIOs or logical outputs,
 * you should call clearNewOutputsMaskFlag().
 *
 * @return true if a new outputs mask is available.
 * @return false otherwise.
 */
bool ClientComm::hasNewOutputsMask() const {
    return _newOutputsMask;
}

/**
 * @brief Get the last outputs mask requested by the host.
 *
 * This is updated when hasNewOutputsMask() becomes true.
 *
 * @return 16-bit mask for CH0..CH15.
 */
uint16_t ClientComm::getOutputsMask() const {
    return _outputsMask;
}

/**
 * @brief Clear the "new outputs mask" flag after handling it.
 *
 * Call this AFTER you have applied the new outputs mask to your GPIOs.
 */
void ClientComm::clearNewOutputsMaskFlag() {
    _newOutputsMask = false;
}

/**
 * @brief Check if the host requested a STATUS frame.
 *
 * This flag is set to true when:
 *   H;GET;STATUS
 * is received.
 *
 * Your application should:
 * - Gather current outputsMask, ADC values and temperature into a
 *   ProtocolStatus struct
 * - Call sendStatus(status)
 * - Then call clearStatusRequestedFlag()
 *
 * @return true if host requested a status frame.
 * @return false otherwise.
 */
bool ClientComm::statusRequested() const {
    return _statusRequested;
}

/**
 * @brief Clear the "status requested" flag after sending a STATUS frame.
 */
void ClientComm::clearStatusRequestedFlag() {
    _statusRequested = false;
}

/**
 * @brief Send a STATUS frame to the host with the given status data.
 *
 * This will build and send:
 *   C;STATUS;<mask>;<a0>;<a1>;<a2>;<a3>;<temp>\r\n
 *
 * You are responsible for filling the ProtocolStatus fields correctly:
 * - outputsMask: current digital states for CH0..CH15
 * - adcRaw[0..3]: raw ADC readings for CH12..CH15
 * - tempRaw: temperature in 0.25°C units
 *
 * @param status Status data to send.
 */
void ClientComm::sendStatus(const ProtocolStatus &status) {
    String msg = ProtocolCodec::buildClientStatus(status);
    sendLine(msg);
}

/**
 * @brief Send an ACK frame for a SET command.
 *
 * This sends:
 *   C;ACK;SET;<mask>\r\n
 *
 * The mask should match exactly what you applied to your IO.
 *
 * @param mask 16-bit outputs mask confirming applied state.
 */
void ClientComm::sendAckSet(uint16_t mask) {
    String msg = ProtocolCodec::buildClientAckSet(mask);
    sendLine(msg);
}

/**
 * @brief Send an ERR frame for a SET command.
 *
 * This sends:
 *   C;ERR;SET;<errorCode>\r\n
 *
 * Use this when you detect invalid or unsupported SET parameters.
 *
 * @param errorCode Application-defined error code.
 */
void ClientComm::sendErrSet(int errorCode) {
    String msg = ProtocolCodec::buildClientErrSet(errorCode);
    sendLine(msg);
}

/**
 * @brief Send a PONG response to a PING request from the host.
 *
 * This sends:
 *   C;PONG\r\n
 */
void ClientComm::sendPong() {
    // RAW("[CLIENT] Receiving PING  from host\n");
    _clientSerialMonitor("Receiving PING  from host", "RX");
    String msg = ProtocolCodec::buildClientPong();
    sendLine(msg);
    _clientSerialMonitor("Sending PONG response to host", "TX");
    //    RAW("[CLIENT] Sending PONG response to host (%s) \n", msg.c_str());
}

/**
 * @brief Handle one complete protocol line (without CR/LF).
 *
 * This function:
 * - Parses the line using ProtocolCodec::parseLine
 * - Updates internal flags and outputsMask for HostSet messages
 * - Responds to HostPing with PONG
 * - Sets statusRequested flag on HostGetStatus
 *
 * Actual IO updates (GPIO, ADC, MAX6675) are handled by your main sketch.
 *
 * @param line A full protocol line without trailing CR/LF.
 */
void ClientComm::handleIncomingLine(const String &line) {
    ProtocolMessageType type;
    ProtocolStatus dummyStatus; // not used for host messages
    uint16_t mask = 0;
    int errorCode = 0;
    uint16_t maskB = 0;
    uint16_t maskC = 0;
    // Parse incoming line. For host→client messages we mainly care about:
    //  - HostSet
    //  - HostGetStatus
    //  - HostPing
    bool ok = ProtocolCodec::parseLine(line, type, dummyStatus, mask, errorCode, maskB, maskC);
    if (!ok) {
        // If parsing fails, we currently do not send an error.
        // You could extend this to log or handle the errorCode.
        (void)errorCode;
        RAW("[CLIENT] Failed to parse line: %s\n", line.c_str());
        return;
    }

    switch (type) {
    case ProtocolMessageType::HostSet:
        // Host wants to set the outputsMask.
        // We store it and notify the application via flag.
        _outputsMask = mask;
        _newOutputsMask = true;
        if (_onOutputsChanged) {
            _onOutputsChanged(_outputsMask);
        }
        // Immediately acknowledge the SET command.
        sendAckSet(mask);
        RAW("[CLIENT] Processed Host SET, new mask=0x%04X\n", mask);
        break;

    case ProtocolMessageType::HostGetStatus: {
        ProtocolStatus s{};
        s.outputsMask = _outputsMask;

        // Safe defaults
        s.adcRaw[0] = s.adcRaw[1] = s.adcRaw[2] = s.adcRaw[3] = 0;
        s.tempRaw = 0;

        // Let the sketch fill real hardware values
        if (_fillStatusCb) {
            _fillStatusCb(s);
        }

        sendStatus(s); // IMPORTANT: sendStatus() must use sendLine() internally
        RAW("[CLIENT] Processed Host GET STATUS\n");
        break;
    }
    case ProtocolMessageType::HostPing:
        // Simple connectivity test. Respond with PONG.
        sendPong();
        break;

    case ProtocolMessageType::HostUpd: {
        const uint16_t setMask = mask;
        const uint16_t clrMask = maskB;

        _outputsMask = (_outputsMask | setMask) & static_cast<uint16_t>(~clrMask);

        if (_onOutputsChanged) {
            _onOutputsChanged(_outputsMask);
        }
        // IMPORTANT: notify main sketch that outputsMask changed
        _newOutputsMask = true;

        // ACK with resulting mask
        sendAckUpd(_outputsMask);
        RAW("[CLIENT] Processed Host UPD, new mask=0x%04X\n", _outputsMask);
        break;
    }

    case ProtocolMessageType::HostTog: {
        const uint16_t togMask = mask;

        _outputsMask ^= togMask;

        // IMPORTANT: notify main sketch that outputsMask changed
        _newOutputsMask = true;

        // IMPORTANT: notify main sketch that outputsMask changed
        _newOutputsMask = true;

        // ACK with resulting mask
        sendAckTog(_outputsMask);
        RAW("[CLIENT] Processed Host TOG, new mask=0x%04X\n", _outputsMask);
        break;
    }
    default:
        // Client should not receive client messages or unknown types.
        // We simply ignore them (could be extended with logging).
        break;
    }
}

void ClientComm::processLine(const String &line) {
    // Directly feed a protocol line into the normal handler.
    // This is used only in test/simulation environments.
    handleIncomingLine(line);
}

void ClientComm::sendAckUpd(uint16_t newMask) {
    sendLine(ProtocolCodec::buildClientAckUpd(newMask));
}

void ClientComm::sendAckTog(uint16_t newMask) {
    sendLine(ProtocolCodec::buildClientAckTog(newMask));
}

void ClientComm::setOutputsChangedCallback(OutputsChangedCallback cb) { _onOutputsChanged = cb; }
void ClientComm::setFillStatusCallback(FillStatusCallback cb) { _fillStatusCb = cb; }
void ClientComm::setTxLineCallback(TxLineCallback cb) { _clientSerialMonitor = cb; }
void ClientComm::setHeartBeatCallback(HeartBeatCallback cb) { _heartBeatCb = cb; }

void ClientComm::sendLine(const String &lineWithCrlf) {
    _linkSerial.print(lineWithCrlf);
    if (_clientSerialMonitor) {
        // String noCrlf = lineWithCrlf;
        // noCrlf.replace("\r", "");
        // noCrlf.replace("\n", "");
        _clientSerialMonitor(lineWithCrlf, "TX");
    }
}