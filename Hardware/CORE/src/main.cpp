/**
 * main.cpp — USV Muestreo Ambiental
 * ============================================================
 * Arquitectura: FSM + FreeRTOS + HAL (LTE | WiFi) + DI Sensores
 *
 * Tareas FreeRTOS:
 *   taskComms   (prioridad 3) — Mantiene viva la conexión LTE/WiFi+MQTT
 *   taskMission (prioridad 2) — MAVLink + lógica de waypoints + sensores
 *   taskTelem   (prioridad 1) — FSM principal + watchdogs + status periódico
 *
 * Diagnóstico y calibración:
 *   Compilar con -D ENABLE_DIAGNOSTICS para activar la consola
 *   interactiva por Serial (comandos: status, sensors, cal ph4, etc.)
 *
 * Compilar con:
 *   pio run -e usv_lte    (módem A7670SA)
 *   pio run -e usv_wifi   (WiFi + AWS IoT)
 * ============================================================
 */

// ── Guardia de selección de transporte ───────────────────────────────────────
#if !defined(USE_LTE) && !defined(USE_WIFI)
  #error "Define USE_LTE o USE_WIFI en build_flags de platformio.ini"
#endif

// ── Includes del framework ────────────────────────────────────────────────────
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_task_wdt.h>

// ── Configuración y certificados ──────────────────────────────────────────────
#include <config.h>
#include <certs/aws_root_ca.h>
#include <certs/client_cert.h>
#include <certs/client_key.h>

// ── HAL: selección en tiempo de compilación ───────────────────────────────────
// Nota: TINY_GSM_MODEM_A7672X y TINYGSM_RX_BUFFER_SIZE se definen
//       vía -D flags en platformio.ini para cada entorno.
#ifdef USE_LTE
  #include <TinyGsmClient.h>
  #include <HAL_LTE.h>
#else
  #include <HAL_WiFi.h>
#endif

// ── Módulos del sistema ───────────────────────────────────────────────────────
#include <MavlinkHandler.h>
#include <Logger.h>
#include <Sensors/ISensor.h>
#include <TurbiditySensor/TurbiditySensor.h>
#include <PHSensor/PHSensor.h>
#include <TemperatureSensor/TemperatureSensor.h>
#include <MissionController.h>
#include <SystemFSM.h>
#include <OrderParser.h>

// ── Consola de diagnóstico (opcional) ─────────────────────────────────────────
#ifdef ENABLE_DIAGNOSTICS
  #include <DiagnosticConsole.h>
#endif

// =============================================================================
// ── Objetos globales ──────────────────────────────────────────────────────────
// =============================================================================

// ── UART ──────────────────────────────────────────────────────────────────────
HardwareSerial serialMAV(2);   // MAVLink: RX=MAV_RX, TX=MAV_TX

// ── HAL de transporte ─────────────────────────────────────────────────────────
#ifdef USE_LTE
  HardwareSerial serialGSM(1); // LTE: RX=GSM_RX, TX=GSM_TX
  TinyGsm        modem(serialGSM);
  HAL_LTE        transport(serialGSM, modem, AWS_ROOT_CA, CLIENT_CERT, CLIENT_KEY);
#else
  HAL_WiFi       transport(WIFI_SSID, WIFI_PASS, AWS_ROOT_CA, CLIENT_CERT, CLIENT_KEY);
#endif

// ── Sensores (instanciados con pines de config.h) ─────────────────────────────
TurbiditySensor  turbSensor(PIN_TURBIDITY);
PHSensor         phSensor(PIN_PH);
TemperatureSensor tempSensor(MAX31865_CS);

// ── Handler MAVLink ───────────────────────────────────────────────────────────
MavlinkHandler mavlink(serialMAV);

// ── Lógica de misión (Inyección de Dependencias) ─────────────────────────────
MissionController mission(
    &turbSensor,
    &phSensor,
    &tempSensor,
    &mavlink,
    &transport
);

// ── FSM del sistema ───────────────────────────────────────────────────────────
SystemFSM systemFSM(&mission, &transport, &mavlink);

// ── Parser de órdenes MQTT ────────────────────────────────────────────────────
OrderParser orderParser(&systemFSM);

// ── Consola de diagnóstico (solo si -D ENABLE_DIAGNOSTICS) ────────────────────
#ifdef ENABLE_DIAGNOSTICS
  DiagnosticConsole diagConsole(
      &transport, &mavlink, &systemFSM,
      &turbSensor, &phSensor, &tempSensor
  );
#endif

// ── Mutex para acceso compartido al transporte ────────────────────────────────
SemaphoreHandle_t xTransportMutex = nullptr;

// =============================================================================
// ── Tareas FreeRTOS ───────────────────────────────────────────────────────────
// =============================================================================

/**
 * taskComms — Prioridad ALTA (3)
 * Responsabilidad: mantener vivo el transporte (LTE o WiFi+MQTT).
 * No ejecuta lógica de misión. Solo pump de red.
 */
void taskComms(void* /*pvParams*/) {
    Logger::info("COMMS", "Tarea iniciada (core %d)", xPortGetCoreID());

    for (;;) {
        if (xSemaphoreTake(xTransportMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            transport.update();
            xSemaphoreGive(xTransportMutex);
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(20));  // 20 ms tick — rápido para AT commands
    }
}

/**
 * taskMission — Prioridad MEDIA (2)
 * Responsabilidad: parsear MAVLink, actualizar sensores, ejecutar waypoints.
 */
void taskMission(void* /*pvParams*/) {
    Logger::info("MISSION", "Tarea iniciada (core %d)", xPortGetCoreID());

    for (;;) {
        mavlink.update();
        mission.update(systemFSM.getState());

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(50));  // 50 ms — suficiente para 57600 bps MAVLink
    }
}

/**
 * taskTelem — Prioridad BAJA (1)
 * Responsabilidad: correr el FSM principal, watchdogs y publicar estado.
 */
void taskTelem(void* /*pvParams*/) {
    Logger::info("TELEM", "Tarea iniciada (core %d)", xPortGetCoreID());

    for (;;) {
        if (xSemaphoreTake(xTransportMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            systemFSM.update();
            xSemaphoreGive(xTransportMutex);
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(500)); // 500 ms — estado general cada ~N * 500ms
    }
}

// =============================================================================
// ── setup() ───────────────────────────────────────────────────────────────────
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println("\n╔══════════════════════════════════════╗");
    Serial.println("║  USV Muestreo Ambiental — Arrancando ║");
#ifdef USE_LTE
    Serial.println("║  Transporte: LTE (A7670SA)           ║");
#else
    Serial.println("║  Transporte: WiFi + AWS IoT           ║");
#endif
    Serial.println("╚══════════════════════════════════════╝\n");

    // ── Watchdog global del sistema ────────────────────────────────────────
    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);

    // ── Mutex de transporte ────────────────────────────────────────────────
    xTransportMutex = xSemaphoreCreateMutex();
    configASSERT(xTransportMutex);

    // ── Logger ────────────────────────────────────────────────────────────
    Logger::setTransport(&transport);
    Logger::setMinLevel(LogLevel::INFO);

    // ── Iniciar HAL de transporte ──────────────────────────────────────────
    transport.begin();

    // ── Registrar callback de mensajes entrantes ───────────────────────────
    transport.setMessageCallback(
        [](const char* topic, const String& payload) {
            orderParser.onMessage(topic, payload);
        }
    );

    // ── Iniciar FSM ────────────────────────────────────────────────────────
    systemFSM.begin();

    // ── Iniciar MAVLink ────────────────────────────────────────────────────
    mavlink.begin(MAV_BAUD);

    // ── Iniciar sensores (vía MissionController) ───────────────────────────
    mission.begin();

    // ── Consola de diagnóstico ─────────────────────────────────────────────
#ifdef ENABLE_DIAGNOSTICS
    diagConsole.begin();
    diagConsole.printDashboard();  // Dashboard inicial al arranque
#endif

    // ── Crear tareas FreeRTOS ──────────────────────────────────────────────
    //   taskComms  → Core 0 (red)
    //   taskMission→ Core 1 (tiempo real)
    //   taskTelem  → Core 1 (baja prioridad)

    xTaskCreatePinnedToCore(taskComms,   "comms",   STACK_COMMS,   nullptr, 3, nullptr, 0);
    xTaskCreatePinnedToCore(taskMission, "mission", STACK_MISSION, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(taskTelem,   "telem",   STACK_TELEMETRY, nullptr, 1, nullptr, 1);

    Logger::info("MAIN", "Todas las tareas creadas — FreeRTOS scheduler activo");
    // Nota: el scheduler ya está corriendo en IDF/Arduino; setup() retorna
    //       y la tarea de Arduino queda en segundo plano.
}

// =============================================================================
// ── loop() — Consola de diagnóstico (si está activa) o idle ──────────────────
// =============================================================================
void loop() {
#ifdef ENABLE_DIAGNOSTICS
    // La consola lee Serial y muestra dashboard periódico
    diagConsole.update();
    vTaskDelay(pdMS_TO_TICKS(100));  // 100 ms — responsividad del teclado
#else
    // Sin diagnóstico: entregar CPU al scheduler
    vTaskDelay(pdMS_TO_TICKS(1000));
#endif
}
