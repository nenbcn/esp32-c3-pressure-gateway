# **🤖 Reglas de Trabajo con GitHub Copilot**

---

## **1. Regla Crítica de Trabajo**

**NUNCA hacer cambios en código sin confirmación explícita del usuario.**

**Proceso obligatorio:**
1. **ANALIZAR** problema
2. **PROPONER** solución específica  
3. **ESPERAR** confirmación (`ok`, `procede`, `adelante`)
4. **SOLO ENTONCES** ejecutar cambios

**PROHIBIDO:** Hacer cambios directos sin permiso.

---

## **2. Workflow de Tareas y Issues**

**TODO.md como lista de trabajo:**
- `TODO.md` contiene la lista de tareas pendientes priorizadas
- Revisar TODO.md regularmente para identificar próximas tareas

**Workflow obligatorio antes de implementar:**
1. **Identificar tarea** en TODO.md
2. **Crear GitHub Issue** con:
   - Título descriptivo
   - Descripción detallada del problema/mejora
   - Label apropiado (bug, enhancement, documentation, etc)
   - Milestone si aplica
3. **Implementar cambios** referenciando el issue en commits
4. **Commit con referencia**: 
   - Para cerrar: `fix: description (closes #N)` o `feat: description (closes #N)`
   - Para referenciar: `refactor: description (ref #N)`
5. **Actualizar TODO.md** moviendo tarea de "Pendiente" a "Completado"

**NUNCA:**
- Implementar cambios sin crear issue primero
- Hacer commits sin referenciar el issue correspondiente
- Dejar tareas completadas en sección "Pendiente" de TODO.md

**Formato de commits:**
```
<type>: <description> (closes #N)

[body opcional con más detalles]

[footer opcional: Breaking Changes, etc]
```

Types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`, `perf`

---

## **3. Uso de PlatformIO**

- **Ruta ejecutable:** `/Users/nenbcn/.platformio/penv/bin/platformio`
- Usar **siempre** esta ruta en scripts de automatización y CI/CD.

---

## **3. Trabajo con la Carpeta Test**

Para pruebas de firmware independientes (no unit tests), puedes usar la carpeta `test/` como tu espacio de experimentación y pruebas rápidas, ignorando el contenido de `src/`.

**Reglas y recomendaciones:**

- Usa la opción global `src_dir = test` en `platformio.ini` para que PlatformIO compile y suba como firmware principal todo lo que pongas en `test/`.
- Asegúrate de que en `test/` haya un único archivo con las funciones `setup()` y `loop()` (por ejemplo, `main.cpp`).
- Puedes tener otros archivos de apoyo en `test/` (por ejemplo, drivers, mocks, etc.).
- Si necesitas headers comunes, colócalos en la carpeta `include/` para que estén disponibles tanto para `src/` como para `test/`.
- No uses `extern "C"` en headers de C++ puro para evitar problemas de linkage.
- El core de Arduino siempre se compila, pero solo se enlaza tu código de `test/`.
- Para volver a compilar el firmware de producción, elimina o comenta la línea `src_dir = test` y usa la estructura estándar con `src/`.

**Ejemplo mínimo de test:**

```cpp
#include <Arduino.h>
void setup() { Serial.begin(115200); }
void loop() { Serial.println("Test OK"); delay(1000); }
```

**Ventajas:**
- Permite pruebas rápidas y aisladas sin tocar el código de producción.
- Ideal para prototipos, pruebas de hardware, y debugging de bajo nivel.

---

## **4. Estándares de Programación**

- **Idioma:** Código y comentarios en inglés
- **Variables locales:** `camelCase`
- **Variables globales:** Prefijo `g_`
- **Constantes:** `MAYÚSCULAS_CON_GUIONES`
- **Funciones:** `camelCase` con verbo claro
- **Macro DEBUG_MODE:** Para habilitar logs
- **Estado global:** Solo modificable en `system_state.cpp`

---

## **5. Regla de Modularidad**

- **Cada módulo independiente** → comunicación por colas y eventos
- **Variables globales** documentadas y protegidas con mutex
- **Activación/desactivación de tareas** solo en `system_state.cpp`

---

## **6. Referencias a Documentación Técnica**

**Para consultar especificaciones técnicas del proyecto:**

- **Arquitectura general, hardware, WiFi, MQTT, FreeRTOS:** `src/gateway_specs.md`
- **Procesamiento específico de señal de presión:** `src/pressure_signal_specs.md`
- **Parámetros configurables:** `src/signal_parameters.h`
- **Guía de navegación de specs:** `src/README_SPECS.md`

**📋 IMPORTANTE:** No duplicar información técnica en este archivo - siempre referenciar los archivos de especificación correspondientes.

---

# ESP32-C3 Gateway de Presión WNK80MA con FreeRTOS

---

## 1. Introducción y Objetivo del Proyecto

Este proyecto implementa un **gateway basado en FreeRTOS** para el ecosistema **MICA**, usando un **ESP32-C3** y un **sensor de presión WNK80MA (I2C)**.

- Sustituye la lectura de **pulsos de caudal** del gateway original por **medición de presión**.
- Mantiene **arquitectura modular**, **sistema de estados** y **compatibilidad** con el firmware base.
- Soporta:
  - Lectura a **100Hz**
  - **Filtrado y detección de eventos de presión**
  - **MQTT** con **AWS IoT**
  - **OTA**, **Wi-Fi config mode** y **low power mode**

**Objetivo:** Obtener un **gateway robusto y modular** que detecte cambios de presión y los reporte en tiempo real a la nube, con mínima modificación del código base.

---

## 2. Flujo de Trabajo con GitHub Copilot

> **Estas reglas indican cómo Copilot debe interactuar con el código.**

### 2.1 Regla Crítica de Trabajo

**NUNCA hacer cambios en código sin confirmación explícita del usuario.**

**Proceso obligatorio:**
1. ANALIZAR problema
2. PROPONER solución específica
3. ESPERAR confirmación (`ok`, `procede`, `adelante`)
4. SOLO ENTONCES ejecutar cambios

**PROHIBIDO:** Hacer cambios directos sin permiso.

---

### 2.2 Uso de PlatformIO

- **Ruta ejecutable:**
  `/Users/nenbcn/.platformio/penv/bin/platformio`
- Usar **siempre** esta ruta en scripts de automatización y CI/CD.

---

## 3. Configuración del Hardware

### 3.1 Microcontrolador Base

- **Seeeduino XIAO ESP32-C3**
- Wi-Fi integrado, bajo consumo (deep sleep ≈ 5 µA)
- Cargador Li-ion integrado (JST 1.25 mm)
- Alimentación: USB-C o powerbank

### 3.2 Mapeo de Pines del Shield

| Función                    | Pin   | GPIO         | Tipo / Notas                                              | Boot safe / Strapping |
|----------------------------|-------|--------------|-----------------------------------------------------------|-----------------------|
| VBAT (ADC batería)         | D10   | GPIO10 (A2)  | ADC                                                      | ✅                    |
| Sensor presión (ANA)       | D1    | GPIO3 (A1)   | ADC                                                      | ✅                    |
| Wake‑up digital            | D2    | GPIO4        | Digital, salida de comparador RC ligado a D1              | ✅                    |
| NeoPixel (WS2812B)         | D3    | GPIO5 (A3)   | Digital                                                   | ✅                    |
| I²C SDA                    | D4    | GPIO6        | I²C estándar, usa pull‑up interno                         | ✅                    |
| I²C SCL                    | D5    | GPIO7        | I²C estándar, usa pull‑up interno                         | ✅                    |
| DIG IN (pulsos)            | D6    | GPIO21       | Digital                                                   | ✅                    |
| Buzzer (PWM)               | D7    | GPIO20       | Digital                                                   | ✅                    |
| Relay CTRL (5 V high‑side) | D8    | GPIO8        | PMOS high‑side OFF=HIGH, ON=LOW → 5 V out                 | ⚠ strapping          |
| 1‑WIRE TEMP (DS18B20)      | D0    | GPIO2 (A0)   | Digital con pull‑up                                       | ⚠ strapping          |
| Botón usuario (int+ext)    | D9    | GPIO9        | Entrada pull‑up, N.O. a GND                               | ⚠ strapping          |

- **Sensor:** WNK80MA (I2C, 3.3V)
- **Dirección I2C:** `0x6D` (hex, fija)

---

## 4. Arquitectura del Sistema

### 4.1 Estados del Sistema

- `SYSTEM_STATE_CONNECTING`
- `SYSTEM_STATE_CONNECTED_WIFI`
- `SYSTEM_STATE_CONFIG_MQTT`
- `SYSTEM_STATE_CONNECTED_MQTT`
- `SYSTEM_STATE_CONFIG_MODE`
- `SYSTEM_STATE_WAITING_BUTTON_RELEASE`
- `SYSTEM_STATE_OTA_UPDATE`
- `SYSTEM_STATE_ERROR`

### 4.2 Módulos Principales

1. **Pressure Reader** – Lectura I2C 100Hz y envío a `g_pressureQueue`
2. **Pressure Telemetry** – Filtrado, derivada y batching de eventos
3. **MQTT Handler** – Mantiene conexión MQTT y publica mensajes de la cola común
4. **System State** – Coordina estados y tareas
5. **Módulos Originales sin cambios:**
   - WiFi Connect
   - WiFi Config Mode
   - LED Manager
   - Button Manager
   - EEPROM Config
6. **OTA Manager** – Gestión OTA (MQTT/HTTPS)
7. **Battery Manager** – Voltaje y % batería
8. **Device ID** – MAC → ID único
9. **Config** – Persistencia de parámetros
10. **Secrets** – Certificados X.509

---

## 5. Arquitectura FreeRTOS

### 5.1 Concepto y Filosofía

- Garantizar lectura a 100Hz
- Comunicación por colas y notificaciones
- Tareas suspendidas en bajo consumo

### 5.2 Prioridades y Stack

| Tarea                     | Prioridad |
|---------------------------|----------|
| `pressureReaderTask`      | 5 |
| `systemStateTask`         | 4 |
| `pressureTelemetryTask`   | 3 |
| `mqttHandlerTask`         | 2 |
| `wifiConnectTask`         | 2 |
| `wifiConfigModeTask`      | 1 |
| `ledManagerTask`          | 1 |
| `buttonHandlerTask`       | 1 |

**Justificación:**
- `pressureReaderTask` crítica (cada 10 ms)
- `systemStateTask` coordina el sistema
- Resto puede suspenderse si hay problemas

---

### 5.3 Colas y Mutexes

> **Nota sobre estructuras de datos globales:**  
> Todas las estructuras de datos globales (colas, mutexes, tipos de eventos, etc.) **se definen en `data_types.h`**.  
> Si aquí se menciona alguna estructura, es solo para dar una visión de alto nivel del sistema.  
> **No se detallan los campos ni la implementación aquí**; consulta siempre `data_types.h` para la definición exacta y actualizada.

- **g_pressureQueue** – RAW 100Hz  
  - Productor: `pressureReaderTask`
  - Consumidor: `pressureTelemetryTask`
  - Propósito: transportar muestras RAW del sensor a alta frecuencia.

- **g_pressureBatchQueue** – Lotes/eventos de presión detectados  
  - Productor: `pressureTelemetryTask`
  - Consumidor: módulo que empaqueta y publica eventos
  - Propósito: aislar el procesamiento de eventos de presión del envío a la nube.

- **g_telemetryQueue** – Mensajes genéricos para MQTT (topic + payload)  
  - Productor: cualquier módulo (telemetría, batería, eventos, etc.)
  - Consumidor: `mqttHandlerTask`
  - Propósito: desacoplar la lógica de sensores y eventos de la publicación MQTT, permitiendo que cualquier módulo publique mensajes sin depender del handler.

- **Mutexes:**  
  - `g_stateMutex`
  - `g_i2cMutex`
  - `g_wifiMutex`
  - `g_mqttMutex`
  - `eepromMutex`

**Regla de estructuras compartidas:**  
- Todas las colas y mutexes globales deben declararse solo en `src/data_types.h` (no en headers de módulos individuales).
- La creación y destrucción de cada cola debe hacerse solo en su módulo responsable.
- El acceso desde otros módulos debe hacerse siempre vía `extern` y el header común.

---

## 6. Ciclo de Vida y Comunicación

- `pressureReaderTask` siempre activa
- Otras tareas creadas/suspendidas según estado
- Patrón **producer-consumer** y **event-driven**
- **Watchdog**, control de **stack y heap**, monitoreo de colas

---

## 7. Protocolo MQTT

- **Topics:**
  - `mica/dev/telemetry/gateway/{deviceId}/pressure-data`
  - `mica/dev/status/gateway/{deviceId}/healthcheck`
  - `mica/dev/command/gateway/{deviceId}/ota`
  - `mica/dev/command/gateway/{deviceId}/config`

- **QoS:**
  - Telemetría / Healthcheck → 0
  - OTA / Config → 1

- **Payload presión (ejemplo):**
```json
{
  "sensor_id": "A1:B2:C3:D4:E5:F6",
  "sentTimestamp": 1634567890123,
  "signal_params": { ... },
  "pressure_events": [
    {
      "startTimestamp": ...,
      "endTimestamp": ...,
      "startValue": ...,
      "endValue": ...,
      "sampleCount": ...,
      "triggerReason": "...",
      "samples": [
        [timestamp, value]
      ]
    }
  ]
}
```
- **Payload healthcheck (ejemplo):**
```json
{
  "sentTimestamp": ...,
  "uptime": ...,
  "additional_data": {
    "wifi_rssi": ...,
    "battery_voltage": ...,
    "free_heap": ...,
    "pressure_samples": ...,
    "events_detected": ...,
    "system_state": "...",
    "i2c_errors": ...
  }
}
```
Tamaño máximo: 8192 bytes

Endpoint AWS IoT:
a2iina9w8kq2z4-ats.iot.eu-west-3.amazonaws.com:8883

---

## 8. Procesamiento de Señal y Detección de Eventos

> **Nota sobre parámetros de señal:**  
> Todos los parámetros de procesamiento de señal (umbrales, filtros, etc.) **se definen exclusivamente en `signal_parameters.h`**.  
> Si se menciona algún parámetro aquí, es solo a modo de ejemplo y **no debe considerarse la fuente de verdad**.  
> **Siempre consulta y modifica los valores reales en `signal_parameters.h`.**

Flujo: RAW → EPA1 → EPA2 → DERIVADA → DETECCIÓN → BATCH → MQTT

Criterios:

- RAW válido: 100.000 < raw < 16.000.000
- Trigger: |derivada| > DERIVATIVE_THRESHOLD
- Histeresis y duración mínima definidas en signal_parameters.h

Evento MQTT:
- startTimestamp, endTimestamp
- startValue, endValue
- sampleCount, triggerReason
- samples como [timestamp, value]

---

## 9. Notification Events

General: EVENT_LONG_PRESS_BUTTON

Wi-Fi: EVENT_WIFI_CONNECTED, EVENT_WIFI_FAIL_CONNECT, EVENT_NO_PARAMETERS_EEPROM

Wi-Fi Config: EVENT_WIFI_CONFIG_*

MQTT: EVENT_MQTT_CONNECTED, EVENT_MQTT_DISCONNECTED

---

## 10. Variables Globales

g_systemState – Estado global (SystemState), protegido por g_stateMutex

g_stateMutex – Mutex de estado global

g_logMessageQueue – Cola de logs (QueueHandle_t)

---

## 11. Estándares de Programación

- Código y comentarios en inglés
- Variables locales → camelCase
- Globales → g_
- Constantes → MAYÚSCULAS_CON_GUIONES
- Funciones → camelCase con verbo claro
- Macro DEBUG_MODE para habilitar logs
- Estado global solo modificable en system_state.cpp

---

## 12. Modularity Rule

- Cada módulo independiente → comunicación por colas y eventos
- Variables globales documentadas y protegidas con mutex
- Activación/desactivación de tareas solo en system_state.cpp