#pragma once

#include "log_core.h"
#include <Arduino.h>

inline const char *bits16ToStr(uint16_t value) {
    static char buf[17]; // 16 bits + '\0'

    for (int i = 0; i < 16; ++i) {
        buf[i] = (value & (1 << (15 - i))) ? '1' : '0';
    }

    buf[16] = '\0';
    return buf;
}

// Holds status data exchanged between host and client
struct ProtocolStatus {
    uint16_t outputsMask; // 16-bit digital states CH0–CH15
    uint16_t adcRaw[4];   // adcRaw[0..3] for analog channels (CH12–CH15)
    int16_t tempRaw;      // temperature in 0.25°C steps (tempRaw = °C * 4)
};

enum class ProtocolMessageType : uint8_t {
    Unknown = 0,
    // Host -> Client
    HostUpd, // H;UPD;SSSS;CCCC
    HostTog, // H;TOG;TTTT
    HostSet,
    HostGetStatus,
    HostPing,
    HostRst,

    // Client -> Host
    ClientAckUpd, // C;ACK;UPD;MMMM
    ClientAckTog, // C;ACK;TOG;MMMM
    ClientAckSet,
    ClientErrSet,
    ClientStatus,
    ClientPong,
    ClientRst,
};

class ProtocolCodec {
  public:
    // Parse a single line (without trailing CR/LF).
    // Fills type, status, mask, errorCode as applicable.
    // Returns true on success, false on parse error.
    static bool parseLine(const String &line,
                          ProtocolMessageType &type,
                          ProtocolStatus &status,
                          uint16_t &mask,
                          int &errorCode,
                          uint16_t &maskB,
                          uint16_t &maskC);

    // Host → Client messages
    static String buildHostSet(uint16_t mask);
    static String buildHostGetStatus();
    static String buildHostPing();

    // Client → Host messages
    static String buildClientAckSet(uint16_t mask);
    static String buildClientErrSet(int errorCode);
    static String buildClientStatus(const ProtocolStatus &status);
    static String buildClientPong();

    static String buildHostRst();
    static String buildClientRst();

    // Host -> Client
    static String buildHostUpd(uint16_t setMask, uint16_t clrMask);
    static String buildHostTog(uint16_t togMask);

    // Client -> Host
    static String buildClientAckUpd(uint16_t newMask);
    static String buildClientAckTog(uint16_t newMask);

  private:
    static String toHex4(uint16_t value);
    static bool parseHex4(const String &text, uint16_t &value);
};

// EOF