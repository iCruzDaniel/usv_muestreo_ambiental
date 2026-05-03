/**
 * HAL_WiFi.h — Implementación de ITransport sobre WiFi + PubSubClient (TLS).
 *
 * Reconexión automática no bloqueante: si el broker cae, reintenta cada
 * RETRY_MS sin bloquear la tarea de comunicaciones.
 */

#pragma once

#ifdef USE_WIFI

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <functional>
#include <HAL/ITransport.h>
#include <config.h>



enum class WifiTransportState : uint8_t {
    IDLE, CONNECTING_WIFI, CONNECTING_MQTT, READY, ERROR
};

class HAL_WiFi : public ITransport {
public:
    HAL_WiFi(const char* ssid, const char* pass,
             const char* rootCa, const char* clientCert, const char* clientKey);

    // ITransport
    void begin()    override;
    void update()   override;
    bool isNetworkReady()  const override;
    bool isMqttConnected() const override;
    bool publish(const char* topic, const String& payload, uint8_t qos = 0) override;
    bool subscribe(const char* topic) override;
    void setMessageCallback(std::function<void(const char*, const String&)> cb) override;

private:
    const char* _ssid;
    const char* _pass;
    const char* _rootCa;
    const char* _clientCert;
    const char* _clientKey;

    WiFiClientSecure _wifiClient;
    PubSubClient     _mqtt;

    WifiTransportState _state    = WifiTransportState::IDLE;
    unsigned long      _lastRetryMs = 0;

    std::function<void(const char*, const String&)> _onMessage;

    static constexpr unsigned long RETRY_MS = 5000;
    static constexpr uint16_t     WIFI_MQTT_KEEPALIVE = 120;

    void _connectWifi();
    void _connectMqtt();

    // Wrapper estático para PubSubClient callback
    static HAL_WiFi* _instance;
    static void _mqttCallbackStatic(char* topic, byte* payload, unsigned int len);
};

#endif // USE_WIFI
