#pragma once

#include <Arduino.h>
#include "protocol.h"

class HostComm
{
public:
    explicit HostComm(HardwareSerial &serial);

    void begin(uint32_t baudrate);
    void loop(); // non-blocking, safe to call alongside LVGL

    // Commands to client
    void setOutputsMask(uint16_t mask); // sends H;SET;...
    void requestStatus();               // sends H;GET;STATUS
    void sendPing();                    // sends H;PING

    // Local / remote state
    uint16_t getLocalOutputsMask() const;
    uint16_t getRemoteOutputsMask() const;
    const ProtocolStatus &getRemoteStatus() const;

    // Flags
    bool hasNewStatus() const;
    void clearNewStatusFlag();

    bool lastSetAcked() const;
    void clearLastSetAckFlag();

    bool hasCommError() const;
    void clearCommErrorFlag();

    // In HostComm.h (public):
    void processLine(const String &line);

private:
    HardwareSerial &_serial;
    String _rxBuffer;

    uint16_t _localOutputsMask;
    ProtocolStatus _remoteStatus;

    bool _newStatus;
    bool _lastSetAcked;
    bool _commError;

    void handleIncomingLine(const String &line);
};