#include "HostComm.h"
#include "Test_Runner.h"
#include "log_core.h"
#include <Arduino.h>

// Host UART pins on ESP32-S3
constexpr int HOST_RX_PIN = 2;
constexpr int HOST_TX_PIN = 40;

HostComm g_host(Serial2);
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

    delay(5000); // wait for Serial Monitor
    INFO("[TestRunner] -----------------------------------------------------------------------\n");
    INFO("[TestRunner] Filament Silicate Dryer - HOST Test Cases\n");
    INFO("[TestRunner] Version 0.1\n");
    INFO("[TestRunner] -----------------------------------------------------------------------\n");

    g_runner.add(get_test_ping_pong());
    g_runner.add(get_test_link_status());
    g_runner.add(get_test_set_ack());

    delay(200); // optional small settle
    g_runner.start(millis());
}

void loop() {
    g_host.loop();           // keep UART processing running
    g_runner.tick(millis()); // run test state machines
}