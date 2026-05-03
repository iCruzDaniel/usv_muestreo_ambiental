/**
 * DiagnosticConsole.cpp — Implementación del dashboard y calibración.
 */

#include "DiagnosticConsole.h"
#include <HAL/ITransport.h>
#include <MavlinkHandler.h>
#include <SystemFSM.h>
#include <TurbiditySensor/TurbiditySensor.h>
#include <PHSensor/PHSensor.h>
#include <TemperatureSensor/TemperatureSensor.h>
#include <Logger.h>
#include <config.h>

// ── Constructor ───────────────────────────────────────────────────────────────
DiagnosticConsole::DiagnosticConsole(
    ITransport*        transport,
    MavlinkHandler*    mavlink,
    SystemFSM*         fsm,
    TurbiditySensor*   turbidity,
    PHSensor*          ph,
    TemperatureSensor* temp
) : _transport(transport), _mavlink(mavlink), _fsm(fsm),
    _turbidity(turbidity), _ph(ph), _temp(temp) {}

// ── begin() ───────────────────────────────────────────────────────────────────
void DiagnosticConsole::begin() {
    Serial.println();
    _printSeparator();
    Serial.println("  DIAGNOSTIC CONSOLE ACTIVA");
    Serial.println("  Escribe 'help' + Enter para ver comandos");
    _printSeparator();
    Serial.println();
    _lastDashMs = millis();
}

// ── update() ─────────────────────────────────────────────────────────────────
void DiagnosticConsole::update() {
    // ── Leer comandos del Serial ──────────────────────────────────────────
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (_inputBuf.length() > 0) {
                _inputBuf.trim();
                _processCommand(_inputBuf);
                _inputBuf = "";
            }
        } else {
            _inputBuf += c;
            if (_inputBuf.length() > 64) _inputBuf = "";  // overflow protection
        }
    }

    // ── Dashboard periódico ───────────────────────────────────────────────
    unsigned long now = millis();
    if (now - _lastDashMs >= DASH_INTERVAL_MS) {
        _lastDashMs = now;
        printDashboard();
    }
}

// ── printDashboard() ─────────────────────────────────────────────────────────
void DiagnosticConsole::printDashboard() {
    static const char* stateNames[] = {"BOOT", "STANDBY", "MISSION_ACTIVE", "EMERGENCY"};

    Serial.println();
    _printHeader("DASHBOARD USV");

    // ── Estado del sistema ────────────────────────────────────────────────
    Serial.printf("  Estado FSM     : %s\n", stateNames[(int)_fsm->getState()]);
    Serial.printf("  Uptime         : %lu s\n", millis() / 1000);

    // ── Conectividad ──────────────────────────────────────────────────────
    Serial.printf("  Red lista      : %s\n", _transport->isNetworkReady()  ? "SI ✓" : "NO ✗");
    Serial.printf("  MQTT conectado : %s\n", _transport->isMqttConnected() ? "SI ✓" : "NO ✗");

    // ── MAVLink ───────────────────────────────────────────────────────────
    Serial.printf("  MAVLink        : %s\n", _mavlink->isConnected() ? "CONECTADO ✓" : "SIN SEÑAL ✗");

    mavlink_global_position_int_t pos;
    if (_mavlink->getPosition(pos)) {
        Serial.printf("  GPS            : lat=%.6f  lon=%.6f  alt=%.1fm\n",
                       pos.lat / 1e7, pos.lon / 1e7, pos.alt / 1000.0);
    } else {
        Serial.println("  GPS            : sin fix");
    }

    // ── Batería ───────────────────────────────────────────────────────────
    uint16_t battAdc = analogRead(PIN_BATTERY);
    float battV = battAdc * (3.3f / 4095.0f);
    Serial.printf("  Bateria ADC    : %d  (%.2f V en pin)\n", battAdc, battV);

    // ── Sensores ──────────────────────────────────────────────────────────
    _printSeparator();
    Serial.println("  SENSORES");
    Serial.printf("  Turbidez  : %.1f NTU   (ready=%s)\n",
                   _turbidity->getValue(), _turbidity->isReady() ? "si" : "no");
    Serial.printf("  pH        : %.2f       (ready=%s)\n",
                   _ph->getValue(), _ph->isReady() ? "si" : "no");
    Serial.printf("  Temp      : %.1f °C    (ready=%s, fault=0x%02X)\n",
                   _temp->getValue(), _temp->isReady() ? "si" : "no", _temp->getFaultCode());

    // ── FreeRTOS ──────────────────────────────────────────────────────────
    _printSeparator();
    Serial.println("  FREERTOS");
    Serial.printf("  Heap libre     : %d bytes\n", ESP.getFreeHeap());
    Serial.printf("  Heap min       : %d bytes\n", ESP.getMinFreeHeap());

    _printSeparator();
    Serial.println();
}

// ── printSensorRaw() ─────────────────────────────────────────────────────────
void DiagnosticConsole::printSensorRaw() {
    Serial.println();
    _printHeader("LECTURAS CRUDAS DE SENSORES");

    // Turbidez
    float turbV = _readVoltage(PIN_TURBIDITY);
    Serial.printf("  [TURBIDEZ] Pin %d\n", PIN_TURBIDITY);
    Serial.printf("    ADC raw  : %d\n", analogRead(PIN_TURBIDITY));
    Serial.printf("    Voltaje  : %.4f V\n", turbV);
    Serial.printf("    NTU calc : %.1f\n", _turbidity->voltageToNTU(turbV));
    Serial.printf("    NTU filt : %.1f (valor filtrado actual)\n", _turbidity->getValue());

    Serial.println();

    // pH
    float phV = _readVoltage(PIN_PH);
    Serial.printf("  [PH] Pin %d\n", PIN_PH);
    Serial.printf("    ADC raw  : %d\n", analogRead(PIN_PH));
    Serial.printf("    Voltaje  : %.4f V\n", phV);
    Serial.printf("    pH calc  : %.2f\n", _ph->voltageToPhValue(phV));
    Serial.printf("    pH filt  : %.2f (valor filtrado actual)\n", _ph->getValue());

    Serial.println();

    // Temperatura
    Serial.printf("  [TEMPERATURA] MAX31865 CS=%d\n", MAX31865_CS);
    Serial.printf("    Temp     : %.2f °C\n", _temp->getValue());
    Serial.printf("    Fault    : 0x%02X %s\n", _temp->getFaultCode(),
                   _temp->getFaultCode() == 0 ? "(sin falla)" : "(¡FALLA!)");

    _printSeparator();
    Serial.println();
}

// ── _processCommand() ────────────────────────────────────────────────────────
void DiagnosticConsole::_processCommand(const String& cmd) {
    Serial.printf("\n> %s\n", cmd.c_str());

    // ── Comandos simples ──────────────────────────────────────────────────
    if (cmd == "help") {
        _printHelp();
        return;
    }
    if (cmd == "status") {
        printDashboard();
        return;
    }
    if (cmd == "sensors") {
        printSensorRaw();
        return;
    }

    // ── Calibración pH ────────────────────────────────────────────────────
    if (cmd == "cal ph4") {
        _phV4 = _readVoltage(PIN_PH);
        _hasV4 = true;
        Serial.printf("  ✓ pH4 capturado: V = %.4f V\n", _phV4);
        Serial.println("  Ahora sumerge en buffer pH 7 y escribe: cal ph7");
        return;
    }
    if (cmd == "cal ph7") {
        _phV7 = _readVoltage(PIN_PH);
        _hasV7 = true;
        Serial.printf("  ✓ pH7 capturado: V = %.4f V\n", _phV7);
        if (_hasV4) {
            Serial.println("  Ambos puntos listos. Escribe: cal phapply");
        } else {
            Serial.println("  Falta pH4. Sumerge en buffer pH 4 y escribe: cal ph4");
        }
        return;
    }
    if (cmd == "cal phapply") {
        if (!_hasV4 || !_hasV7) {
            Serial.println("  ✗ Faltan puntos. Usa 'cal ph4' y 'cal ph7' primero.");
            return;
        }
        _ph->calibrate(_phV4, _phV7);
        Serial.println("  ✓ Calibración pH aplicada.");
        Serial.printf("    pH4 @ %.4fV → 4.00\n", _phV4);
        Serial.printf("    pH7 @ %.4fV → 7.00\n", _phV7);
        Serial.printf("    Verificación: V=%.4f → pH=%.2f\n",
                       _readVoltage(PIN_PH), _ph->voltageToPhValue(_readVoltage(PIN_PH)));
        Serial.println();
        Serial.println("  ⚠ IMPORTANTE: Anota estos valores para hardcodear en build_flags");
        Serial.printf("    -D PH_CAL_V4=%.4f\n", _phV4);
        Serial.printf("    -D PH_CAL_V7=%.4f\n", _phV7);
        return;
    }

    // ── Calibración turbidez ──────────────────────────────────────────────
    if (cmd == "cal turb_clear") {
        _turbVClear = _readVoltage(PIN_TURBIDITY);
        _hasClear = true;
        Serial.printf("  ✓ Agua clara capturada: V = %.4f V (asumimos 0 NTU)\n", _turbVClear);
        Serial.println("  Ahora sumerge en estándar conocido y escribe: cal turb_std");
        return;
    }
    if (cmd == "cal turb_std") {
        _turbVStd = _readVoltage(PIN_TURBIDITY);
        _hasStd = true;
        Serial.printf("  ✓ Estándar capturado: V = %.4f V\n", _turbVStd);
        Serial.println("  Escribe: cal turb_apply <valor_NTU_del_estandar>");
        Serial.println("  Ejemplo: cal turb_apply 1000");
        return;
    }
    if (cmd.startsWith("cal turb_apply")) {
        if (!_hasClear || !_hasStd) {
            Serial.println("  ✗ Faltan puntos. Usa 'cal turb_clear' y 'cal turb_std' primero.");
            return;
        }
        // Extraer valor NTU del comando
        String ntuStr = cmd.substring(15);
        ntuStr.trim();
        float ntuStd = ntuStr.toFloat();
        if (ntuStd <= 0.0f) {
            Serial.println("  ✗ Valor NTU inválido. Ejemplo: cal turb_apply 1000");
            return;
        }
        // Calcular slope y offset: NTU = slope * V + offset
        // punto 1: (V_clear, 0 NTU),  punto 2: (V_std, ntuStd)
        float dv = _turbVStd - _turbVClear;
        if (fabsf(dv) < 0.01f) {
            Serial.println("  ✗ Voltajes demasiado cercanos. Verifica sensor.");
            return;
        }
        float slope  = ntuStd / dv;
        float offset = -slope * _turbVClear;
        _turbidity->setCalibration(slope, offset);

        Serial.println("  ✓ Calibración turbidez aplicada.");
        Serial.printf("    Agua clara: %.4fV → 0 NTU\n", _turbVClear);
        Serial.printf("    Estándar:   %.4fV → %.0f NTU\n", _turbVStd, ntuStd);
        Serial.printf("    Slope=%.2f  Offset=%.2f\n", slope, offset);
        Serial.printf("    Verificación: V=%.4f → %.1f NTU\n",
                       _readVoltage(PIN_TURBIDITY),
                       _turbidity->voltageToNTU(_readVoltage(PIN_TURBIDITY)));
        Serial.println();
        Serial.println("  ⚠ IMPORTANTE: Anota estos valores para hardcodear en build_flags");
        Serial.printf("    -D TURB_CAL_SLOPE=%.4f\n", slope);
        Serial.printf("    -D TURB_CAL_OFFSET=%.4f\n", offset);
        return;
    }

    Serial.printf("  ✗ Comando desconocido: '%s'. Escribe 'help'.\n", cmd.c_str());
}

// ── _printHelp() ─────────────────────────────────────────────────────────────
void DiagnosticConsole::_printHelp() {
    _printHeader("COMANDOS DISPONIBLES");
    Serial.println("  status              Dashboard completo del sistema");
    Serial.println("  sensors             Lecturas crudas (voltaje + conversión)");
    Serial.println();
    Serial.println("  -- CALIBRACION pH (2 puntos) --");
    Serial.println("  cal ph4             Capturar voltaje en buffer pH 4");
    Serial.println("  cal ph7             Capturar voltaje en buffer pH 7");
    Serial.println("  cal phapply         Aplicar calibración con ambos puntos");
    Serial.println();
    Serial.println("  -- CALIBRACION TURBIDEZ (2 puntos) --");
    Serial.println("  cal turb_clear      Capturar voltaje en agua clara (0 NTU)");
    Serial.println("  cal turb_std        Capturar voltaje en estándar conocido");
    Serial.println("  cal turb_apply N    Aplicar con N = valor NTU del estándar");
    Serial.println();
    Serial.println("  help                Mostrar esta ayuda");
    _printSeparator();
}

// ── Helpers de formato ────────────────────────────────────────────────────────
void DiagnosticConsole::_printHeader(const char* title) {
    Serial.println("╔══════════════════════════════════════════╗");
    Serial.printf( "║  %-40s║\n", title);
    Serial.println("╠══════════════════════════════════════════╣");
}

void DiagnosticConsole::_printSeparator() {
    Serial.println("╚══════════════════════════════════════════╝");
}

// ── _readVoltage() — Lectura promedio de 16 muestras ─────────────────────────
float DiagnosticConsole::_readVoltage(uint8_t pin) {
    uint32_t sum = 0;
    const uint8_t N = 16;
    for (uint8_t i = 0; i < N; i++) {
        sum += analogRead(pin);
        delayMicroseconds(200);
    }
    return (sum / N) * (3.3f / 4095.0f);
}
