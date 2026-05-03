/**
 * TemperatureSensor.h — Sensor de temperatura PT100 vía MAX31865 (SPI).
 * Implementa ISensor.
 */

#pragma once
#include <Sensors/ISensor.h>
#include <Adafruit_MAX31865.h>
#include <config.h>

class TemperatureSensor : public ISensor {
public:
    explicit TemperatureSensor(uint8_t csPin = MAX31865_CS);

    bool        begin()         override;
    void        update()        override;
    float       getValue()      const override { return _tempC; }
    const char* getName()       const override { return "temperature"; }
    bool        isReady()       const override { return _ready; }

    /** Retorna el último código de falla del MAX31865 (0 = sin falla) */
    uint8_t     getFaultCode()  const { return _fault; }

private:
    Adafruit_MAX31865 _rtd;
    float         _tempC   = 0.0f;
    bool          _ready   = false;
    uint8_t       _fault   = 0;
    unsigned long _lastMs  = 0;

    // Resistencias para PT100: Rnom=100Ω, Rref=430Ω (ajustar según hardware)
    static constexpr float R_NOMINAL = 100.0f;
    static constexpr float R_REF     = 430.0f;
    static constexpr unsigned long INTERVAL_MS = 500;
};
