/**
 * TestCase: RST handling (Host -> Client)
 *
 * Goal:
 * - Verify that sending H;RST does not break the link permanently.
 * - Verify that the host recovers into a synced state afterwards.
 *
 * Expected behavior:
 * - After H;RST, the client either responds with C;RST (if implemented)
 *   OR the link temporarily loses sync and then can be re-synced via PING/PONG.
 * - No permanent comm error.
 */

#include "HostComm.h"
#include "TestCase.h"
#include "log_core.h"

class Test_RstHandling : public ITestCase {
  public:
    const char *name() const override { return "Link: RST handling + recovery"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "arming";

        _startMs = nowMs;
        _deadlineMs = nowMs + kTimeoutMs;

        // Baseline snapshot for dump()
        _startParseFail = comm.parseFailCount();

        comm.clearCommErrorFlag();
        comm.clearLastPongFlag();
        comm.clearNewStatusFlag();
        comm.clearLastSetAckFlag();
        comm.clearLinkSync(); // force re-sync logic after RST

        _phase = Phase::SendRst;
        _nextActionMs = nowMs; // immediately
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

        switch (_phase) {
        case Phase::SendRst:
            if ((int32_t)(nowMs - _nextActionMs) >= 0) {
                comm.sendRst();
                _detail = "RST sent, waiting for recovery";
                _phase = Phase::Recover;
                _nextActionMs = nowMs + 50;
            }
            break;

        case Phase::Recover:
            // Try to re-sync by periodically pinging until we reach synced state
            if (!comm.linkSynced()) {
                if ((int32_t)(nowMs - _nextActionMs) >= 0) {
                    comm.sendPing();
                    _nextActionMs = nowMs + kPingPeriodMs;
                }
                return;
            }

            // Synced again -> PASS
            _detail = "recovered, link synced";
            _verdict = TestVerdict::Pass;
            return;
        }
    }

    void exit(HostComm &comm, uint32_t /*nowMs*/) override {
        // leave system in a clean state for next tests
        comm.clearLastPongFlag();
    }

    TestVerdict verdict() const override { return _verdict; }
    const char *detail() const override { return _detail; }

    void dump(HostComm &comm) const override {
        const uint32_t nowFail = comm.parseFailCount();
        const uint32_t deltaFail = (nowFail >= _startParseFail) ? (nowFail - _startParseFail) : 0;

        Serial.println("----------------------------------------------");
        Serial.printf("- TestCase: %s\n", name());
        Serial.printf("- %s\n", (_verdict == TestVerdict::Pass) ? "PASSED" : (_verdict == TestVerdict::Fail) ? "FAILED"
                                                                                                             : "RUNNING");
        Serial.printf("- Detail: %s\n", _detail);
        Serial.printf("- Flags: synced=%u streak=%u parseFail=%lu (+%lu) err=%u lastPong=%u\n",
                      (unsigned)comm.linkSynced(),
                      (unsigned)comm.pongStreak(),
                      (unsigned long)nowFail,
                      (unsigned long)deltaFail,
                      (unsigned)comm.hasCommError(),
                      (unsigned)comm.lastPongReceived());
        Serial.printf("- LastBadLine: '%s'\n", comm.lastBadLine().c_str());
        Serial.println("----------------------------------------------");
    }

  private:
    enum class Phase : uint8_t {
        SendRst,
        Recover,
    };

    static constexpr uint32_t kTimeoutMs = 3000;
    static constexpr uint32_t kPingPeriodMs = 200;

    uint32_t _startMs = 0;
    uint32_t _deadlineMs = 0;
    uint32_t _nextActionMs = 0;

    uint32_t _startParseFail = 0;

    Phase _phase = Phase::SendRst;

    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";
};

ITestCase *get_test_rst_handling() {
    static Test_RstHandling inst;
    return &inst;
}