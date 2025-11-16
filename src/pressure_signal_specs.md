# **📊 Especificaciones del Procesamiento de Señal de Presión**

> **🎯 Alcance de este Documento:**  
> Este documento describe **exclusivamente** el flujo de procesamiento de señal de presión desde la lectura del sensor WNK80MA hasta la generación de mensajes MQTT. Incluye:
> - Arquitectura de 3 colas específicas
> - Tareas FreeRTOS de procesamiento (Reader, Telemetry, Formatter)
> - Algoritmo de filtrado y detección de eventos
> - Estructuras de datos y formatos JSON
> 
> **📋 Referencias:**
> - **Módulos genéricos del sistema** (WiFi, LED, Button, System State): `gateway_specs.md`
> - **Parámetros numéricos configurables**: `signal_parameters.h`
> - **Tipos de datos compartidos**: `data_types.h`

---

## **📂 Archivos de Implementación**

**Este documento especifica el pipeline de procesamiento de señal de presión. Los siguientes archivos implementan estas especificaciones:**

### **Pipeline de Procesamiento (3 Tareas + 3 Colas)**
- `pressure_reader.h/cpp` - **Tarea 1:** Lectura I2C a 100Hz del sensor WNK80MA
- `pressure_telemetry.h/cpp` - **Tarea 2:** Filtrado EPA, cálculo de derivada, detección de eventos
- `message_formatter.h/cpp` - **Tarea 3:** Serialización JSON y batching MQTT

### **Configuración y Datos**
- `signal_parameters.h` - Todos los parámetros configurables del algoritmo (umbrales, filtros, tamaños)
- `data_types.h/cpp` - Estructuras de datos (PressureReading, PressureEvent, MqttMessage, colas)

### **Dependencias Genéricas**
- `system_state.h/cpp` - Coordinación de estados y activación de tareas
- `mqtt_handler.h/cpp` - Publicación de mensajes a AWS IoT

> **📋 Nota:** Para módulos genéricos del gateway (WiFi, Button, LED, System State), ver `gateway_specs.md`

---

## **1️⃣ Objetivo del Algoritmo**

Detectar y transmitir cambios de presión del sensor WNK80MA de forma eficiente:

- **Comprimir intervalos estables:** Solo presión promedio + timestamps
- **Preservar intervalos de cambio:** Todas las muestras con timestamps individuales  
- **Detección basada en derivada:** Análisis de tendencia en lugar de cambios absolutos
- **Tiempo real:** Procesamiento a 100Hz con baja latencia

---

## **2️⃣ Algoritmo de Procesamiento**

### **🔹 Flujo Principal**
```
RAW (100Hz) → EPA1 → EPA2 → DERIVADA → DETECCIÓN → EVENTOS → JSON → MQTT
```

### **🔹 Etapas Detalladas**

#### **Etapa 1: Muestreo**
- **Frecuencia:** 100Hz (cada 10ms)
- **Validación:** `10,000 < raw < 16,000,000`
- **Cola:** `g_pressureQueue` → `pressureTelemetryTask`

#### **Etapa 2: Filtrado Doble EPA**
- **EPA Primario:** `α₁ = 0.1` (más agresivo)
- **EPA Secundario:** `α₂ = 0.05` (más suave)
- **Fórmula:** `filtered = α × new + (1-α) × prev`

#### **Etapa 3: Cálculo de Derivada**
- **Ventana:** Últimas N muestras (configurable)
- **Método:** Diferencia entre extremos de ventana dividida por tiempo
- **Fórmula:** `derivada = (filtered[n] - filtered[n-window]) / (window × 0.01s)`
- **Filtrado:** EPA aplicado a la derivada para evitar ruido

#### **Etapa 4: Detección de Estado**
- **Cambio:** `|derivada_filtrada| > DERIVATIVE_THRESHOLD`
- **Estable:** `|derivada_filtrada| ≤ DERIVATIVE_THRESHOLD × HYSTERESIS_FACTOR`
- **Histéresis:** Previene oscilaciones entre estados

---

## **3️⃣ Tipos de Intervalos y Condiciones de Corte**

### **🔹 Intervalo Estable**
**Características:**
- Derivada bajo umbral durante tiempo mínimo
- Solo se almacena presión promedio (sin muestras detalladas)
- Timestamps de inicio y fin
- Número de muestras utilizadas para el promedio

**Condiciones de Corte:**
- ✅ **Cambio de estado:** Cuando la derivada supera el umbral (STABLE → CHANGING)
- ✅ **Timeout:** Si el evento dura **más de 60 segundos** (se genera un nuevo evento STABLE)
- ❌ **NO se corta** por número de muestras (solo acumula estadísticas)

**JSON Output:**
```json
{
  "type": "stable",
  "startTimestamp": 1634567890123,
  "endTimestamp": 1634567895123,
  "pressure": 3450000,
  "sampleCount": 500,
  "duration_ms": 5000
}
```

### **🔹 Intervalo de Cambio**
**Características:**
- Derivada supera umbral
- Se almacenan TODAS las muestras detalladas (timestamp + valor + derivada)
- Máximo 300 muestras por evento (3 segundos @ 100Hz)
- Clasificación automática del tipo de cambio

**Condiciones de Corte:**
- ✅ **Cambio de estado:** Cuando la derivada vuelve bajo el umbral (CHANGING → STABLE)
- ✅ **Timeout:** Si el evento dura **más de 3 segundos** (se genera un nuevo evento CHANGING)
- ✅ **Buffer lleno:** Cuando se alcanzan 300 muestras almacenadas

**JSON Output:**
```json
{
  "type": "change",
  "startTimestamp": 1634567890123,
  "endTimestamp": 1634567892123,
  "triggerReason": "pressure_increase",
  "sampleCount": 200,
  "duration_ms": 2000,
  "samples": [
    [1634567890123, 3450000],
    [1634567890133, 3452000],
    [1634567890143, 3455000]
  ]
}
```

### **🔹 Ejemplo de Secuencia con Timeouts**

**Escenario: Estabilidad prolongada (150s)**
```
STABLE (0-60s) → evento STABLE #1 (60s, timeout)
STABLE (60-120s) → evento STABLE #2 (60s, timeout)
STABLE (120-150s) → evento STABLE #3 (30s, continúa...)
```

**Escenario: Cambio prolongado (10s)**
```
CHANGING (0-3s) → evento CHANGING #1 (3s, 300 muestras, timeout)
CHANGING (3-6s) → evento CHANGING #2 (3s, 300 muestras, timeout)
CHANGING (6-9s) → evento CHANGING #3 (3s, 300 muestras, timeout)
CHANGING (9-10s) → evento CHANGING #4 (1s, 100 muestras, cambio a STABLE)
```

---

## **4️⃣ Parámetros de Configuración**

**📋 IMPORTANTE: Todos los parámetros están definidos en `src/signal_parameters.h` - esa es la única fuente de verdad.**

Los ejemplos a continuación son solo para referencia:

### **🔹 Muestreo y Filtrado**
```cpp
#define SENSOR_SAMPLE_RATE_HZ 100           // Frecuencia principal
#define EPA_ALPHA_PRIMARY 0.1f              // Filtro EPA primario
#define EPA_ALPHA_SECONDARY 0.05f           // Filtro EPA secundario
```

### **🔹 Detección de Derivada**
```cpp
#define DERIVATIVE_WINDOW_SIZE 50           // Ventana para derivada (0.5s a 100Hz)
#define DERIVATIVE_THRESHOLD_PER_SEC 120000.0f  // Umbral de derivada por segundo
#define DERIVATIVE_FILTER_ALPHA 0.1f        // Suavizado de derivada
#define EVENT_HYSTERESIS_FACTOR 0.8f        // Factor de histéresis (80%)
```

### **🔹 Gestión de Eventos**
```cpp
#define MIN_EVENT_DURATION_MS 50                // Duración mínima de evento
#define MAX_STABLE_EVENT_DURATION_MS 60000      // Timeout para eventos STABLE (60s)
#define MAX_CHANGING_EVENT_DURATION_MS 3000     // Timeout para eventos CHANGING (3s)
#define MAX_SAMPLES_PER_EVENT 300               // Máximo 300 muestras (3s @ 100Hz)
```

### **🔹 Validación de Datos**
```cpp
#define RAW_VALUE_MIN 10000UL              // Valor RAW mínimo válido
#define RAW_VALUE_MAX 16000000UL           // Valor RAW máximo válido
#define MAX_SAMPLE_VARIATION_100HZ 300000UL // Variación máxima permitida
```

**⚠️ REGLA CRÍTICA:** Para modificar cualquier parámetro, editar solo `src/signal_parameters.h`

---

## **5️⃣ Módulos y Tareas Específicas de Procesamiento**

### **🔹 Módulos del Pipeline de Presión**

#### **1. Pressure Reader (`pressure_reader.cpp`)**
- **Función:** Adquisición de datos RAW del sensor I2C WNK80MA
- **Frecuencia:** 100Hz crítica (cada 10ms)
- **Prioridad:** 5 (máxima)
- **Stack:** 4096 bytes
- **Entrada:** Sensor WNK80MA (I2C 0x6D)
- **Salida:** `g_pressureQueue` → `PressureReading`
- **Responsabilidad:** 
  - Lectura I2C síncrona
  - Validación básica (límites RAW)
  - Timestamp preciso

#### **2. Pressure Telemetry (`pressure_telemetry.cpp`)**
- **Función:** Procesamiento de señal y detección de eventos
- **Frecuencia:** Procesa cada 100ms (batch de ~10 muestras)
- **Prioridad:** 3 (alta)
- **Stack:** 8192 bytes
- **Entrada:** `g_pressureQueue` ← `PressureReading`
- **Salida:** `g_pressureEventQueue` → `PressureEvent`
- **Responsabilidad:** 
  - Filtrado doble EPA
  - Cálculo de derivada
  - Detección de cambios/estabilidad
  - Gestión de intervalos

#### **3. Message Formatter (`message_formatter.cpp`)**
- **Función:** Formateo JSON y batching para MQTT
- **Frecuencia:** Según timeout o buffer lleno
- **Prioridad:** 2 (media)
- **Stack:** 6144 bytes
- **Entrada:** `g_pressureEventQueue` ← `PressureEvent`
- **Salida:** `g_telemetryQueue` → `MqttMessage`
- **Responsabilidad:** 
  - Serialización JSON optimizada
  - Batching de múltiples intervalos
  - Routing de topics
  - Gestión de QoS

### **🔹 Prioridades y Justificación**

| Tarea | Prioridad | Justificación |
|-------|-----------|---------------|
| `pressureReaderTask` | 5 | Crítica - timing exacto de 100Hz |
| `pressureTelemetryTask` | 3 | Alta - procesamiento en tiempo real |
| `messageFormatterTask` | 2 | Media - puede tolerar latencia |
| `mqttHandlerTask` | 2 | Media - red asíncrona |

---

## **6️⃣ Arquitectura de Comunicación (3 Colas)**

### **🔹 Visión General del Flujo**
```
┌──────────────┐   g_pressureQueue   ┌─────────────────┐   g_pressureEventQueue   ┌─────────────────┐   g_mqttQueue   ┌─────────────┐
│  Pressure    │  ─────────────────> │  Pressure       │  ───────────────────────> │  Message        │  ─────────────> │  MQTT       │
│  Reader      │   PressureReading   │  Telemetry      │      PressureEvent       │  Formatter      │   MqttMessage   │  Publisher  │
│  Task        │                     │  Task           │                          │  Task           │                 │  Task       │
│  (Prio 5)    │                     │  (Prio 3)       │                          │  (Prio 2)       │                 │  (Prio 2)   │
└──────────────┘                     └─────────────────┘                          └─────────────────┘                 └─────────────┘
     100Hz                               Procesa cada                                Agrupa eventos                       AWS IoT
    Raw I2C                                 100ms                                   en mensajes JSON                    Connectivity
```

### **🔹 Cola 1: Muestras Raw (`g_pressureQueue`)**
**Propósito:** Transportar lecturas raw desde el sensor hasta el procesador de señal

```cpp
#define PRESSURE_QUEUE_SIZE 300        // 3 segundos buffer a 100Hz
typedef struct {
    uint64_t timestamp;               // Marca de tiempo
    uint32_t rawValue;               // Valor RAW del sensor I2C
    bool isValid;                    // Validación (límites + variación)
} PressureReading;
```

**Flujo:**
- **Productor:** `pressureReaderTask` (crítica, 100Hz fijo)
- **Consumidor:** `pressureTelemetryTask` (procesa cada 100ms)

### **🔹 Cola 2: Eventos de Presión (`g_pressureEventQueue`)**
**Propósito:** Transportar eventos detectados (estables/cambios) hacia el formateador JSON

```cpp
#define PRESSURE_EVENT_QUEUE_SIZE 15   // Buffer de eventos
typedef struct {
    uint64_t startTimestamp;           // Inicio del evento
    uint64_t endTimestamp;             // Fin del evento
    EventType type;                    // STABLE, RISING, FALLING, OSCILLATION
    uint32_t startValue;               // Presión al inicio (EPA2)
    uint32_t endValue;                 // Presión al final (EPA2)
    uint16_t sampleCount;              // Número de muestras
    TriggerReason triggerReason;       // Razón de detección
    PressureSample samples[MAX_SAMPLES_PER_EVENT]; // Muestras detalladas (solo si cambio)
    bool hasDetailedSamples;           // Flag de detalle
} PressureEvent;
```

**Flujo:**
- **Productor:** `pressureTelemetryTask` (al completar intervalos)
- **Consumidor:** `messageFormatterTask` (nueva tarea)

### **🔹 Cola 3: Mensajes MQTT (`g_mqttQueue`)**
**Propósito:** Transportar mensajes JSON formateados hacia el publicador MQTT

```cpp
#define MQTT_QUEUE_SIZE 10            // Buffer de mensajes
typedef struct {
    char topic[128];                  // Tópico MQTT completo
    char payload[4096];               // JSON serializado
    uint8_t qos;                     // Nivel QoS (0 o 1)
} MqttMessage;
```

**Flujo:**
- **Productores:** `messageFormatterTask` (eventos) + otros módulos (healthcheck, OTA)
- **Consumidor:** `mqttPublishTask` (conectividad AWS IoT)

### **🔹 Ventajas de la Arquitectura de 3 Colas**

✅ **Separación de responsabilidades claras**
✅ **Tolerancia a fallos granular**
✅ **Escalabilidad para múltiples formatos de salida**
✅ **Debugging específico por capa**
✅ **Flexibilidad en batching y optimización JSON**

### **🔹 Nuevas Tareas FreeRTOS**

```cpp
// Nueva tarea para formateo JSON
void messageFormatterTask(void *pvParameters) {
    // - Lee g_pressureEventQueue
    // - Agrupa eventos en lotes
    // - Serializa a JSON optimizado
    // - Envía a g_mqttQueue
}
```

---

## **7️⃣ Protocolo MQTT Específico**

### **🔹 Topics**
- **Telemetría:** `mica/dev/telemetry/gateway/{deviceId}/pressure-data`
- **Healthcheck:** `mica/dev/status/gateway/{deviceId}/healthcheck`

### **🔹 QoS**
- Telemetría → **0** (best effort)
- Healthcheck → **0** (best effort)

### **🔹 Formato de Mensaje de Telemetría**

```json
{
  "sensor_id": "A1:B2:C3:D4:E5:F6",
  "sentTimestamp": 1634567890123,
  "signal_params": {
    "sample_rate_hz": 100,
    "derivative_threshold": 1200.0,
    "epa_alpha_primary": 0.1,
    "epa_alpha_secondary": 0.05
  },
  "intervals": [
    {
      "type": "stable",
      "startTimestamp": 1634567890123,
      "endTimestamp": 1634567895123,
      "pressure": 3450000,
      "sampleCount": 500,
      "duration_ms": 5000
    },
    {
      "type": "change",
      "startTimestamp": 1634567895123,
      "endTimestamp": 1634567897123,
      "triggerReason": "pressure_increase",
      "sampleCount": 200,
      "duration_ms": 2000,
      "samples": [
        [1634567895123, 3450000],
        [1634567895133, 3452000],
        [1634567895143, 3455000]
      ]
    }
  ]
}
```

### **🔹 Formato de Healthcheck**

```json
{
  "sentTimestamp": 1634567890123,
  "uptime": 3600000,
  "additional_data": {
    "wifi_rssi": -45,
    "battery_voltage": 3.7,
    "free_heap": 125000,
    "pressure_samples": 360000,
    "events_detected": 15,
    "system_state": "CONNECTED_MQTT",
    "i2c_errors": 0
  }
}
```

### **🔹 Limitaciones**
- **Tamaño máximo de payload:** 8192 bytes
- **Endpoint AWS IoT:** `a2iina9w8kq2z4-ats.iot.eu-west-3.amazonaws.com:8883`
- **Intervalos por mensaje:** Hasta `MAX_INTERVALS_PER_MESSAGE` (definido en `signal_parameters.h`)

---

## **8️⃣ Eventos Específicos de Procesamiento**

### **🔹 Pressure Events**
- `EVENT_PRESSURE_CHANGE_DETECTED` – Cambio significativo detectado
- `EVENT_PRESSURE_QUEUE_FULL` – Cola de presión llena (overrun)
- `EVENT_I2C_SENSOR_ERROR` – Error en comunicación I2C con WNK80MA

### **🔹 Processing Events**
- `EVENT_PROCESSING_OVERRUN` – Procesamiento retrasado (queue overflow)
- `EVENT_INTERVAL_READY` – Intervalo completado y listo para envío
- `EVENT_BATCH_SENT` – Batch de intervalos enviado a MQTT

---

## **9️⃣ Estructuras de Datos**

### **🔹 Muestra Raw**
```cpp
typedef struct {
    uint64_t timestamp;    // Timestamp en ms
    uint32_t rawValue;     // Valor crudo del sensor
    bool isValid;          // Flag de validación
} PressureReading;
```

### **🔹 Muestra Procesada**
```cpp
typedef struct {
    uint64_t timestamp;
    float filteredValue;   // Después de doble EPA
    float derivative;      // Derivada filtrada
    bool isChanging;       // Estado: cambio o estable
} ProcessedSample;
```

### **🔹 Intervalo de Datos**
```cpp
typedef struct {
    uint64_t startTimestamp;
    uint64_t endTimestamp;
    IntervalType type;           // STABLE o CHANGE
    uint32_t sampleCount;
    
    // Para intervalos estables
    float averagePressure;
    
    // Para intervalos de cambio
    ProcessedSample* samples;    // Array dinámico
    String triggerReason;        // "increase", "decrease", "oscillation"
} PressureInterval;
```

---

## **🔟 Estados de la Máquina de Estados**

### **🔹 Estados Principales**
```cpp
typedef enum {
    SIGNAL_STATE_INITIALIZING,     // Inicializando filtros
    SIGNAL_STATE_STABLE,           // Señal estable
    SIGNAL_STATE_CHANGE_DETECTED,  // Cambio detectado
    SIGNAL_STATE_IN_EVENT,         // Durante evento
    SIGNAL_STATE_POST_EVENT,       // Período post-evento
    SIGNAL_STATE_ERROR             // Error en procesamiento
} SignalProcessingState;
```

### **🔹 Transiciones de Estado**
- `INITIALIZING` → `STABLE`: Después de llenar ventana de derivada
- `STABLE` → `CHANGE_DETECTED`: Derivada > umbral
- `CHANGE_DETECTED` → `IN_EVENT`: Confirmación de evento
- `IN_EVENT` → `POST_EVENT`: Derivada < umbral × histéresis
- `POST_EVENT` → `STABLE`: Fin del período post-evento

---

## **1️⃣1️⃣ Algoritmo de Detección Detallado**

### **🔹 Inicialización**
1. Crear buffers circulares para ventana de derivada
2. Inicializar filtros EPA en 0
3. Establecer estado en `INITIALIZING`

### **🔹 Procesamiento por Muestra**
```cpp
void processPressureSample(PressureSample sample) {
    // 1. Validar muestra
    if (!isValidSample(sample)) return;
    
    // 2. Aplicar filtros EPA
    float primary = applyEPA(sample.rawValue, primaryFiltered, EPA_ALPHA_PRIMARY);
    float secondary = applyEPA(primary, secondaryFiltered, EPA_ALPHA_SECONDARY);
    
    // 3. Añadir a ventana de derivada
    addToDerivativeWindow(secondary, sample.timestamp);
    
    // 4. Calcular derivada si ventana llena
    if (isWindowFull()) {
        float derivative = calculateDerivative();
        float filteredDerivative = applyEPA(derivative, derivativeFiltered, DERIVATIVE_FILTER_ALPHA);
        
        // 5. Detectar estado
        bool isChanging = abs(filteredDerivative) > DERIVATIVE_THRESHOLD;
        
        // 6. Procesar según máquina de estados
        processStateMachine(isChanging, secondary, sample.timestamp);
    }
}
```

### **🔹 Gestión de Intervalos**
- **Cerrar intervalo estable:** Cuando se detecta cambio
- **Cerrar intervalo de cambio:** Cuando termina post-evento
- **Timeout de intervalos:** Máximo 10 segundos por intervalo

---

## **1️⃣2️⃣ Optimizaciones y Consideraciones**

### **🔹 Gestión de Memoria**
- **Buffers circulares** para ventanas de derivada
- **Pool de memoria** para intervalos de cambio
- **Liberación automática** de memoria después del envío MQTT

### **🔹 Tolerancia a Fallos**
- **Recuperación** de samples perdidos
- **Detección** de overruns en colas
- **Fallback** a modo simple si falta memoria

### **🔹 Calibración Adaptativa**
- **Auto-ajuste** de umbrales según nivel de ruido
- **Estadísticas** en tiempo real de derivada
- **Configuración remota** vía MQTT

### **🔹 Rendimiento**
- **Operaciones flotantes** optimizadas
- **Evitar malloc/free** en tiempo real
- **Stack mínimo** para tarea de alta frecuencia

---

## **9️⃣ Formato de Salida JSON**

**Para un lote de intervalos mixtos:**

```json
{
  "intervals": [
    {
      "type": "stable",
      "startTimestamp": 1634567890123,
      "endTimestamp": 1634567895123,
      "pressure": 3450000,
      "sampleCount": 500
    },
    {
      "type": "change", 
      "startTimestamp": 1634567895123,
      "endTimestamp": 1634567897123,
      "triggerReason": "pressure_increase",
      "sampleCount": 200,
      "samples": [
        [1634567895123, 3450000],
        [1634567895133, 3452000]
      ]
    }
  ]
}
```

**📋 Nota:** El formato completo del mensaje MQTT con headers y metadata adicional está descrito en la sección 7️⃣ de este documento.

---

## **📚 Referencias y Documentación Relacionada**

### **🔗 Documentos del Proyecto**
- **Arquitectura general del sistema:** `reference/src/gateway_specs.md`
  - Estados del sistema y eventos de notificación
  - Módulos genéricos (WiFi, LED, Button, System State)
  - Reglas de activación de tareas
  - Estándares de programación

- **Parámetros configurables:** `src/signal_parameters.h`
  - Todos los valores numéricos del algoritmo
  - Tamaños de colas y stacks
  - Umbrales y factores de filtrado

- **Tipos de datos compartidos:** `src/data_types.h`
  - Definición de todas las estructuras
  - Enums y tipos
  - Declaración de colas y mutexes globales

### **🎯 Reglas de Trabajo**
- **Workflow con Copilot:** `.github/copilot-instructions.md`

---

**✅ Esta especificación define exclusivamente el pipeline de procesamiento de señal de presión desde el sensor hasta MQTT.**