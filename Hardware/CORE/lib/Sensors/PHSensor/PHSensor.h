/**
 * PHSensor.h — Sonda de pH analógica.
 * Implementa ISensor. Calibración de dos puntos (pH4 / pH7).
 */

#pragma once
#include <Sensors/ISensor.h>
#include <config.h>

class PHSensor : public ISensor {
public:
    explicit PHSensor(uint8_t pin = PIN_PH, uint8_t filterSize = 4);

    bool        begin()         override;
    void        update()        override;
    float       getValue()      const override { return _filtered; }
    const char* getName()       const override { return "ph"; }
    bool        isReady()       const override { return _ready; }

    /**
     * Calibración de dos puntos.
     * @param v4 Voltaje medido en buffer pH 4
     * @param v7 Voltaje medido en buffer pH 7
     */
    void calibrate(float v4, float v7);

    float voltageToPhValue(float v) const;

private:
    uint8_t       _pin;
    uint8_t       _filterSize;
    float*        _buf;
    uint8_t       _bufIdx   = 0;
    float         _filtered = 7.0f;
    bool          _ready    = false;

    // Parámetros calibración: pH = _m * V + _b
    float         _m = -5.70f;   ///< pendiente (pH/V) — ajustar en calibración
    float         _b = 21.34f;   ///< offset — ajustar en calibración

    unsigned long _lastMs = 0;
    static constexpr unsigned long INTERVAL_MS = 100;
};
