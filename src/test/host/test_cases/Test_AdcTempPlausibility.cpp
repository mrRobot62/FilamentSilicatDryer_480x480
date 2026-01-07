/**
 * TestCase: ADC/TEMP plausibility via repeated STATUS polling
 *
 * Goal:
 * - Verify that STATUS frames contain plausible numeric ranges.
 * - Verify that repeated STATUS requests work reliably (no comm error).
 *
 * Expected behavior:
 * - Host can request STATUS multiple times and receives valid STATUS.
 * - adcRaw[] stays within 0..4095 (typical ESP32 ADC raw range).
 * - tempRaw is within a plausible int16 range. If temp is unused it may be 0.
 * - Link stays synced, no comm error.
 */

#include "HostComm.h"
#include "TestCase.h"
#include "log_core.h"

class Test_AdcTempPlausibility : public ITestCase {
  public:
    const char *name() const override { return "STATUS: ADC/TEMP plausibility"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "syncing";

        _deadlineMs = nowMs + kTimeoutMs;

        _startParseFail = comm.parseFailCount();

        comm.clearCommErrorFlag();
        comm.clearNewStatusFlag();
        comm.clearLastPongFlag();
        comm.clearLinkSync();

        _phase = Phase::Sync;
        _nextPingMs = nowMs;
        _nextStatusMs = nowMs;
        _okCount = 0;
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

        // Keep sync alive
        if (!comm.linkSynced()) {
            if ((int32_t)(nowMs - _nextPingMs) >= 0) {
                comm.sendPing();
                _nextPingMs = nowMs + kPingPeriodMs;
            }
        }

        switch (_phase) {
        case Phase::Sync:
            if (!comm.linkSynced()) {
                _detail = "syncing";
                return;
            }
            _detail = "synced, polling STATUS";
            _phase = Phase::Poll;
            _nextStatusMs = nowMs; // immediately
            return;

        case Phase::Poll:
            // request STATUS periodically
            if ((int32_t)(nowMs - _nextStatusMs) >= 0) {
                comm.clearNewStatusFlag();
                comm.requestStatus();
                _nextStatusMs = nowMs + kStatusPeriodMs;
            }

            if (comm.hasNewStatus()) {
                const ProtocolStatus &st = comm.getRemoteStatus();

                if (!plausible(st)) {
                    _detail = "implausible STATUS values";
                    _verdict = TestVerdict::Fail;
                    return;
                }

                _okCount++;
                comm.clearNewStatusFlag();

                if (_okCount >= kRequiredOkSamples) {
                    _detail = "STATUS values plausible";
                    _verdict = TestVerdict::Pass;
                    return;
                }
            }
            return;
        }
    }

    void exit(HostComm &comm, uint32_t /*nowMs*/) override {
        comm.clearNewStatusFlag();
        comm.clearLastPongFlag();
    }

    TestVerdict verdict() const override { return _verdict; }
    const char *detail() const override { return _detail; }

    void dump(HostComm &comm) const override {
        const uint32_t nowFail = comm.parseFailCount();
        const uint32_t deltaFail = (nowFail >= _startParseFail) ? (nowFail - _startParseFail) : 0;

        const ProtocolStatus &st = comm.getRemoteStatus();

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

        Serial.printf("- Last STATUS: mask=0x%04X adc=[%u,%u,%u,%u] tempRaw=%d okSamples=%u/%u\n",
                      st.outputsMask,
                      st.adcRaw[0], st.adcRaw[1], st.adcRaw[2], st.adcRaw[3],
                      (int)st.tempRaw,
                      (unsigned)_okCount,
                      (unsigned)kRequiredOkSamples);

        Serial.printf("- LastBadLine: '%s'\n", comm.lastBadLine().c_str());
        Serial.println("----------------------------------------------");
    }

  private:
    enum class Phase : uint8_t { Sync,
                                 Poll };

    static constexpr uint32_t kTimeoutMs = 4000;
    static constexpr uint32_t kPingPeriodMs = 200;
    static constexpr uint32_t kStatusPeriodMs = 250;
    static constexpr uint8_t kRequiredOkSamples = 5;

    static bool plausible(const ProtocolStatus &st) {
        // ADC plausibility (raw). If you use different width/scaling, adjust here.
        for (int i = 0; i < 4; ++i) {
            if (st.adcRaw[i] > 4095u) {
                return false;
            }
        }

        // tempRaw plausibility:
        // tempRaw is in 0.25°C units, int16 range.
        // Accept 0 (unused). Otherwise accept roughly -50°C..+500°C by default.
        // Adjust if your sensor range is different.
        const int t = (int)st.tempRaw;
        if (t != 0) {
            if (t < (-50 * 4) || t > (500 * 4)) {
                return false;
            }
        }

        return true;
    }

    uint32_t _deadlineMs = 0;
    uint32_t _nextPingMs = 0;
    uint32_t _nextStatusMs = 0;

    uint32_t _startParseFail = 0;

    Phase _phase = Phase::Sync;

    uint8_t _okCount = 0;

    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";
};

ITestCase *get_test_adc_temp_plausibility() {
    static Test_AdcTempPlausibility inst;
    return &inst;
}