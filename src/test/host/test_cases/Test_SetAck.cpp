#include "HostComm.h"
#include "TestCase.h"
#include "log_core.h"

/**
 * Test: Outputs: SET + ACK;SET updates remote mask
 *
 * Strategy (robust, no assumptions):
 * 1) Sync link: send PING periodically until HostComm.linkSynced()==true
 * 2) Send SET to a known mask
 * 3) Wait for ACK;SET (HostComm.lastSetAcked()==true)
 * 4) Verify HostComm.getRemoteOutputsMask() == expected mask
 *
 * Diagnostics:
 * - comm.hasCommError()
 * - comm.parseFailCount()
 * - comm.lastBadLine()
 */

class Test_SetAck : public ITestCase {
  public:
    const char *name() const override { return "Outputs: SET + ACK;SET updates remote mask"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "syncing link";
        _deadlineMs = nowMs + kTimeoutMs;

        _state = State::Sync;
        _nextPingMs = nowMs; // send immediately

        comm.clearCommErrorFlag();
        comm.clearLastPongFlag();
        comm.clearLinkSync();

        comm.clearLastSetAckFlag();
        _setSent = false;
    }

    void tick(HostComm &comm, uint32_t nowMs) override {
        // Hard fail if comm error is set
        if (comm.hasCommError()) {
            _detail = "comm error flag set";
            _verdict = TestVerdict::Fail;
            return;
        }

        // Timeout
        if ((int32_t)(nowMs - _deadlineMs) >= 0) {
            _detail = "timeout";
            _verdict = TestVerdict::Fail;
            return;
        }

        switch (_state) {
        case State::Sync: {
            // Periodic ping until synced
            if (!comm.linkSynced()) {
                if ((int32_t)(nowMs - _nextPingMs) >= 0) {
                    comm.sendPing();
                    _nextPingMs = nowMs + kPingPeriodMs;
                }

                // Optional periodic debug (kept light)
                if ((int32_t)(nowMs - _nextDbgMs) >= 0) {
                    _nextDbgMs = nowMs + 250;
                    DBG("[SetAck] syncing: streak=%u synced=%u parseFail=%lu err=%u\n",
                        comm.pongStreak(),
                        comm.linkSynced(),
                        (unsigned long)comm.parseFailCount(),
                        comm.hasCommError());
                }
                return;
            }

            // Link synced -> proceed
            _state = State::SendSet;
            _detail = "link synced, sending SET";
            // fallthrough on next tick (keep deterministic)
            return;
        }

        case State::SendSet: {
            if (!_setSent) {
                comm.clearLastSetAckFlag();

                // choose a non-trivial pattern (same as you used before)
                comm.setOutputsMask(kTestMask);
                _setSent = true;

                _detail = "waiting for ACK;SET";
                _state = State::WaitAck;
            }
            return;
        }

        case State::WaitAck: {
            // If parsing issues occur without commError (e.g. junk filtered before parse),
            // we still want useful detail.
            if (comm.lastSetAcked()) {
                _state = State::Verify;
                _detail = "ACK received, verifying mask";
                return;
            }

            // Periodic debug if we’re waiting too long
            if ((int32_t)(nowMs - _nextDbgMs) >= 0) {
                _nextDbgMs = nowMs + 250;
                DBG("[SetAck] waiting: ack=%u remote=0x%04X parseFail=%lu err=%u\n",
                    comm.lastSetAcked(),
                    comm.getRemoteOutputsMask(),
                    (unsigned long)comm.parseFailCount(),
                    comm.hasCommError());

                // If parse failures happen, show last bad line once in a while
                if (comm.parseFailCount() != _lastParseFailSeen) {
                    _lastParseFailSeen = comm.parseFailCount();
                    WARN("[SetAck] parseFail=%lu lastBadLine='%s'\n",
                         (unsigned long)comm.parseFailCount(),
                         comm.lastBadLine().c_str());
                }
            }
            return;
        }

        case State::Verify: {
            const uint16_t remote = comm.getRemoteOutputsMask();
            if (remote == kTestMask) {
                _detail = "ACK received and remote mask ok";
                _verdict = TestVerdict::Pass;
            } else {
                static char buf[96];
                snprintf(buf, sizeof(buf),
                         "remote mask mismatch (expected 0x%04X got 0x%04X)",
                         kTestMask, remote);
                _detail = buf;
                _verdict = TestVerdict::Fail;
            }
            return;
        }

        default:
            _detail = "internal state error";
            _verdict = TestVerdict::Fail;
            return;
        }
    }

    void exit(HostComm &comm, uint32_t /*nowMs*/) override {
        // Keep state clean for next test
        comm.clearLastSetAckFlag();
        comm.clearLastPongFlag();
    }

    TestVerdict verdict() const override { return _verdict; }
    const char *detail() const override { return _detail; }

  private:
    enum class State : uint8_t { Sync,
                                 SendSet,
                                 WaitAck,
                                 Verify };

    static constexpr uint32_t kTimeoutMs = 2500;
    static constexpr uint32_t kPingPeriodMs = 200;
    static constexpr uint16_t kTestMask = 0x00A5;

    State _state = State::Sync;

    uint32_t _deadlineMs = 0;
    uint32_t _nextPingMs = 0;
    uint32_t _nextDbgMs = 0;

    bool _setSent = false;

    uint32_t _lastParseFailSeen = 0;

    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";
    uint16_t _expectedMask = 0x00A5;

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
        Serial.printf("  maskMatches     = %d\n", comm.getRemoteOutputsMask() == _expectedMask ? 1 : 0);
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
};

ITestCase *get_test_set_ack() {
    static Test_SetAck inst;
    return &inst;
}