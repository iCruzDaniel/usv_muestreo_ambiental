/**
 * HAL_LTE.h — Implementación de ITransport sobre módem A7670SA (TinyGSM + AT-MQTT).
 *
 * FSM interna de conexión:
 *   INIT → WAIT_CARD → WRITE_CERTS → GPRS_ATTACH → GPRS_ACTIVATE
 *        → MQTT_CONF → MQTT_START → MQTT_CONNECT → READY
 *
 * La reconexión es automática y no bloqueante.
 */

#pragma once

#ifdef USE_LTE

#include <Arduino.h>
#include <TinyGsmClient.h>
#include <functional>
#include <HAL/ITransport.h>
#include <config.h>

enum class LteState : uint8_t {
    INIT, WAIT_CARD, WRITE_CERTS, GPRS_ATTACH, GPRS_ACTIVATE,
    MQTT_CONF, MQTT_START, MQTT_CONNECT, READY, ERROR
};

class HAL_LTE : public ITransport {
public:
    HAL_LTE(HardwareSerial& serial, TinyGsm& modem,
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
    HardwareSerial& _serial;
    TinyGsm&        _modem;

    const char* _rootCa;
    const char* _clientCert;
    const char* _clientKey;

    LteState _state    = LteState::INIT;
    LteState _prevState= LteState::INIT;

    // AT command state machine
    bool         _pendingCmd  = false;
    String       _expectPat;
    String       _rxBuf;
    unsigned long _cmdSentMs  = 0;
    unsigned long _stateMs    = 0;
    unsigned long _lastRetryMs= 0;
    uint8_t      _failCount   = 0;

    // Sub-states
    uint8_t _certIdx   = 0;
    uint8_t _certPhase = 0;
    uint8_t _gprsPhase = 0;

    // Callback para mensajes MQTT entrantes
    std::function<void(const char*, const String&)> _onMessage;

    // Constantes
    static constexpr unsigned long RETRY_MS  = 5000;
    static constexpr unsigned long CMD_TIMEOUT = 30000;
    static constexpr uint8_t      MAX_FAILS   = 3;

    void _transitionTo(LteState next);
    void _resetModem();
    void _sendAt(const String& cmd, const String& expectPattern);
    bool _checkResponse(const String& pattern);
    void _processIncoming();
};

#endif // USE_LTE
