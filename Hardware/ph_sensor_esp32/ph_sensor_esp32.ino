/*
  Calibración sensor pH analógico - ESP32
  ----------------------------------------
  Calibración de 2 puntos: pH 4.0 y pH 7.0
  Conexión: salida analógica del sensor -> GPIO 34 (ADC1)

  Instrucciones:
    1. Sumergir electrodo en buffer pH 7.0
    2. Enviar 'C7' por Serial
    3. Sumergir electrodo en buffer pH 4.0
    4. Enviar 'C4' por Serial
    5. Enviar 'R' para leer pH en tiempo real
    6. Enviar 'S' para ver valores de calibración guardados
*/

#include <Arduino.h>

// ─── Pines y configuración ──────────────────────────────────────────────────
#define PH_PIN        34          // Pin ADC (usa solo ADC1: GPIO 32-39)
#define ADC_BITS      12          // Resolución del ADC ESP32
#define ADC_MAX       4095.0f
#define VREF          3.3f        // Referencia de voltaje del ESP32
#define SAMPLES       32          // Muestras para promediar (reduce ruido)
#define SAMPLE_DELAY  5           // ms entre muestras

// ─── Variables de calibración ───────────────────────────────────────────────
float cal_voltage_4  = 0.0f;     // Voltaje medido en buffer pH 4.0
float cal_voltage_7  = 0.0f;     // Voltaje medido en buffer pH 7.0
bool  cal_4_done     = false;
bool  cal_7_done     = false;

// ─── Función: leer voltaje promediado ───────────────────────────────────────
float readVoltage() {
  long sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sum += analogRead(PH_PIN);
    delay(SAMPLE_DELAY);
  }
  float raw = (float)(sum / SAMPLES);
  return (raw / ADC_MAX) * VREF;
}

// ─── Función: convertir voltaje a pH ────────────────────────────────────────
// Interpolación lineal entre los 2 puntos de calibración
float voltageToPH(float voltage) {
  if (!cal_4_done || !cal_7_done) {
    return -1.0f;  // Calibración incompleta
  }
  // Pendiente: ΔpH / ΔVoltaje
  float slope = (4.0f - 7.0f) / (cal_voltage_4 - cal_voltage_7);
  float ph = 7.0f + slope * (voltage - cal_voltage_7);
  return ph;
}

// ─── Setup ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  analogReadResolution(ADC_BITS);
  analogSetAttenuation(ADC_11db);  // Rango: 0 - 3.3V

  Serial.println("\n========================================");
  Serial.println("  Calibración Sensor pH - ESP32");
  Serial.println("========================================");
  printMenu();
}

// ─── Loop ───────────────────────────────────────────────────────────────────
void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "C7") {
      calibrate(7);
    } else if (cmd == "C4") {
      calibrate(4);
    } else if (cmd == "R") {
      readContinuous();
    } else if (cmd == "S") {
      showCalibration();
    } else if (cmd == "V") {
      float v = readVoltage();
      Serial.printf("Voltaje actual: %.4f V\n", v);
    } else {
      Serial.println("Comando no reconocido.");
      printMenu();
    }
  }
}

// ─── Calibrar un punto ──────────────────────────────────────────────────────
void calibrate(int phPoint) {
  Serial.printf("\n[CAL] Sumergir electrodo en buffer pH %.1f\n", (float)phPoint);
  Serial.println("[CAL] Esperando 10 segundos para estabilizar...");
  delay(10000);

  float voltage = readVoltage();

  if (phPoint == 7) {
    cal_voltage_7 = voltage;
    cal_7_done = true;
    Serial.printf("[CAL] pH 7.0 guardado  -> %.4f V\n", voltage);
  } else if (phPoint == 4) {
    cal_voltage_4 = voltage;
    cal_4_done = true;
    Serial.printf("[CAL] pH 4.0 guardado  -> %.4f V\n", voltage);
  }

  if (cal_4_done && cal_7_done) {
    float slope = (4.0f - 7.0f) / (cal_voltage_4 - cal_voltage_7);
    Serial.printf("[CAL] Calibración completa! Pendiente: %.4f pH/V\n", slope);
    Serial.println("[CAL] Enviar 'R' para leer pH en tiempo real.");
  } else {
    Serial.println("[CAL] Falta calibrar el otro punto.");
  }
}

// ─── Lectura continua ───────────────────────────────────────────────────────
void readContinuous() {
  if (!cal_4_done || !cal_7_done) {
    Serial.println("[ERROR] Calibración incompleta. Realizar C7 y C4 primero.");
    return;
  }
  Serial.println("\n[LEYENDO] pH en tiempo real. Enviar cualquier tecla para detener.");
  Serial.println("----------------------------------------");

  while (!Serial.available()) {
    float voltage = readVoltage();
    float ph = voltageToPH(voltage);
    Serial.printf("Voltaje: %.4f V  |  pH: %.2f\n", voltage, ph);
    delay(1000);
  }
  Serial.read();  // Limpiar buffer
  Serial.println("[PARADO] Lectura detenida.");
  printMenu();
}

// ─── Mostrar calibración guardada ───────────────────────────────────────────
void showCalibration() {
  Serial.println("\n─── Valores de Calibración ─────────────");
  if (cal_7_done)
    Serial.printf("  pH 7.0  -> %.4f V\n", cal_voltage_7);
  else
    Serial.println("  pH 7.0  -> (sin calibrar)");

  if (cal_4_done)
    Serial.printf("  pH 4.0  -> %.4f V\n", cal_voltage_4);
  else
    Serial.println("  pH 4.0  -> (sin calibrar)");

  if (cal_4_done && cal_7_done) {
    float slope = (4.0f - 7.0f) / (cal_voltage_4 - cal_voltage_7);
    Serial.printf("  Pendiente: %.4f pH/V\n", slope);
  }
  Serial.println("────────────────────────────────────────\n");
}

// ─── Menú de comandos ───────────────────────────────────────────────────────
void printMenu() {
  Serial.println("\n─── Comandos ───────────────────────────");
  Serial.println("  C7  -> Calibrar con buffer pH 7.0");
  Serial.println("  C4  -> Calibrar con buffer pH 4.0");
  Serial.println("  R   -> Leer pH en tiempo real");
  Serial.println("  V   -> Ver voltaje actual (raw)");
  Serial.println("  S   -> Ver calibración guardada");
  Serial.println("────────────────────────────────────────\n");
}
