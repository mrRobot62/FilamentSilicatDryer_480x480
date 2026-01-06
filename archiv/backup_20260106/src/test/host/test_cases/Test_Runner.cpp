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

    logStart(_tests[_index]);
    _tests[_index]->enter(_comm, nowMs);
}

void TestRunner::tick(uint32_t nowMs) {
    if (!_started || _finished) {
        return;
    }
    if (_index < 0 || _index >= (int8_t)_count) {
        return;
    }

    ITestCase *tc = _tests[_index];
    tc->tick(_comm, nowMs);

    const TestVerdict v = tc->verdict();
    if (v == TestVerdict::Running) {
        return;
    }

    tc->exit(_comm, nowMs);
    logEnd(tc);

    if (v == TestVerdict::Pass) {
        _pass++;
    }
    if (v == TestVerdict::Fail) {
        _fail++;
    }

    _index++;
    if (_index >= (int8_t)_count) {
        _finished = true;
        Serial.printf("[TEST] DONE: pass=%u fail=%u\n", _pass, _fail);
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
    //Serial.printf("[TEST] END:   %s -> %s (%s)\n", tc->name(), v, tc->detail());
    INFO("[TEST_RUNNER] END:   %s -> %s (%s)\n", tc->name(), v, tc->detail());
}