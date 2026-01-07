#include "HostComm.h"
#include "TestCase.h"
#include "log_core.h"
#include <Arduino.h>

/**
 * TestCase: Outputs: HOLD mask + STATUS verify
 *
 * Goal:
 * - Verify the link is stable enough for a longer "hold" phase.
 * - Set a known outputs mask via H;SET;MMMM and require C;ACK;SET;MMMM.
 * - During a hold window, request STATUS periodically and verify
 *   that the client reports the SAME outputsMask consistently.
 *
 * Expected behavior:
 * 1) Link sync: at least 2 consecutive PONGs (HostComm.linkSynced()==true)
 * 2) SET mask -> ACK;SET received AND remote mask matches
 * 3) During HOLD:
 *    - Receive multiple STATUS frames
 *    - Each STATUS outputsMask == expected mask
 *    - No commError is raised (after sync)
 *
 * Pass condition:
 * - Sync achieved
 * - ACK;SET OK
 * - At least kMinStatusOk STATUS frames received during hold
 * - No mismatch detected
 *
 * Fail conditions:
 * - Timeout in any phase
 * - comm error after sync
 * - STATUS mask mismatch
 */

class Test_HoldStatusVerify : public ITestCase {
  public:
    const char *name() const override { return "Outputs: HOLD mask + STATUS verify"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "init";

        _tStartMs = nowMs;
        _deadlineMs = nowMs + kTotalTimeoutMs;
        _nextPingMs = nowMs;
        _nextStatusMs = nowMs;

        _baseParseFail = comm.parseFailCount();
        _okStatusCount = 0;
        _badStatusCount = 0;

        // Start deterministic: resync link first
        comm.clearCommErrorFlag();
        comm.clearNewStatusFlag();
        comm.clearLastSetAckFlag();
        comm.clearLastPongFlag();
        comm.clearLinkSync();

        _phase = Phase::Sync;
        _detail = "syncing link (PING/PONG)";
    }

    void tick(HostComm &comm, uint32_t nowMs) override {
        // Hard timeout for entire testcase
        if ((int32_t)(nowMs - _deadlineMs) >= 0) {
            _detail = "timeout (overall)";
            _verdict = TestVerdict::Fail;
            return;
        }

        // After sync: comm error becomes fatal
        if (comm.linkSynced() && comm.hasCommError()) {
            _detail = "comm error flag set (after sync)";
            _verdict = TestVerdict::Fail;
            return;
        }

        switch (_phase) {
        case Phase::Sync:
            tickSync(comm, nowMs);
            break;

        case Phase::SetMask:
            tickSetMask(comm, nowMs);
            break;

        case Phase::HoldVerify:
            tickHold(comm, nowMs);
            break;
        }
    }

    void exit(HostComm &comm, uint32_t /*nowMs*/) override {
        // Keep link synced, but clear test-specific flags to avoid cross-test bleed
        comm.clearNewStatusFlag();
        comm.clearLastSetAckFlag();
        comm.clearLastPongFlag();
    }

    TestVerdict verdict() const override { return _verdict; }
    const char *detail() const override { return _detail; }

    void dump(HostComm &comm) const override {
        const uint32_t nowFail = comm.parseFailCount();
        const uint32_t deltaFail = nowFail - _baseParseFail;

        const ProtocolStatus &st = comm.getRemoteStatus();

        RAW("----------------------------------------------\n");
        RAW("- TestCase: %s\n", name());
        RAW("- %s\n", (_verdict == TestVerdict::Pass) ? "PASSED" : (_verdict == TestVerdict::Fail) ? "FAILED"
                                                                                                   : "RUNNING");
        RAW("- Detail: %s\n", _detail);
        RAW("- Flags: synced=%u streak=%u parseFail=%lu (+%lu) err=%u lastPong=%u\n",
            (unsigned)comm.linkSynced(),
            (unsigned)comm.pongStreak(),
            (unsigned long)nowFail,
            (unsigned long)deltaFail,
            (unsigned)comm.hasCommError(),
            (unsigned)comm.lastPongReceived());
        RAW("- State: phase=%u ackSet=%u newStatus=%u localMask=0x%04X remoteMask=0x%04X\n",
            (unsigned)_phase,
            (unsigned)comm.lastSetAcked(),
            (unsigned)comm.hasNewStatus(),
            (unsigned)comm.getLocalOutputsMask(),
            (unsigned)comm.getRemoteOutputsMask());
        RAW("- Status: mask=0x%04X adc=[%u,%u,%u,%u] tempRaw=%d\n",
            (unsigned)st.outputsMask,
            (unsigned)st.adcRaw[0], (unsigned)st.adcRaw[1], (unsigned)st.adcRaw[2], (unsigned)st.adcRaw[3],
            (int)st.tempRaw);
        RAW("- Counters: okStatus=%u badStatus=%u\n", (unsigned)_okStatusCount, (unsigned)_badStatusCount);
        RAW("- LastBadLine: '%s'\n", comm.lastBadLine().c_str());
        RAW("----------------------------------------------\n");
    }

  private:
    enum class Phase : uint8_t {
        Sync = 0,
        SetMask,
        HoldVerify
    };

    // --- parameters ---
    static constexpr uint32_t kTotalTimeoutMs = 6000;
    static constexpr uint32_t kSyncTimeoutMs = 2000;
    static constexpr uint32_t kSetTimeoutMs = 1500;
    static constexpr uint32_t kHoldDurationMs = 2500;

    static constexpr uint32_t kPingPeriodMs = 200;
    static constexpr uint32_t kStatusPeriodMs = 250;

    static constexpr uint16_t kTestMask = 0x00A5; // matches your existing LED pattern usage
    static constexpr uint8_t kMinStatusOk = 3;

    // --- state ---
    Phase _phase = Phase::Sync;

    uint32_t _tStartMs = 0;
    uint32_t _deadlineMs = 0;

    // sync
    uint32_t _syncStartMs = 0;
    uint32_t _nextPingMs = 0;

    // set
    uint32_t _setStartMs = 0;

    // hold
    uint32_t _holdStartMs = 0;
    uint32_t _nextStatusMs = 0;

    // counters / diagnostics
    uint32_t _baseParseFail = 0;
    uint16_t _okStatusCount = 0;
    uint16_t _badStatusCount = 0;

    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";

  private:
    void tickSync(HostComm &comm, uint32_t nowMs) {
        if (_syncStartMs == 0) {
            _syncStartMs = nowMs;
        }

        // Periodic PING until synced
        if ((int32_t)(nowMs - _nextPingMs) >= 0) {
            comm.sendPing();
            _nextPingMs = nowMs + kPingPeriodMs;
        }

        if (comm.linkSynced()) {
            _phase = Phase::SetMask;
            _setStartMs = nowMs;
            _detail = "synced -> sending SET";
            comm.clearLastSetAckFlag();
            comm.setOutputsMask(kTestMask);
            return;
        }

        if ((int32_t)(nowMs - (_syncStartMs + kSyncTimeoutMs)) >= 0) {
            _detail = "timeout syncing link";
            _verdict = TestVerdict::Fail;
            return;
        }
    }

    void tickSetMask(HostComm &comm, uint32_t nowMs) {
        // Wait for ACK;SET and correct remote mask
        if (comm.lastSetAcked() && comm.getRemoteOutputsMask() == kTestMask) {
            _phase = Phase::HoldVerify;
            _holdStartMs = nowMs;
            _nextStatusMs = nowMs; // request immediately
            comm.clearNewStatusFlag();
            _detail = "ACK ok -> hold/verify STATUS";
            return;
        }

        if ((int32_t)(nowMs - (_setStartMs + kSetTimeoutMs)) >= 0) {
            _detail = "timeout waiting for ACK;SET";
            _verdict = TestVerdict::Fail;
            return;
        }
    }

    void tickHold(HostComm &comm, uint32_t nowMs) {
        // request STATUS periodically
        if ((int32_t)(nowMs - _nextStatusMs) >= 0) {
            comm.requestStatus();
            _nextStatusMs = nowMs + kStatusPeriodMs;
        }

        // consume STATUS events
        if (comm.hasNewStatus()) {
            const uint16_t m = comm.getRemoteStatus().outputsMask;
            if (m == kTestMask) {
                _okStatusCount++;
            } else {
                _badStatusCount++;
                _detail = "STATUS mask mismatch during hold";
                _verdict = TestVerdict::Fail;
                return;
            }
            comm.clearNewStatusFlag();
        }

        // done?
        if ((int32_t)(nowMs - (_holdStartMs + kHoldDurationMs)) >= 0) {
            if (_okStatusCount < kMinStatusOk) {
                _detail = "not enough STATUS frames received";
                _verdict = TestVerdict::Fail;
                return;
            }
            _detail = "hold verified";
            _verdict = TestVerdict::Pass;
            return;
        }
    }
};

ITestCase *get_test_hold_status_verify() {
    static Test_HoldStatusVerify inst;
    return &inst;
}