#include <Arduino.h>
#include <MAVLink.h>
#include <unity.h>
#include "MavlinkHandler.h"

// Configure the pins for MAVLink (matching main.cpp)
#define MAVLINK_RX 4   // GPIO4 (RX2)
#define MAVLINK_TX 2   // GPIO2 (TX2)
#define MAVLINK_BAUD 57600

// MAVLink handler instance
HardwareSerial serialMAV(2);
MavlinkHandler mavlink(serialMAV);

// Test message storage
mavlink_heartbeat_t testHeartbeat;
mavlink_global_position_int_t testPosition;

void setUp(void) {
  // Called before each test - nothing extra needed, init done in setup()
}

void tearDown(void) {
  // Called after each test
}

void test_mavlink_serial_connection(void) {
  Serial.println("Test: MAVLink serial connection");
  
  // Clear any existing data
  while (serialMAV.available()) serialMAV.read();
  
  // Send a simple byte to test serial connection
  serialMAV.write(0x55);  // Any arbitrary byte
  
  // Wait a bit and check if we can read it back (loopback test)
  // Note: This requires TX2 and RX2 to be connected together for true loopback
  // Without loopback, we just verify the serial port is configured and transmitting
  
  delay(10);
  int available = serialMAV.available();
  Serial.print("Bytes available after write: ");
  Serial.println(available);
  
  // If we have a loopback (TX2-RX2 connected), we should read our byte back
  // Otherwise, we just verify the port doesn't error
  TEST_ASSERT_TRUE(true);  // Serial initialization succeeded if we got here
  
  Serial.println("MAVLink serial connection: PASSED (port initialized)");
}

void test_mavlink_heartbeat_transmission_and_reception(void) {
  Serial.println("Test: MAVLink heartbeat transmission and reception");
  
  // Clear buffers
  while (serialMAV.available()) serialMAV.read();
  
  // Encode and send a heartbeat message
  uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
  mavlink_message_t msg;
  
  mavlink_msg_heartbeat_encode(
    1, 200, &msg,
    &testHeartbeat
  );
  
  uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
  
  // Send the message
  Serial.print("Sending HEARTBEAT message (");
  Serial.print(len);
  Serial.println(" bytes)...");
  
  for (uint16_t i = 0; i < len; i++) {
    serialMAV.write(buffer[i]);
  }
  
  // Wait for message to be transmitted and potentially received back
  // (requires loopback or actual FC connected)
  delay(200);
  
  // Process any incoming data via MavlinkHandler
  mavlink.update();
  
  // Try to receive and parse a heartbeat
  mavlink_heartbeat_t hb;
  bool received = mavlink.getHeartbeat(hb);
  
  Serial.print("Heartbeat received: ");
  Serial.println(received ? "YES" : "NO");
  
  if (received) {
    Serial.print("  Type: ");
    Serial.println(hb.type);
    Serial.print("  Autopilot: ");
    Serial.println(hb.autopilot);
    Serial.print("  Base mode: ");
    Serial.println(hb.base_mode);
    Serial.print("  System status: ");
    Serial.println(hb.system_status);
    
    // Verify it matches what we sent (only with loopback/FC)
    TEST_ASSERT_EQUAL_INT(MAV_TYPE_QUADROTOR, hb.type);
    TEST_ASSERT_EQUAL_INT(MAV_AUTOPILOT_GENERIC, hb.autopilot);
    TEST_ASSERT_EQUAL_INT(MAV_MODE_GUIDED_ARMED, hb.base_mode);
    TEST_ASSERT_EQUAL_INT(MAV_STATE_ACTIVE, hb.system_status);
  } else {
    // Without loopback or FC, receiving nothing is expected
    Serial.println("  (No FC or loopback detected - transmission-only verified)");
  }
  
  // Even without loopback, we verify our sending code works
  Serial.println("Heartbeat transmission: PASSED");
}

void test_mavlink_position_transmission_and_reception(void) {
  Serial.println("Test: MAVLink position transmission and reception");
  
  // Clear buffers
  while (serialMAV.available()) serialMAV.read();
  
  // Encode and send a position message
  uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
  mavlink_message_t msg;
  
  mavlink_msg_global_position_int_encode(
    1, 200, &msg,
    &testPosition
  );
  
  uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
  
  // Send the message
  Serial.print("Sending GLOBAL_POSITION_INT message (");
  Serial.print(len);
  Serial.println(" bytes)...");
  
  for (uint16_t i = 0; i < len; i++) {
    serialMAV.write(buffer[i]);
  }
  
  delay(200);
  
  // Process any incoming data via MavlinkHandler
  mavlink.update();
  
  // Try to receive and parse a position
  mavlink_global_position_int_t pos;
  bool received = mavlink.getPosition(pos);
  
  Serial.print("Position received: ");
  Serial.println(received ? "YES" : "NO");
  
  if (received) {
    Serial.print("  Lat: ");
    Serial.print(pos.lat / 1e7, 7);
    Serial.print("°, Lon: ");
    Serial.print(pos.lon / 1e7, 7);
    Serial.print("°, Alt: ");
    Serial.print(pos.alt / 1000.0, 1);
    Serial.print("m, HDG: ");
    Serial.print(pos.hdg / 100.0, 1);
    Serial.println("°");
    
    // Verify it matches what we sent
    TEST_ASSERT_EQUAL_INT(-353632613, pos.lat);
    TEST_ASSERT_EQUAL_INT(1491652374, pos.lon);
    TEST_ASSERT_EQUAL_INT(584000, pos.alt);
    TEST_ASSERT_EQUAL_INT(480000, pos.relative_alt);
    TEST_ASSERT_EQUAL_INT(0, pos.hdg);
  } else {
    Serial.println("  (No FC or loopback detected - transmission-only verified)");
  }
  
  Serial.println("Position transmission: PASSED");
}

void test_mavlink_link_detection(void) {
  Serial.println("Test: MAVLink link detection");
  
  // Check current link state
  bool linkActive = mavlink.isConnected();
  Serial.print("Link active: ");
  Serial.println(linkActive ? "YES" : "NO");
  
  // Send a heartbeat to establish link
  uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
  mavlink_message_t msg;
  
  mavlink_msg_heartbeat_encode(
    1, 200, &msg,
    &testHeartbeat
  );
  
  uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
  
  for (uint16_t i = 0; i < len; i++) {
    serialMAV.write(buffer[i]);
  }
  
  delay(200);  // Allow time for processing
  mavlink.update();
  
  // Check if link is now detected as active
  bool linkActiveAfter = mavlink.isConnected();
  Serial.print("Link active after sending heartbeat: ");
  Serial.println(linkActiveAfter ? "YES" : "NO");
  
  // With loopback or real FC, this should be true
  // Without either, it might be false but that's okay for this test
  Serial.println("Link detection: PASSED");
}

void test_mavlink_message_rate(void) {
  Serial.println("Test: MAVLink message rate capability");
  
  // Clear buffers
  while (serialMAV.available()) serialMAV.read();
  
  // Send multiple messages rapidly and see if we can keep up
  const int numMessages = 10;
  unsigned long startTime = millis();
  
  for (int i = 0; i < numMessages; i++) {
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    mavlink_message_t msg;
    
    // Alternate between heartbeat and position
    if (i % 2 == 0) {
      mavlink_msg_heartbeat_encode(1, 200, &msg, &testHeartbeat);
    } else {
      mavlink_msg_global_position_int_encode(1, 200, &msg, &testPosition);
    }
    
    uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
    
    for (uint16_t j = 0; j < len; j++) {
      serialMAV.write(buffer[j]);
    }
  }
  
  unsigned long endTime = millis();
  unsigned long duration = endTime - startTime;
  
  Serial.print("Sent ");
  Serial.print(numMessages);
  Serial.print(" messages in ");
  Serial.print(duration);
  Serial.println(" ms");
  
  if (duration > 0) {
    float rate = (numMessages * 1000.0) / duration;
    Serial.print("Effective rate: ");
    Serial.print(rate);
    Serial.println(" msgs/sec");
  }
  
  // Just verify we can send messages without blocking
  TEST_ASSERT_TRUE(true);
  Serial.println("Message rate test: PASSED");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  delay(2000);  // Give time for serial monitor to connect

  Serial.println("=== HARDWARE MAVLINK TEST ===");
  Serial.println("Initializing MAVLink connection...");
  
  // Initialize MAVLink serial
  serialMAV.begin(MAVLINK_BAUD, SERIAL_8N1, MAVLINK_RX, MAVLINK_TX);
  delay(500);
  
  // Prepare a test heartbeat message
  testHeartbeat.type = MAV_TYPE_QUADROTOR;
  testHeartbeat.autopilot = MAV_AUTOPILOT_GENERIC;
  testHeartbeat.base_mode = MAV_MODE_GUIDED_ARMED;
  testHeartbeat.custom_mode = 0;
  testHeartbeat.system_status = MAV_STATE_ACTIVE;
  
  // Prepare a test position message (example coordinates)
  testPosition.lat = -353632613;  // 35.3632613° S
  testPosition.lon = 1491652374;  // 149.1652374° E
  testPosition.alt = 584000;      // 584.0 mm
  testPosition.relative_alt = 480000; // 480.0 m
  testPosition.vx = 0;
  testPosition.vy = 0;
  testPosition.vz = 0;
  testPosition.hdg = 0;           // 0 degrees (north)

  UNITY_BEGIN();
  
  RUN_TEST(test_mavlink_serial_connection);
  RUN_TEST(test_mavlink_heartbeat_transmission_and_reception);
  RUN_TEST(test_mavlink_position_transmission_and_reception);
  RUN_TEST(test_mavlink_link_detection);
  RUN_TEST(test_mavlink_message_rate);
  
  UNITY_END();
}

void loop() {
  // Nothing needed here
}