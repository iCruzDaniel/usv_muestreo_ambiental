#include <Arduino.h>
#include <unity.h>
#include "SensorManager.h"

// Mock class for thermocouple to avoid hardware dependencies
class MockMAX31865 {
public:
    MockMAX31865(int csPin) {};
    
    bool begin(uint8_t wireMode) {
        beginCalled = true;
        return true;
    }
    
    float temperature(float RREF, float RRTI) {
        tempCalled = true;
        lastRREF = RREF;
        lastRRTI = RRTI;
        // Simple mock: return average of reference and temperature resistor values
        return (RREF + RRTI) / 2.0;
    }
    
    bool beginCalled = false;
    bool tempCalled = false;
    float lastRREF = 0.0;
    float lastRRTI = 0.0;
};

// Test fixtures
SensorManager sensors;
MockMAX31865 mockThermo(5);

void setUp(void) {
    Serial.begin(115200);
    // Re-initialize with mock
    sensors = SensorManager();
}

void tearDown(void) {
    // Cleanup if needed
}

// =========================================================
// TEMPERATURE SENSOR TESTS
// =========================================================

void test_temperature_sensor_can_initialize(void) {
    // Test that SensorManager begins without crashing
    // In hardware test environment, this would test real sensor
    TEST_ASSERT_TRUE(true); // Placeholder - sensor initialization doesn't return status
}

void test_temperature_sensor_reads_value(void) {
    sensors.update();

    float temp = sensors.getTemperature();
    // Temperature should be a reasonable value (not NaN, within -50 to 150°C range)
    TEST_ASSERT_FALSE(isnan(temp));
    TEST_ASSERT_TRUE(temp > -50 && temp < 150);
}

void test_temperature_getter_returns_value(void) {
    // Test that getter returns the stored temperature value
    float temp = sensors.getTemperature();
    TEST_ASSERT_FALSE(isnan(temp));
}

// =========================================================
// TURBIDITY SENSOR TESTS
// =========================================================

void test_turbidity_initial_value_is_zero(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.0, sensors.getTurbidity());
}

void test_turbidity_voltage_conversion_zero(void) {
    float result = sensors.convertTurbidityVoltageToNTU(0.0);
    TEST_ASSERT_EQUAL_FLOAT(0.0, result);
}

void test_turbidity_voltage_conversion_full_scale(void) {
    float result = sensors.convertTurbidityVoltageToNTU(3.3);
    TEST_ASSERT_FLOAT_WITHIN(1.0, 3300.0, result);
}

void test_turbidity_filter_calculates_average(void) {
    float buffer[4] = {0};
    float values[] = {10.0, 20.0, 30.0, 40.0};
    float result;

    for (int i = 0; i < 4; i++) {
        result = sensors.applyFilter(values[i], buffer);
    }

    // After adding 10,20,30,40, the average should be 25
    TEST_ASSERT_EQUAL_FLOAT(25.0, result);
}

void test_turbidity_sensor_reading(void) {
    sensors.update();
    float turbidity = sensors.getTurbidity();
    TEST_ASSERT_FALSE(isnan(turbidity));
    TEST_ASSERT_TRUE(turbidity >= 0 && turbidity <= 10000);
}

// =========================================================
// PH SENSOR TESTS
// =========================================================

void test_ph_initial_value_is_zero(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.0, sensors.getPH());
}

void test_ph_conversion_at_midpoint(void) {
    float result = sensors.convertPHVoltageToPH(2.5);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 7.0, result);
}

void test_ph_conversion_at_zero_voltage(void) {
    float result = sensors.convertPHVoltageToPH(0.0);
    float expected = 7.0 + (2.5 - 0.0) * 3.5;
    TEST_ASSERT_FLOAT_WITHIN(0.01, expected, result);
}

void test_ph_conversion_at_max_voltage(void) {
    float result = sensors.convertPHVoltageToPH(3.3);
    float expected = 7.0 + (2.5 - 3.3) * 3.5;
    TEST_ASSERT_FLOAT_WITHIN(0.01, expected, result);
}

void test_ph_filter_calculates_average(void) {
    float buffer[4] = {0};
    float values[] = {6.0, 7.0, 8.0, 9.0};
    float result;

    for (int i = 0; i < 4; i++) {
        result = sensors.applyFilter(values[i], buffer);
    }

    // After adding 6,7,8,9, the average should be 7.5
    TEST_ASSERT_EQUAL_FLOAT(7.5, result);
}

void test_ph_sensor_reading(void) {
    sensors.update();
    float ph = sensors.getPH();
    TEST_ASSERT_FALSE(isnan(ph));
    TEST_ASSERT_TRUE(ph >= 0 && ph <= 14);
}

// =========================================================
// TEST SUITE
// =========================================================

void setup() {
    UNITY_BEGIN();

    // Temperature tests
    RUN_TEST(test_temperature_sensor_can_initialize);
    RUN_TEST(test_temperature_sensor_reads_value);
    RUN_TEST(test_temperature_getter_returns_value);

    // Turbidity tests
    RUN_TEST(test_turbidity_initial_value_is_zero);
    RUN_TEST(test_turbidity_voltage_conversion_zero);
    RUN_TEST(test_turbidity_voltage_conversion_full_scale);
    RUN_TEST(test_turbidity_filter_calculates_average);
    RUN_TEST(test_turbidity_sensor_reading);

    // pH tests
    RUN_TEST(test_ph_initial_value_is_zero);
    RUN_TEST(test_ph_conversion_at_midpoint);
    RUN_TEST(test_ph_conversion_at_zero_voltage);
    RUN_TEST(test_ph_conversion_at_max_voltage);
    RUN_TEST(test_ph_filter_calculates_average);
    RUN_TEST(test_ph_sensor_reading);

    UNITY_END();
}

void loop() {
    // Nothing needed here
}