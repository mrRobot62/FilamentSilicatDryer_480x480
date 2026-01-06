#pragma once

#include "HostComm.h"
#include "log_core.h"
#include <Arduino.h>

struct LinkSync {
    uint32_t pingPeriodMs = 200;
    uint32_t timeoutMs = 2000;
    uint8_t needStreak = 2;

    uint32_t startMs = 0;
    uint32_t deadlineMs = 0;
    uint32_t nextPingMs = 0;

    uint32_t parseFailStart = 0;

    void reset(HostComm &comm, uint32_t nowMs) {
        startMs = nowMs;
        deadlineMs = nowMs + timeoutMs;
        nextPingMs = nowMs;

        comm.clearCommErrorFlag();
        comm.clearLastPongFlag();
        comm.clearLinkSync();

        parseFailStart = comm.parseFailCount();
    }

    bool tick(HostComm &comm, uint32_t nowMs) {
        if (comm.hasCommError()) {
            return false;
        }

        if ((int32_t)(nowMs - nextPingMs) >= 0) {
            comm.sendPing();
            nextPingMs = nowMs + pingPeriodMs;
        }

        if (comm.lastPongReceived()) {
            comm.clearLastPongFlag();
            // HostComm baut intern _pongStreak/_linkSynced auf
        }

        return comm.linkSynced() && (comm.pongStreak() >= needStreak);
    }

    bool timedOut(uint32_t nowMs) const {
        return (int32_t)(nowMs - deadlineMs) >= 0;
    }

    uint32_t parseFailDelta(const HostComm &comm) const {
        return comm.parseFailCount() - parseFailStart;
    }
};