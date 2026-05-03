/**
 * Logger.cpp
 */

#include "Logger.h"
#include <stdarg.h>

// Statics
ITransport* Logger::_transport = nullptr;
LogLevel    Logger::_minLevel  = LogLevel::INFO;

// ─────────────────────────────────────────────────────────────────────────────
void Logger::setTransport(ITransport* transport) { _transport = transport; }
void Logger::setMinLevel(LogLevel minLevel)       { _minLevel  = minLevel; }

// ── API pública ───────────────────────────────────────────────────────────────
void Logger::info(const char* tag, const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    _log(LogLevel::INFO, tag, fmt, args);
    va_end(args);
}
void Logger::warn(const char* tag, const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    _log(LogLevel::WARN, tag, fmt, args);
    va_end(args);
}
void Logger::error(const char* tag, const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    _log(LogLevel::ERROR_LVL, tag, fmt, args);
    va_end(args);
}

// ── Implementación interna ────────────────────────────────────────────────────
void Logger::_log(LogLevel level, const char* tag, const char* fmt, va_list args) {
    static const char* levelNames[] = {"INFO", "WARN", "ERROR"};
    const char* lvlStr = levelNames[(uint8_t)level];

    // Formatear mensaje
    char msgBuf[256];
    vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);

    // Salida por Serial siempre (con color ANSI si DEBUG_LEVEL >= 3)
#if CORE_DEBUG_LEVEL >= 3
    static const char* colors[] = {"\033[0m", "\033[33m", "\033[31m"};
    Serial.printf("%s[%s][%s] %s\033[0m\n", colors[(uint8_t)level], lvlStr, tag, msgBuf);
#else
    Serial.printf("[%s][%s] %s\n", lvlStr, tag, msgBuf);
#endif

    // Publicar vía MQTT solo si el transporte está listo y nivel >= mínimo
    if (level >= _minLevel && _transport && _transport->isMqttConnected()) {
        _publishMqtt(level, tag, msgBuf);
    }
}

void Logger::_publishMqtt(LogLevel level, const char* tag, const char* msg) {
    static const char* levelNames[] = {"INFO", "WARN", "ERROR"};
    StaticJsonDocument<256> doc;
    doc["ts"]  = millis();
    doc["lvl"] = levelNames[(uint8_t)level];
    doc["tag"] = tag;
    doc["msg"] = msg;

    String out;
    serializeJson(doc, out);
    _transport->publish(TOPIC_LOGS, out, 0);
}
