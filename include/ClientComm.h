#pragma once

#include "log_client.h"
#include "protocol.h"
#include <Arduino.h>

enum class ClientSafetyReason : uint8_t {
    Boot = 0,
    HostTimeout = 1,
    ParseError = 2,
    RxOverflow = 3,
    UnexpectedFrame = 4,
    HostRst = 5
};
class ClientComm {

  public:
    explicit ClientComm(HardwareSerial &serial, uint8_t rx, uint8_t tx);

    void begin(uint32_t baudrate);
    void loop(); // non-blocking

    // Host requested new outputs mask (H;SET;...)
    bool hasNewOutputsMask() const;
    uint16_t getOutputsMask() const;
    void clearNewOutputsMaskFlag();

    // Host requested a status frame (H;GET;STATUS)
    bool statusRequested() const;
    void clearStatusRequestedFlag();

    // Your application calls this to send a STATUS response
    void sendStatus(const ProtocolStatus &status);

    // For simulation/testing: directly process a complete line
    // (without CR/LF). This bypasses the UART reader in loop().
    void processLine(const String &line);

    // In ClientComm.h (public section)
    using OutputsChangedCallback = void (*)(uint16_t newMask);
    using FillStatusCallback = void (*)(ProtocolStatus &st);
    using TxLineCallback = void (*)(const String &line, const String &dir);
    using HeartBeatCallback = void (*)();

    void setOutputsChangedCallback(OutputsChangedCallback cb);
    void setFillStatusCallback(FillStatusCallback cb);
    void setTxLineCallback(TxLineCallback cb);
    void setHeartBeatCallback(HeartBeatCallback cb);

  private:
    HardwareSerial &_linkSerial;
    uint8_t rx, tx;
    String _rxBuffer;

    uint16_t _outputsMask;
    bool _newOutputsMask;
    bool _statusRequested;
    uint8_t _rx;
    uint8_t _tx;

    void handleIncomingLine(const String &line);

    void sendAckSet(uint16_t mask);
    void sendErrSet(int errorCode);
    void sendPong();

    void sendAckUpd(uint16_t newMask);
    void sendAckTog(uint16_t newMask);

    void sendLine(const String &lineWithCrlf);
    void debugLED(bool on = true, int durationMs = 100);

    OutputsChangedCallback _onOutputsChanged = nullptr;
    FillStatusCallback _fillStatusCb = nullptr;
    TxLineCallback _clientSerialMonitor = nullptr;
    HeartBeatCallback _heartBeatCb = nullptr;

    // --- T14.0 SafetyGuard additions (Step 1) ---
    void enterSafeState_(uint8_t reasonRaw);
    void clearSafetyLatch_();

    bool _safetyLatched = false;
    uint8_t _lastSafetyReason = 0;
    uint32_t _lastSafetyMs = 0;
};

// EOF