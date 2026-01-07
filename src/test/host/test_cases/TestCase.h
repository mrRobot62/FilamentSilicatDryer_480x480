#pragma once
#include <Arduino.h>

class HostComm;

enum class TestVerdict : uint8_t {
    Running = 0,
    Pass,
    Fail,
    Skip
};

class ITestCase {
  public:
    virtual ~ITestCase() = default;

    virtual const char *name() const = 0;

    // Called once when the test becomes active.
    virtual void enter(HostComm &comm, uint32_t nowMs) = 0;

    // Called repeatedly (non-blocking).
    virtual void tick(HostComm &comm, uint32_t nowMs) = 0;

    // Called once when the test ends (Pass/Fail/Skip).
    virtual void exit(HostComm &comm, uint32_t nowMs) = 0;

    virtual TestVerdict verdict() const = 0;
    virtual const char *detail() const = 0;

    virtual void dump(HostComm &comm) const = 0;
};