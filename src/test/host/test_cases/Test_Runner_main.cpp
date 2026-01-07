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
ITestCase *get_test_visual_led_sweep();
ITestCase *get_test_digital_chase();
ITestCase *get_test_digital_blink_all();
ITestCase *get_test_upd_ack();
ITestCase *get_test_tog_ack();
ITestCase *get_test_seq_set_upd_tog_status();
ITestCase *get_test_malformed_frames();
ITestCase *get_test_line_fragmentation_burst();
ITestCase *get_test_hold_status_verify();
ITestCase *get_test_tc14a_seq_set_upd_tog();
ITestCase *get_test_tc14b_status_after_seq();
ITestCase *get_test_rst_handling();
ITestCase *get_test_adc_temp_plausibility();

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

    g_runner.add(get_test_ping_pong());
    g_runner.add(get_test_link_status());
    g_runner.add(get_test_set_ack());
    g_runner.add(get_test_visual_led_sweep());
    g_runner.add(get_test_digital_chase());
    g_runner.add(get_test_digital_blink_all());
    g_runner.add(get_test_upd_ack());
    g_runner.add(get_test_tog_ack());
    g_runner.add(get_test_seq_set_upd_tog_status());
    g_runner.add(get_test_malformed_frames());
    g_runner.add(get_test_line_fragmentation_burst());
    g_runner.add(get_test_hold_status_verify());
    g_runner.add(get_test_tc14a_seq_set_upd_tog());
    g_runner.add(get_test_tc14b_status_after_seq());
    g_runner.add(get_test_rst_handling());
    g_runner.add(get_test_adc_temp_plausibility());

    //---------------------------------------------------------------------
    delay(200); // optional small settle
    g_runner.start(millis());
}

void loop() {
    g_host.loop();           // keep UART processing running
    g_runner.tick(millis()); // run test state machines
}