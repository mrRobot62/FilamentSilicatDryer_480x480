#include "HostComm.h"
#include "TestCase.h"

class Test_SetAck : public ITestCase {
  public:
    const char *name() const override { return "Outputs: SET + ACK;SET updates remote mask"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _deadlineMs = nowMs + kTimeoutMs;
        _verdict = TestVerdict::Running;
        _detail = "setting mask";

        comm.clearCommErrorFlag();
        comm.clearLastSetAckFlag();

        _prevMask = comm.getLocalOutputsMask();
        _testMask = kPatternMask;

        comm.setOutputsMask(_testMask);
    }

    void tick(HostComm &comm, uint32_t nowMs) override {
        if (comm.hasCommError()) {
            _detail = "comm error flag set";
            _verdict = TestVerdict::Fail;
            return;
        }

        if (comm.lastSetAcked()) {
            const uint16_t remote = comm.getRemoteOutputsMask();
            if (remote != _testMask) {
                _detail = "ACK received but remote mask mismatch";
                _verdict = TestVerdict::Fail;
            } else {
                _detail = "ACK received and remote mask ok";
                _verdict = TestVerdict::Pass;
            }
            return;
        }

        if ((int32_t)(nowMs - _deadlineMs) >= 0) {
            _detail = "timeout waiting for ACK;SET";
            _verdict = TestVerdict::Fail;
            return;
        }
    }

    void exit(HostComm &comm, uint32_t /*nowMs*/) override {
        // Restore previous mask (optional, but keeps the system in a known state)
        comm.setOutputsMask(_prevMask);
        comm.clearLastSetAckFlag();
    }

    TestVerdict verdict() const override { return _verdict; }
    const char *detail() const override { return _detail; }

  private:
    static constexpr uint32_t kTimeoutMs = 800;
    static constexpr uint16_t kPatternMask = 0x00A5; // 0000 0000 1010 0101

    uint32_t _deadlineMs = 0;
    uint16_t _prevMask = 0;
    uint16_t _testMask = 0;

    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";
};

Test_SetAck g_test_set_ack;

ITestCase *get_test_set_ack() {
    static Test_SetAck inst;
    return &inst;
}