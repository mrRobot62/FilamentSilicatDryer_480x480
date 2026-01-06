#include "HostComm.h"
#include "LinkSync.h"
#include "TestCase.h"
#include "log_core.h"

class Test_PingPong : public ITestCase {
  public:
    const char *name() const override { return "Link: PING/PONG"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "syncing link";

        _sync.timeoutMs = 2000;
        _sync.pingPeriodMs = 200;
        _sync.needStreak = 2;
        _sync.reset(comm, nowMs);
    }

    void tick(HostComm &comm, uint32_t nowMs) override {
        if (_sync.tick(comm, nowMs)) {
            _detail = "PONG received, link synced";
            _verdict = TestVerdict::Pass;
            // DBG("[PingPong] streak=%u synced=%u parseFail=%lu err=%u\n",
            //     _sync.pongStreak, _sync.synced,
            //     (unsigned long)_sync.parseFailDelta(comm),
            //     comm.hasCommError());
            return;
        }

        if (comm.hasCommError()) {
            _detail = "comm error flag set";
            _verdict = TestVerdict::Fail;
            return;
        }

        if (_sync.timedOut(nowMs)) {
            _detail = "timeout waiting for PONG";
            _verdict = TestVerdict::Fail;
            // DBG("[PingPong] streak=%u synced=%u parseFail=%lu err=%u\n",
            //     _sync.pongStreak, _sync.synced,
            //     (unsigned long)_sync.parseFailDelta(comm),
            //     comm.hasCommError());
            return;
        }
    }

    void exit(HostComm &comm, uint32_t /*nowMs*/) override {
        comm.clearLastPongFlag();
    }

    TestVerdict verdict() const override { return _verdict; }
    const char *detail() const override { return _detail; }

  private:
    LinkSync _sync{};
    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";
};

ITestCase *get_test_ping_pong() {
    static Test_PingPong inst;
    return &inst;
}