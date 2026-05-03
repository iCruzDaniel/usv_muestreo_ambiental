/**
 * TurbiditySensor.h — Sensor de turbidez analógico (NTU).
 * Implementa ISensor. Filtro de media móvil de N muestras.
 */

#pragma once
#include <Sensors/ISensor.h>
#include <config.h>

class TurbiditySensor : public ISensor {
public:
    explicit TurbiditySensor(uint8_t pin = PIN_TURBIDITY, uint8_t filterSize = 4);

    bool        begin()         override;
    void        update()        override;
    float       getValue()      const override { return _filtered; }
    const char* getName()       const override { return "turbidity"; }
    bool        isReady()       const override { return _ready; }

    /** Calibración en campo: establece la pendiente lineal voltaje→NTU */
    void setCalibration(float slope, float offset) {
        _slope = slope; _offset = offset;
    }

    // Acceso directo para tests
    float voltageToNTU(float v) const;

private:
    uint8_t       _pin;
    uint8_t       _filterSize;
    float*        _buf;
    uint8_t       _bufIdx   = 0;
    float         _filtered = 0.0f;
    bool          _ready    = false;
    float         _slope    = 1000.0f;  ///< NTU/V (calibrar)
    float         _offset   = 0.0f;
    unsigned long _lastMs   = 0;
    static constexpr unsigned long INTERVAL_MS = 100;
};
