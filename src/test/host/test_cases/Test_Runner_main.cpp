#include "HostComm.h"
#include "Test_Runner.h"
#include "log_core.h"
#include <Arduino.h>

// Host UART pins on ESP32-S3
constexpr int HOST_RX_PIN = 2;
constexpr int HOST_TX_PIN = 40;

HardwareSerial &LINK = Serial2;

HostComm g_host(LINK);
TestRunner g_runner(g_host);

// forward declarations ONLY
class ITestCase;
ITestCase *get_test_link_status();
ITestCase *get_test_set_ack();
ITestCase *get_test_status_plausibility();
ITestCase *get_test_ping_pong();

void setup() {
    Serial.begin(115200);

    g_host.begin(115200, HOST_RX_PIN, HOST_TX_PIN);

    uint32_t until = millis() + 5000;
    while ((int32_t)(millis() - until) < 0) {
        g_host.loop(); // UART RX aktiv halten
        delay(1);
    }

    INFO("[TestRunner] -----------------------------------------------------------------------\n");
    INFO("[TestRunner] Test_Runner_main.cpp \n");
    INFO("[TestRunner] Version 0.1\n");
    INFO("[TestRunner] -----------------------------------------------------------------------\n");

    INFO("[TestRunner] add Test_PingPong\n");
    g_runner.add(get_test_ping_pong());
    INFO("[TestRunner] add Test_LinkStatus\n");
    g_runner.add(get_test_link_status());
    INFO("[TestRunner] add Test_SetAck\n");
    g_runner.add(get_test_set_ack());

    delay(200); // optional small settle
    g_runner.start(millis());
}

void loop() {
    g_host.loop();           // keep UART processing running
    g_runner.tick(millis()); // run test state machines
}