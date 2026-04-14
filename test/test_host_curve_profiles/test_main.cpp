#include <unity.h>

#include "host_curve_profiles.h"

static void test_all_default_heater_profiles_are_valid(void) {
    for (uint8_t i = 0; i < HOST_PARAMETER_HEATER_PROFILE_COUNT; ++i) {
        const HostHeaterProfileParameters profile = host_curve_default_heater_profile(i);
        TEST_ASSERT_TRUE(host_curve_validate_heater_profile(profile));
    }
}

static void test_all_default_pulse_curves_are_valid(void) {
    for (uint8_t i = 0; i < HOST_PARAMETER_HEATER_PROFILE_COUNT; ++i) {
        const HostPulseCurveParameters pulse = host_curve_default_pulse_curve(i);
        TEST_ASSERT_TRUE(host_curve_validate_pulse_curve(pulse));
    }
}

static void test_profile_specific_default_targets_match_expected_curves(void) {
    TEST_ASSERT_EQUAL_INT16(45, host_curve_default_heater_profile(0).targetC);
    TEST_ASSERT_EQUAL_INT16(60, host_curve_default_heater_profile(1).targetC);
    TEST_ASSERT_EQUAL_INT16(80, host_curve_default_heater_profile(2).targetC);
    TEST_ASSERT_EQUAL_INT16(100, host_curve_default_heater_profile(3).targetC);
}

static void test_profile_specific_default_pulse_curves_match_expected_values(void) {
    const HostPulseCurveParameters low = host_curve_default_pulse_curve(0);
    const HostPulseCurveParameters mid = host_curve_default_pulse_curve(1);
    const HostPulseCurveParameters high = host_curve_default_pulse_curve(2);
    const HostPulseCurveParameters silica = host_curve_default_pulse_curve(3);

    TEST_ASSERT_EQUAL_UINT16(30000, low.reheatSoakMs);
    TEST_ASSERT_EQUAL_UINT16(6000, low.holdPulseMaxMs);
    TEST_ASSERT_EQUAL_INT16(30, low.reheatEnableBelowTarget_dC);
    TEST_ASSERT_EQUAL_INT16(10, low.forceOffBeforeTarget_dC);

    TEST_ASSERT_EQUAL_UINT16(30000, mid.reheatSoakMs);
    TEST_ASSERT_EQUAL_UINT16(5000, mid.holdPulseMaxMs);
    TEST_ASSERT_EQUAL_INT16(30, mid.reheatEnableBelowTarget_dC);
    TEST_ASSERT_EQUAL_INT16(10, mid.forceOffBeforeTarget_dC);

    TEST_ASSERT_EQUAL_UINT16(30000, high.reheatSoakMs);
    TEST_ASSERT_EQUAL_UINT16(6000, high.holdPulseMaxMs);
    TEST_ASSERT_EQUAL_INT16(20, high.reheatEnableBelowTarget_dC);
    TEST_ASSERT_EQUAL_INT16(10, high.forceOffBeforeTarget_dC);

    TEST_ASSERT_EQUAL_UINT16(25000, silica.reheatSoakMs);
    TEST_ASSERT_EQUAL_UINT16(8000, silica.holdPulseMaxMs);
    TEST_ASSERT_EQUAL_INT16(30, silica.reheatEnableBelowTarget_dC);
    TEST_ASSERT_EQUAL_INT16(10, silica.forceOffBeforeTarget_dC);
}

static void test_invalid_profiles_are_rejected(void) {
    HostHeaterProfileParameters heater = host_curve_default_heater_profile(0);
    heater.targetC = 20;
    TEST_ASSERT_FALSE(host_curve_validate_heater_profile(heater));

    HostPulseCurveParameters pulse = host_curve_default_pulse_curve(0);
    pulse.reheatSoakMs = 1000;
    TEST_ASSERT_FALSE(host_curve_validate_pulse_curve(pulse));
}

static void test_out_of_range_profile_index_falls_back_to_profile_zero_defaults(void) {
    const HostHeaterProfileParameters heater = host_curve_default_heater_profile(99);
    const HostPulseCurveParameters pulse = host_curve_default_pulse_curve(99);

    TEST_ASSERT_EQUAL_INT16(45, heater.targetC);
    TEST_ASSERT_EQUAL_UINT16(30000, pulse.reheatSoakMs);
    TEST_ASSERT_EQUAL_UINT16(6000, pulse.holdPulseMaxMs);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_all_default_heater_profiles_are_valid);
    RUN_TEST(test_all_default_pulse_curves_are_valid);
    RUN_TEST(test_profile_specific_default_targets_match_expected_curves);
    RUN_TEST(test_profile_specific_default_pulse_curves_match_expected_values);
    RUN_TEST(test_invalid_profiles_are_rejected);
    RUN_TEST(test_out_of_range_profile_index_falls_back_to_profile_zero_defaults);

    return UNITY_END();
}
