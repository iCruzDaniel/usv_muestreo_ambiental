/**
 * ISensor.h — Interfaz base para todos los sensores ambientales.
 *
 * Permite Inyección de Dependencias en MissionController.
 * Cada sensor concreto (Turbidez, pH, Temperatura) implementa esta interfaz.
 */

#pragma once
#include <Arduino.h>

class ISensor {
public:
    virtual ~ISensor() = default;

    /** Inicializa el hardware del sensor. */
    virtual bool begin() = 0;

    /**
     * Lee y procesa el sensor. Debe llamarse periódicamente.
     * Implementaciones internas deben ser no bloqueantes (usar millis()).
     */
    virtual void update() = 0;

    /** Retorna el último valor procesado (unidad depende del sensor). */
    virtual float getValue() const = 0;

    /** Nombre del sensor para logging ("turbidity", "ph", "temperature"). */
    virtual const char* getName() const = 0;

    /** true si la última lectura es válida */
    virtual bool isReady() const = 0;
};
