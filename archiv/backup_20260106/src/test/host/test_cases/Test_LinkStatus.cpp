#include "HostComm.h"
#include "TestCase.h"

class Test_LinkStatus : public ITestCase {
  public:
    const char *name() const override { return "Link: GET/STATUS receives STATUS"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _startMs = nowMs;
        _deadlineMs = nowMs + kTimeoutMs;
        _detail = "waiting";
        _verdict = TestVerdict::Running;

        comm.clearCommErrorFlag();
        comm.clearNewStatusFlag();
        comm.requestStatus();
    }

    void tick(HostComm &comm, uint32_t nowMs) override {
        if (comm.hasCommError()) {
            _detail = "comm error flag set";
            _verdict = TestVerdict::Fail;
            return;
        }

        if (comm.hasNewStatus()) {
            _detail = "status received";
            _verdict = TestVerdict::Pass;
            return;
        }

        if ((int32_t)(nowMs - _deadlineMs) >= 0) {
            _detail = "timeout waiting for STATUS";
            _verdict = TestVerdict::Fail;
            return;
        }
    }

    void exit(HostComm &comm, uint32_t /*nowMs*/) override {
        comm.clearNewStatusFlag();
    }

    TestVerdict verdict() const override { return _verdict; }
    const char *detail() const override { return _detail; }

  private:
    static constexpr uint32_t kTimeoutMs = 800;

    uint32_t _startMs = 0;
    uint32_t _deadlineMs = 0;
    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";
};

// Export a factory or create a global instance (recommended: static lifetime)
Test_LinkStatus g_test_link_status;

ITestCase *get_test_link_status() {
    static Test_LinkStatus inst;
    return &inst;
}