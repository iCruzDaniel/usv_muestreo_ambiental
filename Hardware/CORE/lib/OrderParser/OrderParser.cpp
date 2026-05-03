/**
 * OrderParser.cpp — Despacha comandos MQTT al SystemFSM.
 */

#include "OrderParser.h"
#include <SystemFSM.h>
#include <Logger.h>

OrderParser::OrderParser(SystemFSM* fsm) : _fsm(fsm) {}

// ── onMessage() ───────────────────────────────────────────────────────────────
void OrderParser::onMessage(const char* topic, const String& payload) {
    // Solo procesar el tópico de órdenes
    if (strcmp(topic, TOPIC_ORDERS) != 0) return;

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err != DeserializationError::Ok) {
        Logger::error("ORDERS", "JSON inválido: %s", err.c_str());
        return;
    }

    const char* cmd = doc["cmd"] | "";
    if (!cmd || strlen(cmd) == 0) {
        Logger::warn("ORDERS", "Mensaje sin campo 'cmd'");
        return;
    }

    SystemEvent event = _resolveCommand(cmd);
    if (event == SystemEvent::NONE) {
        Logger::warn("ORDERS", "Comando desconocido: %s", cmd);
        return;
    }

    // Para SET_COORDS, reserializar el sub-objeto "payload" y pasarlo al FSM
    String innerPayload;
    if (event == SystemEvent::CMD_SET_COORDS && doc.containsKey("payload")) {
        serializeJson(doc["payload"], innerPayload);
    }

    Logger::info("ORDERS", "Comando recibido: %s", cmd);
    _fsm->dispatch(event, innerPayload);
}

// ── _resolveCommand() ─────────────────────────────────────────────────────────
SystemEvent OrderParser::_resolveCommand(const char* cmd) const {
    // switch/case sobre strings usando hash manual o strcmp en cascada
    if (strcmp(cmd, "PREPARE")    == 0) return SystemEvent::CMD_PREPARE;
    if (strcmp(cmd, "SET_COORDS") == 0) return SystemEvent::CMD_SET_COORDS;
    if (strcmp(cmd, "START")      == 0) return SystemEvent::CMD_START;
    if (strcmp(cmd, "CANCEL")     == 0) return SystemEvent::CMD_CANCEL;
    if (strcmp(cmd, "PAUSE")      == 0) return SystemEvent::CMD_PAUSE;
    return SystemEvent::NONE;
}
