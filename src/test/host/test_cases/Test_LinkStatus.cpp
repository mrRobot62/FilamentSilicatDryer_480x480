// src/test/host/test_cases/Test_LinkStatus.cpp
#include "HostComm.h"
#include "TestCase.h"
#include "log_core.h"

class Test_LinkStatus : public ITestCase {
  public:
    const char *name() const override { return "Link: GET/STATUS receives STATUS"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "syncing link";

        _phase = Phase::Sync;
        _deadlineMs = nowMs + kTimeoutMs;

        _nextPingMs = nowMs; // ping immediately
        _statusSent = false;

        comm.clearCommErrorFlag();
        comm.clearNewStatusFlag();
        comm.clearLastPongFlag();
        comm.clearLinkSync();
    }

    void tick(HostComm &comm, uint32_t nowMs) override {
        // Only hard-fail on commError AFTER we tried to sync a bit.
        // During early sync we tolerate parse fails/junk (HostComm counts them).
        if (_phase != Phase::Sync && comm.hasCommError()) {
            _detail = "comm error flag set";
            _verdict = TestVerdict::Fail;
            return;
        }

        // global timeout
        if ((int32_t)(nowMs - _deadlineMs) >= 0) {
            _detail = (_phase == Phase::WaitStatus) ? "timeout waiting for STATUS" : "timeout syncing link";
            _verdict = TestVerdict::Fail;
            return;
        }

        switch (_phase) {
        case Phase::Sync: {
            // Resend PING periodically until HostComm declares link synced
            if ((int32_t)(nowMs - _nextPingMs) >= 0) {
                comm.sendPing();
                _nextPingMs = nowMs + kPingPeriodMs;
            }

            // Optional debug heartbeat (not too spammy)
            if ((int32_t)(nowMs - _nextDbgMs) >= 0) {
                _nextDbgMs = nowMs + 250;
                DBG("[LinkStatus] syncing: streak=%u synced=%u parseFail=%lu err=%d\n",
                    (unsigned)comm.pongStreak(),
                    (unsigned)comm.linkSynced(),
                    (unsigned long)comm.parseFailCount(),
                    comm.hasCommError());
            }

            if (comm.linkSynced()) {
                _phase = Phase::SendStatus;
                _detail = "link synced";
            }
            return;
        }

        case Phase::SendStatus: {
            // Clear stale flags and request STATUS once.
            comm.clearCommErrorFlag();
            comm.clearNewStatusFlag();

            comm.requestStatus();
            _statusSent = true;

            _phase = Phase::WaitStatus;
            _detail = "waiting for STATUS";
            return;
        }

        case Phase::WaitStatus: {
            if (comm.hasNewStatus()) {
                _detail = "STATUS received";
                _verdict = TestVerdict::Pass;
                return;
            }

            // Keep the RX side alive with occasional ping if you want (optional):
            if ((int32_t)(nowMs - _nextPingMs) >= 0) {
                comm.sendPing();
                _nextPingMs = nowMs + 800;
            }
            return;
        }
        }
    }

    void exit(HostComm &comm, uint32_t /*nowMs*/) override {
        comm.clearNewStatusFlag();
        comm.clearLastPongFlag();
    }

    TestVerdict verdict() const override { return _verdict; }
    const char *detail() const override { return _detail; }

  private:
    enum class Phase : uint8_t { Sync,
                                 SendStatus,
                                 WaitStatus };

    static constexpr uint32_t kTimeoutMs = 2500;
    static constexpr uint32_t kPingPeriodMs = 200;

    Phase _phase = Phase::Sync;

    uint32_t _deadlineMs = 0;
    uint32_t _nextPingMs = 0;
    uint32_t _nextDbgMs = 0;

    bool _statusSent = false;

    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";
};

ITestCase *get_test_link_status() {
    static Test_LinkStatus inst;
    return &inst;
}