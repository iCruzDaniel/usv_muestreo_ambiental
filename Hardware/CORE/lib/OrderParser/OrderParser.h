/**
 * OrderParser.h — Parsea mensajes del tópico `usv/orders` y los convierte
 * en eventos SystemEvent para el FSM.
 *
 * Formato JSON esperado en el payload:
 * {
 *   "cmd": "PREPARE" | "SET_COORDS" | "START" | "CANCEL" | "PAUSE",
 *   "payload": { ... }   // opcional, según el comando
 * }
 *
 * Ejemplo SET_COORDS:
 * {
 *   "cmd": "SET_COORDS",
 *   "payload": {
 *     "waypoints": [
 *       {"lat": 20.678, "lon": -87.073, "alt": 0.5},
 *       {"lat": 20.679, "lon": -87.074, "alt": 0.5}
 *     ],
 *     "radius_m": 3.0
 *   }
 * }
 */

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FSM/SystemState.h>

// Forward declaration
class SystemFSM;

class OrderParser {
public:
    explicit OrderParser(SystemFSM* fsm);

    /**
     * Procesar un mensaje entrante del tópico orders.
     * Llamar desde el callback del transporte.
     * @param topic   Tópico MQTT recibido
     * @param payload Payload JSON como String
     */
    void onMessage(const char* topic, const String& payload);

private:
    SystemFSM* _fsm;

    // Mapea string de comando a SystemEvent
    SystemEvent _resolveCommand(const char* cmd) const;
};
