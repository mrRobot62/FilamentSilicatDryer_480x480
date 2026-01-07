/**
 * TestCase: TC14b - GET/STATUS after sequence (status consistency)
 *
 * Goal:
 * - After TC14a executed SET->UPD->TOG, request STATUS and verify
 *   that C;STATUS reflects the final expected outputsMask.
 *
 * Expectations:
 * - Link must be synced before requesting STATUS.
 * - A valid STATUS must be received (hasNewStatus()).
 * - status.outputsMask must match the expected final mask.
 * - No commError must occur.
 */

#include "HostComm.h"
#include "TestCase.h"
#include "log_core.h"
#include <Arduino.h>

class Test_TC14b_Status_AfterSeq : public ITestCase {
  public:
    const char *name() const override { return "TC14b: STATUS after SEQ (mask matches)"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "syncing link";

        _t0Ms = nowMs;
        _deadlineMs = nowMs + kTimeoutMs;

        comm.clearCommErrorFlag();
        comm.clearLastPongFlag();
        comm.clearNewStatusFlag();

        _syncNextPingMs = nowMs;
        _requested = false;
    }

    void tick(HostComm &comm, uint32_t nowMs) override {
        if (comm.hasCommError()) {
            _detail = "comm error flag set";
            _verdict = TestVerdict::Fail;
            return;
        }

        if ((int32_t)(nowMs - _deadlineMs) >= 0) {
            _detail = "timeout";
            _verdict = TestVerdict::Fail;
            return;
        }

        // Ensure synced
        if (!comm.linkSynced()) {
            if ((int32_t)(nowMs - _syncNextPingMs) >= 0) {
                comm.sendPing();
                _syncNextPingMs = nowMs + kPingPeriodMs;
            }
            _detail = "syncing link";
            return;
        }

        // Request STATUS exactly once
        if (!_requested) {
            comm.clearNewStatusFlag();
            comm.requestStatus();
            _requested = true;
            _detail = "requested STATUS, waiting";
            return;
        }

        if (!comm.hasNewStatus()) {
            _detail = "waiting for STATUS";
            return;
        }

        const ProtocolStatus &st = comm.getRemoteStatus();
        if (st.outputsMask != kExpectedFinalMask) {
            _detail = "STATUS mask mismatch";
            _verdict = TestVerdict::Fail;
            return;
        }

        _detail = "STATUS received and mask ok";
        _verdict = TestVerdict::Pass;
    }

    void exit(HostComm &comm, uint32_t /*nowMs*/) override {
        comm.clearNewStatusFlag();
        comm.clearLastPongFlag();
    }

    TestVerdict verdict() const override { return _verdict; }
    const char *detail() const override { return _detail; }

    void dump(HostComm &comm) const override {
        const ProtocolStatus &st = comm.getRemoteStatus();

        INFO("----------------------------------------------\n");
        INFO("- TestCase: %s\n", name());
        INFO("- %s\n", verdictToText(_verdict));
        INFO("- Detail: %s\n", _detail);
        INFO("- Flags: synced=%d streak=%u parseFail=%lu err=%d newStatus=%d\n",
             (int)comm.linkSynced(),
             (unsigned)comm.pongStreak(),
             (unsigned long)comm.parseFailCount(),
             (int)comm.hasCommError(),
             (int)comm.hasNewStatus());
        INFO("- STATUS: mask=0x%04X expected=0x%04X adc=[%u,%u,%u,%u] tempRaw=%d\n",
             (unsigned)st.outputsMask,
             (unsigned)kExpectedFinalMask,
             st.adcRaw[0], st.adcRaw[1], st.adcRaw[2], st.adcRaw[3],
             st.tempRaw);
        INFO("- LastBadLine: '%s'\n", comm.lastBadLine().c_str());
        INFO("----------------------------------------------\n");
    }

  private:
    static constexpr uint32_t kTimeoutMs = 2000;
    static constexpr uint32_t kPingPeriodMs = 200;

    // Must match TC14a’s computed final mask:
    // start: 0x00A5
    // upd:   (0x00A5 | 0x0002) & ~0x0004 = 0x00A3
    // tog:   0x00A3 ^ 0x000F = 0x00AC
    static constexpr uint16_t kExpectedFinalMask = 0x00AC;

    static const char *verdictToText(TestVerdict v) {
        switch (v) {
        case TestVerdict::Pass:
            return "PASSED";
        case TestVerdict::Fail:
            return "FAILED";
        case TestVerdict::Skip:
            return "SKIPPED";
        default:
            return "RUNNING";
        }
    }

    uint32_t _t0Ms = 0;
    uint32_t _deadlineMs = 0;
    uint32_t _syncNextPingMs = 0;

    bool _requested = false;

    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";
};

ITestCase *get_test_tc14b_status_after_seq() {
    static Test_TC14b_Status_AfterSeq inst;
    return &inst;
}