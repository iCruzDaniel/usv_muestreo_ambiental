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
#include <TimeManager.h>
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
    static const char* stateNames[] = {"INICIALIZANDO", "STANDBY", "EN_MISION", "EMERGENCIA"};

    StaticJsonDocument<1024> doc;
    JsonObject s = doc.createNestedObject("general_usv_status");
    
    s["usv_id"]    = CLIENT_ID;
    s["conexion"]  = _transport->isMqttConnected() ? "ONLINE" : "OFFLINE";
    s["actividad"] = stateNames[(int)_state];

    // Batería y Energía
    mavlink_battery_status_t batt;
    if (_mavlink->getBattery(batt)) {
        s["bateria_porcentaje"] = batt.battery_remaining;
        s["corriente_motor_1_a"] = batt.current_battery / 100.0f; // Ejemplo, mapear según HW
        s["corriente_motor_2_a"] = 0.0;
        s["voltaje_celda_1_v"]  = batt.voltages[0] / 1000.0f;
        s["voltaje_celda_2_v"]  = batt.voltages[1] / 1000.0f;
        s["voltaje_celda_3_v"]  = batt.voltages[2] / 1000.0f;
    } else {
        s["bateria_porcentaje"] = (analogRead(PIN_BATTERY) / 4095.0f) * 100.0f; // Fallback
    }

    // Orientación (Attitude)
    mavlink_attitude_t att;
    if (_mavlink->getAttitude(att)) {
        s["roll_grados"]  = att.roll * RAD_TO_DEG;
        s["pitch_grados"] = att.pitch * RAD_TO_DEG;
        s["yaw_grados"]   = att.yaw * RAD_TO_DEG;
    }

    // Posición GPS
    mavlink_global_position_int_t pos;
    if (_mavlink->getPosition(pos)) {
        s["latitud"]  = pos.lat / 1e7;
        s["longitud"] = pos.lon / 1e7;
    }

    s["timestamp_utc"] = TimeManager::getIsoTimestamp();

    String out;
    serializeJson(doc, out);
    _transport->publish(TOPIC_STATUS, out, 0);
}
