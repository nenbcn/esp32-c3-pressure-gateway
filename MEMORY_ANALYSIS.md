# Análisis de Memoria - ESP32-C3 Pressure Gateway

**Fecha:** 25 Noviembre 2025  
**Build:** RAM 17.4% (57,012 bytes) | Flash 79.3% (1,039,822 bytes)

---

## 📊 Situación Actual

### Hardware: ESP32-C3 (Seeed XIAO)
- **RAM Total:** 327,680 bytes (320 KB)
- **RAM Usada:** 57,012 bytes (17.4%)
- **RAM Libre:** 270,668 bytes (82.6%)
- **Flash Total:** 1,310,720 bytes (1.25 MB)
- **Flash Usada:** 1,039,822 bytes (79.3%)

### ⚠️ Preocupaciones
- **17.4% RAM estática** parece bajo, pero...
- **Heap dinámico** se consume durante ejecución (colas, AsyncWebServer, etc)
- **Stack de tareas** no está en RAM estática (se aloca en heap)
- **Fragmentación** es crítica en ESP32-C3 (por eso AsyncWebServer es pointer)

---

## 🧮 Cálculo de Consumo de Memoria Dinámica

### 1. Estructuras de Datos

#### **PressureSample** (1 muestra procesada)
```cpp
typedef struct {
    uint64_t timestamp;        // 8 bytes
    uint32_t filteredValue;    // 4 bytes
    float derivative;          // 4 bytes
} PressureSample;              // TOTAL: 16 bytes
```

#### **PressureEvent** (1 evento completo)
```cpp
typedef struct {
    uint64_t startTimestamp;                           // 8 bytes
    uint64_t endTimestamp;                             // 8 bytes
    EventType type;                                    // 4 bytes (enum)
    uint32_t startValue;                               // 4 bytes
    uint32_t endValue;                                 // 4 bytes
    uint16_t sampleCount;                              // 2 bytes
    TriggerReason triggerReason;                       // 4 bytes (enum)
    bool hasDetailedSamples;                           // 1 byte
    PressureSample samples[MAX_SAMPLES_PER_EVENT];     // 16 * 100 = 1,600 bytes
} PressureEvent;                                       // TOTAL: ~1,635 bytes (con padding)
```

#### **MqttMessage** (1 mensaje MQTT)
```cpp
typedef struct {
    char topic[128];           // 128 bytes
    char payload[4096];        // 4,096 bytes
    uint8_t qos;               // 1 byte
    bool retain;               // 1 byte
} MqttMessage;                 // TOTAL: 4,226 bytes
```

#### **PressureReading** (1 lectura RAW)
```cpp
typedef struct {
    uint64_t timestamp;        // 8 bytes
    uint32_t rawValue;         // 4 bytes
    bool isValid;              // 1 byte
} PressureReading;             // TOTAL: 16 bytes (con padding)
```

---

### 2. Colas FreeRTOS

| Cola                    | Tamaño Item | Profundidad | Memoria Total  |
|-------------------------|-------------|-------------|----------------|
| `g_pressureQueue`       | 16 bytes    | 300         | **4,800 bytes** |
| `g_pressureEventQueue`  | 1,635 bytes | 10          | **16,350 bytes** |
| `g_mqttQueue`           | 4,226 bytes | 10          | **42,260 bytes** |
| **TOTAL COLAS**         |             |             | **63,410 bytes** |

---

### 3. Stacks de Tareas FreeRTOS

| Tarea                      | Stack Size | Estado    | Memoria     |
|----------------------------|------------|-----------|-------------|
| `wifiConnectTask`          | 4,096      | Active    | 4,096 bytes |
| `wifiConfigModeTask`       | 4,096      | Suspended | 4,096 bytes |
| `mqttConnectTask`          | 4,096      | Suspended | 4,096 bytes |
| `mqttPublishTask`          | 10,000     | Suspended | 10,000 bytes |
| `stateManagementTask`      | 4,096      | Active    | 4,096 bytes |
| `ledTask`                  | 2,048      | Active    | 2,048 bytes |
| `buttonTask`               | 2,048      | Active    | 2,048 bytes |
| `logTask`                  | 2,048      | Active    | 2,048 bytes |
| `pressureReaderTask`       | 3,072      | Active    | 3,072 bytes |
| `pressureTelemetryTask`    | 8,192      | Active    | 8,192 bytes |
| `messageFormatterTask`     | 10,240     | Suspended | 10,240 bytes |
| **TOTAL STACKS**           |            |           | **54,032 bytes** |

---

### 4. Objetos Globales Estimados

| Objeto                  | Tamaño Estimado | Notas |
|-------------------------|-----------------|-------|
| WiFiClientSecure        | ~8,000 bytes    | Buffers TLS/SSL |
| PubSubClient            | ~4,000 bytes    | MQTT buffers |
| Adafruit_NeoPixel       | ~100 bytes      | Minimal overhead |
| ArduinoJson DynamicDoc  | 4,096 bytes     | JSON serialization buffer |
| AsyncWebServer (cuando activo) | ~15,000 bytes | Solo en CONFIG_MODE |
| AsyncTCP tasks/buffers  | ~8,000 bytes    | TCP internal buffers |
| **TOTAL OBJETOS**       | **~39,200 bytes** | **~55,000 con AsyncWebServer** |

---

## 📈 Consumo Total de RAM (Estimado)

### Escenario 1: Operación Normal (Sin CONFIG_MODE)

```
RAM Estática:               57,012 bytes (17.4%)
Colas FreeRTOS:            63,410 bytes (19.4%)
Stacks de Tareas:          54,032 bytes (16.5%)
Objetos Globales:          39,200 bytes (12.0%)
────────────────────────────────────────────
TOTAL USADO:              213,654 bytes (65.2%)
RAM LIBRE:                114,026 bytes (34.8%)
```

### Escenario 2: CONFIG_MODE Activo (AsyncWebServer)

```
RAM Estática:               57,012 bytes (17.4%)
Colas FreeRTOS:            63,410 bytes (19.4%)
Stacks de Tareas:          54,032 bytes (16.5%)
Objetos Globales:          55,000 bytes (16.8%)
────────────────────────────────────────────
TOTAL USADO:              229,454 bytes (70.0%)
RAM LIBRE:                 98,226 bytes (30.0%)
```

---

## 🔴 Puntos Críticos Identificados

### 1. **g_mqttQueue es ENORME**
- **42,260 bytes** (12.9% de toda la RAM)
- Cada mensaje puede ser hasta 4 KB de JSON
- Profundidad de 10 mensajes → 40 KB solo en esta cola

**Problema:** Si la red está lenta o MQTT desconectado, la cola se llena y consume toda esta memoria.

**Solución:**
```cpp
// Opción A: Reducir profundidad (10 → 5)
#define MQTT_QUEUE_SIZE 5  // 21,130 bytes (ahorra 21 KB)

// Opción B: Reducir tamaño de payload (4096 → 2048)
char payload[2048];  // Reduce MqttMessage a ~2,177 bytes
                     // Cola completa: 21,770 bytes (ahorra 20 KB)
```

### 2. **g_pressureEventQueue es la segunda más grande**
- **16,350 bytes** (5.0% de RAM)
- Cada evento tiene 100 muestras detalladas (1.6 KB)

**Problema:** Ya optimizado de 300→100 muestras. Difícil reducir más sin perder funcionalidad.

**Alternativa:** Comprimir muestras (solo deltas), pero complica código.

### 3. **messageFormatterTask stack es muy grande**
- **10,240 bytes** (3.1% de RAM)
- Necesario para ArduinoJson DynamicJsonDocument en stack

**Solución:** Ya está al mínimo para ArduinoJson 4 KB + overhead.

### 4. **Fragmentación de Heap**
- AsyncWebServer necesita bloques contiguos grandes
- Si el heap está fragmentado, falla aunque haya RAM libre
- **Por eso AsyncWebServer es pointer** → crear solo cuando se necesita

---

## 🎯 Optimizaciones Propuestas

### **Nivel 1: Sin Cambio de Hardware (Optimizar C3)**

#### A. Reducir g_mqttQueue (Ahorro: ~20 KB)
```cpp
// signal_parameters.h
#define MQTT_QUEUE_SIZE 5          // Era 10
#define MQTT_PAYLOAD_SIZE 2048     // Era 4096
```

**Impacto:**
- ✅ Libera 20 KB de RAM (6% del total)
- ⚠️ Menor tolerancia a ráfagas de eventos
- ✅ Payload de 2 KB aún permite ~4-5 eventos por mensaje

#### B. Reducir g_pressureQueue (Ahorro: ~2.4 KB)
```cpp
// signal_parameters.h
#define PRESSURE_QUEUE_SIZE 150    // Era 300
```

**Impacto:**
- ✅ Libera 2.4 KB de RAM
- ⚠️ Buffer de solo 1.5 segundos a 100Hz (era 3s)
- ⚠️ Más riesgo de pérdida de muestras si telemetry se retrasa

#### C. Monitoring de Stack Watermark
```cpp
// En cada tarea, añadir:
UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
Log::debug("[Task] Stack free: %u bytes", stackHighWaterMark * 4);
```

**Impacto:**
- ✅ Detecta overflows antes de crashes
- ✅ Permite ajustar stack sizes con datos reales
- ✅ Sin costo de RAM (solo logging)

---

### **Nivel 2: Migración a ESP32-S3 (Recomendado)**

#### Hardware: ESP32-S3 (Seeed XIAO ESP32-S3)

**Ventajas:**
```
ESP32-C3:  320 KB RAM,  RISC-V single core
ESP32-S3:  512 KB RAM,  Xtensa dual core (+60% RAM)
           ↓
Sin cambios de código (mismo framework Arduino)
Compatible con shield actual (mismo pinout XIAO)
```

**Con S3 tendríamos:**
```
RAM Total:                524,288 bytes (512 KB)
Consumo Actual:           229,454 bytes (43.8%)
RAM Libre:                294,834 bytes (56.2%)
```

**Beneficios:**
- ✅ **56% de RAM libre** vs 30% actual
- ✅ Sin necesidad de optimizaciones agresivas
- ✅ Margen para añadir features futuras
- ✅ Mismo código, solo cambiar `board = seeed_xiao_esp32s3`
- ✅ Dual core → pressure reader en core 0, WiFi/MQTT en core 1

**Coste:** ~$7 USD (vs $5 del C3) - diferencia mínima

---

## 📋 Decisión Recomendada

### **Corto Plazo (1-2 semanas):**
1. Implementar **monitoring de stack watermark**
2. Reducir `MQTT_QUEUE_SIZE` a 5 (test con carga real)
3. Medir heap fragmentation durante CONFIG_MODE

### **Medio Plazo (1 mes):**
4. Si memoria sigue crítica o aparecen crashes → **migrar a S3**
5. S3 es la solución definitiva sin comprometer funcionalidad

### **Regla de decisión:**
```
SI heap_libre < 80 KB en operación normal
O crashes por fragmentación en CONFIG_MODE
ENTONCES migrar a ESP32-S3
```

Actualmente: **98 KB libres en CONFIG_MODE** → Ajustado pero funcional.

---

## 🔬 Monitoreo Continuo

Añadir a `system_state.cpp`:

```cpp
// Cada 60 segundos
void logMemoryStats() {
    Log::info("[Memory] Free heap: %u bytes, Largest block: %u bytes, Min free: %u bytes",
              ESP.getFreeHeap(),
              ESP.getMaxAllocHeap(),
              ESP.getMinFreeHeap());
    
    // Stack watermarks
    Log::info("[Stack] WiFi: %u, MQTT: %u, Telemetry: %u, Formatter: %u",
              uxTaskGetStackHighWaterMark(g_wifiConnectTaskHandle) * 4,
              uxTaskGetStackHighWaterMark(g_mqttTaskHandle) * 4,
              uxTaskGetStackHighWaterMark(g_pressureTelemetryTaskHandle) * 4,
              uxTaskGetStackHighWaterMark(g_messageFormatterTaskHandle) * 4);
}
```

---

## 📊 Comparativa ESP32-C3 vs S3

| Característica      | ESP32-C3        | ESP32-S3 (XIAO)  | Ganancia     |
|---------------------|-----------------|------------------|--------------|
| RAM                 | 320 KB          | 512 KB           | +60%         |
| Flash               | 4 MB            | 8 MB             | +100%        |
| CPU                 | 160 MHz (1 core)| 240 MHz (2 cores)| +50% clock   |
| WiFi                | 802.11 b/g/n    | 802.11 b/g/n     | Igual        |
| GPIO                | 22              | 30+              | +36%         |
| ADC                 | 6 canales       | 20 canales       | +233%        |
| Pinout              | Compatible XIAO | Compatible XIAO  | ✅ Drop-in   |
| Precio              | ~$5             | ~$7              | +40%         |
| **Recomendación**   | ⚠️ Ajustado     | ✅ Ideal         | **MIGRAR**   |

---

**Última actualización:** 25 Noviembre 2025  
**Próxima revisión:** Tras implementar monitoring y optimizaciones nivel 1
