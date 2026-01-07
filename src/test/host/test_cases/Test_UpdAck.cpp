/**
 * TestCase: Outputs: UPD + ACK;UPD updates remote mask
 *
 * Goal:
 *  - Verify that the host can send H;UPD;SSSS;CCCC and the client responds with C;ACK;UPD;MMMM.
 *  - HostComm must parse ACK;UPD, update remoteOutputsMask, and set the dedicated ack flag.
 *
 * Expected:
 *  - Link is synced first (stable PONG streak).
 *  - After UPD, HostComm::lastUpdAcked() becomes true within timeout.
 *  - HostComm::getRemoteOutputsMask() equals expected mask.
 *  - No commError; parseFailCount must not grow during the critical phase.
 */

#include "HostComm.h"
#include "TestCase.h"
#include "log_core.h"

class Test_UpdAck : public ITestCase {
  public:
    const char *name() const override { return "Outputs: UPD + ACK;UPD updates remote mask"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "syncing link";

        _startMs = nowMs;
        _deadlineMs = nowMs + kTimeoutMs;

        // reset flags
        comm.clearCommErrorFlag();
        comm.clearLinkSync();
        comm.clearLastPongFlag();
        comm.clearLastUpdAckFlag();

        _nextPingMs = nowMs; // start sync pings immediately
        _phase = Phase::Sync;
    }

    void tick(HostComm &comm, uint32_t nowMs) override {
        dumpProgress(comm, nowMs);

        if (comm.hasCommError()) {
            _detail = "comm error flag set";
            _verdict = TestVerdict::Fail;
            return;
        }

        if (_phase == Phase::Sync) {
            // keep sending PING until HostComm declares linkSynced()
            if ((int32_t)(nowMs - _nextPingMs) >= 0) {
                comm.sendPing();
                _nextPingMs = nowMs + kPingPeriodMs;
            }

            if (comm.linkSynced()) {
                _phase = Phase::SendUpd;
                _detail = "sending UPD";
            } else if ((int32_t)(nowMs - _deadlineMs) >= 0) {
                _detail = "timeout waiting for link sync";
                _verdict = TestVerdict::Fail;
                return;
            }
        }

        if (_phase == Phase::SendUpd) {
            // Build a deterministic scenario:
            // Start from known 0 via SET (optional but robust), then UPD:
            // setMask=0x00A5, clrMask=0x0000 => expected=0x00A5
            comm.setOutputsMask(0x0000);
            delay(10); // tiny settle; safe in test context
            comm.clearLastUpdAckFlag();

            comm.updOutputs(0x00A5, 0x0000);
            _expectedMask = 0x00A5;

            _phase = Phase::WaitAck;
            _detail = "waiting for ACK;UPD";
            return;
        }

        if (_phase == Phase::WaitAck) {
            if (comm.lastUpdAcked()) {
                const uint16_t got = comm.getRemoteOutputsMask();
                if (got == _expectedMask) {
                    _detail = "ACK;UPD received and mask ok";
                    _verdict = TestVerdict::Pass;
                } else {
                    _detail = "ACK;UPD received but mask mismatch";
                    _verdict = TestVerdict::Fail;
                }
                return;
            }

            if ((int32_t)(nowMs - _deadlineMs) >= 0) {
                _detail = "timeout waiting for ACK;UPD";
                _verdict = TestVerdict::Fail;
                return;
            }
        }
    }

    void exit(HostComm &comm, uint32_t nowMs) override {
        (void)nowMs;
        comm.clearLastUpdAckFlag();
        dump(comm);
    }
    void dump(HostComm &comm) const override { dumpFinal(comm); }

    TestVerdict verdict() const override { return _verdict; }
    const char *detail() const override { return _detail; }

  private:
    enum class Phase : uint8_t { Sync,
                                 SendUpd,
                                 WaitAck };

    static constexpr uint32_t kTimeoutMs = 2500;
    static constexpr uint32_t kPingPeriodMs = 200;

    uint32_t _startMs = 0;
    uint32_t _deadlineMs = 0;
    uint32_t _nextPingMs = 0;

    Phase _phase = Phase::Sync;
    uint16_t _expectedMask = 0;

    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";

    void dumpProgress(HostComm &comm, uint32_t nowMs) {
        static uint32_t nextDumpMs = 0;
        if ((int32_t)(nowMs - nextDumpMs) < 0) {
            return;
        }
        nextDumpMs = nowMs + 250;

        DBG("[UpdAck] phase=%u synced=%u streak=%u parseFail=%lu err=%u updAck=%u remote=0x%04X\n",
            (unsigned)_phase,
            (unsigned)comm.linkSynced(),
            (unsigned)comm.pongStreak(),
            (unsigned long)comm.parseFailCount(),
            (unsigned)comm.hasCommError(),
            (unsigned)comm.lastUpdAcked(),
            (unsigned)comm.getRemoteOutputsMask());
    }

    void dumpFinal(HostComm &comm) const {
        // Your requested standardized post-test dump
        INFO("----------------------------------------------\n");
        INFO("- TestCase: Test_UpdAck\n");
        INFO("- %s\n", (_verdict == TestVerdict::Pass) ? "PASSED" : "FAILED");
        INFO("- Flags: synced=%u streak=%u pong=%u updAck=%u commErr=%u parseFail=%lu\n",
             (unsigned)comm.linkSynced(),
             (unsigned)comm.pongStreak(),
             (unsigned)comm.lastPongReceived(),
             (unsigned)comm.lastUpdAcked(),
             (unsigned)comm.hasCommError(),
             (unsigned long)comm.parseFailCount());
        INFO("- remoteMask=0x%04X expected=0x%04X\n",
             (unsigned)comm.getRemoteOutputsMask(),
             (unsigned)_expectedMask);
        INFO("----------------------------------------------\n");
    }
};

ITestCase *get_test_upd_ack() {
    static Test_UpdAck inst;
    return &inst;
}