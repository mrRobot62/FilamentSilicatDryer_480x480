#include "HostComm.h"
#include "TestCase.h"

class Test_PingPong : public ITestCase {
  public:
    const char *name() const override { return "Link: PING/PONG"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "sending PING";

        _deadlineMs = nowMs + kTimeoutMs;

        comm.clearCommErrorFlag();
        comm.clearLastPongFlag();

        comm.sendPing();
        _detail = "waiting for PONG";
    }

    void tick(HostComm &comm, uint32_t nowMs) override {
        if (comm.hasCommError()) {
            _detail = "comm error flag set";
            _verdict = TestVerdict::Fail;
            return;
        }

        if (comm.lastPongReceived()) {
            _detail = "PONG received";
            _verdict = TestVerdict::Pass;
            return;
        }

        if ((int32_t)(nowMs - _deadlineMs) >= 0) {
            _detail = "timeout waiting for PONG";
            _verdict = TestVerdict::Fail;
            return;
        }
    }

    void exit(HostComm &comm, uint32_t /*nowMs*/) override {
        comm.clearLastPongFlag();
    }

    TestVerdict verdict() const override { return _verdict; }
    const char *detail() const override { return _detail; }

  private:
    static constexpr uint32_t kTimeoutMs = 800;

    uint32_t _deadlineMs = 0;
    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";
};

ITestCase *get_test_ping_pong() {
    static Test_PingPong inst;
    return &inst;
}