#pragma once
#include "TestCase.h"
#include "log_core.h"
#include <Arduino.h>

class HostComm;

class TestRunner {
  public:
    explicit TestRunner(HostComm &comm);

    void add(ITestCase *tc); // caller owns test objects (static lifetime recommended)
    void start(uint32_t nowMs);
    void tick(uint32_t nowMs);

    bool finished() const;
    uint16_t passedCount() const;
    uint16_t failedCount() const;

  private:
    HostComm &_comm;

    static constexpr uint8_t kMaxTests = 16;
    ITestCase *_tests[kMaxTests];
    uint8_t _count = 0;
    int8_t _index = -1;

    uint16_t _pass = 0;
    uint16_t _fail = 0;

    bool _started = false;
    bool _finished = false;

    void logStart(ITestCase *tc);
    void logEnd(ITestCase *tc);
};