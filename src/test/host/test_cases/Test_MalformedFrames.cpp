/**
 * TestCase: Test_MalformedFrames
 *
 * Goal:
 *   Ensure HostComm parser/line filtering is robust against malformed frames and junk.
 *
 * Expected behavior:
 *   - Feeding malformed lines via HostComm::processLine(...) increments parseFailCount.
 *   - HostComm does NOT latch commError just because of junk.
 *   - After junk injection, HostComm can still recover on real UART traffic:
 *     send PING and receive PONG (link synced / PONG received).
 */

#include "HostComm.h"
#include "TestCase.h"
#include "log_core.h"
#include <Arduino.h>

class Test_MalformedFrames : public ITestCase {
  public:
    const char *name() const override { return "Parser: malformed/junk handling + recovery"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "injecting junk";

        _tEnterMs = nowMs;
        _deadlineMs = nowMs + kTimeoutMs;

        // reset relevant flags
        comm.clearCommErrorFlag();
        comm.clearLastPongFlag();
        comm.clearLinkSync();

        _startParseFail = comm.parseFailCount();
        _phase = Phase::Inject;
    }

    void tick(HostComm &comm, uint32_t nowMs) override {
        if ((int32_t)(nowMs - _deadlineMs) >= 0) {
            _detail = "timeout";
            _verdict = TestVerdict::Fail;
            return;
        }

        switch (_phase) {
        case Phase::Inject:
            _injectedCount = injectJunk(comm);
            _detail = "verifying parseFailCount increased";
            _phase = Phase::Verify;
            return;

        case Phase::Verify: {
            const uint32_t nowFail = comm.parseFailCount();
            const uint32_t delta = (nowFail >= _startParseFail) ? (nowFail - _startParseFail) : 0;

            // We injected kExpectedMinFails "bad" lines (see injectJunk()).
            //            if (delta < kExpectedMinFails) {
            if (delta < _injectedCount) {
                _detail = "parseFailCount did not increase as expected";
                _verdict = TestVerdict::Fail;

                DBG("[TC10] parseFail: start=%lu now=%lu delta=%lu expectedMin=%lu\n",
                    (unsigned long)_startParseFail,
                    (unsigned long)comm.parseFailCount(),
                    (unsigned long)delta,
                    (unsigned long)_injectedCount);

                return;
            }

            // IMPORTANT: junk should NOT latch commError
            if (comm.hasCommError()) {
                _detail = "comm error flag set by junk (should not happen)";
                _verdict = TestVerdict::Fail;
                return;
            }

            _detail = "junk ok; starting recovery ping";
            _phase = Phase::Recover;
            _nextPingMs = nowMs; // send immediately
            return;
        }

        case Phase::Recover:
            if (comm.lastPongReceived()) {
                _detail = "recovered: PONG received";
                _verdict = TestVerdict::Pass;
                return;
            }
            if (comm.hasCommError()) {
                _detail = "comm error during recovery";
                _verdict = TestVerdict::Fail;
                return;
            }

            // ping periodically until PONG
            if ((int32_t)(nowMs - _nextPingMs) >= 0) {
                comm.sendPing();
                _nextPingMs = nowMs + kPingPeriodMs;
            }
            return;
        }
    }

    void exit(HostComm &comm, uint32_t /*nowMs*/) override {
        // keep link flags clean for next tests
        comm.clearLastPongFlag();
    }

    TestVerdict verdict() const override { return _verdict; }
    const char *detail() const override { return _detail; }

    void dump(HostComm &comm) const override {
        RAW("----------------------------------------------\n");
        RAW("- TestCase: %s\n", name());

        const char *v = "UNKNOWN";
        switch (_verdict) {
        case TestVerdict::Pass:
            v = "PASSED";
            break;
        case TestVerdict::Fail:
            v = "FAILED";
            break;
        case TestVerdict::Skip:
            v = "SKIPPED";
            break;
        default:
            break;
        }
        RAW("- %s\n", v);
        RAW("- Detail: %s\n", _detail);

        const uint32_t nowFail = comm.parseFailCount();
        const uint32_t delta = (nowFail >= _startParseFail) ? (nowFail - _startParseFail) : 0;

        RAW("- Flags: synced=%u streak=%u parseFail=%lu (+%lu) err=%u lastPong=%u\n",
            (unsigned)comm.linkSynced(),
            (unsigned)comm.pongStreak(),
            (unsigned long)nowFail,
            (unsigned long)delta,
            (unsigned)comm.hasCommError(),
            (unsigned)comm.lastPongReceived());

        RAW("- LastBadLine: '%s'\n", comm.lastBadLine().c_str());
        RAW("----------------------------------------------\n");
    }

  private:
    enum class Phase : uint8_t { Inject = 0,
                                 Verify,
                                 Recover };

    static constexpr uint32_t kTimeoutMs = 3500;
    static constexpr uint32_t kPingPeriodMs = 200;

    // We intentionally inject at least this many "bad" lines:
    static uint32_t constexpr kExpectedMinFails = 0;

    uint8_t _injectedCount;

    static uint32_t injectJunk(HostComm &comm) {
        // Note: processLine bypasses UART and directly tests parser robustness.

        const char *junk[] = {
            // count into failed
            "XYZ",
            ";;;;;",
            "C;PON",
            "C;PONG", // falls du das absichtlich als “good” drin hast: dann NICHT mitzählen
            "C;ACK;SET",
            "C;STATUS;BLA",
            "X;PING",
            "H;PONG",
        };

        uint32_t badCount = 0;
        for (auto s : junk) {
            // zählt nur die, die “bad” sein sollen:
            const bool isBad = (strcmp(s, "C;PONG") != 0);
            comm.processLine(s);
            if (isBad) {
                badCount++;
            }
        }
        return badCount;

        // // 1) empty / whitespace
        // comm.processLine("");
        // comm.processLine("   ");

        // // 2) random garbage
        // comm.processLine("XYZ");
        // comm.processLine(";;;;;");
        // comm.processLine("C;PON");        // truncated
        // comm.processLine("C;PONG;EXTRA"); // wrong token count
        // comm.processLine("C;ACK;SET");    // missing mask
        // comm.processLine("C;STATUS;BLA"); // nonsense

        // // 3) nearly correct but wrong prefix
        // comm.processLine("X;PING");
        // comm.processLine("H;PONG"); // wrong direction

        // // 4) valid should NOT increment fails (control sample)
        // // (If this fails, protocol mismatch exists.)
        // comm.processLine("C;PONG");
    }

    Phase _phase = Phase::Inject;

    uint32_t _tEnterMs = 0;
    uint32_t _deadlineMs = 0;
    uint32_t _nextPingMs = 0;

    uint32_t _startParseFail = 0;

    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";
};

ITestCase *get_test_malformed_frames() {
    static Test_MalformedFrames inst;
    return &inst;
}