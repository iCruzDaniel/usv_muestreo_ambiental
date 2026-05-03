/**
 * SystemFSM.h — Máquina de Estados Finitos del sistema USV.
 *
 * Estados: BOOT → STANDBY ↔ MISSION_ACTIVE → EMERGENCY
 *
 * Recibe eventos (SystemEvent) y coordina las transiciones.
 * Es el único componente que modifica el estado global del sistema.
 */

#pragma once
#include <Arduino.h>
#include <FSM/SystemState.h>
#include <HAL/ITransport.h>
#include <config.h>

// Forward declarations
class MissionController;
class MavlinkHandler;

class SystemFSM {
public:
    SystemFSM(MissionController* mission, ITransport* transport,
              MavlinkHandler* mavlink);

    void begin();

    /**
     * Procesar el estado actual. Llamar desde la tarea principal de FreeRTOS.
     * Internamente comprueba condiciones de transición automática.
     */
    void update();

    /** Despachar un evento externo (desde parser de órdenes MQTT). */
    void dispatch(SystemEvent event, const String& payload = "");

    SystemState getState() const { return _state; }

private:
    MissionController* _mission;
    ITransport*        _transport;
    MavlinkHandler*    _mavlink;

    SystemState        _state = SystemState::BOOT;
    unsigned long      _stateMs = 0;

    // ── Handlers de cada estado ────────────────────────────────────────────
    void _handleBoot();
    void _handleStandby();
    void _handleMissionActive();
    void _handleEmergency();

    // ── Transición ────────────────────────────────────────────────────────
    void _transitionTo(SystemState next);

    // ── Publicar estado general ────────────────────────────────────────────
    void _publishStatus();

    unsigned long _lastStatusMs = 0;
    static constexpr unsigned long STATUS_INTERVAL_MS = TELEMETRY_INTERVAL_MS;

    // ── Watchdog MAVLink ──────────────────────────────────────────────────
    static constexpr unsigned long MAVLINK_TIMEOUT_MS = 5000;

    // ── Batería baja (umbral en bits ADC 12-bit) ──────────────────────────
    static constexpr uint16_t BATTERY_LOW_ADC = 1500;  // ~1.2V en divisor
};
