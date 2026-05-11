/**
 * MavlinkHandler.h — Parsea mensajes MAVLink v2 desde UART2.
 * No bloqueante. Expone heartbeat, posición GPS y estado de conexión.
 */

#pragma once
#include <Arduino.h>
#include <MAVLink.h>

class MavlinkHandler {
public:
    explicit MavlinkHandler(HardwareSerial& serial);

    void begin(uint32_t baud = 57600);
    void update();

    /** Envía heartbeat de companion computer — llamar periódicamente */
    void sendHeartbeat();

    /** Solicita streams de datos al autopiloto (posición, actitud, batería) */
    void requestDataStreams(uint8_t targetSys = 1, uint8_t targetComp = 1);

    // ── Datos MAVLink ─────────────────────────────────────────────────────
    bool getHeartbeat(mavlink_heartbeat_t& hb) const;
    bool getPosition(mavlink_global_position_int_t& pos) const;

    /** true si se recibió heartbeat en los últimos HEARTBEAT_TIMEOUT_MS ms */
    bool isConnected() const;

    /** Datos de actitud (roll, pitch, yaw) */
    bool getAttitude(mavlink_attitude_t& att) const;

    /** Datos de batería y potencia */
    bool getBattery(mavlink_battery_status_t& batt) const;

    /** Retorna el último RSSI estimado (0 si no disponible) */
    uint8_t getRssi() const { return _rssi; }

private:
    HardwareSerial& _serial;

    mavlink_status_t             _mavStatus   = {};
    mavlink_message_t            _lastMsg     = {};
    mavlink_heartbeat_t          _lastHB      = {};
    mavlink_global_position_int_t _lastPos    = {};
    mavlink_sys_status_t         _lastSysSt   = {};
    mavlink_attitude_t           _lastAtt     = {};
    mavlink_battery_status_t     _lastBatt    = {};

    unsigned long _lastHbMs       = 0;
    unsigned long _lastOwnHbMs    = 0;
    unsigned long _lastStreamReq  = 0;
    bool          _hasPos         = false;
    bool          _hasAtt         = false;
    bool          _hasBatt        = false;
    bool          _streamsRequested = false;
    uint8_t       _rssi           = 0;

    static constexpr unsigned long HEARTBEAT_TIMEOUT_MS   = 5000;
    static constexpr unsigned long OWN_HEARTBEAT_MS       = 1000;  // Enviar HB cada 1s
    static constexpr unsigned long STREAM_REQUEST_INTERVAL = 10000; // Re-request cada 10s
    static constexpr uint8_t      SYSTEM_ID               = 254;   // Companion computer
    static constexpr uint8_t      COMPONENT_ID            = 191;   // MAV_COMP_ID_ONBOARD_COMPUTER

    void _dispatchMessage(const mavlink_message_t& msg);
};
