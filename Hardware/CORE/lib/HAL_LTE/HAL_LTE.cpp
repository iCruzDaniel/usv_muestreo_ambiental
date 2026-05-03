/**
 * HAL_LTE.cpp — Implementación completa del transporte LTE.
 * Toda la lógica AT/MQTT es asíncrona (millis-based, sin delay()).
 */

#ifdef USE_LTE

#include "HAL_LTE.h"
#include <certs/aws_root_ca.h>
#include <certs/client_cert.h>
#include <certs/client_key.h>

// ── Constructor ───────────────────────────────────────────────────────────────
HAL_LTE::HAL_LTE(HardwareSerial& serial, TinyGsm& modem,
                 const char* rootCa, const char* clientCert, const char* clientKey)
    : _serial(serial), _modem(modem),
      _rootCa(rootCa), _clientCert(clientCert), _clientKey(clientKey) {}

// ── begin() ───────────────────────────────────────────────────────────────────
void HAL_LTE::begin() {
    _serial.begin(GSM_BAUD, SERIAL_8N1, GSM_RX, GSM_TX);
    delay(100);
    Serial.println("[LTE] Iniciando módem...");
    if (!_modem.init()) {
        Serial.println("[LTE] ERROR: Módem no responde");
        _transitionTo(LteState::ERROR);
        return;
    }
    Serial.println("[LTE] Módem OK");
    _transitionTo(LteState::WAIT_CARD);
}

// ── update() ─────────────────────────────────────────────────────────────────
void HAL_LTE::update() {
    // Acumular bytes del UART
    while (_serial.available()) {
        char c = _serial.read();
        _rxBuf += c;
        if (_rxBuf.length() > 2048) _rxBuf = _rxBuf.substring(_rxBuf.length() - 1024);
    }

    unsigned long now = millis();

    // Procesar mensajes MQTT entrantes (URC: +MQTTREC)
    _processIncoming();

    // Timeout global de comando AT
    if (_pendingCmd && (now - _cmdSentMs > CMD_TIMEOUT)) {
        Serial.println("[LTE] Timeout de comando AT");
        _pendingCmd = false;
        _rxBuf.clear();
        _transitionTo(LteState::ERROR);
        return;
    }

    switch (_state) {

    // ── INIT ──────────────────────────────────────────────────────────────
    case LteState::INIT:
        _transitionTo(LteState::WAIT_CARD);
        break;

    // ── WAIT_CARD ─────────────────────────────────────────────────────────
    case LteState::WAIT_CARD:
        if (!_pendingCmd) {
            _sendAt("AT+CPIN?", "+CPIN: READY");
        } else if (_checkResponse("+CPIN: READY")) {
            Serial.println("[LTE] SIM lista");
            _certIdx = 0; _certPhase = 0;
            _transitionTo(LteState::WRITE_CERTS);
        }
        break;

    // ── WRITE_CERTS ───────────────────────────────────────────────────────
    case LteState::WRITE_CERTS: {
        static const char* paths[] = {"/cert/ca.pem", "/cert/client.crt", "/cert/client.key"};
        static const char* datas[] = {_rootCa, _clientCert, _clientKey};

        // Validar que los certs no son placeholders
        if (!_pendingCmd && _certPhase == 0) {
            if (strstr(datas[_certIdx], "[PEGAR") != nullptr) {
                Serial.println("[LTE] ERROR: Certificado placeholder. Reemplazar.");
                _transitionTo(LteState::ERROR);
                break;
            }
            String cmd = "AT+CFSWRITE=\"" + String(paths[_certIdx]) + "\",0," +
                         String(strlen(datas[_certIdx]));
            _sendAt(cmd, ">");
            _certPhase = 1;
        } else if (_certPhase == 1 && _checkResponse(">")) {
            _rxBuf.clear();
            _serial.write((const uint8_t*)datas[_certIdx], strlen(datas[_certIdx]));
            _serial.write(0x1A); // Ctrl+Z
            _expectPat  = "+CFSWRITE:";
            _cmdSentMs  = millis();
            _certPhase  = 2;
        } else if (_certPhase == 2 && _checkResponse("+CFSWRITE:")) {
            _pendingCmd = false;
            _rxBuf.clear();
            _certIdx++;
            _certPhase = 0;
            if (_certIdx >= 3) {
                Serial.println("[LTE] Certificados escritos OK");
                _gprsPhase = 0;
                _transitionTo(LteState::GPRS_ATTACH);
            }
        }
        break;
    }

    // ── GPRS_ATTACH ───────────────────────────────────────────────────────
    case LteState::GPRS_ATTACH:
        if (!_pendingCmd) {
            _sendAt("AT+CGATT?", "+CGATT: 1");
        } else if (_checkResponse("+CGATT: 1")) {
            Serial.println("[LTE] GPRS adjunto");
            _gprsPhase = 0;
            _transitionTo(LteState::GPRS_ACTIVATE);
        }
        break;

    // ── GPRS_ACTIVATE ─────────────────────────────────────────────────────
    case LteState::GPRS_ACTIVATE:
        if (_prevState != LteState::GPRS_ACTIVATE) _gprsPhase = 0;
        if (!_pendingCmd) {
            switch (_gprsPhase) {
                case 0: _sendAt("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", "OK"); break;
                case 1: _sendAt("AT+SAPBR=3,1,\"APN\",\"" DEFAULT_APN "\"", "OK"); break;
                case 2: _sendAt("AT+SAPBR=1,1", "OK"); break;
                case 3: _sendAt("AT+SAPBR=2,1", "+SAPBR:"); break;
            }
            _gprsPhase++;
        } else {
            String pat = (_gprsPhase >= 4) ? "+SAPBR:" : "OK";
            if (_checkResponse(pat)) {
                if (_gprsPhase >= 4) {
                    Serial.printf("[LTE] IP: %s\n", _modem.getLocalIP().c_str());
                    _failCount = 0;
                    _transitionTo(LteState::MQTT_CONF);
                }
            }
        }
        break;

    // ── MQTT_CONF ─────────────────────────────────────────────────────────
    case LteState::MQTT_CONF: {
        if (!_pendingCmd) {
            String cid = String(CLIENT_ID) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
            _sendAt("AT+MQTTCONF=\"" + cid + "\",120,0,1", "OK");
        } else if (_checkResponse("OK")) {
            _transitionTo(LteState::MQTT_START);
        }
        break;
    }

    // ── MQTT_START ────────────────────────────────────────────────────────
    case LteState::MQTT_START:
        if (!_pendingCmd) {
            _sendAt("AT+MQTTSTART", "OK");
        } else if (_checkResponse("OK")) {
            _transitionTo(LteState::MQTT_CONNECT);
        }
        break;

    // ── MQTT_CONNECT ──────────────────────────────────────────────────────
    case LteState::MQTT_CONNECT:
        if (!_pendingCmd) {
            String cmd = "AT+MQTTCONN=\"" + String(MQTT_BROKER) + "\"," + String(MQTT_PORT);
            _sendAt(cmd, "+MQTTCONN:0");
        } else if (_checkResponse("+MQTTCONN:0") || _checkResponse("+MQTTCONN: 0")) {
            Serial.println("[LTE] MQTT conectado");
            _failCount = 0;
            _pendingCmd = false;
            _rxBuf.clear();
            // Suscribirse al tópico de órdenes
            subscribe(TOPIC_ORDERS);
            _transitionTo(LteState::READY);
        } else if (_checkResponse("ERROR")) {
            _pendingCmd = false;
            _rxBuf.clear();
            _failCount++;
            _transitionTo(LteState::ERROR);
        }
        break;

    // ── READY ─────────────────────────────────────────────────────────────
    case LteState::READY:
        // Detectar desconexión inesperada (URC del módem)
        if (_rxBuf.indexOf("+MQTTDISCONN") != -1) {
            Serial.println("[LTE] Broker desconectado inesperadamente");
            _rxBuf.clear();
            _failCount++;
            _transitionTo(LteState::MQTT_CONNECT);
        }
        break;

    // ── ERROR ─────────────────────────────────────────────────────────────
    case LteState::ERROR:
    default:
        if (now - _lastRetryMs > RETRY_MS) {
            _lastRetryMs = now;
            _pendingCmd = false;
            _rxBuf.clear();
            _certIdx = 0; _certPhase = 0; _gprsPhase = 0;
            if (_failCount >= MAX_FAILS) {
                _resetModem();
            } else {
                _transitionTo(LteState::INIT);
            }
        }
        break;
    }
}

// ── isNetworkReady / isMqttConnected ─────────────────────────────────────────
bool HAL_LTE::isNetworkReady() const {
    return _state == LteState::READY || _state == LteState::MQTT_CONNECT;
}
bool HAL_LTE::isMqttConnected() const {
    return _state == LteState::READY;
}

// ── publish() ─────────────────────────────────────────────────────────────────
bool HAL_LTE::publish(const char* topic, const String& payload, uint8_t qos) {
    if (_state != LteState::READY) return false;
    // Escapar comillas del JSON para el comando AT
    String escaped = payload;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    String cmd = "AT+MQTTPUB=\"" + String(topic) + "\",\"" + escaped + "\"," + String(qos);
    _serial.println(cmd);
    return true;
}

// ── subscribe() ───────────────────────────────────────────────────────────────
bool HAL_LTE::subscribe(const char* topic) {
    String cmd = "AT+MQTTSUB=\"" + String(topic) + "\",0";
    _serial.println(cmd);
    return true;
}

// ── setMessageCallback() ─────────────────────────────────────────────────────
void HAL_LTE::setMessageCallback(std::function<void(const char*, const String&)> cb) {
    _onMessage = cb;
}

// ── _processIncoming() — Parsea URC +MQTTREC ─────────────────────────────────
void HAL_LTE::_processIncoming() {
    // URC del A7670SA: +MQTTREC: <topic_len>,<topic>,<payload_len>,<payload>
    int idx = _rxBuf.indexOf("+MQTTREC:");
    if (idx == -1 || !_onMessage) return;

    // Parseo básico: extraer tópico y payload
    int comma1 = _rxBuf.indexOf(',', idx);
    if (comma1 == -1) return;
    int topicLen = _rxBuf.substring(idx + 9, comma1).toInt();
    String topicStr = _rxBuf.substring(comma1 + 1, comma1 + 1 + topicLen);

    int comma2 = _rxBuf.indexOf(',', comma1 + 1 + topicLen);
    if (comma2 == -1) return;
    int payloadLen = _rxBuf.substring(comma2 + 1, _rxBuf.indexOf(',', comma2 + 1)).toInt();
    int payloadStart = _rxBuf.indexOf(',', comma2 + 1) + 1;
    String payloadStr = _rxBuf.substring(payloadStart, payloadStart + payloadLen);

    // Limpiar el URC procesado del buffer
    _rxBuf = _rxBuf.substring(payloadStart + payloadLen);

    _onMessage(topicStr.c_str(), payloadStr);
}

// ── _sendAt() ────────────────────────────────────────────────────────────────
void HAL_LTE::_sendAt(const String& cmd, const String& pattern) {
    _serial.println(cmd);
    _expectPat  = pattern;
    _pendingCmd = true;
    _cmdSentMs  = millis();
}

// ── _checkResponse() ─────────────────────────────────────────────────────────
bool HAL_LTE::_checkResponse(const String& pattern) {
    if (_rxBuf.indexOf(pattern) != -1) {
        _pendingCmd = false;
        _rxBuf.clear();
        return true;
    }
    return false;
}

// ── _transitionTo() ──────────────────────────────────────────────────────────
void HAL_LTE::_transitionTo(LteState next) {
    static const char* names[] = {
        "INIT","WAIT_CARD","WRITE_CERTS","GPRS_ATTACH","GPRS_ACTIVATE",
        "MQTT_CONF","MQTT_START","MQTT_CONNECT","READY","ERROR"
    };
    _prevState = _state;
    _state     = next;
    _stateMs   = millis();
    Serial.printf("[LTE] %s → %s\n", names[(int)_prevState], names[(int)_state]);
}

// ── _resetModem() ────────────────────────────────────────────────────────────
void HAL_LTE::_resetModem() {
    Serial.println("[LTE] Reset de módem...");
    _modem.restart();
    _failCount  = 0;
    _pendingCmd = false;
    _rxBuf.clear();
    _transitionTo(LteState::INIT);
}

#endif // USE_LTE
