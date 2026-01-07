#include "Test_Runner.h"
#include "HostComm.h" // adjust include path if needed

TestRunner::TestRunner(HostComm &comm) : _comm(comm) {
    for (auto &t : _tests) {
        t = nullptr;
    }
}

void TestRunner::add(ITestCase *tc) {
    if (_count >= kMaxTests || tc == nullptr) {
        return;
    }
    _tests[_count++] = tc;
}

void TestRunner::start(uint32_t nowMs) {
    if (_started || _count == 0) {
        return;
    }
    _started = true;
    _finished = false;
    _index = 0;
    _startMs = nowMs;

    logStart(_tests[_index]);
    _tests[_index]->enter(_comm, nowMs);
}

void TestRunner::tick(uint32_t nowMs) {
    if (!_started || _finished) {
        return;
    }

    ITestCase *tc = _tests[_index];
    tc->tick(_comm, nowMs);

    if (tc->verdict() == TestVerdict::Running) {
        return;
    }

    tc->exit(_comm, nowMs);
    logEnd(tc);

    switch (tc->verdict()) {
    case TestVerdict::Pass:
        _pass++;
        break;
    case TestVerdict::Fail:
        _fail++;
        break;
    case TestVerdict::Skip:
        _skip++;
        break;
    default:
        break;
    }

    _index++;
    if (_index >= (int8_t)_count) {
        _finished = true;
        _endMs = nowMs;
        dumpSummary();
        return;
    }

    logStart(_tests[_index]);
    _tests[_index]->enter(_comm, nowMs);
}

bool TestRunner::finished() const { return _finished; }
uint16_t TestRunner::passedCount() const { return _pass; }
uint16_t TestRunner::failedCount() const { return _fail; }

void TestRunner::logStart(ITestCase *tc) {
    INFO("[TEST] START: %s\n", tc->name());
}
void TestRunner::logEnd(ITestCase *tc) {
    if (!tc) {
        return;
    }
    const char *v = "UNKNOWN";
    switch (tc->verdict()) {
    case TestVerdict::Pass:
        v = "PASS";
        break;
    case TestVerdict::Fail:
        v = "FAIL";
        break;
    case TestVerdict::Skip:
        v = "SKIP";
        break;
    default:
        break;
    }
    // Serial.printf("[TEST] END:   %s -> %s (%s)\n", tc->name(), v, tc->detail());
    INFO("[TEST_RUNNER] END:   %s -> %s (%s)\n", tc->name(), v, tc->detail());
    tc->dump(_comm);
}

void TestRunner::dumpSummary() {
    Serial.println("=============================================================");
    Serial.println("TEST SUMMARY");
    Serial.println("-------------------------------------------------------------");

    for (uint8_t i = 0; i < _count; i++) {
        const ITestCase *tc = _tests[i];
        const char *res = "UNKNOWN";

        switch (tc->verdict()) {
        case TestVerdict::Pass:
            res = "PASS";
            break;
        case TestVerdict::Fail:
            res = "FAIL";
            break;
        case TestVerdict::Skip:
            res = "SKIP";
            break;
        default:
            break;
        }

        Serial.printf("%2u. %-50s\t%s\n",
                      (unsigned)(i + 1),
                      tc->name(),
                      res);
    }

    Serial.println("-------------------------------------------------------------");
    Serial.printf("PASSED:  %u\n", _pass);
    Serial.printf("FAILED:  %u\n", _fail);
    Serial.printf("SKIPPED: %u\n", _skip);

    const float runtimeSec =
        (_endMs - _startMs) / 1000.0f;
    Serial.printf("RUNTIME: %.2f s\n", runtimeSec);

    Serial.println("=============================================================");
}