/**
 * ITransport.h — Interfaz de Abstracción de Hardware (HAL) para conectividad.
 *
 * Ambas implementaciones (HAL_LTE y HAL_WiFi) heredan de esta interfaz.
 * El código de negocio NUNCA depende de la implementación concreta.
 *
 * Selección en tiempo de compilación:
 *   -D USE_LTE   →  HAL_LTE
 *   -D USE_WIFI  →  HAL_WiFi
 */

#pragma once
#include <Arduino.h>

class ITransport {
public:
    virtual ~ITransport() = default;

    // ── Ciclo de vida ────────────────────────────────────────────────────────
    /** Inicializa el transporte (UART, WiFi stack, etc.). No bloqueante. */
    virtual void begin() = 0;

    /** Debe llamarse en la tarea de comunicaciones. Pump de estado interno. */
    virtual void update() = 0;

    // ── Estado ───────────────────────────────────────────────────────────────
    /** true cuando el transporte está listo para enviar/recibir datos */
    virtual bool isNetworkReady() const = 0;

    /** true cuando el broker MQTT está conectado */
    virtual bool isMqttConnected() const = 0;

    // ── MQTT ─────────────────────────────────────────────────────────────────
    /**
     * Publica un payload en un tópico dado.
     * @param topic  Tópico MQTT (null-terminated string)
     * @param payload JSON como String
     * @param qos    Calidad de servicio (0 o 1)
     * @return true si el envío fue aceptado por la cola interna
     */
    virtual bool publish(const char* topic, const String& payload, uint8_t qos = 0) = 0;

    /**
     * Suscribirse a un tópico.
     * @param topic Tópico a suscribirse
     */
    virtual bool subscribe(const char* topic) = 0;

    /**
     * Registrar callback para mensajes entrantes.
     * Signature: void(const char* topic, const String& payload)
     */
    virtual void setMessageCallback(
        std::function<void(const char* topic, const String& payload)> cb
    ) = 0;
};
