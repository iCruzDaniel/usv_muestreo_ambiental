/**
 * MissionController.cpp — Lógica de misión: muestreo en waypoints GPS.
 */

#include "MissionController.h"
#include <MavlinkHandler.h>
#include <Logger.h>
#include <TimeManager.h>
#include <math.h>

static constexpr double EARTH_R_M  = 6371000.0;

// ── Constructor ───────────────────────────────────────────────────────────────
MissionController::MissionController(ISensor* turbidity, ISensor* ph, ISensor* temp,
                                     MavlinkHandler* mavlink, ITransport* transport)
    : _turbidity(turbidity), _ph(ph), _temp(temp),
      _mavlink(mavlink), _transport(transport) {}

// ── begin() ───────────────────────────────────────────────────────────────────
void MissionController::begin() {
    _turbidity->begin();
    _ph->begin();
    _temp->begin();
    Logger::info("MISSION", "Sensores inicializados");
}

// ── update() ─────────────────────────────────────────────────────────────────
void MissionController::update(SystemState sysState) {
    // Actualizar todos los sensores (polling no bloqueante)
    _turbidity->update();
    _ph->update();
    _temp->update();

    if (sysState != SystemState::MISSION_ACTIVE) return;
    if (!_active || _paused || _completed) return;
    if (_mission.count == 0) return;

    // Obtener posición actual de MAVLink
    mavlink_global_position_int_t pos;
    if (!_mavlink->getPosition(pos)) return;

    double curLat = pos.lat / 1e7;
    double curLon = pos.lon / 1e7;

    Waypoint& wp = _mission.waypoints[_mission.current];
    if (!wp.sampled && _isAtWaypoint(wp, curLat, curLon)) {
        _captureSample(wp, curLat, curLon);
    }
}

// ── Control ───────────────────────────────────────────────────────────────────
void MissionController::setWaypoints(const MissionConfig& cfg) {
    _mission   = cfg;
    _completed = false;
    Logger::info("MISSION", "Waypoints cargados: %d", cfg.count);
}

void MissionController::start() {
    if (_mission.count == 0) {
        Logger::warn("MISSION", "Sin waypoints definidos");
        return;
    }
    _active    = true;
    _paused    = false;
    _completed = false;
    _mission.current = 0;
    Logger::info("MISSION", "Misión iniciada — %d waypoints", _mission.count);
}

void MissionController::pause() {
    _paused = true;
    Logger::info("MISSION", "Misión pausada en WP %d", _mission.current);
}

void MissionController::cancel() {
    _active    = false;
    _paused    = false;
    _completed = false;
    Logger::warn("MISSION", "Misión cancelada");
}

// ── Privados ──────────────────────────────────────────────────────────────────
bool MissionController::_isAtWaypoint(const Waypoint& wp,
                                       double curLat, double curLon) const {
    double dist = _haversineDistM(curLat, curLon, wp.lat, wp.lon);
    return dist <= _mission.radiusM;
}

void MissionController::_captureSample(const Waypoint& wp,
                                        double lat, double lon) {
    Logger::info("MISSION", "Muestra WP %d @ (%.6f, %.6f)",
                 _mission.current, lat, lon);

    StaticJsonDocument<512> doc;
    JsonObject m = doc.createNestedObject("mision");
    
    m["mission_id"]   = "MISION-" + String(millis()); // Placeholder o ID real
    m["usv_id"]       = CLIENT_ID;
    m["tipo_mision"]  = "MCA"; // Muestreo de Calidad de Agua
    m["estado_mision"]= "EN_PROGRESO";

    m["punto_id"]     = "PUNTO-" + String(_mission.current);
    m["latitud"]      = lat;
    m["longitud"]     = lon;
    m["profundidad_m"]= 1.5; // Placeholder (requiere ecosonda)

    m["temperatura_agua_c"]   = _temp->getValue();
    m["ph_agua"]              = _ph->getValue();
    m["turbidez_ntu"]         = _turbidity->getValue();
    m["oxigeno_disuelto_ppm"] = 0.0; // Placeholder

    m["timestamp_utc"] = TimeManager::getIsoTimestamp();

    String out;
    serializeJson(doc, out);
    _transport->publish(TOPIC_MISSION, out, 1);  // QoS 1 para datos de misión

    // Marcar como muestreado y avanzar
    _mission.waypoints[_mission.current].sampled = true;
    _advanceWaypoint();
}

void MissionController::_advanceWaypoint() {
    _mission.current++;
    if (_mission.current >= _mission.count) {
        _completed = true;
        _active    = false;
        Logger::info("MISSION", "Misión completada — todos los waypoints visitados");
    } else {
        Logger::info("MISSION", "Avanzando a WP %d", _mission.current);
    }
}

double MissionController::_haversineDistM(double lat1, double lon1,
                                           double lat2, double lon2) const {
    double dlat = (lat2 - lat1) * DEG_TO_RAD;
    double dlon = (lon2 - lon1) * DEG_TO_RAD;
    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) *
               sin(dlon / 2) * sin(dlon / 2);
    return EARTH_R_M * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
}
