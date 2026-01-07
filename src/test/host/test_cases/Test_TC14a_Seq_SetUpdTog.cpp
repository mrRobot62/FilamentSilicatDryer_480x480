/**
 * TestCase: TC14a - Sequence SET -> UPD -> TOG (ACK + mask progression)
 *
 * Goal:
 * - Verify that the host can execute a deterministic command sequence:
 *     1) H;SET;MMMM        -> expect C;ACK;SET;MMMM
 *     2) H;UPD;SSSS;CCCC   -> expect C;ACK;UPD;MMMM (new mask)
 *     3) H;TOG;TTTT        -> expect C;ACK;TOG;MMMM (new mask)
 *
 * Expectations:
 * - Link must be synced (PONG streak >= 2) before starting.
 * - Each command must be acknowledged (SET/UPD/TOG flags).
 * - Remote mask must match the expected mask after each step.
 * - No commError must be raised during the test.
 */

#include "HostComm.h"
#include "TestCase.h"
#include "log_core.h"
#include <Arduino.h>

class Test_TC14a_Seq_SetUpdTog : public ITestCase {
  public:
    const char *name() const override { return "TC14a: SEQ SET->UPD->TOG (ACK + mask)"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "syncing link";

        _t0Ms = nowMs;
        _deadlineMs = nowMs + kTimeoutMs;

        // Reset comm-side flags that could affect verdict
        comm.clearCommErrorFlag();
        comm.clearLastPongFlag();
        comm.clearLastSetAckFlag();
        comm.clearLastUpdAckFlag();
        comm.clearLastTogAckFlag();

        // We do NOT clear link sync on purpose; but we are robust if not synced.
        _syncNextPingMs = nowMs; // ping immediately
        _step = Step::Sync;
        _nextActionMs = nowMs;
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

        // Keep syncing until comm.linkSynced() == true
        if (_step == Step::Sync) {
            if (comm.linkSynced()) {
                _detail = "link synced -> starting sequence";
                _step = Step::SendSet;
                _nextActionMs = nowMs; // proceed immediately
            } else {
                if ((int32_t)(nowMs - _syncNextPingMs) >= 0) {
                    comm.sendPing();
                    _syncNextPingMs = nowMs + kPingPeriodMs;
                }
                _detail = "syncing link";
                return;
            }
        }

        // Small pacing between commands (avoids “burst noise”/boot junk edge cases)
        if ((int32_t)(nowMs - _nextActionMs) < 0) {
            return;
        }

        switch (_step) {
        case Step::SendSet:
            comm.clearLastSetAckFlag();
            comm.setOutputsMask(kMaskSet);
            _expectedMask = kMaskSet;
            _detail = "sent SET, waiting ACK;SET";
            _step = Step::WaitSetAck;
            break;

        case Step::WaitSetAck:
            if (comm.lastSetAcked() && comm.getRemoteOutputsMask() == _expectedMask) {
                _detail = "SET ACK ok";
                _step = Step::SendUpd;
                _nextActionMs = nowMs + kInterStepDelayMs;
            }
            break;

        case Step::SendUpd:
            comm.clearLastUpdAckFlag();
            comm.updOutputs(kUpdSetMask, kUpdClrMask);
            _expectedMask = (uint16_t)((kMaskSet | kUpdSetMask) & (uint16_t)~kUpdClrMask);
            _detail = "sent UPD, waiting ACK;UPD";
            _step = Step::WaitUpdAck;
            break;

        case Step::WaitUpdAck:
            if (comm.lastUpdAcked() && comm.getRemoteOutputsMask() == _expectedMask) {
                _detail = "UPD ACK ok";
                _step = Step::SendTog;
                _nextActionMs = nowMs + kInterStepDelayMs;
            }
            break;

        case Step::SendTog:
            comm.clearLastTogAckFlag();
            comm.togOutputs(kTogMask);
            _expectedMask ^= kTogMask;
            _detail = "sent TOG, waiting ACK;TOG";
            _step = Step::WaitTogAck;
            break;

        case Step::WaitTogAck:
            if (comm.lastTogAcked() && comm.getRemoteOutputsMask() == _expectedMask) {
                _detail = "sequence complete";
                _verdict = TestVerdict::Pass;
            }
            break;

        default:
            _detail = "internal state error";
            _verdict = TestVerdict::Fail;
            break;
        }
    }

    void exit(HostComm &comm, uint32_t /*nowMs*/) override {
        // Keep remote state, but clear “one-shot” flags
        comm.clearLastPongFlag();
        comm.clearLastSetAckFlag();
        comm.clearLastUpdAckFlag();
        comm.clearLastTogAckFlag();
    }

    TestVerdict verdict() const override { return _verdict; }
    const char *detail() const override { return _detail; }

    void dump(HostComm &comm) const override {
        INFO("----------------------------------------------\n");
        INFO("- TestCase: %s\n", name());
        INFO("- %s\n", verdictToText(_verdict));
        INFO("- Detail: %s\n", _detail);
        INFO("- Flags: synced=%d streak=%u parseFail=%lu err=%d lastPong=%d\n",
             (int)comm.linkSynced(),
             (unsigned)comm.pongStreak(),
             (unsigned long)comm.parseFailCount(),
             (int)comm.hasCommError(),
             (int)comm.lastPongReceived());
        INFO("- ACK: set=%d upd=%d tog=%d  remoteMask=0x%04X expected=0x%04X\n",
             (int)comm.lastSetAcked(),
             (int)comm.lastUpdAcked(),
             (int)comm.lastTogAcked(),
             (unsigned)comm.getRemoteOutputsMask(),
             (unsigned)_expectedMask);
        INFO("- LastBadLine: '%s'\n", comm.lastBadLine().c_str());
        INFO("----------------------------------------------\n");
    }

  private:
    enum class Step : uint8_t {
        Sync,
        SendSet,
        WaitSetAck,
        SendUpd,
        WaitUpdAck,
        SendTog,
        WaitTogAck,
    };

    static constexpr uint32_t kTimeoutMs = 3000;
    static constexpr uint32_t kPingPeriodMs = 200;
    static constexpr uint32_t kInterStepDelayMs = 80;

    // --- Sequence definition (feel free to tweak masks) ---
    static constexpr uint16_t kMaskSet = 0x00A5;    // 1010 0101
    static constexpr uint16_t kUpdSetMask = 0x0002; // set bit1
    static constexpr uint16_t kUpdClrMask = 0x0004; // clear bit2
    static constexpr uint16_t kTogMask = 0x000F;    // toggle bits0..3

    static const char *verdictToText(TestVerdict v) {
        switch (v) {
        case TestVerdict::Pass:
            return "PASSED";
        case TestVerdict::Fail:
            return "FAILED";
        case TestVerdict::Skip:
            return "SKIPPED";
        default:
            return "RUNNING";
        }
    }

    uint32_t _t0Ms = 0;
    uint32_t _deadlineMs = 0;
    uint32_t _syncNextPingMs = 0;
    uint32_t _nextActionMs = 0;

    mutable uint16_t _expectedMask = 0;
    Step _step = Step::Sync;

    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";
};

ITestCase *get_test_tc14a_seq_set_upd_tog() {
    static Test_TC14a_Seq_SetUpdTog inst;
    return &inst;
}