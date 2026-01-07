#include "HostComm.h"
#include "TestCase.h"

class Test_VisualLedSweep : public ITestCase {
  public:
    const char *name() const override {
        return "Outputs: Visual LED Sweep CH0-CH7";
    }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "starting LED sweep";

        _channel = 0;
        _nextStepMs = nowMs;

        comm.clearCommErrorFlag();
    }

    void tick(HostComm &comm, uint32_t nowMs) override {
        if (comm.hasCommError()) {
            _detail = "comm error";
            _verdict = TestVerdict::Fail;
            return;
        }

        if ((int32_t)(nowMs - _nextStepMs) < 0) {
            return;
        }

        if (_channel < 8) {
            uint16_t mask = (1u << _channel);
            comm.setOutputsMask(mask);

            _detail = "LED CH" + String(_channel);
            _channel++;
            _nextStepMs = nowMs + kStepDelayMs;
            return;
        }

        // done
        comm.setOutputsMask(0x0000);
        _detail = "LED sweep done";
        _verdict = TestVerdict::Pass;
    }

    void exit(HostComm &comm, uint32_t) override {
        comm.setOutputsMask(0x0000);
    }

    TestVerdict verdict() const override { return _verdict; }
    const char *detail() const override { return _detail.c_str(); }

    void dump(HostComm &comm) const override {
        Serial.println("--------------------------------------------------");
        Serial.printf("TestCase: %s\n", name());
        Serial.printf("Result:   %s\n", verdict() == TestVerdict::Pass ? "PASS" : "FAIL");

        Serial.println("\nFlags:");
        Serial.printf("  linkSynced      = %d\n", comm.linkSynced());
        Serial.printf("  pongStreak      = %u\n", comm.pongStreak());
        Serial.printf("  lastPong        = %d\n", comm.lastPongReceived());
        Serial.printf("  newStatus       = %d\n", comm.hasNewStatus());
        Serial.printf("  lastSetAcked    = %d\n", comm.lastSetAcked());
        Serial.printf("  commError       = %d\n", comm.hasCommError());
        Serial.printf("  parseFailCount  = %lu\n", comm.parseFailCount());

        Serial.println("\nRemote:");
        Serial.printf("  outputsMask     = 0x%04X\n", comm.getRemoteOutputsMask());

        Serial.println("--------------------------------------------------");
    }

  private:
    static constexpr uint32_t kStepDelayMs = 700;

    uint8_t _channel = 0;
    uint32_t _nextStepMs = 0;

    TestVerdict _verdict = TestVerdict::Running;
    String _detail;
};

ITestCase *get_test_visual_led_sweep() {
    static Test_VisualLedSweep inst;
    return &inst;
}