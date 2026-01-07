#include "HostComm.h"
#include "TestCase.h"
#include "log_core.h"
#include <Arduino.h>

/**
 * TestCase: Parser: line fragmentation + burst RX handling
 *
 * Goal:
 * - Verify HostComm can assemble lines correctly when RX is fragmented (byte-wise / chunk-wise)
 *   and when multiple frames arrive in one burst chunk.
 * - Verify the same line assembler path is used as real UART RX (via HostComm::processRxBytes()).
 *
 * Expected:
 * - No commError
 * - parseFailCount delta == 0 (we only inject valid frames)
 * - Link sync achieved (>=2 PONG -> linkSynced==true)
 * - STATUS frame parsed: hasNewStatus==true and remoteStatus.outputsMask matches expected
 */
class Test_LineFragmentationBurst : public ITestCase {
  public:
    const char *name() const override { return "Parser: line fragmentation + burst RX handling"; }

    void enter(HostComm &comm, uint32_t nowMs) override {
        _verdict = TestVerdict::Running;
        _detail = "injecting fragmented RX";

        _t0 = nowMs;
        _deadlineMs = nowMs + kTimeoutMs;

        // clean start
        comm.clearCommErrorFlag();
        comm.clearLastPongFlag();
        comm.clearNewStatusFlag();
        comm.clearLinkSync();

        _startFail = comm.parseFailCount();
        _expectedMask = 0x00A5;

        // Phase A: PONG byte-wise (single frame)
        feedByteWise(comm, "C;PONG\r\n");

        // Phase B: STATUS fragmented into chunks (valid frame)
        // Use a valid STATUS line your ProtocolCodec supports:
        // C;STATUS;<mask>;<a0>;<a1>;<a2>;<a3>;<temp>\r\n
        feedChunks(comm, {"C;STATUS;00",
                          "A5;1;2;3",
                          ";4;5\r\n"});

        // Phase C: Burst: two PONG frames in one chunk (should sync link)
        feedChunk(comm, "C;PONG\r\nC;PONG\r\n");

        // Phase D (optional but useful): leading junk byte before 'C'
        // Should be removed by sanitizer WITHOUT increasing parseFailCount.
        feedLeadingJunkThen(comm, 0x01, "C;PONG\r\n");

        _detail = "evaluating";
    }

    void tick(HostComm &comm, uint32_t nowMs) override {
        if (comm.hasCommError()) {
            _detail = "comm error flag set";
            _verdict = TestVerdict::Fail;
            return;
        }

        const uint32_t nowFail = comm.parseFailCount();
        const uint32_t delta = nowFail - _startFail;

        // We expect 0 parse failures (all injected frames are valid; junk is "leading junk" only)
        if (delta != 0) {
            _detail = "unexpected parseFailCount increase";
            _verdict = TestVerdict::Fail;
            return;
        }

        // We expect link synced via >=2 PONG
        if (!comm.linkSynced()) {
            // allow some time (though injections are synchronous, keep it tolerant)
            if ((int32_t)(nowMs - _deadlineMs) < 0) {
                return;
            }
            _detail = "link not synced";
            _verdict = TestVerdict::Fail;
            return;
        }

        // We expect STATUS received with expected mask
        if (!comm.hasNewStatus()) {
            if ((int32_t)(nowMs - _deadlineMs) < 0) {
                return;
            }
            _detail = "no STATUS received";
            _verdict = TestVerdict::Fail;
            return;
        }

        if (comm.getRemoteStatus().outputsMask != _expectedMask) {
            _detail = "STATUS mask mismatch";
            _verdict = TestVerdict::Fail;
            return;
        }

        _detail = "fragmentation + burst OK";
        _verdict = TestVerdict::Pass;
    }

    void exit(HostComm &comm, uint32_t nowMs) override {
        (void)nowMs;
        // keep status for dump; optionally clear:
        // comm.clearNewStatusFlag();
        // comm.clearLastPongFlag();
    }

    TestVerdict verdict() const override { return _verdict; }
    const char *detail() const override { return _detail; }

    void dump(HostComm &comm) const override {
        const char *vf = "FAILED";
        if (_verdict == TestVerdict::Pass) {
            vf = "PASSED";
        }
        if (_verdict == TestVerdict::Skip) {
            vf = "SKIPPED";
        }

        const uint32_t endFail = comm.parseFailCount();
        const uint32_t delta = endFail - _startFail;

        RAW("----------------------------------------------\n");
        RAW("- TestCase: %s\n", name());
        RAW("- %s\n", vf);
        RAW("- Detail: %s\n", _detail);
        RAW("- Flags: synced=%u streak=%u parseFail=%lu (+%lu) err=%u lastPong=%u hasNewStatus=%u remoteMask=0x%04X\n",
            (unsigned)comm.linkSynced(),
            (unsigned)comm.pongStreak(),
            (unsigned long)endFail,
            (unsigned long)delta,
            (unsigned)comm.hasCommError(),
            (unsigned)comm.lastPongReceived(),
            (unsigned)comm.hasNewStatus(),
            (unsigned)comm.getRemoteStatus().outputsMask);
        RAW("- LastBadLine: '%s'\n", comm.lastBadLine().c_str());
        RAW("----------------------------------------------\n");
    }

  private:
    static constexpr uint32_t kTimeoutMs = 500;

    uint32_t _t0 = 0;
    uint32_t _deadlineMs = 0;

    uint32_t _startFail = 0;
    uint16_t _expectedMask = 0;

    TestVerdict _verdict = TestVerdict::Running;
    const char *_detail = "init";

    static void feedChunk(HostComm &comm, const char *s) {
        const size_t n = strlen(s);
        comm.processRxBytes((const uint8_t *)s, n);
    }

    static void feedByteWise(HostComm &comm, const char *s) {
        const size_t n = strlen(s);
        for (size_t i = 0; i < n; ++i) {
            comm.processRxBytes((const uint8_t *)&s[i], 1);
        }
    }

    static void feedChunks(HostComm &comm, std::initializer_list<const char *> chunks) {
        for (auto *c : chunks) {
            feedChunk(comm, c);
        }
    }

    static void feedLeadingJunkThen(HostComm &comm, uint8_t junk, const char *line) {
        comm.processRxBytes(&junk, 1);
        feedChunk(comm, line);
    }
};

ITestCase *get_test_line_fragmentation_burst() {
    static Test_LineFragmentationBurst inst;
    return &inst;
}