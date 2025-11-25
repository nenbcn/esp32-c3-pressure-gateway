# TODO - ESP32-C3 Pressure Gateway

> **Workflow:** Ver `.github/copilot-instructions.md` sección "Workflow de Tareas y Issues"

---

## 🔴 Pendiente - Alta Prioridad

### Alineación Arquitectónica con mica-gateway
- [ ] Eliminar pre-suspension de tareas en creación (system_state.cpp)
- [ ] Activar button task en CONNECTING state (no suspender)
- [ ] Activar pressure reader en CONNECTING state (no suspender)
- [ ] Simplificar MQTT reconnection (eliminar rate limiting del loop)

### Logging y Debugging
- [ ] Reducir CORE_DEBUG_LEVEL de 4 a 3 en platformio.ini (silenciar Wire.cpp errors)
- [ ] Verificar que I2C state-based logging funciona correctamente

---

## 🟡 Pendiente - Media Prioridad

### Optimización
- [ ] Revisar si se puede re-añadir Vector.h sin impacto en memoria
- [ ] Documentar trade-offs de AsyncWebServer pointer vs global

### Testing
- [ ] Pruebas con sensor I2C conectado (verificar recovery y logging)
- [ ] Pruebas de WiFi config mode (AP + web server + AsyncWebServer cleanup)
- [ ] Pruebas de OTA desde MQTT

---

## 🟢 Pendiente - Baja Prioridad

### Documentación
- [ ] Actualizar gateway_specs.md con cambios de pressure system
- [ ] Revisar comentarios de código para claridad

---

## ✅ Completado

### 2025-11-25
- ✅ Optimización de memoria: MAX_SAMPLES_PER_EVENT 300→100 (33KB liberados)
- ✅ AsyncWebServer cambiado a pointer para evitar fragmentación ESP32-C3
- ✅ Stack sizes alineados con mica-gateway (WiFi: 4096, MQTT: 10000, etc)
- ✅ I2C logging optimizado: solo transiciones de estado, no cada error
- ✅ Tareas suspendidas desde creación para estabilidad en boot
- ✅ Safety check en messageFormatterTask: solo procesa si MQTT conectado
- ✅ Revertido: OTA validation code de main.cpp (mantener consistencia con mica-gateway)
- ✅ Documentación: ARCHITECTURAL_DIFFERENCES.md creado con análisis completo

---

### **📝 PASO 2B: Implementar Arquitectura de 3 Colas**
**Archivos:** `src/pressure_telemetry.h`, `src/pressure_telemetry.cpp`, `src/system_state.cpp`
**Descripción:** Implementar arquitectura de 3 colas para separación de responsabilidades

**Cambios:**
- **Cola 1:** `g_pressureQueue` (ya existe) - Raw readings
- **Cola 2:** `g_pressureEventQueue` (nueva) - Eventos detectados
- **Cola 3:** `g_mqttQueue` (ya existe) - Mensajes JSON
- **Nueva tarea:** `messageFormatterTask` para formateo JSON
- Actualizar `pressureTelemetryTask` para producir eventos
- Actualizar gestión de tareas en `system_state.cpp`

**Flujo:**
```
PressureReader → g_pressureQueue → PressureTelemetry → g_pressureEventQueue → MessageFormatter → g_mqttQueue → MQTT
```

**Ventajas:**
- Separación clara de responsabilidades (señal vs formato vs conectividad)
- Tolerancia a fallos granular
- Escalabilidad para múltiples formatos de salida
- Debugging específico por capa

**Resultado:** Arquitectura modular con 3 colas funcionando

---

### **📝 PASO 3: Implementar Cálculo de Derivada**
**Archivo:** `src/pressure_telemetry.cpp`
**Descripción:** Añadir ventana deslizante y cálculo de derivada

**Cambios:**
- Buffer circular para ventana de derivada (50 muestras)
- Función `calculateDerivative()` entre extremos de ventana
- Filtrado EPA de la derivada calculada
- Manejo de ventana no llena al inicio

**Resultado:** Cálculo de derivada funcionando con ventana de 0.5s

---

### **📝 PASO 4: Implementar Estados Locales de Señal**
**Archivo:** `src/pressure_telemetry.cpp`
**Descripción:** Máquina de estados local para detectar estable/cambio

**Cambios:**
- Estados locales: `SIGNAL_STABLE`, `SIGNAL_CHANGING`, `SIGNAL_POST_EVENT`
- Función `processSignalStateMachine()` 
- Transiciones basadas en umbral de derivada + histéresis
- Sin interferir con `system_state` global

**Resultado:** Detección inteligente de cambios basada en derivada

---

### **📝 PASO 5: Implementar Gestión de Intervalos Completos**
**Archivo:** `src/pressure_telemetry.cpp`
**Descripción:** Crear intervalos adaptativos según especificaciones

**Cambios:**
- Intervalos estables: solo presión promedio + timestamps
- Intervalos de cambio: todas las muestras [timestamp, valor]
- Gestión de memoria para arrays dinámicos
- Períodos pre-evento y post-evento

**Resultado:** Intervalos adaptativos que optimizan ancho de banda

---

### **📝 PASO 6: Implementar Tarea de Formateo JSON**
**Archivo:** `src/message_formatter.cpp` (nuevo) + `src/pressure_telemetry.cpp`
**Descripción:** Crear tarea dedicada al formateo JSON y batching

**Cambios:**
- **Nuevo archivo:** `src/message_formatter.cpp/h`
- Tarea `messageFormatterTask()` que lee `g_pressureEventQueue`
- Agrupa eventos en lotes óptimos
- JSON diferenciado para intervalos estables vs cambio
- Campo `type`: "stable" o "change"
- Campo `samples` solo en intervalos de cambio
- Incluir `triggerReason` en cambios
- Optimización de tamaño JSON y compresión

**Resultado:** Formateo JSON optimizado según tipo de evento en tarea dedicada

---

### **📝 PASO 7: Optimizar para Producción**
**Archivo:** `src/pressure_reader.cpp` y `src/main.cpp`
**Descripción:** Desactivar debug mode y optimizar rendimiento

**Cambios:**
- Cambiar mensaje de "Smart City Mode" a "Production Mode"
- Asegurar que `DEBUG_MODE` esté deshabilitado por defecto
- Optimizar gestión de memoria para 100Hz
- Revisar stack sizes para mayor carga

**Resultado:** Firmware optimizado para producción

---

### **📝 PASO 8: Validación y Testing**
**Archivo:** Configuración general
**Descripción:** Verificar que el sistema funciona correctamente a 100Hz

**Cambios:**
- Verificar que las colas manejan 100Hz sin desbordarse
- Confirmar timing preciso de muestreo
- Probar detección de cambios y intervalos
- Validar formato JSON de salida

**Resultado:** Sistema funcionando según especificaciones completas

---

## **⚡ NOTAS IMPORTANTES**

### **🔧 Orden de Implementación:**
- Los pasos 1-2 son **preparatorios** (parámetros y estructuras)
- El paso 2B es **arquitectural** (implementar 3 colas)
- Los pasos 3-5 son **lógica principal** (algoritmo de derivada)
- Los pasos 6-8 son **formateo** y optimización

### **🛡️ Riesgos y Precauciones:**
- **Paso 2B:** Gestión cuidadosa de 3 tareas concurrentes
- **Paso 3:** Cuidado con overflow en cálculo de derivada
- **Paso 5:** Gestión de memoria para arrays de muestras
- **Paso 6:** Optimización JSON para no saturar ancho de banda
- **Paso 7:** Verificar que 100Hz no sature el sistema

### **📊 Validación por Paso:**
Cada paso debe probarse independientemente antes de continuar al siguiente.
**Especial atención:** Monitorear uso de colas tras implementar paso 2B.

---

**¿Continuamos con el PASO 2B (Implementar Arquitectura de 3 Colas)?**

---

## **📊 PROGRESO ACTUAL**

✅ **PASO 1:** Actualizar Parámetros Base - **COMPLETADO**
🔄 **PASO 2:** Implementar Estructuras de Datos - **COMPLETADO**
⏳ **PASO 2B:** Implementar Arquitectura de 3 Colas - **PRÓXIMO**
⏸️ **PASO 3:** Implementar Cálculo de Derivada - **PENDIENTE**
⏸️ **PASO 4:** Implementar Estados Locales de Señal - **PENDIENTE**  
⏸️ **PASO 5:** Implementar Gestión de Intervalos Completos - **PENDIENTE**
⏸️ **PASO 6:** Implementar Tarea de Formateo JSON - **PENDIENTE**
⏸️ **PASO 7:** Optimizar para Producción - **PENDIENTE**
⏸️ **PASO 8:** Validación y Testing - **PENDIENTE**