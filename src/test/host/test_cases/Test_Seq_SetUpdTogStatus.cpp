/**
 * TestCase: Test_Seq_SetUpdTogStatus
 *
 * Goal:
 *   Verify that the full command sequence works end-to-end:
 *     SET -> UPD -> TOG -> GET/STATUS
 *   and that the remote mask and STATUS agree with the expected final state.
 *
 * Expected behavior:
 *   - After SET(0x0001): client ACK;SET and remote mask becomes 0x0001 (CH0 ON)
 *   - After UPD(set=0x0002, clr=0x0000): client ACK;UPD and remote mask becomes 0x0003 (CH0+CH1 ON)
 *   - After TOG(0x0001): client ACK;TOG and remote mask becomes 0x0002 (CH1 ON)
 *   - After GET/STATUS: client STATUS mask == 0x0002
 *
 * Visual expectation:
 *   On the client LEDs (CH0..CH7), you should see:
 *     - CH0 ON briefly
 *     - then CH0+CH1 ON briefly
 *     - then CH1 ON briefly
 */

#include "HostComm.h"
#include "TestCase.h"
#include "log_core.h"
#include <Arduino.h>

class Test_Seq_SetUpdTogStatus : public ITestCase {
  public:
    const char *name() const override { return "Seq: SET->UPD->TOG->GET/STATUS"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "syncing link";

        _tEnterMs = nowMs;
        _stepDeadlineMs = nowMs + kStepTimeoutMs;
        _nextPingMs = nowMs;

        // reset host-side flags/state
        comm.clearCommErrorFlag();
        comm.clearLastPongFlag();
        comm.clearLastSetAckFlag();
        comm.clearNewStatusFlag();
        comm.clearLinkSync();

        _phase = Phase::Sync;
    }

    void tick(HostComm &comm, uint32_t nowMs) override {
        if (comm.hasCommError()) {
            _detail = "comm error flag set";
            _verdict = TestVerdict::Fail;
            return;
        }

        // overall timeout safety
        if ((int32_t)(nowMs - (_tEnterMs + kOverallTimeoutMs)) >= 0) {
            _detail = "overall timeout";
            _verdict = TestVerdict::Fail;
            return;
        }

        switch (_phase) {
        case Phase::Sync:
            // ping until we consider the link "synced"
            if ((int32_t)(nowMs - _nextPingMs) >= 0) {
                comm.sendPing();
                _nextPingMs = nowMs + kPingPeriodMs;
            }

            // we accept linkSynced() as readiness
            if (comm.linkSynced()) {
                _detail = "synced; sending SET";
                _phase = Phase::SendSet;
                _stepDeadlineMs = nowMs + kStepTimeoutMs;
            }
            return;

        case Phase::SendSet:
            // Visual: CH0 ON
            comm.clearLastSetAckFlag();
            comm.setOutputsMask(kMaskSet);
            _expectedMask = kMaskSet;
            _detail = "waiting ACK;SET";
            _phase = Phase::WaitAckSet;
            _stepDeadlineMs = nowMs + kStepTimeoutMs;
            return;

        case Phase::WaitAckSet:
            if (comm.lastSetAcked() && comm.getRemoteOutputsMask() == kMaskSet) {
                _detail = "SET ok; visual hold";
                _holdUntilMs = nowMs + kVisualHoldMs;
                _phase = Phase::HoldAfterSet;
                return;
            }
            if ((int32_t)(nowMs - _stepDeadlineMs) >= 0) {
                _detail = "timeout waiting ACK;SET";
                _verdict = TestVerdict::Fail;
                return;
            }
            return;

        case Phase::HoldAfterSet:
            if ((int32_t)(nowMs - _holdUntilMs) >= 0) {
                _detail = "sending UPD";
                _phase = Phase::SendUpd;
                _stepDeadlineMs = nowMs + kStepTimeoutMs;
            }
            return;

        case Phase::SendUpd:
            // Visual: CH0+CH1 ON
            comm.updOutputs(kUpdSetMask, kUpdClrMask);
            _expectedMask = (uint16_t)((kMaskSet | kUpdSetMask) & (uint16_t)~kUpdClrMask); // should be 0x0003
            _detail = "waiting ACK;UPD";
            _phase = Phase::WaitAckUpd;
            _stepDeadlineMs = nowMs + kStepTimeoutMs;
            return;

        case Phase::WaitAckUpd:
            if (comm.getRemoteOutputsMask() == _expectedMask) {
                _detail = "UPD ok; visual hold";
                _holdUntilMs = nowMs + kVisualHoldMs;
                _phase = Phase::HoldAfterUpd;
                return;
            }
            if ((int32_t)(nowMs - _stepDeadlineMs) >= 0) {
                _detail = "timeout waiting ACK;UPD";
                _verdict = TestVerdict::Fail;
                return;
            }
            return;

        case Phase::HoldAfterUpd:
            if ((int32_t)(nowMs - _holdUntilMs) >= 0) {
                _detail = "sending TOG";
                _phase = Phase::SendTog;
                _stepDeadlineMs = nowMs + kStepTimeoutMs;
            }
            return;

        case Phase::SendTog:
            // Visual: CH1 only (toggle CH0 off)
            comm.togOutputs(kTogMask);
            _expectedMask = (uint16_t)(kExpectedAfterUpd ^ kTogMask); // 0x0003 ^ 0x0001 = 0x0002
            _detail = "waiting ACK;TOG";
            _phase = Phase::WaitAckTog;
            _stepDeadlineMs = nowMs + kStepTimeoutMs;
            return;

        case Phase::WaitAckTog:
            if (comm.getRemoteOutputsMask() == _expectedMask) {
                _detail = "TOG ok; visual hold";
                _holdUntilMs = nowMs + kVisualHoldMs;
                _phase = Phase::HoldAfterTog;
                return;
            }
            if ((int32_t)(nowMs - _stepDeadlineMs) >= 0) {
                _detail = "timeout waiting ACK;TOG";
                _verdict = TestVerdict::Fail;
                return;
            }
            return;

        case Phase::HoldAfterTog:
            if ((int32_t)(nowMs - _holdUntilMs) >= 0) {
                _detail = "requesting STATUS";
                _phase = Phase::SendStatusReq;
                _stepDeadlineMs = nowMs + kStepTimeoutMs;
            }
            return;

        case Phase::SendStatusReq:
            comm.clearNewStatusFlag();
            comm.requestStatus();
            _detail = "waiting STATUS";
            _phase = Phase::WaitStatus;
            _stepDeadlineMs = nowMs + kStepTimeoutMs;
            return;

        case Phase::WaitStatus:
            if (comm.hasNewStatus()) {
                const uint16_t stMask = comm.getRemoteStatus().outputsMask;
                if (stMask == kExpectedFinal) {
                    _detail = "STATUS ok (final mask matches)";
                    _verdict = TestVerdict::Pass;
                } else {
                    _detail = "STATUS mismatch (final mask wrong)";
                    _verdict = TestVerdict::Fail;
                }
                return;
            }
            if ((int32_t)(nowMs - _stepDeadlineMs) >= 0) {
                _detail = "timeout waiting STATUS";
                _verdict = TestVerdict::Fail;
                return;
            }
            return;
        }
    }

    void exit(HostComm &comm, uint32_t /*nowMs*/) override {
        // keep it clean for next test
        comm.clearLastSetAckFlag();
        comm.clearNewStatusFlag();
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

        RAW("- Flags: synced=%u streak=%u parseFail=%lu err=%u\n",
            (unsigned)comm.linkSynced(),
            (unsigned)comm.pongStreak(),
            (unsigned long)comm.parseFailCount(),
            (unsigned)comm.hasCommError());

        RAW("- Flags2: lastPong=%u lastSetAcked=%u newStatus=%u\n",
            (unsigned)comm.lastPongReceived(),
            (unsigned)comm.lastSetAcked(),
            (unsigned)comm.hasNewStatus());

        RAW("- Masks: local=0x%04X remote=0x%04X expected=0x%04X\n",
            comm.getLocalOutputsMask(),
            comm.getRemoteOutputsMask(),
            _expectedMask);

        RAW("- Status: mask=0x%04X adc=[%u,%u,%u,%u] tempRaw=%d\n",
            comm.getRemoteStatus().outputsMask,
            comm.getRemoteStatus().adcRaw[0],
            comm.getRemoteStatus().adcRaw[1],
            comm.getRemoteStatus().adcRaw[2],
            comm.getRemoteStatus().adcRaw[3],
            comm.getRemoteStatus().tempRaw);

        RAW("----------------------------------------------\n");
    }

  private:
    enum class Phase : uint8_t {
        Sync = 0,
        SendSet,
        WaitAckSet,
        HoldAfterSet,
        SendUpd,
        WaitAckUpd,
        HoldAfterUpd,
        SendTog,
        WaitAckTog,
        HoldAfterTog,
        SendStatusReq,
        WaitStatus,
    };

    static constexpr uint32_t kOverallTimeoutMs = 9000;
    static constexpr uint32_t kStepTimeoutMs = 2000;

    static constexpr uint32_t kPingPeriodMs = 200;
    static constexpr uint32_t kVisualHoldMs = 350;

    // sequence definition
    static constexpr uint16_t kMaskSet = 0x0001;    // CH0
    static constexpr uint16_t kUpdSetMask = 0x0002; // +CH1
    static constexpr uint16_t kUpdClrMask = 0x0000;
    static constexpr uint16_t kExpectedAfterUpd = 0x0003;
    static constexpr uint16_t kTogMask = 0x0001; // toggle CH0 off -> leaves CH1
    static constexpr uint16_t kExpectedFinal = 0x0002;

    Phase _phase = Phase::Sync;

    uint32_t _tEnterMs = 0;
    uint32_t _stepDeadlineMs = 0;
    uint32_t _nextPingMs = 0;
    uint32_t _holdUntilMs = 0;

    uint16_t _expectedMask = 0x0000;

    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";
};

ITestCase *get_test_seq_set_upd_tog_status() {
    static Test_Seq_SetUpdTogStatus inst;
    return &inst;
}