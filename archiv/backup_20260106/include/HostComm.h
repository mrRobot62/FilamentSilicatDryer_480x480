#pragma once

#include "protocol.h"
#include <Arduino.h>

/**
 * Version 0.2
 * @brief HostComm handles communication from the host (ESP32-WROOM) side.
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
 */
class HostComm {
  public:
    explicit HostComm(HardwareSerial &serial);

    void begin(uint32_t baudrate, uint8_t rx, uint8_t tx);
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

    // Commands to client
    void sendRst();                                      // sends H;RST
    void updOutputs(uint16_t setMask, uint16_t clrMask); // sends H;UPD;SSSS;CCCC
    void togOutputs(uint16_t togMask);                   // sends H;TOG;TTTT

    bool lastPongReceived() const;
    void clearLastPongFlag();

  private:
    HardwareSerial &_serial;
    uint8_t _rx, _tx;
    String _rxBuffer;

    uint16_t _localOutputsMask;
    ProtocolStatus _remoteStatus;

    bool _newStatus;
    bool _lastSetAcked;
    bool _commError;
    bool _lastPong;
    void handleIncomingLine(const String &line);
};