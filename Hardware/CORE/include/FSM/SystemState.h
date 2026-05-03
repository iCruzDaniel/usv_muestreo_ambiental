/**
 * SystemState.h — Definición de estados y eventos del sistema (FSM principal).
 *
 * Máquina de estados del USV:
 *
 *   BOOT ──► STANDBY ──► MISSION_ACTIVE ──► STANDBY
 *     │          │               │
 *     └──────────┴───────────────┴──► EMERGENCY
 *
 * Transiciones:
 *   BOOT       → STANDBY       : conectividad establecida
 *   STANDBY    → MISSION_ACTIVE: comando START recibido
 *   MISSION_ACTIVE → STANDBY   : misión completada o CANCEL
 *   any        → EMERGENCY     : falla crítica (pérdida de MAVLink, batería baja)
 *   EMERGENCY  → STANDBY       : recuperación manual (PREPARE)
 */

#pragma once
#include <stdint.h>

// ── Estado global del sistema ─────────────────────────────────────────────────
enum class SystemState : uint8_t {
    BOOT          = 0,  ///< Arranque: inicializa hardware y conectividad
    STANDBY       = 1,  ///< Espera activa: conectado, sin misión activa
    MISSION_ACTIVE= 2,  ///< Misión en curso: capturando waypoints
    EMERGENCY     = 3,  ///< Falla crítica: retorno RTL / modo seguro
};

// ── Eventos que disparan transiciones ────────────────────────────────────────
enum class SystemEvent : uint8_t {
    NONE            = 0,
    CONNECTIVITY_UP = 1,  ///< Red MQTT disponible
    CMD_PREPARE     = 2,  ///< Orden PREPARE desde broker
    CMD_START       = 3,  ///< Orden START desde broker
    CMD_CANCEL      = 4,  ///< Orden CANCEL desde broker
    CMD_PAUSE       = 5,  ///< Orden PAUSE desde broker
    CMD_SET_COORDS  = 6,  ///< Orden SET_COORDS desde broker
    MISSION_DONE    = 7,  ///< Todos los waypoints visitados
    CRITICAL_FAULT  = 8,  ///< Fallo crítico de hardware o conectividad
    RECOVERY_OK     = 9,  ///< Sistema recuperado tras EMERGENCY
};

// ── Contexto de misión (waypoints) ───────────────────────────────────────────
struct Waypoint {
    double lat;
    double lon;
    float  altM;      ///< Altitud en metros
    bool   sampled;   ///< true = ya se tomó muestra en este punto
};

static constexpr uint8_t MAX_WAYPOINTS = 20;

struct MissionConfig {
    Waypoint waypoints[MAX_WAYPOINTS];
    uint8_t  count    = 0;
    uint8_t  current  = 0;
    float    radiusM  = 3.0f;  ///< Radio de captura en metros
};
