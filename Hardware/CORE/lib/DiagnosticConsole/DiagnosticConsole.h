/**
 * DiagnosticConsole.h — Consola de diagnóstico y calibración por Serial.
 *
 * Funciones:
 *   1. Dashboard periódico: vista consolidada de tareas, sensores, FSM, red.
 *   2. Comandos Serial interactivos para calibración de sensores.
 *
 * Comandos disponibles (escribir en Serial Monitor y presionar Enter):
 *
 *   status          — Imprime dashboard completo una vez
 *   sensors         — Lecturas crudas de sensores (voltaje + valor convertido)
 *   cal ph4         — Marcar lectura actual como referencia pH 4
 *   cal ph7         — Marcar lectura actual como referencia pH 7
 *   cal phapply     — Aplicar calibración pH con los dos puntos capturados
 *   cal turb_clear  — Marcar lectura actual como agua clara (0 NTU)
 *   cal turb_std    — Marcar lectura actual como estándar conocido (NTU)
 *   cal turb_apply <NTU_valor>  — Aplicar calibración con el valor estándar
 *   help            — Lista de comandos
 *
 * Activar/desactivar compilando con:
 *   -D ENABLE_DIAGNOSTICS    (en platformio.ini build_flags)
 */

#pragma once
#include <Arduino.h>

// Forward declarations para evitar dependencias circulares
class ITransport;
class ISensor;
class MavlinkHandler;
class SystemFSM;
class TurbiditySensor;
class PHSensor;
class TemperatureSensor;

class DiagnosticConsole {
public:
    DiagnosticConsole(
        ITransport*        transport,
        MavlinkHandler*    mavlink,
        SystemFSM*         fsm,
        TurbiditySensor*   turbidity,
        PHSensor*          ph,
        TemperatureSensor* temp
    );

    /** Inicializar (llamar en setup). */
    void begin();

    /**
     * Llamar periódicamente (cada ~100ms) desde una tarea FreeRTOS.
     * Lee comandos Serial, imprime dashboard si toca.
     */
    void update();

    /** Forzar impresión del dashboard ahora. */
    void printDashboard();

    /** Imprimir lecturas crudas de sensores (voltaje ADC + conversión). */
    void printSensorRaw();

private:
    ITransport*        _transport;
    MavlinkHandler*    _mavlink;
    SystemFSM*         _fsm;
    TurbiditySensor*   _turbidity;
    PHSensor*          _ph;
    TemperatureSensor* _temp;

    // Buffer de entrada serial
    String _inputBuf;

    // Dashboard periódico
    unsigned long _lastDashMs   = 0;
    static constexpr unsigned long DASH_INTERVAL_MS = 10000; // cada 10 seg

    // Calibración pH: voltajes capturados
    float _phV4  = 0.0f;
    float _phV7  = 0.0f;
    bool  _hasV4 = false;
    bool  _hasV7 = false;

    // Calibración turbidez: voltaje capturado
    float _turbVClear = 0.0f;
    float _turbVStd   = 0.0f;
    bool  _hasClear   = false;
    bool  _hasStd     = false;

    // ── Procesamiento ─────────────────────────────────────────────────────
    void _processCommand(const String& cmd);
    void _printHelp();
    void _printHeader(const char* title);
    void _printSeparator();

    // ── Lectura cruda de ADC ──────────────────────────────────────────────
    float _readVoltage(uint8_t pin);
};
