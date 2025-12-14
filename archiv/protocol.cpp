//
// protocol.cpp
//
// This file implements the ProtocolCodec class used by both HostComm and
// ClientComm to serialize and deserialize the ASCII-based communication
// protocol exchanged between HOST (ESP32-S3) and CLIENT (ESP32-WROOM).
//
// The protocol consists of *text lines* terminated by CR+LF ("\r\n").
// Each line contains semicolon-separated fields:
//
//   <Sender>;<Command>[;Arguments...]\r\n
//
// Examples:
//
//   H;SET;0019\r\n
//   H;GET;STATUS\r\n
//   C;ACK;SET;0019\r\n
//   C;STATUS;0019;123;456;789;1023;112\r\n
//
// This codec converts between these textual frames and structured
// C++ data types (ProtocolStatus, ProtocolMessageType).
//

#include "protocol.h"

//
// A global constant for CRLF termination.
// All outgoing protocol messages use "\r\n" as defined.
//
static const char *CRLF = "\r\n";

// ============================================================================
//  Helper: Convert uint16_t to 4-digit uppercase HEX string (e.g. "00AF").
// ============================================================================

String ProtocolCodec::toHex4(uint16_t value)
{
    // snprintf ensures safe formatting into a fixed buffer.
    // "%04X" formats the number in uppercase hex, padded to 4 digits.
    char buf[5];
    snprintf(buf, sizeof(buf), "%04X", static_cast<unsigned>(value));
    return String(buf);
}

// ============================================================================
//  Helper: Parse 4-digit hex string into uint16_t.
//  Returns true on success, false on malformed input.
// ============================================================================

bool ProtocolCodec::parseHex4(const String &text, uint16_t &value)
{
    // The protocol mandates exactly 4 hex digits.
    if (text.length() != 4)
    {
        return false;
    }

    // Use strtol to convert; base 16 for hex.
    char *endPtr = nullptr;
    long v = strtol(text.c_str(), &endPtr, 16);

    // endPtr must point to string end → full parsing success.
    if (*endPtr != '\0')
        return false;

    // Check legal uint16 range.
    if (v < 0 || v > 0xFFFF)
        return false;

    value = static_cast<uint16_t>(v);
    return true;
}

// ============================================================================
//  Host → Client Message Builders
// ============================================================================

/**
 * @brief Build a SET message:
 *
 *   H;SET;<mask>\r\n
 *
 * <mask> is a 4-digit uppercase hex value.
 */
String ProtocolCodec::buildHostSet(uint16_t mask)
{
    String msg = F("H;SET;");
    msg += toHex4(mask);
    msg += CRLF;
    return msg;
}

/**
 * @brief Build a GET STATUS message:
 *
 *   H;GET;STATUS\r\n
 */
String ProtocolCodec::buildHostGetStatus()
{
    String msg = F("H;GET;STATUS");
    msg += CRLF;
    return msg;
}

/**
 * @brief Build a PING message:
 *
 *   H;PING\r\n
 */
String ProtocolCodec::buildHostPing()
{
    String msg = F("H;PING");
    msg += CRLF;
    return msg;
}

// ============================================================================
//  Client → Host Message Builders
// ============================================================================

/**
 * @brief Build an ACK to a SET message:
 *
 *   C;ACK;SET;<mask>\r\n
 */
String ProtocolCodec::buildClientAckSet(uint16_t mask)
{
    String msg = F("C;ACK;SET;");
    msg += toHex4(mask);
    msg += CRLF;
    return msg;
}

/**
 * @brief Build an ERR response to a SET message:
 *
 *   C;ERR;SET;<errorCode>\r\n
 *
 * The <errorCode> is application-specific (e.g. "1", "42").
 */
String ProtocolCodec::buildClientErrSet(int errorCode)
{
    String msg = F("C;ERR;SET;");
    msg += String(errorCode);
    msg += CRLF;
    return msg;
}

/**
 * @brief Build a STATUS frame:
 *
 *   C;STATUS;<mask>;<adc0>;<adc1>;<adc2>;<adc3>;<temp>\r\n
 *
 * <mask> = 4-char HEX
 * <adc*> = integer raw values (0..4095 typical)
 * <temp> = temperature × 4 (0.25°C resolution)
 */
String ProtocolCodec::buildClientStatus(const ProtocolStatus &status)
{
    String msg = F("C;STATUS;");
    msg += toHex4(status.outputsMask);
    msg += ';';
    msg += String(status.adcRaw[0]);
    msg += ';';
    msg += String(status.adcRaw[1]);
    msg += ';';
    msg += String(status.adcRaw[2]);
    msg += ';';
    msg += String(status.adcRaw[3]);
    msg += ';';
    msg += String(status.tempRaw);
    msg += CRLF;
    return msg;
}

/**
 * @brief Build a PONG message to respond to a host PING:
 *
 *   C;PONG\r\n
 */
String ProtocolCodec::buildClientPong()
{
    String msg = F("C;PONG");
    msg += CRLF;
    return msg;
}

// ============================================================================
//  Parse an incoming protocol line
// ============================================================================

/**
 * @brief Parse a single protocol line into a structured message.
 *
 * Input:
 *   - `line`: a completed text line WITHOUT CR/LF.
 *
 * Output (by reference):
 *   - type: which protocol message type was recognized
 *   - status: valid only for ClientStatus frames
 *   - mask:   valid only for SET/ACK frames
 *   - errorCode: valid only for ERR frames
 *
 * Returns:
 *   - true  → valid protocol message
 *   - false → parse error (wrong field count, invalid hex, unknown command)
 *
 * The parser:
 *   1. Splits at ';'
 *   2. Examines parts[0] (sender): "H" or "C"
 *   3. Examines parts[1] (command)
 *   4. Extracts additional values depending on the command type
 */
bool ProtocolCodec::parseLine(const String &line,
                              ProtocolMessageType &type,
                              ProtocolStatus &status,
                              uint16_t &mask,
                              int &errorCode,
                              uint16_t &maskB,
                              uint16_t &maskC)
{
    type = ProtocolMessageType::Unknown;
    mask = 0;
    errorCode = 0;
    maskB = 0;
    maskC = 0;
    // --- Step 1: split line by semicolons -------------------------------
    const int maxParts = 10;
    String parts[maxParts];
    int partCount = 0;

    int start = 0;
    while (start <= line.length() && partCount < maxParts)
    {
        int sep = line.indexOf(';', start);

        if (sep < 0)
        {
            // Last chunk: no more separators
            parts[partCount++] = line.substring(start);
            break;
        }
        else
        {
            parts[partCount++] = line.substring(start, sep);
            start = sep + 1;
        }
    }

    // Minimum: sender + command
    if (partCount < 2)
        return false;

    String sender = parts[0]; // "H" or "C"
    String cmd = parts[1];    // e.g. "SET", "GET", "STATUS", ...

    // =========================================================================
    //  HOST → CLIENT MESSAGES
    // =========================================================================
    if (sender == "H")
    {
        // ----------
        // H;SET;<mask>
        // ----------
        if (cmd == "SET")
        {
            if (partCount != 3)
                return false;

            if (!parseHex4(parts[2], mask))
                return false;

            type = ProtocolMessageType::HostSet;
            return true;
        }

        // ----------
        // H;GET;STATUS
        // ----------
        else if (cmd == "GET")
        {
            if (partCount != 3)
                return false;

            if (parts[2] != "STATUS")
                return false;

            type = ProtocolMessageType::HostGetStatus;
            return true;
        }

        // ----------
        // H;PING
        // ----------
        else if (cmd == "PING")
        {
            type = ProtocolMessageType::HostPing;
            return true;
        }
        else if (cmd == "RST")
        {
            // Accept exactly: H;RST
            if (partCount != 2)
                return false;
            type = ProtocolMessageType::HostRst;
            return true;
        }

        else if (cmd == "UPD")
        {
            // H;UPD;SSSS;CCCC
            if (partCount != 4)
                return false;

            uint16_t setMask = 0;
            uint16_t clrMask = 0;
            if (!parseHex4(parts[2], setMask))
                return false;
            if (!parseHex4(parts[3], clrMask))
                return false;

            type = ProtocolMessageType::HostUpd;
            mask = setMask;
            maskB = clrMask;
            return true;
        }
        else if (cmd == "TOG")
        {
            // H;TOG;TTTT
            if (partCount != 3)
                return false;

            uint16_t togMask = 0;
            if (!parseHex4(parts[2], togMask))
                return false;

            type = ProtocolMessageType::HostTog;
            mask = togMask;
            return true;
        }
        // Unknown host command
        return false;
    }

    // =========================================================================
    //  CLIENT → HOST MESSAGES
    // =========================================================================
    if (sender == "C")
    {
        // ----------
        // C;ACK;SET;<mask>
        // ----------
        if (cmd == "ACK")
        {
            if (partCount != 4)
                return false;

            if (parts[2] != "SET")
                return false;

            if (!parseHex4(parts[3], mask))
                return false;

            type = ProtocolMessageType::ClientAckSet;
            return true;
        }

        // ----------
        // C;ERR;SET;<errorCode>
        // ----------
        else if (cmd == "ERR")
        {
            if (partCount != 4)
                return false;

            if (parts[2] != "SET")
                return false;

            errorCode = parts[3].toInt();
            type = ProtocolMessageType::ClientErrSet;
            return true;
        }

        // ----------
        // C;STATUS;<mask>;<a0>;<a1>;<a2>;<a3>;<temp>
        // ----------
        else if (cmd == "STATUS")
        {
            // Expect exactly 8 parts:
            // [0]=C
            // [1]=STATUS
            // [2]=mask hex
            // [3]=a0
            // [4]=a1
            // [5]=a2
            // [6]=a3
            // [7]=temp
            if (partCount != 8)
                return false;

            uint16_t m;
            if (!parseHex4(parts[2], m))
                return false;

            status.outputsMask = m;
            status.adcRaw[0] = static_cast<uint16_t>(parts[3].toInt());
            status.adcRaw[1] = static_cast<uint16_t>(parts[4].toInt());
            status.adcRaw[2] = static_cast<uint16_t>(parts[5].toInt());
            status.adcRaw[3] = static_cast<uint16_t>(parts[6].toInt());
            status.tempRaw = static_cast<int16_t>(parts[7].toInt());

            type = ProtocolMessageType::ClientStatus;
            return true;
        }

        // ----------
        // C;PONG
        // ----------
        else if (cmd == "PONG")
        {
            type = ProtocolMessageType::ClientPong;
            return true;
        }
        // ----------
        // RESET
        // ----------
        else if (cmd == "RST")
        {
            // Accept exactly: C;RST
            if (partCount != 2)
                return false;
            type = ProtocolMessageType::ClientRst;
            return true;
        }
        // Unknown client command
        return false;
    }

    // =========================================================================
    //  UNKNOWN SENDER
    // =========================================================================
    return false;
}

String ProtocolCodec::buildHostRst()
{
    String msg = F("H;RST");
    msg += "\r\n";
    return msg;
}

String ProtocolCodec::buildClientRst()
{
    String msg = F("C;RST");
    msg += "\r\n";
    return msg;
}

String ProtocolCodec::buildHostUpd(uint16_t setMask, uint16_t clrMask)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "H;UPD;%04X;%04X\r\n", setMask, clrMask);
    return String(buf);
}

String ProtocolCodec::buildHostTog(uint16_t togMask)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "H;TOG;%04X\r\n", togMask);
    return String(buf);
}

String ProtocolCodec::buildClientAckUpd(uint16_t newMask)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "C;ACK;UPD;%04X\r\n", newMask);
    return String(buf);
}

String ProtocolCodec::buildClientAckTog(uint16_t newMask)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "C;ACK;TOG;%04X\r\n", newMask);
    return String(buf);
}