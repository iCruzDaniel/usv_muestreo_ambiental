/**
 * HAL_WiFi.cpp — Implementación de ITransport sobre WiFi + PubSubClient (TLS/mTLS).
 * Reconexión no bloqueante con millis().
 */

#ifdef USE_WIFI

#include "HAL_WiFi.h"
#include <certs/aws_root_ca.h>
#include <certs/client_cert.h>
#include <certs/client_key.h>

// Singleton para el callback estático de PubSubClient
HAL_WiFi* HAL_WiFi::_instance = nullptr;

// ── Constructor ───────────────────────────────────────────────────────────────
HAL_WiFi::HAL_WiFi(const char* ssid, const char* pass,
                   const char* rootCa, const char* clientCert, const char* clientKey)
    : _ssid(ssid), _pass(pass),
      _rootCa(rootCa), _clientCert(clientCert), _clientKey(clientKey),
      _mqtt(_wifiClient)
{
    _instance = this;
}

// ── begin() ───────────────────────────────────────────────────────────────────
void HAL_WiFi::begin() {
    // Configurar TLS (mTLS con certificados cliente)
    _wifiClient.setCACert(_rootCa);
    _wifiClient.setCertificate(_clientCert);
    _wifiClient.setPrivateKey(_clientKey);

    _mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    _mqtt.setKeepAlive(WIFI_MQTT_KEEPALIVE);
    _mqtt.setCallback(_mqttCallbackStatic);
    _mqtt.setBufferSize(2048);

    Serial.println("[WiFi] HAL inicializada");
    _state = WifiTransportState::CONNECTING_WIFI;
    _connectWifi();
}

// ── update() ─────────────────────────────────────────────────────────────────
void HAL_WiFi::update() {
    unsigned long now = millis();

    switch (_state) {

    case WifiTransportState::IDLE:
        _state = WifiTransportState::CONNECTING_WIFI;
        break;

    case WifiTransportState::CONNECTING_WIFI:
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WiFi] Conectado — IP: %s\n", WiFi.localIP().toString().c_str());
            _state = WifiTransportState::CONNECTING_MQTT;
            _connectMqtt();
        } else if (now - _lastRetryMs > RETRY_MS) {
            _lastRetryMs = now;
            _connectWifi();
        }
        break;

    case WifiTransportState::CONNECTING_MQTT:
        if (WiFi.status() != WL_CONNECTED) {
            _state = WifiTransportState::CONNECTING_WIFI;
            break;
        }
        if (_mqtt.connected()) {
            _state = WifiTransportState::READY;
            Serial.println("[WiFi] MQTT conectado");
            // Suscribir al tópico de órdenes
            _mqtt.subscribe(TOPIC_ORDERS);
        } else if (now - _lastRetryMs > RETRY_MS) {
            _lastRetryMs = now;
            _connectMqtt();
        }
        break;

    case WifiTransportState::READY:
        // Loop del cliente MQTT (ACKs, keepalive, incoming)
        if (!_mqtt.loop()) {
            Serial.println("[WiFi] Broker desconectado");
            _state = WifiTransportState::CONNECTING_MQTT;
        }
        break;

    case WifiTransportState::ERROR:
        if (now - _lastRetryMs > RETRY_MS) {
            _lastRetryMs = now;
            _state = WifiTransportState::CONNECTING_WIFI;
            WiFi.reconnect();
        }
        break;
    }
}

// ── isNetworkReady / isMqttConnected ─────────────────────────────────────────
bool HAL_WiFi::isNetworkReady() const {
    return WiFi.status() == WL_CONNECTED;
}
bool HAL_WiFi::isMqttConnected() const {
    return _state == WifiTransportState::READY && const_cast<PubSubClient&>(_mqtt).connected();
}

// ── publish() ─────────────────────────────────────────────────────────────────
bool HAL_WiFi::publish(const char* topic, const String& payload, uint8_t qos) {
    if (!isMqttConnected()) return false;
    return _mqtt.publish(topic, payload.c_str(), false);
}

// ── subscribe() ───────────────────────────────────────────────────────────────
bool HAL_WiFi::subscribe(const char* topic) {
    if (!isMqttConnected()) return false;
    return _mqtt.subscribe(topic);
}

// ── setMessageCallback() ─────────────────────────────────────────────────────
void HAL_WiFi::setMessageCallback(std::function<void(const char*, const String&)> cb) {
    _onMessage = cb;
}

// ── _connectWifi() ───────────────────────────────────────────────────────────
void HAL_WiFi::_connectWifi() {
    Serial.printf("[WiFi] Conectando a %s...\n", _ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid, _pass);
    _lastRetryMs = millis();
}

// ── _connectMqtt() ───────────────────────────────────────────────────────────
void HAL_WiFi::_connectMqtt() {
    String cid = String(CLIENT_ID) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.printf("[WiFi] Conectando MQTT como %s...\n", cid.c_str());
    _mqtt.connect(cid.c_str());
    _lastRetryMs = millis();
}

// ── _mqttCallbackStatic() ────────────────────────────────────────────────────
void HAL_WiFi::_mqttCallbackStatic(char* topic, byte* payload, unsigned int len) {
    if (!_instance || !_instance->_onMessage) return;
    String payloadStr;
    payloadStr.reserve(len);
    for (unsigned int i = 0; i < len; i++) payloadStr += (char)payload[i];
    _instance->_onMessage(topic, payloadStr);
}

#endif // USE_WIFI
