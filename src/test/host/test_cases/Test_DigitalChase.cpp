#include "HostComm.h"
#include "TestCase.h"

class Test_DigitalChase : public ITestCase {
  public:
    const char *name() const override { return "Outputs: CH0..CH7 chase (visual)"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "syncing link";

        _deadlineMs = nowMs + kTimeoutMs;

        // reset local test state
        _phase = Phase::Sync;
        _nextPingMs = nowMs;
        _nextStepMs = 0;
        _idx = 0;
        _expectedMask = 0;

        comm.clearCommErrorFlag();
        comm.clearLastPongFlag();
        comm.clearLinkSync();
        comm.clearLastSetAckFlag();

        // start with all off
        comm.setOutputsMask(0x0000);
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

        // --- Phase 1: Sync like PingPong ---
        if (_phase == Phase::Sync) {
            if (comm.linkSynced()) {
                _phase = Phase::Run;
                _detail = "running chase pattern";
                _nextStepMs = nowMs; // start immediately
                return;
            }

            if ((int32_t)(nowMs - _nextPingMs) >= 0) {
                comm.sendPing();
                _nextPingMs = nowMs + kPingPeriodMs;
            }
            return;
        }

        // --- Phase 2: Run chase pattern ---
        if ((int32_t)(nowMs - _nextStepMs) < 0) {
            return;
        }

        // generate mask: exactly one bit 0..7
        _expectedMask = (uint16_t)(1u << _idx);

        comm.clearLastSetAckFlag();
        comm.setOutputsMask(_expectedMask);

        // We don't hard-require ACK per step (could be noisy), but we can sanity-check:
        // if ACK arrives, remote mask should match.
        if (comm.lastSetAcked()) {
            if (comm.getRemoteOutputsMask() != _expectedMask) {
                _detail = "ACK received but remote mask mismatch";
                _verdict = TestVerdict::Fail;
                return;
            }
        }

        _idx++;
        if (_idx >= 8) {
            _idx = 0;
            _loopsDone++;
        }

        _nextStepMs = nowMs + kStepPeriodMs;

        // Finish after N loops so it doesn't run forever in runner
        if (_loopsDone >= kLoopsToRun) {
            comm.setOutputsMask(0x0000); // all off at end
            _detail = "completed visual chase";
            _verdict = TestVerdict::Pass;
            return;
        }
    }

    void exit(HostComm &comm, uint32_t /*nowMs*/) override {
        comm.setOutputsMask(0x0000);
        comm.clearLastSetAckFlag();
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
        Serial.printf("  expectedMask    = 0x%04X\n", _expectedMask);

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
    enum class Phase : uint8_t { Sync,
                                 Run };

    static constexpr uint32_t kTimeoutMs = 15000;  // overall test max
    static constexpr uint32_t kPingPeriodMs = 200; // sync ping rate
    static constexpr uint32_t kStepPeriodMs = 250; // LED step speed
    static constexpr uint8_t kLoopsToRun = 4;      // how many 0..7 cycles

    Phase _phase = Phase::Sync;

    uint32_t _deadlineMs = 0;
    uint32_t _nextPingMs = 0;
    uint32_t _nextStepMs = 0;

    uint8_t _idx = 0;
    uint8_t _loopsDone = 0;

    mutable uint16_t _expectedMask = 0;

    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";
};

ITestCase *get_test_digital_chase() {
    static Test_DigitalChase inst;
    return &inst;
}