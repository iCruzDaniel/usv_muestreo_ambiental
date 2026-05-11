/**
 * MavlinkHandler.cpp — Lectura no bloqueante de mensajes MAVLink v2.
 */

#include "MavlinkHandler.h"
#include <Logger.h>
#include <TimeManager.h>

MavlinkHandler::MavlinkHandler(HardwareSerial& serial)
    : _serial(serial) {}

void MavlinkHandler::begin(uint32_t baud) {
    _serial.begin(baud, SERIAL_8N1, MAV_RX, MAV_TX);
    Logger::info("MAV", "UART iniciado @ %lu bps", baud);
}

// ── update() — llamar desde tarea de misión ───────────────────────────────────
void MavlinkHandler::update() {
    unsigned long now = millis();

    // ── Enviar heartbeat propio periódicamente ────────────────────────────
    if (now - _lastOwnHbMs >= OWN_HEARTBEAT_MS) {
        _lastOwnHbMs = now;
        sendHeartbeat();
    }

    // ── Solicitar streams de datos si aún no los tenemos ──────────────────
    if (!_streamsRequested || (now - _lastStreamReq >= STREAM_REQUEST_INTERVAL)) {
        _lastStreamReq = now;
        requestDataStreams();
    }

    // ── Leer y parsear datos entrantes ────────────────────────────────────
    while (_serial.available()) {
        uint8_t byte = _serial.read();
        mavlink_message_t msg;
        if (mavlink_parse_char(MAVLINK_COMM_0, byte, &msg, &_mavStatus)) {
            _dispatchMessage(msg);
        }
    }
}

// ── sendHeartbeat() — anuncia al autopiloto que existimos ─────────────────────
void MavlinkHandler::sendHeartbeat() {
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];

    mavlink_msg_heartbeat_pack(
        SYSTEM_ID, COMPONENT_ID, &msg,
        MAV_TYPE_ONBOARD_CONTROLLER,    // tipo: companion computer
        MAV_AUTOPILOT_INVALID,          // no somos autopiloto
        0, 0, MAV_STATE_ACTIVE          // base_mode, custom, state
    );

    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    _serial.write(buf, len);
}

// ── requestDataStreams() — pedir al autopiloto que envíe datos ─────────────────
void MavlinkHandler::requestDataStreams(uint8_t targetSys, uint8_t targetComp) {
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];

    // Solicitar todos los streams a ~4 Hz
    // Streams: POSITION, EXTRA1 (attitude), EXTRA2 (sys_status), RC_CHANNELS
    const uint8_t streams[] = {
        MAV_DATA_STREAM_POSITION,       // GPS
        MAV_DATA_STREAM_EXTRA1,         // Attitude
        MAV_DATA_STREAM_EXTRA2,         // SYS_STATUS
        MAV_DATA_STREAM_RC_CHANNELS,    // RC info + RSSI
        MAV_DATA_STREAM_EXTENDED_STATUS // Battery
    };

    for (uint8_t s : streams) {
        mavlink_msg_request_data_stream_pack(
            SYSTEM_ID, COMPONENT_ID, &msg,
            targetSys, targetComp,
            s,      // stream_id
            4,      // rate_hz
            1       // start (1 = enable)
        );
        uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
        _serial.write(buf, len);
        delay(10);  // Pequeña pausa entre requests
    }

    _streamsRequested = true;
}

// ── _dispatchMessage() ────────────────────────────────────────────────────────
void MavlinkHandler::_dispatchMessage(const mavlink_message_t& msg) {
    switch (msg.msgid) {

    case MAVLINK_MSG_ID_HEARTBEAT:
        mavlink_msg_heartbeat_decode(&msg, &_lastHB);
        if (_lastHbMs == 0) {
            Logger::info("MAV", "¡Primer heartbeat recibido! (sysid=%d, type=%d)",
                         msg.sysid, _lastHB.type);
        }
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

    case MAVLINK_MSG_ID_ATTITUDE:
        mavlink_msg_attitude_decode(&msg, &_lastAtt);
        _hasAtt = true;
        break;

    case MAVLINK_MSG_ID_BATTERY_STATUS:
        mavlink_msg_battery_status_decode(&msg, &_lastBatt);
        _hasBatt = true;
        break;

    case MAVLINK_MSG_ID_SYSTEM_TIME:
        {
            mavlink_system_time_t t;
            mavlink_msg_system_time_decode(&msg, &t);
            if (t.time_unix_usec > 0) {
                TimeManager::sync(t.time_unix_usec / 1000000, (t.time_unix_usec / 1000) % 1000);
            }
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

bool MavlinkHandler::getAttitude(mavlink_attitude_t& att) const {
    if (!_hasAtt) return false;
    att = _lastAtt;
    return true;
}

bool MavlinkHandler::getBattery(mavlink_battery_status_t& batt) const {
    if (!_hasBatt) return false;
    batt = _lastBatt;
    return true;
}

bool MavlinkHandler::isConnected() const {
    return (millis() - _lastHbMs) < HEARTBEAT_TIMEOUT_MS;
}
