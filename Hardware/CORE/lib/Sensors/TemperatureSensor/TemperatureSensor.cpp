/**
 * TemperatureSensor.cpp
 */

#include "TemperatureSensor.h"

TemperatureSensor::TemperatureSensor(uint8_t csPin)
    : _rtd(csPin) {}

bool TemperatureSensor::begin() {
    if (!_rtd.begin(MAX31865_3WIRE)) {
        Serial.println("[TEMP] ERROR: MAX31865 no detectado");
        _ready = false;
        return false;
    }
    Serial.println("[TEMP] MAX31865 inicializado (PT100, 3-wire)");
    _ready = true;
    return true;
}

void TemperatureSensor::update() {
    unsigned long now = millis();
    if (now - _lastMs < INTERVAL_MS) return;
    _lastMs = now;

    // Leer temperatura y verificar fallas
    _fault = _rtd.readFault();
    if (_fault) {
        Serial.printf("[TEMP] Falla MAX31865: 0x%02X\n", _fault);
        _rtd.clearFault();
        _ready = false;
        return;
    }

    _tempC = _rtd.temperature(R_NOMINAL, R_REF);
    _ready = true;
}
