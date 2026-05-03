/**
 * config.h — Valores de respaldo (fallback).
 * Los valores reales se inyectan desde platformio.ini con -D build_flags.
 * NO hardcodear credenciales ni endpoints aquí.
 */

#pragma once

// ── Broker MQTT ──────────────────────────────────────────────────────────────
#ifndef MQTT_BROKER
  #define MQTT_BROKER "your-endpoint.iot.region.amazonaws.com"
#endif
#ifndef MQTT_PORT
  #define MQTT_PORT 8883
#endif
#ifndef CLIENT_ID
  #define CLIENT_ID "usv-001"
#endif

// ── APN celular ──────────────────────────────────────────────────────────────
#ifndef DEFAULT_APN
  #define DEFAULT_APN "internet"
#endif

// ── WiFi (solo en entorno USE_WIFI) ──────────────────────────────────────────
#ifndef WIFI_SSID
  #define WIFI_SSID "your-ssid"
#endif
#ifndef WIFI_PASS
  #define WIFI_PASS "your-password"
#endif

// ── Pines de hardware ─────────────────────────────────────────────────────────
#ifndef PIN_TURBIDITY
  #define PIN_TURBIDITY 34
#endif
#ifndef PIN_PH
  #define PIN_PH 35
#endif
#ifndef MAX31865_CS
  #define MAX31865_CS 5
#endif
#ifndef PIN_BATTERY
  #define PIN_BATTERY 36
#endif
#ifndef GSM_TX
  #define GSM_TX 17
#endif
#ifndef GSM_RX
  #define GSM_RX 16
#endif
#ifndef GSM_BAUD
  #define GSM_BAUD 115200
#endif
#ifndef MAV_TX
  #define MAV_TX 2
#endif
#ifndef MAV_RX
  #define MAV_RX 4
#endif
#ifndef MAV_BAUD
  #define MAV_BAUD 57600
#endif

// ── Intervalos de misión ──────────────────────────────────────────────────────
#ifndef SAMPLING_INTERVAL_MS
  #define SAMPLING_INTERVAL_MS 10000
#endif
#ifndef TELEMETRY_INTERVAL_MS
  #define TELEMETRY_INTERVAL_MS 5000
#endif
#ifndef WATCHDOG_TIMEOUT_S
  #define WATCHDOG_TIMEOUT_S 60
#endif

// ── Stacks FreeRTOS (palabras de 4 bytes) ─────────────────────────────────────
#ifndef STACK_TELEMETRY
  #define STACK_TELEMETRY 4096
#endif
#ifndef STACK_MISSION
  #define STACK_MISSION 4096
#endif
#ifndef STACK_COMMS
  #define STACK_COMMS 6144
#endif

// ── Tópicos MQTT ──────────────────────────────────────────────────────────────
#ifndef TOPIC_STATUS
  #define TOPIC_STATUS "usv/general_status"
#endif
#ifndef TOPIC_MISSION
  #define TOPIC_MISSION "usv/mision_data"
#endif
#ifndef TOPIC_LOGS
  #define TOPIC_LOGS "usv/logs"
#endif
#ifndef TOPIC_ORDERS
  #define TOPIC_ORDERS "usv/orders"
#endif
