/**
 * hardware_modem_test.cpp
 * 
 * Test de hardware para el módem A7670SA (usando TinyGSM con TINY_GSM_MODEM_A7672X).
 * Verifica comunicación AT, estado de SIM, registro en red, calidad de señal,
 * y opcionalmente conexión GPRS y MQTT hacia AWS IoT Core.
 * 
 * Requiere: Módem A7670SA conectado - GPIO16 (RX), GPIO17 (TX)
 */

#ifndef TINYGSM_RX_BUFFER_SIZE
  #define TINYGSM_RX_BUFFER_SIZE 1024
#endif
#ifndef TINY_GSM_MODEM_A7672X
  #define TINY_GSM_MODEM_A7672X
#endif

#include <Arduino.h>
#include <TinyGsmClient.h>
#include <unity.h>

// Configure the pins for the modem (matching main.cpp)
#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 115200

// APN for your carrier - CHANGE THIS TO MATCH YOUR NETWORK
#ifndef DEFAULT_APN
  #define DEFAULT_APN "internet"
#endif

// Instantiate the modem
HardwareSerial serialGSM(1);
TinyGsm modem(serialGSM);

void setUp(void) {
  // Called before each test
}

void tearDown(void) {
  // Called after each test
}

void test_modem_basic_at(void) {
  Serial.println("Test: Basic AT command");
  
  // Use testAT() which sends AT and expects OK  
  bool atOk = modem.testAT(5000);
  
  Serial.print("AT response: ");
  Serial.println(atOk ? "OK" : "FAIL");
  
  TEST_ASSERT_TRUE_MESSAGE(atOk, "Modem did not respond to AT command");
  
  Serial.println("Basic AT: PASSED");
}

void test_modem_info(void) {
  Serial.println("Test: Modem identification");
  
  String modemName = modem.getModemName();
  String modemInfo = modem.getModemInfo();
  
  Serial.print("Modem name: ");
  Serial.println(modemName);
  Serial.print("Modem info: ");
  Serial.println(modemInfo);
  
  // We should get some non-empty string back
  TEST_ASSERT_TRUE_MESSAGE(modemName.length() > 0 || modemInfo.length() > 0,
                           "Modem returned empty identification");
  
  Serial.println("Modem info: PASSED");
}

void test_modem_sim_status(void) {
  Serial.println("Test: SIM status");
  
  SimStatus simStatus = modem.getSimStatus(10000);
  
  Serial.print("SIM status: ");
  switch (simStatus) {
    case SIM_READY:
      Serial.println("READY");
      break;
    case SIM_LOCKED:
      Serial.println("LOCKED (PIN required)");
      break;
    case SIM_ANTITHEFT_LOCKED:
      Serial.println("ANTITHEFT LOCKED");
      break;
    default:
      Serial.println("ERROR / NOT INSERTED");
      break;
  }
  
  // SIM should at least be detected (READY or LOCKED)
  TEST_ASSERT_TRUE_MESSAGE(
    simStatus == SIM_READY || simStatus == SIM_LOCKED,
    "SIM not detected - check SIM card insertion"
  );
  
  Serial.println("SIM status: PASSED");
}

void test_modem_network_registration(void) {
  Serial.println("Test: Network registration");
  Serial.println("Waiting for network (up to 60s)...");
  
  bool registered = modem.waitForNetwork(60000);
  
  Serial.print("Network registered: ");
  Serial.println(registered ? "YES" : "NO");
  
  if (registered) {
    bool networkConnected = modem.isNetworkConnected();
    Serial.print("Network connected: ");
    Serial.println(networkConnected ? "YES" : "NO");
  }
  
  TEST_ASSERT_TRUE_MESSAGE(registered, 
    "Could not register to network - check antenna and coverage");
  
  Serial.println("Network registration: PASSED");
}

void test_modem_signal_quality(void) {
  Serial.println("Test: Signal quality");
  
  int16_t signalQuality = modem.getSignalQuality();
  
  Serial.print("Signal quality (CSQ): ");
  Serial.println(signalQuality);
  
  // CSQ values: 0-31 (valid), 99 (unknown/not detectable)
  // 0 = -113 dBm, 31 = -51 dBm
  if (signalQuality == 99) {
    Serial.println("  Signal: Unknown (99) - may need better antenna placement");
  } else if (signalQuality >= 0 && signalQuality <= 31) {
    int dbm = -113 + (signalQuality * 2);
    Serial.print("  Signal strength: ");
    Serial.print(dbm);
    Serial.println(" dBm");
    
    if (signalQuality < 10) {
      Serial.println("  WARNING: Signal is weak");
    } else if (signalQuality < 20) {
      Serial.println("  Signal: Moderate");
    } else {
      Serial.println("  Signal: Good");
    }
  }
  
  // CSQ should be a valid value (0-31) or 99
  TEST_ASSERT_TRUE_MESSAGE(
    (signalQuality >= 0 && signalQuality <= 31) || signalQuality == 99,
    "Invalid signal quality value"
  );
  
  Serial.println("Signal quality: PASSED");
}

void test_modem_gprs_connection(void) {
  Serial.println("Test: GPRS connection (requires valid SIM and data plan)");
  
  // Disconnect first to ensure clean state
  modem.gprsDisconnect();
  delay(1000);
  
  // Connect to GPRS
  Serial.print("Connecting to APN: ");
  Serial.println(DEFAULT_APN);
  
  bool gprsConnected = modem.gprsConnect(DEFAULT_APN);
  
  Serial.print("GPRS connected: ");
  Serial.println(gprsConnected ? "YES" : "NO");
  
  if (gprsConnected) {
    String localIP = modem.getLocalIP();
    Serial.print("Local IP: ");
    Serial.println(localIP);
    
    TEST_ASSERT_TRUE_MESSAGE(localIP.length() > 0, "Got GPRS but no IP address");
  }
  
  TEST_ASSERT_TRUE_MESSAGE(gprsConnected, 
    "GPRS connection failed - check APN and data plan");
  
  // Clean up
  modem.gprsDisconnect();
  
  Serial.println("GPRS connection: PASSED");
}

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  delay(2000);  // Give time for serial monitor to connect
  
  Serial.println("=== HARDWARE MODEM TEST ===");
  Serial.println("Initializing modem serial...");
  
  // Initialize modem serial
  serialGSM.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(1000);
  
  // Initialize / restart modem
  Serial.println("Initializing modem...");
  if (!modem.init()) {
    Serial.println("WARNING: modem.init() failed, trying restart...");
    modem.restart();
    delay(5000);
  } else {
    Serial.println("Modem initialized OK");
  }
  delay(3000);  // Wait for modem to stabilize

  UNITY_BEGIN();
  
  // Basic connectivity tests
  RUN_TEST(test_modem_basic_at);
  RUN_TEST(test_modem_info);
  RUN_TEST(test_modem_sim_status);
  RUN_TEST(test_modem_signal_quality);
  RUN_TEST(test_modem_network_registration);
  
  // GPRS test - requires active SIM with data plan
  // Comment out if you only want to test basic modem communication
  RUN_TEST(test_modem_gprs_connection);
  
  UNITY_END();
}

void loop() {
  // Nothing needed here
}