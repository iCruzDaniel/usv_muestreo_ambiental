# 🚤 USV Muestreo Ambiental — Firmware ESP32

Firmware de alta ingeniería para el vehículo de superficie no tripulado (USV) de muestreo ambiental. Construido sobre ESP32 con arquitectura **FSM + FreeRTOS + HAL intercambiable + Inyección de Dependencias**.

---

## Descripción general

El sistema actúa como puente entre un autopiloto (vía MAVLink v2), sensores de calidad del agua (turbidez, pH, temperatura) y un broker MQTT (AWS IoT Core). El firmware soporta dos transportes de red intercambiables en tiempo de compilación — **LTE (A7670SA)** o **WiFi** — sin modificar una sola línea de código fuente.

### Características clave

- **Máquina de Estados Finitos** (BOOT → STANDBY → MISSION_ACTIVE → EMERGENCY)
- **FreeRTOS** con 3 tareas con prioridades y core-pinning en dual-core ESP32
- **HAL de conectividad** (`ITransport`) con implementaciones LTE y WiFi
- **Inyección de dependencias** de sensores vía interfaz `ISensor`
- **Cero hardcoding** — todas las constantes configurables desde `platformio.ini`
- **Reconexión no bloqueante** automática para broker MQTT y transporte
- **Watchdog hardware** (`esp_task_wdt`) configurable
- **Logger con niveles** (INFO, WARN, ERROR) publicados en Serial + MQTT

---

## Hardware requerido

| Componente | Descripción | Notas |
|---|---|---|
| ESP32 DevKit v1 | Placa principal (dual core, 520 KB SRAM) | |
| Módem A7670SA | 4G/LTE con soporte MQTT AT (solo en modo LTE) | Alimentación 5V/2A |
| SIM (solo LTE) | Con plan de datos GPRS/LTE activo | |
| MAX31865 | Interfaz SPI para RTD PT100 | 3-wire, Rref=430Ω |
| Sensor de turbidez | Analógico 0–3.3V | Calibrar con estándar Formazin |
| Sensor de pH | Analógico 0–3.3V | Calibrar con buffers pH 4 y pH 7 |
| Antena LTE | Para A7670SA (solo en modo LTE) | |
| Fuente 5V / 2A | Alimentación estable para ESP32 + módem | |

---

## Pinout ESP32

| GPIO | Función | Componente | Configurable vía |
|------|---------|------------|-------------------|
| 16 | RX1 (UART1) | A7670SA TX | `-D GSM_RX` |
| 17 | TX1 (UART1) | A7670SA RX | `-D GSM_TX` |
| 25 | TX2 (UART2) | Flight Controller MAVLink RX | `-D MAV_TX` |
| 26 | RX2 (UART2) | Flight Controller MAVLink TX | `-D MAV_RX` |
| 34 | ADC (input only) | Sensor de turbidez | `-D PIN_TURBIDITY` |
| 35 | ADC (input only) | Sensor de pH | `-D PIN_PH` |
| 36 | ADC (input only) | Divisor de voltaje batería | `-D PIN_BATTERY` |
| 18 | SCK | MAX31865 (SPI) | — |
| 19 | MISO | MAX31865 (SPI) | — |
| 23 | MOSI | MAX31865 (SPI) | — |
| 5 | CS | MAX31865 (SPI) | `-D MAX31865_CS` |

> **Nota**: Los pines RX/TX se cruzan: TX del ESP32 (17) → RX del módem; RX del ESP32 (16) → TX del módem. Igual para MAVLink.

---

## Arquitectura del sistema

### Diagrama de componentes

```
┌──────────────────────────────────────────────────────────────────────┐
│                          platformio.ini                             │
│  -D USE_LTE / -D USE_WIFI    -D MQTT_BROKER    -D CLIENT_ID  ...   │
└──────────────────────┬───────────────────────────────────────────────┘
                       │ compile-time
┌──────────────────────▼───────────────────────────────────────────────┐
│                         main.cpp (FreeRTOS)                         │
│                                                                     │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────┐              │
│  │ taskComms    │  │ taskMission  │  │ taskTelem      │              │
│  │ Core 0 Prio3│  │ Core 1 Prio2 │  │ Core 1  Prio 1 │              │
│  │ transport.  │  │ mavlink.     │  │ systemFSM.     │              │
│  │ update()    │  │ update()     │  │ update()       │              │
│  └──────┬──────┘  │ mission.     │  │ watchdogs      │              │
│         │         │ update()     │  │ publishStatus()│              │
│         │         └──────┬───────┘  └───────┬────────┘              │
│    ┌────▼────┐     ┌─────▼──────┐     ┌─────▼──────┐               │
│    │ITransport│    │ISensor × 3 │     │ SystemFSM  │               │
│    │(interfaz)│    │(interfaz)  │     │ (FSM)      │               │
│    └────┬────┘     └─────┬──────┘     └─────┬──────┘               │
│    ┌────┴─────┐    ┌─────┼──────┐           │                      │
│    │HAL_LTE   │    │Turb │pH    │     ┌─────▼──────┐               │
│    │HAL_WiFi  │    │idity│Sensor│     │OrderParser │               │
│    └──────────┘    │     │Temp  │     │(MQTT→Event)│               │
│                    └─────┴──────┘     └────────────┘               │
└─────────────────────────────────────────────────────────────────────┘
```

### Tareas FreeRTOS

| Tarea | Core | Prioridad | Stack (words) | Responsabilidad |
|---|---|---|---|---|
| `taskComms` | 0 | 3 (alta) | `STACK_COMMS` (6144) | Pump de red LTE/WiFi + MQTT. Sin lógica de misión. |
| `taskMission` | 1 | 2 (media) | `STACK_MISSION` (4096) | Parser MAVLink + lectura de sensores + muestreo en waypoints |
| `taskTelem` | 1 | 1 (baja) | `STACK_TELEMETRY` (4096) | FSM principal + watchdogs (batería, MAVLink) + publicación `general_status` |

- **Mutex `xTransportMutex`**: protege acceso concurrente a `ITransport` entre `taskComms` y `taskTelem`.
- **Watchdog** (`esp_task_wdt`): timeout configurable con `-D WATCHDOG_TIMEOUT_S`.

---

## Máquina de estados (FSM)

```
                    ┌──────────────────────────────────┐
                    │                                  │
    ┌─────┐    CONNECTIVITY_UP    ┌─────────┐    CMD_START    ┌────────────────┐
    │BOOT │ ────────────────────► │ STANDBY │ ──────────────► │MISSION_ACTIVE  │
    └──┬──┘                       └────┬────┘                 └───────┬────────┘
       │                               │                              │
       │     CRITICAL_FAULT            │  CRITICAL_FAULT              │ MISSION_DONE
       │ ◄─────────────────────────────┤◄─────────────────────────────┤ o CMD_CANCEL
       │                               │                              │
       │                          ┌────▼──────┐                       │
       └─────────────────────────►│ EMERGENCY │◄──────────────────────┘
                                  └─────┬─────┘    CRITICAL_FAULT
                                        │
                                        │ CMD_PREPARE
                                        ▼
                                   ┌─────────┐
                                   │ STANDBY │
                                   └─────────┘
```

### Transiciones

| Desde | Hacia | Evento | Descripción |
|---|---|---|---|
| `BOOT` | `STANDBY` | `CONNECTIVITY_UP` | MQTT conectado |
| `STANDBY` | `MISSION_ACTIVE` | `CMD_START` | Orden START recibida |
| `MISSION_ACTIVE` | `STANDBY` | `MISSION_DONE` | Todos los waypoints visitados |
| `MISSION_ACTIVE` | `STANDBY` | `CMD_CANCEL` | Misión cancelada |
| Cualquiera | `EMERGENCY` | `CRITICAL_FAULT` | MAVLink timeout o batería baja |
| `EMERGENCY` | `STANDBY` | `CMD_PREPARE` | Recuperación manual |

---

## Tópicos MQTT

| Tópico | Dirección | QoS | Frecuencia | Descripción |
|---|---|---|---|---|
| `usv/general_status` | ESP32 → Broker | 0 | Cada `TELEMETRY_INTERVAL_MS` | Estado general del sistema |
| `usv/mision_data` | ESP32 → Broker | **1** | Solo al alcanzar un waypoint | Datos de sensores en punto de muestreo |
| `usv/logs` | ESP32 → Broker | 0 | Bajo demanda | Logs INFO / WARN / ERROR |
| `usv/orders` | Broker → ESP32 | 0 | Bajo demanda | Órdenes del operador/HMI |

### Formato JSON — `general_status`

```json
{
  "ts": 123456,
  "state": "STANDBY",
  "wp_current": 0,
  "battery_adc": 2850,
  "mavlink_ok": true,
  "client_id": "usv-001"
}
```

### Formato JSON — `mision_data`

```json
{
  "wp_idx": 2,
  "lat": 20.678912,
  "lon": -87.073145,
  "ts": 234567,
  "sensors": {
    "turbidity_ntu": 145.3,
    "ph": 7.21,
    "temp_c": 24.8
  }
}
```

### Formato JSON — `logs`

```json
{
  "ts": 345678,
  "lvl": "WARN",
  "tag": "FSM",
  "msg": "Batería crítica (ADC=1420)"
}
```

### Formato JSON — `orders` (Broker → ESP32)

```json
{ "cmd": "PREPARE" }
{ "cmd": "START" }
{ "cmd": "CANCEL" }
{ "cmd": "PAUSE" }
{
  "cmd": "SET_COORDS",
  "payload": {
    "radius_m": 3.0,
    "waypoints": [
      { "lat": 20.6789, "lon": -87.0731, "alt": 0.5 },
      { "lat": 20.6792, "lon": -87.0728, "alt": 0.5 }
    ]
  }
}
```

### Flujo de misión completo

1. **HMI** publica `SET_COORDS` con waypoints → `OrderParser` → `SystemFSM::CMD_SET_COORDS`
2. **HMI** publica `START` → FSM transiciona a `MISSION_ACTIVE`
3. **MAVLink** reporta posición → `MissionController` calcula distancia Haversine al WP actual
4. Al entrar al radio de captura → lee sensores → publica en `mision_data` (QoS 1)
5. Avanza al siguiente WP → repite hasta completar todos → FSM → `STANDBY`

---

## Abstracción de Hardware (HAL)

El código de negocio nunca depende de la implementación concreta de red. La interfaz `ITransport` define:

```cpp
class ITransport {
    virtual void begin() = 0;
    virtual void update() = 0;
    virtual bool isNetworkReady() const = 0;
    virtual bool isMqttConnected() const = 0;
    virtual bool publish(const char* topic, const String& payload, uint8_t qos) = 0;
    virtual bool subscribe(const char* topic) = 0;
    virtual void setMessageCallback(std::function<void(const char*, const String&)> cb) = 0;
};
```

### Selección en tiempo de compilación

```bash
# Compilar con LTE (A7670SA)
pio run -e usv_lte

# Compilar con WiFi
pio run -e usv_wifi
```

| Entorno | Define | Implementación | Dependencias extra |
|---|---|---|---|
| `usv_lte` | `-D USE_LTE` | `HAL_LTE` (TinyGSM + AT-MQTT) | `TinyGSM` |
| `usv_wifi` | `-D USE_WIFI` | `HAL_WiFi` (WiFiClientSecure + PubSubClient) | `PubSubClient` |

> ⚠️ **Nunca** definir `USE_LTE` y `USE_WIFI` simultáneamente. `main.cpp` tiene un `#error` de compilación que lo previene.

---

## Inyección de dependencias — Sensores

Cada sensor implementa la interfaz `ISensor` y se inyecta al `MissionController`:

```cpp
MissionController mission(
    &turbSensor,   // ISensor* (TurbiditySensor)
    &phSensor,     // ISensor* (PHSensor)
    &tempSensor,   // ISensor* (TemperatureSensor)
    &mavlink,      // MavlinkHandler*
    &transport     // ITransport*
);
```

### Calibración de sensores

| Sensor | Método | Parámetros | Procedimiento |
|---|---|---|---|
| **Turbidez** | `setCalibration(slope, offset)` | Pendiente (NTU/V), offset | Medir con agua pura (0 NTU) y estándar Formazin conocido |
| **pH** | `calibrate(v4, v7)` | Voltaje en buffer pH 4 y pH 7 | Sumergir sonda en cada buffer y leer voltaje |
| **Temperatura** | Constantes en `.cpp` | `R_NOMINAL=100Ω`, `R_REF=430Ω` | Ajustar según la PT100 y resistor de referencia instalados |

---

## Estructura de archivos

```
CORE/
├── platformio.ini                      # Configuración — cero hardcoding
├── .gitignore
├── README.md
│
├── src/
│   └── main.cpp                        # Ensamblaje: FreeRTOS + DI + HAL selector
│
├── include/
│   ├── config.h                        # Fallback defaults (valores reales desde -D flags)
│   ├── FSM/
│   │   └── SystemState.h               # Enum SystemState, SystemEvent, MissionConfig
│   ├── HAL/
│   │   └── ITransport.h                # Interfaz pura de transporte
│   ├── Sensors/
│   │   └── ISensor.h                   # Interfaz pura de sensor
│   └── certs/                          # ⚠️ NO commitear — en .gitignore
│       ├── aws_root_ca.h               # Amazon Root CA 1
│       ├── client_cert.h               # Certificado X.509 del dispositivo
│       ├── client_key.h                # Clave privada del dispositivo
│       ├── aws_root_ca.example         # Plantilla de referencia
│       ├── client_cert.example
│       └── client_key.example
│
├── lib/
│   ├── HAL_LTE/                        # ITransport sobre A7670SA (TinyGSM + AT-MQTT)
│   │   ├── HAL_LTE.h
│   │   └── HAL_LTE.cpp
│   ├── HAL_WiFi/                       # ITransport sobre WiFiClientSecure + PubSubClient
│   │   ├── HAL_WiFi.h
│   │   └── HAL_WiFi.cpp
│   ├── Sensors/
│   │   ├── TurbiditySensor/            # ISensor — NTU, filtro media móvil, calibración
│   │   │   ├── TurbiditySensor.h
│   │   │   └── TurbiditySensor.cpp
│   │   ├── PHSensor/                   # ISensor — calibración 2-puntos (pH4/pH7)
│   │   │   ├── PHSensor.h
│   │   │   └── PHSensor.cpp
│   │   └── TemperatureSensor/          # ISensor — MAX31865 PT100, detección de fallas
│   │       ├── TemperatureSensor.h
│   │       └── TemperatureSensor.cpp
│   ├── MavlinkHandler/                 # Handler MAVLink v2 (Heartbeat + Data Streams)
│   │   ├── MavlinkHandler.h
│   │   └── MavlinkHandler.cpp
│   ├── Logger/                         # Niveles INFO/WARN/ERROR → Serial + MQTT
│   │   ├── Logger.h
│   │   └── Logger.cpp
│   ├── MissionController/              # DI sensores, Haversine, muestreo por waypoints
│   │   ├── MissionController.h
│   │   └── MissionController.cpp
│   ├── SystemFSM/                      # FSM BOOT/STANDBY/MISSION_ACTIVE/EMERGENCY
│   │   ├── SystemFSM.h
│   │   └── SystemFSM.cpp
│   └── OrderParser/                    # Parser JSON tópico orders → SystemEvent
│       ├── OrderParser.h
│       └── OrderParser.cpp
│
├── test/                               # Tests unitarios y de hardware (Unity)
│   ├── test_sensor_manager/
│   ├── test_mavlink_communication/
│   ├── test_modem_communication/
│   ├── test_hardware_sensors/
│   ├── test_hardware_mavlink/
│   └── test_hardware_modem/
│
└── docs/
    └── AT_commands.md                  # Referencia de comandos AT del A7670SA
```

---

## Variables de entorno (`platformio.ini` → `build_flags`)

Todas las constantes se inyectan en tiempo de compilación:

### Conectividad

| Flag | Descripción | Valor ejemplo |
|---|---|---|
| `-D MQTT_BROKER` | Endpoint AWS IoT Core | `"abc.iot.us-east-1.amazonaws.com"` |
| `-D MQTT_PORT` | Puerto MQTT | `8883` |
| `-D CLIENT_ID` | ID único del dispositivo | `"usv-001"` |
| `-D DEFAULT_APN` | APN celular (solo LTE) | `"internet"` |
| `-D WIFI_SSID` | SSID WiFi (solo WiFi) | `"mi-red"` |
| `-D WIFI_PASS` | Password WiFi (solo WiFi) | `"clave123"` |

### Pines de hardware

| Flag | Descripción | Default |
|---|---|---|
| `-D PIN_TURBIDITY` | Pin ADC turbidez | `34` |
| `-D PIN_PH` | Pin ADC pH | `35` |
| `-D PIN_BATTERY` | Pin ADC batería | `36` |
| `-D MAX31865_CS` | Pin CS del MAX31865 | `5` |
| `-D GSM_TX / GSM_RX` | UART1 del módem | `17` / `16` |
| `-D MAV_TX / MAV_RX` | UART2 MAVLink | `25` / `26` |

### Intervalos y tamaños

| Flag | Descripción | Default |
|---|---|---|
| `-D TELEMETRY_INTERVAL_MS` | Periodo de `general_status` | `5000` |
| `-D SAMPLING_INTERVAL_MS` | Periodo de muestreo en waypoint | `10000` |
| `-D WATCHDOG_TIMEOUT_S` | Timeout del watchdog hardware | `60` |
| `-D STACK_COMMS` | Stack tarea de comunicaciones | `6144` |
| `-D STACK_MISSION` | Stack tarea de misión | `4096` |
| `-D STACK_TELEMETRY` | Stack tarea de telemetría | `4096` |

### Tópicos MQTT

| Flag | Default |
|---|---|
| `-D TOPIC_STATUS` | `"usv/general_status"` |
| `-D TOPIC_MISSION` | `"usv/mision_data"` |
| `-D TOPIC_LOGS` | `"usv/logs"` |
| `-D TOPIC_ORDERS` | `"usv/orders"` |

---

## Configuración AWS IoT Core

### 1. Crear Thing y certificados

1. AWS Console → **IoT Core** → **Manage** → **Things** → Create
2. Crear certificado X.509 y descargar:
   - `AmazonRootCA1.pem` → convertir a `aws_root_ca.h`
   - `xxx-certificate.pem.crt` → convertir a `client_cert.h`
   - `xxx-private.pem.key` → convertir a `client_key.h`
3. Anotar el **Endpoint** en **Settings** → Device data endpoint

### 2. Formato de headers de certificado

Usar los archivos `.example` como plantilla. Ejemplo para `client_cert.h`:

```cpp
const char CLIENT_CERT[] = R"EOF(
-----BEGIN CERTIFICATE-----
MIICxDCCAaygAwIBAgIU... (contenido completo PEM)
-----END CERTIFICATE-----
)EOF";
```

### 3. Política IoT mínima

```json
{
  "Version": "2012-10-17",
  "Statement": [
    { "Effect": "Allow", "Action": "iot:Connect", "Resource": "*" },
    { "Effect": "Allow", "Action": "iot:Publish", "Resource": "arn:aws:iot:*:*:topic/usv/*" },
    { "Effect": "Allow", "Action": "iot:Subscribe", "Resource": "arn:aws:iot:*:*:topicfilter/usv/*" },
    { "Effect": "Allow", "Action": "iot:Receive", "Resource": "arn:aws:iot:*:*:topic/usv/*" }
  ]
}
```

### 4. Configurar el endpoint

En `platformio.ini`, reemplazar en `build_flags`:

```ini
-DMQTT_BROKER=\"a3k7odshaiipe8-ats.iot.us-east-1.amazonaws.com\"
```

---

## Compilación, carga y monitoreo

```bash
# ── Compilar ──────────────────────────────────────────────
pio run -e usv_lte          # Transporte LTE
pio run -e usv_wifi         # Transporte WiFi

# ── Flashear + monitorear ─────────────────────────────────
pio run -e usv_lte -t upload && pio device monitor -e usv_lte

# ── Tests unitarios ───────────────────────────────────────
pio test -e test_sensors
pio test -e test_mavlink
pio test -e test_modem

# ── Tests de hardware (en ESP32 conectado) ────────────────
pio test -e hw_sensors
pio test -e hw_mavlink
pio test -e hw_modem
```

---

## Monitoreo Serial

Monitor a **115200 baud**. Log con colores ANSI y prefijos:

| Prefijo | Módulo |
|---|---|
| `[LTE]` / `[WiFi]` | Transporte de red |
| `[MAV]` | Parser MAVLink |
| `[TURB]` `[PH]` `[TEMP]` | Sensores individuales |
| `[FSM]` | Máquina de estados del sistema |
| `[MISSION]` | Controlador de misión |
| `[ORDERS]` | Parser de órdenes MQTT |
| `[COMMS]` `[TELEM]` | Tareas FreeRTOS |

---

## Resolución de problemas

| Síntoma | Causa probable | Solución |
|---|---|---|
| `#error "Define USE_LTE o USE_WIFI"` | Falta flag de transporte | Usar entorno `usv_lte` o `usv_wifi` |
| `[LTE] ERROR: Módem no responde` | UART o alimentación | Verificar pines 16/17, fuente 5V/2A |
| `[LTE] Certificado placeholder` | Certs no configurados | Reemplazar contenido en `include/certs/` |
| `[LTE] MQTT connect error` | Endpoint o certs erróneos | Verificar `-D MQTT_BROKER` y policy IoT |
| `[WiFi] Conectando a ...` (loop) | SSID/contraseña | Verificar `-D WIFI_SSID` y `-D WIFI_PASS` |
| `[MAV]` sin datos | FC no envía MAVLink | Verificar UART2 (pines 2/4), baudrate 57600 |
| `[TEMP] Falla MAX31865: 0x...` | Cableado SPI | Verificar CS=5, MISO/MOSI/SCK, 3-wire |
| `[FSM] EMERGENCY: Batería crítica` | Voltaje bajo | Verificar umbral `BATTERY_LOW_ADC` y divisor |
| Stack overflow / WDT reset | Stack insuficiente | Incrementar `-D STACK_*` en `platformio.ini` |

---

## Dependencias

| Librería | Versión | Uso |
|---|---|---|
| [TinyGSM](https://github.com/vshymanskyy/TinyGSM) | ^0.12.0 | Comunicación AT con A7670SA (solo LTE) |
| [MAVLink](https://github.com/okalachev/MAVLink) | latest | Headers MAVLink v2 auto-generados |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | ^6 | Serialización/deserialización JSON |
| [Adafruit MAX31865](https://github.com/adafruit/Adafruit_MAX31865) | ^1.1.0 | Driver SPI para PT100 |
| [PubSubClient](https://github.com/knolleary/pubsubclient) | ^2.8 | Cliente MQTT (solo WiFi) |

---

## Notas de rendimiento

- **RAM estimada**: ~45 KB (buffers JSON + MAVLink + stacks FreeRTOS)
- **Ciclo típico de tarea**: `taskComms` 20 ms, `taskMission` 50 ms, `taskTelem` 500 ms
- **Consumo de corriente**: ~150 mA (ESP32) + ~800 mA promedio / 2A pico (A7670SA en TX)

---

**Documentación técnica adicional**: ver [`docs/AT_commands.md`](docs/AT_commands.md) para referencia completa de comandos AT del módem.
