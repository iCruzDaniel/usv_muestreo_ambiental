# 🛠️ Monitoring & Calibration Guide

La **Consola de Diagnóstico** es una herramienta de terminal en tiempo real que permite monitorear el estado completo del USV (FreeRTOS, red, sensores, MAVLink) y realizar procedimientos de calibración interactivos directamente en el sitio sin tener que reprogramar o hacer iteraciones manuales.

---

## 1. Habilitar la Consola de Diagnóstico

La consola está desactivada por defecto para no consumir tiempo de procesamiento. Para habilitarla, debes asegurarte de que el flag `-DENABLE_DIAGNOSTICS` esté descomentado en el archivo `platformio.ini` para tu entorno (por ejemplo, en `usv_wifi` o `usv_lte`):

```ini
[env:usv_wifi]
; ...
build_flags =
    ${common.build_flags_common}
    -DUSE_WIFI
    -DWIFI_SSID=\"MI_RED\"
    -DWIFI_PASS=\"MI_PASS\"
    -DENABLE_DIAGNOSTICS   <-- ¡Descomentar esta línea!
```

---

## 2. Uso del Dashboard y Comandos Básicos

Una vez flasheado el USV, abre el **Monitor Serial** (`pio device monitor` o a través de VSCode) configurado a `115200` baudios. El dashboard se imprimirá cada 10 segundos, pero puedes interactuar enviando comandos:

*   `status`: Muestra instantáneamente el dashboard general (estado FSM, red, MQTT, MAVLink, GPS, batería y sensores).
*   `sensors`: Muestra los voltajes *crudos* (raw) de los ADC y los valores matemáticos resultantes, útil para detectar problemas de hardware.
*   `help`: Muestra la lista de todos los comandos de calibración.

---

## 3. Guía de Calibración de Sensores

### 💧 Calibración de pH (2 puntos)
Se requieren dos soluciones tampón (buffers), típicamente de pH 4 y pH 7.

1.  Limpia la sonda y sumérgela en la solución **pH 4**. Espera a que la lectura se estabilice.
2.  Escribe en la consola: `cal ph4`
3.  Limpia la sonda y sumérgela en la solución **pH 7**. Espera a que se estabilice.
4.  Escribe en la consola: `cal ph7`
5.  Escribe: `cal phapply`. La consola calculará automáticamente la pendiente ($m$) y el *offset* ($b$) de la recta.
6.  **¡ATENCIÓN!** La consola imprimirá los valores calculados de voltaje (`V4` y `V7`). Anótalos para el siguiente paso.

### 🌫️ Calibración de Turbidez (2 puntos)
1.  Sumerge el sensor en agua limpia/destilada (0 NTU).
2.  Escribe: `cal turb_clear`
3.  Sumerge el sensor en tu solución estándar conocida (por ejemplo, 1000 NTU).
4.  Escribe: `cal turb_std`
5.  Escribe: `cal turb_apply 1000` (reemplaza `1000` con el valor real de tu estándar).
6.  **¡ATENCIÓN!** La consola calculará e imprimirá un *Slope* y un *Offset*. Anótalos.

---

## 4. Evitando el Hardcode: Calibración Permanente

Los valores que calculaste arriba se perderán al reiniciar el microcontrolador. Para hacer que la calibración sea **permanente** para este hardware en específico, usaremos el sistema de `build_flags` en `platformio.ini`. Esto inyecta los valores en el código durante la compilación, evitando tener "números mágicos" hardcodeados en el `.cpp`.

### Paso 4.1: Modificar `platformio.ini`
Agrega los voltajes de pH (o los *slope/offset* de turbidez) que te dio la consola al bloque `build_flags_common` (afectará tanto a WiFi como a LTE):

```ini
build_flags_common =
    ; ... (tus otras configuraciones)
    
    ; -- Calibración Permanente de pH --
    -DPH_CAL_V4=1.8234    ; Reemplazar con tus valores
    -DPH_CAL_V7=2.5012    ; Reemplazar con tus valores
```

### Paso 4.2: Adaptar el Sensor para leer los Flags
Modifica el método `begin()` de tu clase sensora (`lib/Sensors/PHSensor/PHSensor.cpp`) para que aplique automáticamente estos valores al arrancar, si están definidos en el entorno:

```cpp
bool PHSensor::begin() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    Serial.printf("[PH] Iniciado — pin %d\n", _pin);
    
    // Inyección condicional de parámetros desde platformio.ini
#if defined(PH_CAL_V4) && defined(PH_CAL_V7)
    calibrate(PH_CAL_V4, PH_CAL_V7);
#else
    Serial.println("[PH] ADVERTENCIA: Usando valores de calibración por defecto. ¡Se requiere calibrar!");
#endif

    _ready = true;
    return true;
}
```

De la misma manera, para el sensor de turbidez en `TurbiditySensor.cpp`, puedes adaptar su `begin()`:

```cpp
bool TurbiditySensor::begin() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    Serial.printf("[TURB] Iniciado — pin %d\n", _pin);

#if defined(TURB_CAL_SLOPE) && defined(TURB_CAL_OFFSET)
    setCalibration(TURB_CAL_SLOPE, TURB_CAL_OFFSET);
    Serial.println("[TURB] Calibración cargada desde entorno.");
#endif

    _ready = true;
    return true;
}
```

¡Listo! De esta manera tu USV estará siempre calibrado para ese entorno específico, manteniendo el código limpio y libre de *hardcodes*. Las compilaciones por WiFi o LTE compartirán la misma calibración al estar ubicadas en `build_flags_common`.
