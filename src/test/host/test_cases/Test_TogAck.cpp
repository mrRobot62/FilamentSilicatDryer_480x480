// Test_TogAck.cpp
//
// TestCase: TOG + ACK;TOG
// Goal:
//   Verify that the host can toggle outputs using H;TOG and that the client
//   responds with C;ACK;TOG containing the resulting outputs mask.
// Expected:
//   - Link is synced (stable PONG streak).
//   - Host sends H;TOG;TTTT.
//   - Client replies C;ACK;TOG;MMMM.
//   - HostComm remote mask equals (startMask XOR togMask).
//   - No comm error, parseFailCount stable (or at least not exploding).

#include "HostComm.h"
#include "TestCase.h"
#include "log_core.h"

class Test_TogAck : public ITestCase {
  public:
    const char *name() const override { return "Outputs: TOG + ACK;TOG updates remote mask"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "init";
        _tStartMs = nowMs;
        _deadlineMs = nowMs + kTimeoutMs;

        // reset flags for a clean run
        comm.clearCommErrorFlag();
        comm.clearLastPongFlag();
        comm.clearLinkSync();

        // if you added the flag:
        comm.clearLastTogAckFlag();

        _baselineParseFail = comm.parseFailCount();

        // Choose a deterministic starting mask for the test
        _startMask = 0x0000;
        _togMask = 0x000F; // toggle CH0..CH3 (visible if LEDs are connected)

        _expectedMask = (uint16_t)(_startMask ^ _togMask);

        _phase = Phase::Sync;
        _nextPingMs = nowMs; // send immediately
        _detail = "syncing link";
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

        // periodic ping while not synced (or as keepalive)
        if ((int32_t)(nowMs - _nextPingMs) >= 0) {
            comm.sendPing();
            _nextPingMs = nowMs + kPingPeriodMs;
        }

        switch (_phase) {
        case Phase::Sync:
            // We consider link synced once HostComm marks it
            if (comm.linkSynced()) {
                _phase = Phase::SetBaseline;
                _detail = "synced, setting baseline mask";
                // Make sure client is in known state before toggling
                comm.clearLastSetAckFlag();
                comm.setOutputsMask(_startMask);
            }
            return;

        case Phase::SetBaseline:
            if (comm.lastSetAcked() && comm.getRemoteOutputsMask() == _startMask) {
                _phase = Phase::SendTog;
                _detail = "baseline ok, sending TOG";
            }
            return;

        case Phase::SendTog:
            // send TOG and wait for ACK;TOG
            comm.clearLastTogAckFlag();
            comm.togOutputs(_togMask);
            _phase = Phase::WaitAck;
            _detail = "waiting for ACK;TOG";
            return;

        case Phase::WaitAck:
            if (comm.lastTogAcked()) {
                const uint16_t got = comm.getRemoteOutputsMask();
                if (got == _expectedMask) {
                    _detail = "ACK received and remote mask ok";
                    _verdict = TestVerdict::Pass;
                } else {
                    _detail = "ACK received but remote mask mismatch";
                    _verdict = TestVerdict::Fail;
                }
                return;
            }
            return;

        default:
            _detail = "invalid state";
            _verdict = TestVerdict::Fail;
            return;
        }
    }

    void exit(HostComm &comm, uint32_t /*nowMs*/) override {
        // clear volatile flags for next tests
        comm.clearLastPongFlag();
        comm.clearLastTogAckFlag();
        dump(comm);
    }

    TestVerdict verdict() const override { return _verdict; }
    const char *detail() const override { return _detail; }

    void dump(HostComm &comm) const override {
        RAW("----------------------------------------------\n");
        RAW("- TestCase: %s\n", name());
        RAW("- Verdict:  %s\n", verdictStr(_verdict));
        RAW("- Detail:   %s\n", _detail);
        RAW("- Flags: synced=%u pongStreak=%u commErr=%u parseFail=%lu (+%lu)\n",
            comm.linkSynced() ? 1u : 0u,
            (unsigned)comm.pongStreak(),
            comm.hasCommError() ? 1u : 0u,
            (unsigned long)comm.parseFailCount(),
            (unsigned long)(comm.parseFailCount() - _baselineParseFail));
        RAW("- RemoteMask: 0x%04X\n", comm.getRemoteOutputsMask());
        RAW("- StartMask:  0x%04X\n", _startMask);
        RAW("- TogMask:    0x%04X\n", _togMask);
        RAW("- Expected:   0x%04X\n", _expectedMask);
        RAW("----------------------------------------------\n");
    }

  private:
    enum class Phase : uint8_t { Sync,
                                 SetBaseline,
                                 SendTog,
                                 WaitAck };

    static constexpr uint32_t kTimeoutMs = 3000;
    static constexpr uint32_t kPingPeriodMs = 200;

    static const char *verdictStr(TestVerdict v) {
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

    Phase _phase = Phase::Sync;

    uint32_t _tStartMs = 0;
    uint32_t _deadlineMs = 0;
    uint32_t _nextPingMs = 0;

    uint32_t _baselineParseFail = 0;

    uint16_t _startMask = 0;
    uint16_t _togMask = 0;
    uint16_t _expectedMask = 0;

    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";
};

ITestCase *get_test_tog_ack() {
    static Test_TogAck inst;
    return &inst;
}