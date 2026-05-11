/**
 * TurbiditySensor.cpp
 */

#include "TurbiditySensor.h"
#include <stdlib.h>

TurbiditySensor::TurbiditySensor(uint8_t pin, uint8_t filterSize)
    : _pin(pin), _filterSize(filterSize)
{
    _buf = new float[_filterSize]();  // zero-init
}

bool TurbiditySensor::begin() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    Serial.printf("[TURB] Iniciado — pin %d\n", _pin);

#if defined(TURB_CAL_SLOPE) && defined(TURB_CAL_OFFSET)
    setCalibration(TURB_CAL_SLOPE, TURB_CAL_OFFSET);
    Serial.println("[TURB] Calibración cargada desde platformio.ini");
#else
    Serial.println("[TURB] ADVERTENCIA: Valores por defecto. Requiere calibración.");
#endif

    _ready = true;
    return true;
}

void TurbiditySensor::update() {
    unsigned long now = millis();
    if (now - _lastMs < INTERVAL_MS) return;
    _lastMs = now;

    int   raw     = analogRead(_pin);
    float voltage = raw * (3.3f / 4095.0f);
    float ntu     = voltageToNTU(voltage);

    // Media móvil circular
    _buf[_bufIdx] = ntu;
    _bufIdx = (_bufIdx + 1) % _filterSize;

    float sum = 0.0f;
    for (uint8_t i = 0; i < _filterSize; i++) sum += _buf[i];
    _filtered = sum / _filterSize;
}

float TurbiditySensor::voltageToNTU(float v) const {
    // Modelo lineal: NTU = slope * V + offset
    // Calibrar con estándar Formazin 0 NTU (agua pura) y 1000 NTU.
    float ntu = _slope * v + _offset;
    return (ntu < 0.0f) ? 0.0f : ntu;
}
