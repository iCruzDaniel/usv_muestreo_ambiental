/**
 * MissionController.h — Controlador de misión con inyección de sensores.
 *
 * Recibe punteros ISensor* (turbidez, pH, temperatura) y un ITransport*
 * para publicar los datos de waypoints en TOPIC_MISSION.
 *
 * Solo toma muestras cuando el USV está dentro del radio del waypoint activo.
 */

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Sensors/ISensor.h>
#include <HAL/ITransport.h>
#include <FSM/SystemState.h>
#include <config.h>

// Forward declaration
class MavlinkHandler;

class MissionController {
public:
    /**
     * Constructor — Inyección de dependencias.
     * @param turbidity  Sensor de turbidez (ISensor*)
     * @param ph         Sensor de pH      (ISensor*)
     * @param temp       Sensor de temp.   (ISensor*)
     * @param mavlink    Fuente de posición GPS (MAVLink)
     * @param transport  HAL de comunicación (ITransport*)
     */
    MissionController(ISensor* turbidity, ISensor* ph, ISensor* temp,
                      MavlinkHandler* mavlink, ITransport* transport);

    void begin();

    /**
     * Debe llamarse periódicamente desde la tarea de misión (FreeRTOS).
     * Solo actúa si el estado del sistema es MISSION_ACTIVE.
     */
    void update(SystemState sysState);

    // ── Control de misión ─────────────────────────────────────────────────
    /** Carga un nuevo conjunto de waypoints. */
    void setWaypoints(const MissionConfig& cfg);

    /** Inicia la misión desde el waypoint 0. */
    void start();

    /** Pausa la misión en el waypoint actual. */
    void pause();

    /** Cancela la misión y resetea estado. */
    void cancel();

    bool isCompleted() const { return _completed; }
    uint8_t currentWaypointIdx() const { return _mission.current; }

private:
    ISensor*        _turbidity;
    ISensor*        _ph;
    ISensor*        _temp;
    MavlinkHandler* _mavlink;
    ITransport*     _transport;

    MissionConfig   _mission;
    bool            _active    = false;
    bool            _paused    = false;
    bool            _completed = false;

    unsigned long   _lastSampleMs = 0;

    bool _isAtWaypoint(const Waypoint& wp, double curLat, double curLon) const;
    void _captureSample(const Waypoint& wp, double lat, double lon);
    void _advanceWaypoint();
    double _haversineDistM(double lat1, double lon1, double lat2, double lon2) const;
};
