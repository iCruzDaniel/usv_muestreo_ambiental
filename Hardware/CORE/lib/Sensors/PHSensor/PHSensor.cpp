/**
 * PHSensor.cpp
 */

#include "PHSensor.h"
#include <stdlib.h>

PHSensor::PHSensor(uint8_t pin, uint8_t filterSize)
    : _pin(pin), _filterSize(filterSize)
{
    _buf = new float[_filterSize]();
}

bool PHSensor::begin() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    Serial.printf("[PH] Iniciado — pin %d\n", _pin);

#if defined(PH_CAL_V4) && defined(PH_CAL_V7)
    calibrate(PH_CAL_V4, PH_CAL_V7);
    Serial.println("[PH] Calibración cargada desde platformio.ini");
#else
    Serial.println("[PH] ADVERTENCIA: Valores por defecto. Requiere calibración.");
#endif

    _ready = true;
    return true;
}

void PHSensor::update() {
    unsigned long now = millis();
    if (now - _lastMs < INTERVAL_MS) return;
    _lastMs = now;

    int   raw     = analogRead(_pin);
    float voltage = raw * (3.3f / 4095.0f);
    float ph      = voltageToPhValue(voltage);

    // Media móvil circular
    _buf[_bufIdx] = ph;
    _bufIdx = (_bufIdx + 1) % _filterSize;

    float sum = 0.0f;
    for (uint8_t i = 0; i < _filterSize; i++) sum += _buf[i];
    _filtered = sum / _filterSize;
}

float PHSensor::voltageToPhValue(float v) const {
    // Ecuación lineal: pH = m*V + b
    // Calibrar con buffers pH 4 y pH 7:
    //   m = (7 - 4) / (v7 - v4)
    //   b = 7 - m * v7
    float ph = _m * v + _b;
    // Acotar a rango físico válido
    if (ph < 0.0f)  ph = 0.0f;
    if (ph > 14.0f) ph = 14.0f;
    return ph;
}

void PHSensor::calibrate(float v4, float v7) {
    if (fabsf(v7 - v4) < 0.01f) {
        Serial.println("[PH] ERROR calibración: voltajes demasiado cercanos");
        return;
    }
    _m = (7.0f - 4.0f) / (v7 - v4);
    _b = 7.0f - _m * v7;
    Serial.printf("[PH] Calibrado — m=%.4f  b=%.4f\n", _m, _b);
}
