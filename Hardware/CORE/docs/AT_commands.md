# Referencia de Comandos AT — Módem A7670SA (Solo para entorno `usv_lte`)

> [!NOTE]
> Esta referencia documenta los comandos AT que `HAL_LTE` envía internamente.
> No es necesario ejecutarlos manualmente; están aquí como documentación técnica.

## 1. Inicialización y verificación de SIM

| Comando | Respuesta esperada | Descripción |
|---|---|---|
| `AT` | `OK` | Test de comunicación UART |
| `AT+CPIN?` | `+CPIN: READY` | SIM insertada y desbloqueada |
| `AT+CREG?` | `+CREG: 0,1` | Registrado en red (0,5 = roaming) |
| `AT+CGATT?` | `+CGATT: 1` | GPRS adjunto |

## 2. Sistema de archivos — Escritura de certificados TLS

```
AT+CFSWRITE="/cert/ca.pem",0,<len>     → '>' (prompt) → enviar PEM + Ctrl+Z → +CFSWRITE: <len>
AT+CFSWRITE="/cert/client.crt",0,<len>  → idem
AT+CFSWRITE="/cert/client.key",0,<len>  → idem
AT+FS LS=/cert/                          → Lista archivos en /cert/
AT+FSREAD="/cert/ca.pem",0,<len>         → Lectura para verificación
```

## 3. Configuración GPRS

```
AT+SAPBR=3,1,"CONTYPE","GPRS"   → OK
AT+SAPBR=3,1,"APN","<apn>"      → OK       (APN configurable vía -D DEFAULT_APN)
AT+SAPBR=1,1                    → OK       (Abrir bearer)
AT+SAPBR=2,1                    → +SAPBR: 1,1,"<ip>"
AT+SAPBR=0,1                    → OK       (Cerrar bearer)
```

## 4. Cliente MQTT (TLS sobre puerto 8883)

| Comando | Respuesta | Descripción |
|---|---|---|
| `AT+MQTTCONF="<clientId>",120,0,1` | `OK` | Keepalive 120s, QoS 0, clean=1 |
| `AT+MQTTSTART` | `OK` | Iniciar stack MQTT |
| `AT+MQTTCONN="<endpoint>",8883` | `+MQTTCONN:0` | Conectar al broker (0=éxito) |
| `AT+MQTTPUB="<topic>","<payload>",<qos>` | `+MQTTPUB:0` | Publicar mensaje |
| `AT+MQTTSUB="<topic>",0` | `OK` | Suscribirse a tópico |
| `AT+MQTTDISCONN` | `OK` | Desconectar |
| `AT+MQTTSTOP` | `OK` | Detener cliente |

## 5. URC (Unsolicited Result Codes) relevantes

| URC | Significado |
|---|---|
| `+MQTTREC: <topic_len>,<topic>,<payload_len>,<payload>` | Mensaje MQTT recibido |
| `+MQTTDISCONN` | Desconexión inesperada del broker |
| `+CGEV: NW PDN DEACT` | GPRS desactivado por la red |

## 6. Secuencia completa implementada por HAL_LTE

```
1. AT+CPIN?                                  → WAIT_CARD
2. AT+CFSWRITE="/cert/ca.pem",0,<len>       → WRITE_CERTS (×3)
3. AT+CGATT?                                 → GPRS_ATTACH
4. AT+SAPBR=3,1,"CONTYPE","GPRS"            → GPRS_ACTIVATE
5. AT+SAPBR=3,1,"APN","internet"
6. AT+SAPBR=1,1
7. AT+SAPBR=2,1
8. AT+MQTTCONF="usv-001-abc123",120,0,1     → MQTT_CONF
9. AT+MQTTSTART                              → MQTT_START
10. AT+MQTTCONN="endpoint.iot...",8883       → MQTT_CONNECT
11. AT+MQTTSUB="usv/orders",0               → READY
```

## 7. Notas

- Los certificados deben estar en formato PEM (texto Base64, incluyendo `-----BEGIN...` y `-----END...`).
- El `clientId` se genera automáticamente: `CLIENT_ID` + `-` + MAC de ESP32.
- Para reiniciar el módem por software: `AT+CFUN=1,1` o reset por pin PWRKEY.
- Consultar el manual AT de SIMCOM A7670 para comandos adicionales no listados aquí.
