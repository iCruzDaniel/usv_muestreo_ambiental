#include <Arduino.h>
#include <unity.h>
#include "SensorManager.h"

// Hardware sensor test - REQUIRES PHYSICAL SENSORS CONNECTED
// Instructions:
// 1. Connect turbidity sensor to pin 34
// 2. Connect pH sensor to pin 35  
// 3. Connect MAX31865 to SPI (CS=5, SCK=18, MOSI=23, MISO=19)
// 4. For meaningful tests: 
//    - Turbidity: test in air (low) vs water with particles (high)
//    - pH: test in buffer solutions if available
//    - Temperature: test at room temperature

SensorManager hw_sensors;

void setUp(void) {
  // Called before each test
}

void tearDown(void) {
  // Called after each test
}

void test_sensors_initialize(void) {
  Serial.println("=== HARDWARE SENSOR TEST ===");
  Serial.println("Initializing hw_sensors...");
  hw_sensors.begin();
  delay(1000);  // Let sensors stabilize

  Serial.println("Test: Sensors initialize without error");
  // If begin() didn't crash, we're good
  TEST_ASSERT_TRUE(true);

  Serial.println("Sensors initialized. Prepare for testing...");
  delay(2000);  // Give user time to prepare
}

void test_turbidity_reading_plausible_range(void) {
  Serial.println("Test: Turbidity reading in plausible range (0-4000 NTU)");
  
  // Take multiple readings to avoid transient spikes
  float sum = 0;
  int validReadings = 0;
  
  for (int i = 0; i < 10; i++) {
    hw_sensors.update();
    delay(150);  // Wait longer than READ_INTERVAL (100ms)
    
    float turb = hw_sensors.getTurbidity();
    
    // Skip invalid readings (shouldn't happen with proper connection)
    if (!isnan(turb) && turb >= 0 && turb <= 5000) {
      sum += turb;
      validReadings++;
    }
  }
  
  TEST_ASSERT_GREATER_THAN(0, validReadings);  // Got at least one valid reading
  
  if (validReadings > 0) {
    float avg = sum / validReadings;
    Serial.print("Average turbidity: ");
    Serial.print(avg);
    Serial.println(" NTU");
    
    // Plausible range for most turbidity sensors in normal conditions
    TEST_ASSERT_TRUE(avg >= 0);
    TEST_ASSERT_TRUE(avg <= 4000);  // Adjust based on your sensor spec
  }
}

void test_ph_reading_plausible_range(void) {
  Serial.println("Test: pH reading in plausible range (0-14)");
  
  float sum = 0;
  int validReadings = 0;
  
  for (int i = 0; i < 10; i++) {
    hw_sensors.update();
    delay(150);
    
    float ph = hw_sensors.getPH();
    
    if (!isnan(ph) && ph >= 0 && ph <= 14) {
      sum += ph;
      validReadings++;
    }
  }
  
  TEST_ASSERT_GREATER_THAN(0, validReadings);
  
  if (validReadings > 0) {
    float avg = sum / validReadings;
    Serial.print("Average pH: ");
    Serial.println(avg);
    
    TEST_ASSERT_TRUE(avg >= 0);
    TEST_ASSERT_TRUE(avg <= 14);
  }
}

void test_temperature_reading_plausible_range(void) {
  Serial.println("Test: Temperature reading in plausible range (-40 to 125°C)");
  
  float sum = 0;
  int validReadings = 0;
  
  for (int i = 0; i < 10; i++) {
    hw_sensors.update();
    delay(150);
    
    float temp = hw_sensors.getTemperature();
    
    if (!isnan(temp)) {
      sum += temp;
      validReadings++;
    }
  }
  
  TEST_ASSERT_GREATER_THAN(0, validReadings);
  
  if (validReadings > 0) {
    float avg = sum / validReadings;
    Serial.print("Average temperature: ");
    Serial.print(avg);
    Serial.println(" °C");
    
    // Wide range to accommodate sensor error and environment
    TEST_ASSERT_TRUE(avg >= -40);
    TEST_ASSERT_TRUE(avg <= 125);
  }
}

void test_sensor_readings_change_with_stimulus(void) {
  Serial.println("Test: Verify readings change when sensors are stimulated");
  Serial.println("NOTE: This test requires manual interaction!");
  Serial.println("For turbidity: cover sensor vs expose to light");
  Serial.println("For pH: test in different solutions if available");
  Serial.println("For temperature: warm/cool the sensor tip");
  
  // Take baseline reading
  hw_sensors.update();
  delay(150);
  float baselineTurb = hw_sensors.getTurbidity();
  float baselinePh = hw_sensors.getPH();
  float baselineTemp = hw_sensors.getTemperature();
  
  Serial.print("Baseline - Turb: ");
  Serial.print(baselineTurb);
  Serial.print(" NTU, pH: ");
  Serial.print(baselinePh);
  Serial.print(", Temp: ");
  Serial.print(baselineTemp);
  Serial.println(" °C");
  
  // Wait for user to stimulate sensors
  Serial.println("STIMULATE SENSORS NOW...");
  Serial.println("Cover turbidity sensor, change pH solution, or change temperature");
  Serial.println("Waiting 10 seconds...");
  delay(10000);
  
  // Take stimulated reading
  hw_sensors.update();
  delay(150);
  float stimulatedTurb = hw_sensors.getTurbidity();
  float stimulatedPh = hw_sensors.getPH();
  float stimulatedTemp = hw_sensors.getTemperature();
  
  Serial.print("Stimulated - Turb: ");
  Serial.print(stimulatedTurb);
  Serial.print(" NTU, pH: ");
  Serial.print(stimulatedPh);
  Serial.print(", Temp: ");
  Serial.print(stimulatedTemp);
  Serial.println(" °C");
  
  // Check that at least one reading changed significantly
  bool turbChanged = !isnan(baselineTurb) && !isnan(stimulatedTurb) && 
                     abs(baselineTurb - stimulatedTurb) > 50.0;  // 50 NTU change
  bool phChanged = !isnan(baselinePh) && !isnan(stimulatedPh) && 
                   abs(baselinePh - stimulatedPh) > 0.5;  // 0.5 pH change
  bool tempChanged = !isnan(baselineTemp) && !isnan(stimulatedTemp) && 
                     abs(baselineTemp - stimulatedTemp) > 2.0;  // 2°C change
  
  Serial.print("Changes detected - Turb: ");
  Serial.print(turbChanged ? "YES" : "NO");
  Serial.print(", pH: ");
  Serial.print(phChanged ? "YES" : "NO");
  Serial.print(", Temp: ");
  Serial.println(tempChanged ? "YES" : "NO");
  
  // At least one should change with reasonable stimulation
  TEST_ASSERT_TRUE(turbChanged || phChanged || tempChanged);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  delay(2000);  // Give time for serial monitor to connect

  UNITY_BEGIN();
  
  RUN_TEST(test_sensors_initialize);
  RUN_TEST(test_turbidity_reading_plausible_range);
  RUN_TEST(test_ph_reading_plausible_range);
  RUN_TEST(test_temperature_reading_plausible_range);
  RUN_TEST(test_sensor_readings_change_with_stimulus);
  
  UNITY_END();
}

void loop() {
  // Nothing needed here
}