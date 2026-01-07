
#include "HostComm.h"
#include "TestCase.h"

class Test_DigitalBlinkAll : public ITestCase {
  public:
    const char *name() const override { return "Outputs: CH0..CH7 blink ALL (visual)"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "syncing link";

        _deadlineMs = nowMs + kTimeoutMs;

        _phase = Phase::Sync;
        _nextPingMs = nowMs; // immediate
        _nextToggleMs = 0;

        _blinksDone = 0;
        _stateOn = false;
        _expectedMask = 0x0000;

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

        // -------- Phase 1: Link sync --------
        if (_phase == Phase::Sync) {
            if (comm.linkSynced()) {
                _phase = Phase::Run;
                _detail = "blinking all outputs";
                _nextToggleMs = nowMs; // start immediately
                return;
            }

            if ((int32_t)(nowMs - _nextPingMs) >= 0) {
                comm.sendPing();
                _nextPingMs = nowMs + kPingPeriodMs;
            }
            return;
        }

        // -------- Phase 2: Blink ALL --------
        if ((int32_t)(nowMs - _nextToggleMs) < 0) {
            return;
        }

        _stateOn = !_stateOn;
        _expectedMask = _stateOn ? kAllMask : 0x0000;

        comm.clearLastSetAckFlag();
        comm.setOutputsMask(_expectedMask);

        // optional sanity check if ACK arrives
        if (comm.lastSetAcked()) {
            if (comm.getRemoteOutputsMask() != _expectedMask) {
                _detail = "ACK received but remote mask mismatch";
                _verdict = TestVerdict::Fail;
                return;
            }
        }

        // count full blink cycle (OFF->ON->OFF) as 1
        if (!_stateOn) {
            _blinksDone++;
        }

        _nextToggleMs = nowMs + kTogglePeriodMs;

        if (_blinksDone >= kBlinkCycles) {
            comm.setOutputsMask(0x0000);
            _detail = "completed blink all";
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

    static constexpr uint16_t kAllMask = 0x00FF; // CH0..CH7
    static constexpr uint32_t kTimeoutMs = 15000;
    static constexpr uint32_t kPingPeriodMs = 200;
    static constexpr uint32_t kTogglePeriodMs = 350; // blink speed
    static constexpr uint8_t kBlinkCycles = 8;       // number of ON/OFF cycles

    Phase _phase = Phase::Sync;

    uint32_t _deadlineMs = 0;
    uint32_t _nextPingMs = 0;
    uint32_t _nextToggleMs = 0;

    uint8_t _blinksDone = 0;
    bool _stateOn = false;

    mutable uint16_t _expectedMask = 0x0000;

    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";
};

ITestCase *get_test_digital_blink_all() {
    static Test_DigitalBlinkAll inst;
    return &inst;
}