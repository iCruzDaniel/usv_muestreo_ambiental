/**
 * MavlinkHandler.cpp — Lectura no bloqueante de mensajes MAVLink v2.
 */

#include "MavlinkHandler.h"
#include <Logger.h>

MavlinkHandler::MavlinkHandler(HardwareSerial& serial)
    : _serial(serial) {}

void MavlinkHandler::begin(uint32_t baud) {
    _serial.begin(baud, SERIAL_8N1, MAV_RX, MAV_TX);
    Logger::info("MAV", "UART iniciado @ %lu bps", baud);
}

// ── update() — llamar desde tarea de misión ───────────────────────────────────
void MavlinkHandler::update() {
    while (_serial.available()) {
        uint8_t byte = _serial.read();
        mavlink_message_t msg;
        if (mavlink_parse_char(MAVLINK_COMM_0, byte, &msg, &_mavStatus)) {
            _dispatchMessage(msg);
        }
    }
}

// ── _dispatchMessage() ────────────────────────────────────────────────────────
void MavlinkHandler::_dispatchMessage(const mavlink_message_t& msg) {
    switch (msg.msgid) {

    case MAVLINK_MSG_ID_HEARTBEAT:
        mavlink_msg_heartbeat_decode(&msg, &_lastHB);
        _lastHbMs = millis();
        break;

    case MAVLINK_MSG_ID_GLOBAL_POSITION_INT:
        mavlink_msg_global_position_int_decode(&msg, &_lastPos);
        _hasPos = true;
        break;

    case MAVLINK_MSG_ID_SYS_STATUS:
        mavlink_msg_sys_status_decode(&msg, &_lastSysSt);
        break;

    case MAVLINK_MSG_ID_RC_CHANNELS:
        // Extraer RSSI del canal 16 (convención ArduPilot)
        {
            mavlink_rc_channels_t rc;
            mavlink_msg_rc_channels_decode(&msg, &rc);
            _rssi = rc.rssi;
        }
        break;

    default:
        break;
    }
}

bool MavlinkHandler::getHeartbeat(mavlink_heartbeat_t& hb) const {
    if (_lastHbMs == 0) return false;
    hb = _lastHB;
    return true;
}

bool MavlinkHandler::getPosition(mavlink_global_position_int_t& pos) const {
    if (!_hasPos) return false;
    pos = _lastPos;
    return true;
}

bool MavlinkHandler::isConnected() const {
    return (millis() - _lastHbMs) < HEARTBEAT_TIMEOUT_MS;
}
