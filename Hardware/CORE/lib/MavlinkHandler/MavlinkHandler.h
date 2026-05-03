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

    // ── Datos MAVLink ─────────────────────────────────────────────────────
    bool getHeartbeat(mavlink_heartbeat_t& hb) const;
    bool getPosition(mavlink_global_position_int_t& pos) const;

    /** true si se recibió heartbeat en los últimos HEARTBEAT_TIMEOUT_MS ms */
    bool isConnected() const;

    /** Retorna el último RSSI estimado (0 si no disponible) */
    uint8_t getRssi() const { return _rssi; }

private:
    HardwareSerial& _serial;

    mavlink_status_t             _mavStatus   = {};
    mavlink_message_t            _lastMsg     = {};
    mavlink_heartbeat_t          _lastHB      = {};
    mavlink_global_position_int_t _lastPos    = {};
    mavlink_sys_status_t         _lastSysSt   = {};

    unsigned long _lastHbMs = 0;
    bool          _hasPos   = false;
    uint8_t       _rssi     = 0;

    static constexpr unsigned long HEARTBEAT_TIMEOUT_MS = 5000;

    void _dispatchMessage(const mavlink_message_t& msg);
};
