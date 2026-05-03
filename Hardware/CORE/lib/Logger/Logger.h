/**
 * Logger.h — Sistema de logging con niveles y publicación MQTT asíncrona.
 *
 * Uso:
 *   Logger::info("GSM", "Conectado al broker");
 *   Logger::warn("SENS", "Turbidez fuera de rango: %.1f NTU", val);
 *   Logger::error("FSM", "Falla crítica estado %d", state);
 *
 * Si se configura un transporte (setTransport), los mensajes de nivel >= INFO
 * se serializan como JSON y se publican en TOPIC_LOGS (no bloqueante).
 *
 * Formato JSON publicado:
 * {
 *   "ts":   <millis>,
 *   "lvl":  "INFO"|"WARN"|"ERROR",
 *   "tag":  "<módulo>",
 *   "msg":  "<mensaje>"
 * }
 */

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HAL/ITransport.h>
#include <config.h>

enum class LogLevel : uint8_t { INFO = 0, WARN = 1, ERROR_LVL = 2 };

class Logger {
public:
    /** Inyecta el transporte para publicar logs en MQTT. Opcional. */
    static void setTransport(ITransport* transport);

    /** Filtra los mensajes: solo los >= minLevel se publican vía MQTT. */
    static void setMinLevel(LogLevel minLevel);

    // ── API de logging ────────────────────────────────────────────────────
    static void info (const char* tag, const char* fmt, ...);
    static void warn (const char* tag, const char* fmt, ...);
    static void error(const char* tag, const char* fmt, ...);

private:
    static void _log(LogLevel level, const char* tag, const char* fmt, va_list args);
    static void _publishMqtt(LogLevel level, const char* tag, const char* msg);

    static ITransport* _transport;
    static LogLevel    _minLevel;
};
