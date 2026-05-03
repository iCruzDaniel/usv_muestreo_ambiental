/**
 * SystemFSM.cpp — Máquina de estados del USV.
 *
 * Transiciones:
 *   BOOT           → STANDBY       : conectividad MQTT lista
 *   STANDBY        → MISSION_ACTIVE : CMD_START
 *   MISSION_ACTIVE → STANDBY       : misión completada | CMD_CANCEL
 *   any            → EMERGENCY     : CRITICAL_FAULT (MAVLink timeout, batería baja)
 *   EMERGENCY      → STANDBY       : CMD_PREPARE + conectividad restaurada
 */

#include "SystemFSM.h"
#include <MavlinkHandler.h>
#include <MissionController.h>
#include <Logger.h>
#include <ArduinoJson.h>

// ── Constructor ───────────────────────────────────────────────────────────────
SystemFSM::SystemFSM(MissionController* mission, ITransport* transport,
                     MavlinkHandler* mavlink)
    : _mission(mission), _transport(transport), _mavlink(mavlink) {}

void SystemFSM::begin() {
    _stateMs = millis();
    Logger::info("FSM", "Sistema iniciado — estado BOOT");
}

// ── update() — llamado desde tarea de telemetría ──────────────────────────────
void SystemFSM::update() {
    unsigned long now = millis();

    // ── Watchdog batería (desde cualquier estado) ─────────────────────────
    uint16_t battAdc = analogRead(PIN_BATTERY);
    if (battAdc < BATTERY_LOW_ADC && _state != SystemState::EMERGENCY) {
        Logger::error("FSM", "Batería crítica (ADC=%d). Entrando en EMERGENCY", battAdc);
        dispatch(SystemEvent::CRITICAL_FAULT);
    }

    // ── Watchdog MAVLink (solo en misión activa) ──────────────────────────
    if (_state == SystemState::MISSION_ACTIVE && !_mavlink->isConnected()) {
        Logger::error("FSM", "Pérdida de MAVLink. Entrando en EMERGENCY");
        dispatch(SystemEvent::CRITICAL_FAULT);
    }

    // ── Detectar misión completada ────────────────────────────────────────
    if (_state == SystemState::MISSION_ACTIVE && _mission->isCompleted()) {
        dispatch(SystemEvent::MISSION_DONE);
    }

    // ── Handler del estado actual ─────────────────────────────────────────
    switch (_state) {
        case SystemState::BOOT:          _handleBoot();          break;
        case SystemState::STANDBY:       _handleStandby();       break;
        case SystemState::MISSION_ACTIVE:_handleMissionActive(); break;
        case SystemState::EMERGENCY:     _handleEmergency();     break;
    }

    // ── Publicar estado periódicamente ────────────────────────────────────
    if (_transport->isMqttConnected() && (now - _lastStatusMs >= STATUS_INTERVAL_MS)) {
        _lastStatusMs = now;
        _publishStatus();
    }
}

// ── dispatch() — router de eventos ───────────────────────────────────────────
void SystemFSM::dispatch(SystemEvent event, const String& payload) {
    switch (event) {

    case SystemEvent::CONNECTIVITY_UP:
        if (_state == SystemState::BOOT) _transitionTo(SystemState::STANDBY);
        break;

    case SystemEvent::CMD_PREPARE:
        if (_state == SystemState::EMERGENCY) {
            Logger::info("FSM", "PREPARE recibido — recuperando sistema");
            _transitionTo(SystemState::STANDBY);
        }
        break;

    case SystemEvent::CMD_SET_COORDS: {
        // Parsear JSON de waypoints: {"waypoints":[{"lat":X,"lon":Y,"alt":Z},...]}
        StaticJsonDocument<1024> doc;
        if (deserializeJson(doc, payload) != DeserializationError::Ok) {
            Logger::error("FSM", "SET_COORDS: JSON inválido");
            break;
        }
        MissionConfig cfg;
        JsonArray wps = doc["waypoints"].as<JsonArray>();
        for (JsonObject wp : wps) {
            if (cfg.count >= MAX_WAYPOINTS) break;
            cfg.waypoints[cfg.count] = {
                wp["lat"].as<double>(),
                wp["lon"].as<double>(),
                wp["alt"] | 0.0f,
                false
            };
            cfg.count++;
        }
        cfg.radiusM = doc["radius_m"] | 3.0f;
        _mission->setWaypoints(cfg);
        break;
    }

    case SystemEvent::CMD_START:
        if (_state == SystemState::STANDBY) {
            _mission->start();
            _transitionTo(SystemState::MISSION_ACTIVE);
        } else {
            Logger::warn("FSM", "CMD_START ignorado — estado actual no es STANDBY");
        }
        break;

    case SystemEvent::CMD_CANCEL:
        if (_state == SystemState::MISSION_ACTIVE) {
            _mission->cancel();
            _transitionTo(SystemState::STANDBY);
        }
        break;

    case SystemEvent::CMD_PAUSE:
        if (_state == SystemState::MISSION_ACTIVE) {
            _mission->pause();
            Logger::info("FSM", "Misión pausada");
        }
        break;

    case SystemEvent::MISSION_DONE:
        _transitionTo(SystemState::STANDBY);
        break;

    case SystemEvent::CRITICAL_FAULT:
        _transitionTo(SystemState::EMERGENCY);
        break;

    default:
        break;
    }
}

// ── Handlers por estado ───────────────────────────────────────────────────────
void SystemFSM::_handleBoot() {
    // Transición automática cuando el broker MQTT está listo
    if (_transport->isMqttConnected()) {
        Logger::info("FSM", "Conectividad establecida");
        dispatch(SystemEvent::CONNECTIVITY_UP);
    }
}

void SystemFSM::_handleStandby() {
    // En STANDBY solo esperamos órdenes externas vía MQTT
}

void SystemFSM::_handleMissionActive() {
    _mission->update(SystemState::MISSION_ACTIVE);
}

void SystemFSM::_handleEmergency() {
    // En EMERGENCY, el transporte sigue publicando logs y esperando CMD_PREPARE
    // El autopiloto debería estar en RTL por MAVLink (implementar si es necesario)
}

// ── _transitionTo() ──────────────────────────────────────────────────────────
void SystemFSM::_transitionTo(SystemState next) {
    static const char* names[] = {"BOOT", "STANDBY", "MISSION_ACTIVE", "EMERGENCY"};
    Logger::info("FSM", "Transición: %s → %s",
                 names[(int)_state], names[(int)next]);
    _state   = next;
    _stateMs = millis();
}

// ── _publishStatus() — tópico general_status ─────────────────────────────────
void SystemFSM::_publishStatus() {
    static const char* stateNames[] = {"BOOT", "STANDBY", "MISSION_ACTIVE", "EMERGENCY"};

    StaticJsonDocument<256> doc;
    doc["ts"]         = millis();
    doc["state"]      = stateNames[(int)_state];
    doc["wp_current"] = _mission->currentWaypointIdx();
    doc["battery_adc"]= analogRead(PIN_BATTERY);
    doc["mavlink_ok"] = _mavlink->isConnected();
    doc["client_id"]  = CLIENT_ID;

    String out;
    serializeJson(doc, out);
    _transport->publish(TOPIC_STATUS, out, 0);
}
