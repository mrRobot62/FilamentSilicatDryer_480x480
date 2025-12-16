#include "HostComm.h"

/**
 * @brief Construct a new HostComm communication handler.
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
      _rxBuffer(),
      _localOutputsMask(0)
{
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
void HostComm::begin(uint32_t baudrate)
{
    _serial.begin(baudrate);
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
void HostComm::loop()
{
    // Read all currently available characters from the UART
    while (_serial.available() > 0)
    {
        char c = static_cast<char>(_serial.read());

        // We treat '\r' as optional and ignore it,
        // and use '\n' as the end-of-line separator.
        if (c == '\r')
        {
            continue;
        }

        if (c == '\n')
        {
            // End of line reached: process the collected line if not empty
            if (_rxBuffer.length() > 0)
            {
                String line = _rxBuffer;
                _rxBuffer = "";
                handleIncomingLine(line);
            }
        }
        else
        {
            // Accumulate character into buffer
            _rxBuffer += c;
        }
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
void HostComm::setOutputsMask(uint16_t mask)
{
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
void HostComm::requestStatus()
{
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
void HostComm::sendPing()
{
    String msg = ProtocolCodec::buildHostPing();
    _serial.print(msg);
}

/**
 * @brief Get the last mask that was sent by the host (local desired state).
 *
 * This reflects what your UI or logic *requested*, not necessarily what the
 * client currently uses (for that, check getRemoteOutputsMask()).
 *
 * @return 16-bit local outputs mask.
 */
uint16_t HostComm::getLocalOutputsMask() const
{
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
uint16_t HostComm::getRemoteOutputsMask() const
{
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
const ProtocolStatus &HostComm::getRemoteStatus() const
{
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
bool HostComm::hasNewStatus() const
{
    return _newStatus;
}

/**
 * @brief Clear the "new status" flag after processing the data.
 */
void HostComm::clearNewStatusFlag()
{
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
bool HostComm::lastSetAcked() const
{
    return _lastSetAcked;
}

/**
 * @brief Clear the "last SET acknowledged" flag.
 *
 * Use this if you want to detect a new ACK event after another SET.
 */
void HostComm::clearLastSetAckFlag()
{
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
bool HostComm::hasCommError() const
{
    return _commError;
}

/**
 * @brief Clear the communication error flag.
 */
void HostComm::clearCommErrorFlag()
{
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
 */
void HostComm::handleIncomingLine(const String &line)
{
    ProtocolMessageType type;
    ProtocolStatus statusTmp;
    uint16_t mask = 0;
    int errorCode = 0;
    uint16_t maskB = 0;
    uint16_t maskC = 0;
    // Try to parse the line according to the agreed protocol
    bool ok = ProtocolCodec::parseLine(line, type, statusTmp, mask, errorCode, maskB, maskC);
    if (!ok)
    {
        // Any parsing problem is treated as a communication error
        _commError = true;
        return;
    }

    // Handle message depending on its type
    switch (type)
    {
    case ProtocolMessageType::ClientAckSet:
        // Client confirmed a SET command:
        // we set the flag and optionally synchronize the remote outputsMask.
        _lastSetAcked = true;
        _remoteStatus.outputsMask = mask;
        break;

    case ProtocolMessageType::ClientErrSet:
        // Client reports an error processing the SET command.
        // We mark communication as faulty. errorCode could be logged.
        _commError = true;
        (void)errorCode; // suppress unused variable warning if not logged
        break;

    case ProtocolMessageType::ClientStatus:
        // Full status frame from client: update our status cache and flag.
        _remoteStatus = statusTmp;
        _newStatus = true;
        break;

    case ProtocolMessageType::ClientPong:
        // PONG is a simple reply to PING.
        // We do not maintain any special state here, but you could
        // extend this by setting a "linkAlive" flag if needed.
        break;

    default:
        // Host should not receive host messages or unknown types.
        // Treat as protocol/communication error.
        _commError = true;
        break;
    }
}

// In HostComm.cpp:
void HostComm::processLine(const String &line)
{
    handleIncomingLine(line);
}

// END OF FILE
