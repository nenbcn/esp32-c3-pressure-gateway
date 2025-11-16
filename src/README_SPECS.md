# 📚 Documentación de Especificaciones

## 📋 Estructura de Documentos

Este directorio contiene las especificaciones técnicas del firmware y su implementación.

### **📄 `gateway_specs.md`**
**Especificación de arquitectura genérica del gateway**

**Implementado por:**
- `system_state.h/cpp` - Gestión de estados y coordinación
- `wifi_connect.h/cpp` - Conexión WiFi
- `wifi_config_mode.h/cpp` - Modo configuración
- `mqtt_handler.h/cpp` - Cliente MQTT
- `led_manager.h/cpp` - Control de LEDs
- `button_manager.h/cpp` - Gestión de botones
- `eeprom_config.h/cpp` - Persistencia
- `device_id.h/cpp` - Identificación
- `ota_manager.h/cpp` - Actualizaciones OTA
- `board.h` - Pines hardware
- `config.h` - Configuración general
- `secrets.h` - Credenciales

---

### **📄 `pressure_signal_specs.md`**
**Especificación del pipeline de procesamiento de señal de presión**

**Implementado por:**
- `pressure_reader.h/cpp` - Lectura I2C 100Hz
- `pressure_telemetry.h/cpp` - Filtrado EPA + detección eventos
- `message_formatter.h/cpp` - Serialización JSON + batching
- `signal_parameters.h` - Parámetros configurables
- `data_types.h/cpp` - Estructuras y colas

---

### **📄 `signal_parameters.h`**
**Parámetros numéricos del algoritmo**

Define todos los valores configurables:
- Frecuencia de muestreo
- Coeficientes de filtros EPA
- Umbrales de derivada
- Tamaños de colas
- Duraciones de eventos

---

### **📄 `data_types.h/cpp`**
**Tipos de datos compartidos**

Define:
- Estructuras: `PressureReading`, `PressureEvent`, `MqttMessage`
- Enums: `EventType`, `TriggerReason`, `SignalState`
- Colas globales: `g_pressureQueue`, `g_pressureEventQueue`, `g_telemetryQueue`
- Mutexes globales: `g_stateMutex`, `g_i2cMutex`, `g_wifiMutex`, `g_mqttMutex`

---

## 🔄 Relación entre Documentos

```
gateway_specs.md (genérico)
    ├── Estados del sistema
    ├── WiFi, MQTT, LED, Button
    └── System State coordination
    
pressure_signal_specs.md (específico)
    ├── Pipeline: Reader → Telemetry → Formatter
    ├── Algoritmo: RAW → EPA → Derivada → Eventos
    └── 3 Colas: pressure → events → mqtt

signal_parameters.h
    └── Valores numéricos configurables
    
data_types.h
    └── Estructuras compartidas por ambos
```

---

## 📖 Guía de Uso

### **Para entender la arquitectura general:**
→ Lee `gateway_specs.md`

### **Para entender el procesamiento de presión:**
→ Lee `pressure_signal_specs.md`

### **Para modificar parámetros del algoritmo:**
→ Edita `signal_parameters.h`

### **Para modificar estructuras de datos:**
→ Edita `data_types.h`

---

## 🔗 Documentos Adicionales

- **Reglas de trabajo y arquitectura general:** `../.github/copilot-instructions.md`
