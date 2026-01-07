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

    void dump(HostComm &comm) const override {
        Serial.println("--------------------------------------------------");
        Serial.printf("TestCase: %s\n", name());

        const char *res = "UNKNOWN";
        if (verdict() == TestVerdict::Pass) {
            res = "PASS";
        }
        if (verdict() == TestVerdict::Fail) {
            res = "FAIL";
        }
        if (verdict() == TestVerdict::Skip) {
            res = "SKIP";
        }
        Serial.printf("Result:   %s\n", res);

        Serial.println();
        Serial.println("Flags:");
        Serial.printf("  linkSynced      = %d\n", comm.linkSynced() ? 1 : 0);
        Serial.printf("  pongStreak      = %u\n", (unsigned)comm.pongStreak());
        Serial.printf("  lastPong        = %d\n", comm.lastPongReceived() ? 1 : 0);
        Serial.printf("  newStatus       = %d\n", comm.hasNewStatus() ? 1 : 0);
        Serial.printf("  lastSetAcked    = %d\n", comm.lastSetAcked() ? 1 : 0);
        Serial.printf("  commError       = %d\n", comm.hasCommError() ? 1 : 0);
        Serial.printf("  parseFailCount  = %lu\n", (unsigned long)comm.parseFailCount());

        Serial.println();
        Serial.println("Remote:");
        Serial.printf("  outputsMask     = 0x%04X\n", comm.getRemoteOutputsMask());

        // Optional diagnostics (helpful when parseFailCount > 0)
        if (comm.parseFailCount() > 0) {
            Serial.println();
            Serial.println("LastBadLine:");
            Serial.printf("  '%s'\n", comm.lastBadLine().c_str());
        }

        Serial.println();
        Serial.println("Details:");
        Serial.printf("  %s\n", detail());

        Serial.println("--------------------------------------------------");
    }

  private:
    LinkSync _sync{};
    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";
};

ITestCase *get_test_ping_pong() {
    static Test_PingPong inst;
    return &inst;
}