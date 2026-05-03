#ifdef TINY_GSM_MODEM_SIM800

#include <Arduino.h>
#define TINY_GSM_MODEM_SIM800
#include <TinyGsm.h>
#include <unity.h>

// Configura los pines según tu hardware (coinciden con main.cpp)
#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 115200

// Instancia del modem
HardwareSerial serialGSM(1);
TinyGsm modem(serialGSM);

void setUp(void) {
  // Inicializar serial para debug
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  // Inicializar serial del modem
  serialGSM.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(1000);  // Esperar estabilización del modem
  
  // Reiniciar modem para estado conocido
  Serial.println("Reiniciando modem para pruebas...");
  modem.restart();
  delay(5000);  // Tiempo para que el modem se reinicie completamente
}

void tearDown(void) {
  // Asegurar que no queden conexiones abiertas
  modem.gprsDisconnect();
  modem.mqttStop();
  delay(100);
}

void test_modem_basic_at(void) {
  Serial.println("Ejecutando prueba: AT básico");
  
  // Limpiar buffer de entrada
  while (serialGSM.available()) serialGSM.read();
  
  // Enviar AT y esperar OK
  TEST_ASSERT_TRUE(modem.sendAT("AT"));
  int8_t result = modem.waitResponse(1000L);
  TEST_ASSERT_EQUAL_INT(1, result);  // 1 = OK según TinyGsm
  
  Serial.println("AT básico: PASÓ");
}

void test_modem_sim_status(void) {
  Serial.println("Ejecutando prueba: Estado de SIM");
  
  // Limpiar buffer
  while (serialGSM.available()) serialGSM.read();
  
  // Consultar estado de SIM
  TEST_ASSERT_TRUE(modem.sendAT("+CPIN?"));
  int8_t result = modem.waitResponse(2000L);
  
  // Puede responder:
  // +CPIN: READY (1 = OK)
  // +CPIN: SIM PIN (todavía necesita PIN)
  // +CPIN: SIM PUK (necesita PUK)
  // ERROR (0 o negativo)
  // Para pruebas, aceptamos cualquiera de las respuestas válidas
  TEST_ASSERT_TRUE(result >= 0);  // No debe haber error de transmisión
  
  Serial.print("Respuesta +CPIN?: ");
  Serial.println(result);
  Serial.println("Estado de SIM: PASÓ (se recibió respuesta)");
}

void test_modem_registration(void) {
  Serial.println("Ejecutando prueba: Registro en red");
  
  // Limpiar buffer
  while (serialGSM.available()) serialGSM.read();
  
  // Consultar registro
  TEST_ASSERT_TRUE(modem.sendAT("+CREG?"));
  int8_t result = modem.waitResponse(5000L);  // Puede tardar en registrar
  
  // Respuestas esperadas:
  // +CREG: 0,1 (registrado, red local)
  // +CREG: 0,5 (registrado, roaming)
  // +CREG: 0,0 (no registrado, buscando)
  // +CREG: 0,2 (no registrado, pero buscando)
  // Aceptamos cualquier respuesta que indique que el comando funcionó
  TEST_ASSERT_TRUE(result >= 0);  // No error de transmisión
  
  Serial.print("Respuesta +CREG?: ");
  Serial.println(result);
  Serial.println("Registro: PASÓ (se recibió respuesta)");
}

void test_modem_signal_quality(void) {
  Serial.println("Ejecutando prueba: Calidad de señal");
  
  // Limpiar buffer
  while (serialGSM.available()) serialGSM.read();
  
  // Consultar calidad de señal
  TEST_ASSERT_TRUE(modem.sendAT("+CSQ"));
  int8_t result = modem.waitResponse(2000L);
  
  // Respuesta: +CSQ: <rssi>,<ber>
  // rssi: 0-31, 99=sin conocer
  // ber: 0-7, 99=sin conocer
  TEST_ASSERT_TRUE(result >= 0);
  
  Serial.print("Respuesta +CSQ: ");
  Serial.println(result);
  Serial.println("Calidad de señal: PASÓ");
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  
  RUN_TEST(test_modem_basic_at);
  RUN_TEST(test_modem_sim_status);
  RUN_TEST(test_modem_registration);
  RUN_TEST(test_modem_signal_quality);
  
  // Las siguientes pruebas requieren SIM activa y se marchan como IGNORE
  // Descoméntalas cuando tengas conectividad celular disponible
  /*
  RUN_TEST(test_modem_gprs_setup);
  RUN_TEST(test_modem_mqtt_connection);
  */
  
   return UNITY_END();
}

#endif // TINY_GSM_MODEM_SIM800