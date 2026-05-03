#ifdef TEST_MAVLINK

#include <Arduino.h>
#include <MAVLink.h>
#include <unity.h>
#include "MavlinkHandler.h"

// Configura los pines según tu hardware (coinciden con main.cpp)
#define MAVLINK_RX 4   // GPIO4 (RX2)
#define MAVLINK_TX 2   // GPIO2 (TX2)
#define MAVLINK_BAUD 57600

// Instancia del manejador MAVLink
HardwareSerial serialMAV(2);
MavlinkHandler mavlink(serialMAV);

// Buffer para almacenar mensajes de prueba
mavlink_heartbeat_t testHeartbeat;
mavlink_global_position_int_t testPosition;

void setUp(void) {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("Inicializando prueba MAVLink...");
  serialMAV.begin(MAVLINK_BAUD, SERIAL_8N1, MAVLINK_RX, MAVLINK_TX);
  delay(500);
  
  // Preparar mensaje de prueba HEARTBEAT
  testHeartbeat.type = MAV_TYPE_QUADROTOR;
  testHeartbeat.autopilot = MAV_AUTOPILOT_GENERIC;
  testHeartbeat.base_mode = MAV_MODE_GUIDED_ARMED;
  testHeartbeat.custom_mode = 0;
  testHeartbeat.system_status = MAV_STATE_ACTIVE;
  
  // Preparar mensaje de prueba GLOBAL_POSITION_INT
  testPosition.lat = -353632613;  // 35.3632613° S (ejemplo: Canberra)
  testPosition.lon = 1491652374;  // 149.1652374° E
  testPosition.alt = 584000;      // 584.0 mm sobre origen WGS84
  testPosition.relative_alt = 480000; // 480.0 m sobre ground
  testPosition.vx = 0;            // velocidad X en cm/s
  testPosition.vy = 0;            // velocidad Y en cm/s
  testPosition.vz = 0;            // velocidad Z en cm/s
  testPosition.hdg = 0;           // heading en centi-degrees (0° = norte)
}

void tearDown(void) {
  serialMAV.end();
  delay(100);
}

void test_mavlink_heartbeat_parsing(void) {
  Serial.println("Ejecutando prueba: Parsing HEARTBEAT");
  
  // Limpiar buffer de entrada
  while (serialMAV.available()) serialMAV.read();
  
  // Codificar mensaje HEARTBEAT
  uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
  mavlink_message_t msg;
  
  mavlink_msg_heartbeat_encode(
    1, 200, &msg,
    &testHeartbeat
  );
  
  uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
  
  // Enviar el buffer por el serial
  for (uint16_t i = 0; i < len; i++) {
    serialMAV.write(buffer[i]);
  }
  
  // Dar tiempo para procesar (el baud rate es lento)
  delay(150);
  
  // Verificar que se parseó correctamente
  mavlink_heartbeat_t hb;
  bool received = mavlink.getHeartbeat(hb);
  
  TEST_ASSERT_TRUE(received);
  TEST_ASSERT_EQUAL_INT(MAV_TYPE_QUADROTOR, hb.type);
  TEST_ASSERT_EQUAL_INT(MAV_AUTOPILOT_GENERIC, hb.autopilot);
  TEST_ASSERT_EQUAL_INT(MAV_MODE_GUIDED_ARMED, hb.base_mode);
  TEST_ASSERT_EQUAL_INT(MAV_STATE_ACTIVE, hb.system_status);
  
  Serial.println("HEARTBEAT parsing: PASÓ");
}

void test_mavlink_position_parsing(void) {
  Serial.println("Ejecutando prueba: Parsing GLOBAL_POSITION_INT");
  
  // Limpiar buffer de entrada
  while (serialMAV.available()) serialMAV.read();
  
  // Codificar mensaje GLOBAL_POSITION_INT
  uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
  mavlink_message_t msg;
  
  mavlink_msg_global_position_int_encode(
    1, 200, &msg,
    &testPosition
  );
  
  uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
  
  // Enviar el buffer por el serial
  for (uint16_t i = 0; i < len; i++) {
    serialMAV.write(buffer[i]);
  }
  
  // Dar tiempo para procesar
  delay(150);
  
  // Verificar que se parseó correctamente
  mavlink_global_position_int_t pos;
  bool received = mavlink.getPosition(pos);
  
  TEST_ASSERT_TRUE(received);
  TEST_ASSERT_EQUAL_INT(-353632613, pos.lat);
  TEST_ASSERT_EQUAL_INT(1491652374, pos.lon);
  TEST_ASSERT_EQUAL_INT(584000, pos.alt);
  TEST_ASSERT_EQUAL_INT(480000, pos.relative_alt);
  TEST_ASSERT_EQUAL_INT(0, pos.hdg);
  
  Serial.println("GLOBAL_POSITION_INT parsing: PASÓ");
}

void test_mavlink_timeout_handling(void) {
  Serial.println("Ejecutando prueba: Manejo de timeout");
  
  // Limpiar buffer
  while (serialMAV.available()) serialMAV.read();
  
  // Verificar que getHeartbeat() devuelve false cuando no hay mensajes recientes
  mavlink_heartbeat_t hb;
  bool received = mavlink.getHeartbeat(hb);
  TEST_ASSERT_FALSE(received);  // No debería haber mensaje válido
  
  // Enviar datos basura para asegurar que no se interprete como MAVLink
  for (int i = 0; i < 20; i++) {
    serialMAV.write(0xFF);  // Byte inválido para comenzar un paquete MAKELINK
  }
  delay(50);
  
  received = mavlink.getHeartbeat(hb);
  TEST_ASSERT_FALSE(received);  // Aún así no debería ser un heartbeat válido
  
  Serial.println("Manejo de timeout: PASÓ");
}

void test_mavlink_link_detection(void) {
  Serial.println("Ejecutando prueba: Detección de enlace activo");
  
  // Inicialmente no debería haber enlace
  bool linkActiveBefore = mavlink.isConnected();
  TEST_ASSERT_FALSE(linkActiveBefore);
  
  // Enviar un HEARTBEAT válido
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
  
  delay(150);  // Tiempo de procesamiento
  
  // Ahora debería detectar enlace activo
  bool linkActiveAfter = mavlink.isConnected();
  TEST_ASSERT_TRUE(linkActiveAfter);
  
  Serial.println("Detección de enlace: PASÓ");
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  
  RUN_TEST(test_mavlink_heartbeat_parsing);
  RUN_TEST(test_mavlink_position_parsing);
  RUN_TEST(test_mavlink_timeout_handling);
  RUN_TEST(test_mavlink_link_detection);
  
   return UNITY_END();
}

#endif // TEST_MAVLINK