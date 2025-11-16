# **📌 Especificaciones del Gateway ESP32**

> **⚠️ NOTA:** Este es el archivo de referencia. La versión de producción está en `src/gateway_specs.md`

> **🔗 Documentos Relacionados:**
> - **Procesamiento de señal de presión:** `../../src/pressure_signal_specs.md` (arquitectura de colas, algoritmo, estructuras de datos)
> - **Parámetros configurables:** `../../src/signal_parameters.h`
> - **Reglas de trabajo:** `../../.github/copilot-instructions.md`

---

## **📂 Archivos de Implementación en `/src`**

**Este documento especifica la arquitectura genérica del gateway. Los siguientes archivos implementan estas especificaciones:**

### **Sistema y Coordinación**
- `system_state.h/cpp` - Gestión de estados globales y coordinación de tareas
- `main.cpp` - Punto de entrada y configuración inicial

### **Comunicación y Red**
- `wifi_connect.h/cpp` - Conexión WiFi con credenciales de EEPROM
- `wifi_config_mode.h/cpp` - Modo configuración WiFi (AP + web server)
- `mqtt_handler.h/cpp` - Cliente MQTT genérico para AWS IoT

### **Interfaz de Usuario**
- `led_manager.h/cpp` - Control de LEDs según estado del sistema
- `button_manager.h/cpp` - Gestión de botones con ISR y detección de pulsación larga

### **Configuración y Persistencia**
- `eeprom_config.h/cpp` - Lectura/escritura de parámetros en EEPROM
- `config.h` - Definiciones de configuración general
- `secrets.h` - Certificados X.509 y credenciales AWS IoT

### **Utilidades**
- `device_id.h/cpp` - Generación de ID único desde MAC
- `ota_manager.h/cpp` - Gestión de actualizaciones OTA
- `board.h` - Definición de pines del hardware

### **Arquitectura Compartida**
- `data_types.h/cpp` - Estructuras de datos, colas y mutexes globales
- `includes.h` - Headers comunes del proyecto

> **📋 Nota:** Para módulos específicos de procesamiento de presión (Pressure Reader, Telemetry, Message Formatter), ver `../../src/pressure_signal_specs.md`

---

## **🔧 Configuración del Hardware**

### **Microcontrolador Base**
- **Seeeduino XIAO ESP32-C3**
- Wi-Fi integrado, bajo consumo (deep sleep ≈ 5 µA)
- Cargador Li-ion integrado (JST 1.25 mm)
- Alimentación: USB-C o powerbank

### **Mapeo de Pines del Shield**

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
| Relay CTRL (5 V high‑side) | D8    | GPIO8        | PMOS high‑side OFF=HIGH, ON=LOW → 5 V out                 | ⚠ strapping          |
| 1‑WIRE TEMP (DS18B20)      | D0    | GPIO2 (A0)   | Digital con pull‑up                                       | ⚠ strapping          |
| Botón usuario (int+ext)    | D9    | GPIO9        | Entrada pull‑up, N.O. a GND                               | ⚠ strapping          |

### **Sensor de Presión**
- **Modelo:** WNK80MA (I2C, 3.3V)
- **Dirección I2C:** `0x6D` (hex, fija)
- **Especificaciones completas:** Ver `src/pressure_signal_specs.md`

---

## **1️⃣ Enfoque de la Gestión del Sistema**
- ✅ **Solo `system_state.cpp` puede modificar el estado del sistema.**
- ✅ **Las tareas se activan y desactivan en `system_state.cpp`, NO dentro de sus módulos.**
- ✅ **Cada módulo debe ser independiente y contener sus funciones de inicialización, ejecución y finalización.**
- ✅ **Las tareas comunican eventos a `system_state` mediante notificaciones (`notifySystemState`).**
- ✅ **Las tareas pueden leer variables globales pero NO modificar el estado del sistema directamente.**

---

## **2️⃣ Estados del Sistema**
| Estado | Descripción |
|--------|------------|
| `SYSTEM_STATE_CONNECTING` | Intentando conectarse a WiFi o MQTT. |
| `SYSTEM_STATE_CONNECTED_WIFI` | Conexión WiFi establecida. |
| `SYSTEM_STATE_CONNECTED_MQTT` | Conexión WiFi y MQTT activa. |
| `SYSTEM_STATE_CONFIG_MODE` | Modo de configuración WiFi. |
| `SYSTEM_STATE_WAITING_BUTTON_RELEASE` | El botón fue presionado, esperando liberación. |
| `SYSTEM_STATE_ERROR` | Error general. |

---

## **3️⃣ Eventos de Notificación**
Estos eventos permiten la comunicación entre módulos y el **`system_state`**.

### **🔹 Generales**
- `EVENT_LONG_PRESS_BUTTON`: Botón presionado más de 5 segundos.

### **🔹 Wi-Fi**
- `EVENT_WIFI_CONNECTED`: Conexión WiFi exitosa.
- `EVENT_WIFI_FAIL_CONNECT`: Falla en la conexión WiFi.
- `EVENT_NO_PARAMETERS_EEPROM`: Parámetros de conexión no encontrados en EEPROM.

### **🔹 Modo Configuración Wi-Fi**
- `EVENT_WIFI_CONFIG_STARTED`: Inicio del modo de configuración WiFi.
- `EVENT_WIFI_CONFIG_SAVED`: Configuración WiFi guardada exitosamente.
- `EVENT_WIFI_CONFIG_FAILED`: Error al guardar la configuración WiFi.
- `EVENT_WIFI_CONFIG_STOPPED`: Modo de configuración WiFi desactivado.

### **🔹 Módulo LoRa**
- `EVENT_LORA_DATA_RECEIVED`: Datos LoRa recibidos correctamente.
- `EVENT_LORA_QUEUE_FULL`: Cola LoRa llena.
- `EVENT_LORA_ERROR`: Error en la inicialización de LoRa.

### **🔹 MQTT**
- `EVENT_MQTT_CONNECTED`: Conexión a MQTT exitosa.
- `EVENT_MQTT_DISCONNECTED`: Desconexión del broker MQTT.

---

## **4️⃣ Variables Globales**
- `g_systemState`: Representa el estado global del sistema.
  - **Tipo:** `SystemState`
  - **Acceso Protegido por:** `g_stateMutex`.
- `g_stateMutex`: Mutex para proteger el acceso y las modificaciones al estado global.
  - **Tipo:** `SemaphoreHandle_t`.
- `g_logMessageQueue`: Cola para mensajes de log.
  - **Tipo:** `QueueHandle_t`.

---

## **5️⃣ Funciones Públicas de `system_state`**
- `initializeSystemState()`: Inicializa el estado del sistema y los recursos asociados.
- `setSystemState(SystemState state)`: Cambia el estado global del sistema.
- `getSystemState()`: Obtiene el estado actual del sistema.
- `stateManagementTask(void *pvParameters)`: Tarea FreeRTOS para gestionar el estado del sistema.
- `notifySystemState(TaskNotificationEvent event)`: Envía un evento al sistema.
- `logMessage(LogLevel level, const char *format, ...)`: Registra un mensaje en el sistema de logs.

---

## **6️⃣ Estándares de Programación**
- **Idioma:** Todo en inglés.
- **Nombres de Variables:**
  - Locales: `camelCase`.
  - Globales: Prefijo `g_`.
  - Constantes: `UPPER_CASE_WITH_UNDERSCORES`.
- **Nombres de Funciones:**
  - `camelCase`.
  - Deben describir claramente la acción que realizan.
- **Comentarios:**
  - Cada función debe incluir comentarios explicativos, incluyendo parámetros y valores de retorno.
- **Depuración:**
  - Usar un macro activable/desactivable para depuración: `#define DEBUG_MODE`.
  - Los mensajes de depuración solo estarán activos si `DEBUG_MODE` está definido.
- **Consistencia:**
  - Código modular, organizado por archivos específicos según funcionalidad.
  - Separación clara entre lógica del programa y procesos de FreeRTOS.

---

## **7️⃣ Reglas de Activación de Tareas**

| **Tarea**            | **Estados Activos**                 | **Estados Inactivos**             |
|----------------------|---------------------------------|----------------------------------|
| **WiFi Connect**    | CONNECTING, CONNECTED_WIFI, CONNECTED_MQTT | CONFIG_MODE, ERROR               |
| **WiFi Config Mode** | CONFIG_MODE                     | CONNECTING, CONNECTED_WIFI, CONNECTED_MQTT, ERROR |
| **MQTT Handler**    | CONNECTED_MQTT                  | CONNECTING, CONNECTED_WIFI, CONFIG_MODE, ERROR |
| **Pressure Reader** | Siempre activa                   | -                                |
| **LED Manager**     | Siempre activa                   | -                                |
| **Button Handler**  | WAITING_BUTTON_RELEASE           | CONNECTING, CONNECTED_WIFI, CONFIG_MODE, CONNECTED_MQTT, ERROR |
| **Log Task**        | Siempre activa                   | -                                |

> **📋 Nota:** Para tareas específicas de procesamiento de señal de presión (Pressure Reader, Pressure Telemetry, Message Formatter), consultar `src/pressure_signal_specs.md`

---

## **8️⃣ Módulos del Sistema

## Módulo: Wi-Fi Connect

### Descripción
El módulo **Wi-Fi Connect** gestiona la conexión del dispositivo a una red Wi-Fi utilizando credenciales almacenadas en EEPROM. Si la conexión falla, notifica el estado al sistema y reintenta según un intervalo establecido.

### Archivos Asociados
- `wifi_connect.h`
- `wifi_connect.cpp`

---

### Responsabilidades
- Inicializar el hardware Wi-Fi en modo estación.
- Intentar la conexión a Wi-Fi usando las credenciales almacenadas.
- Manejar el estado de conexión y notificar al sistema de los resultados (éxito o fallo).
- Reintentar conexiones en caso de fallo con un intervalo definido.

---

### Funciones Principales

#### **`initializeWiFiConnection`**
- **Propósito:** Configura el hardware Wi-Fi en modo estación (`WIFI_STA`) e inicializa el mutex para garantizar el acceso seguro al módulo.
- **Detalles:**
  - Activa el modo estación.
  - Crea un mutex para proteger las operaciones del módulo.
  - Registra en los logs el estado de inicialización.
- **Errores Gestionados:**
  - Si no se puede crear el mutex, el sistema entra en un bucle infinito con un mensaje de error.

#### **`wifiConnectTask`**
- **Propósito:** Gestiona la conexión a Wi-Fi de manera recurrente en una tarea de FreeRTOS.
- **Detalles:**
  - Verifica si el dispositivo ya está conectado antes de intentar una nueva conexión.
  - Carga las credenciales desde EEPROM:
    - Si no encuentra credenciales válidas, notifica `EVENT_NO_PARAMETERS_EEPROM`.
    - Si las credenciales son válidas, intenta conectar con un tiempo de espera (`timeout`) de 10 segundos.
  - Notifica al sistema según el resultado:
    - `EVENT_WIFI_CONNECTED` si la conexión es exitosa.
    - `EVENT_WIFI_FAIL_CONNECT` si la conexión falla.
  - Reintenta cada 5 segundos en caso de fallo.
- **Errores Gestionados:**
  - Credenciales no válidas o inexistentes.
  - Fallo al conectar dentro del tiempo límite.

---

### Notificaciones Emitidas
- **`EVENT_WIFI_CONNECTED`**: Conexión exitosa a Wi-Fi.
- **`EVENT_WIFI_FAIL_CONNECT`**: Fallo al conectar a Wi-Fi.
- **`EVENT_NO_PARAMETERS_EEPROM`**: Credenciales no encontradas en EEPROM.

---

### Requisitos y Limitaciones
- **Dependencias:**
  - `system_state.h`: Para la gestión de logs y notificaciones al sistema.
  - `eeprom_config.h`: Para cargar credenciales desde EEPROM.
- **Limitaciones:**
  - El intervalo de reintento está fijo en 5 segundos.
  - El tiempo de espera para la conexión (`timeout`) es de 10 segundos.
  - No implementa estrategias avanzadas de reconexión (e.g., uso de múltiples SSIDs).

---

### Consideraciones
1. **Logs:** Registra mensajes de depuración e información para facilitar la identificación de problemas.
2. **Modularidad:** El diseño permite separar la lógica de conexión y su manejo en FreeRTOS.
3. **Eficiencia:** Suspende la tarea durante períodos de inactividad para ahorrar recursos.



## Módulo: Wi-Fi Config Mode

### Descripción
El módulo **Wi-Fi Config Mode** permite configurar credenciales de Wi-Fi mediante un punto de acceso (AP) y un servidor web. El usuario puede conectarse al AP, acceder a una interfaz web, y enviar las credenciales, que luego se guardan en EEPROM. Este módulo se activa cuando el sistema está en el estado `SYSTEM_STATE_CONFIG_MODE`.

### Archivos Asociados
- `wifi_config_mode.h`
- `wifi_config_mode.cpp`

---

### Responsabilidades
- Configurar el ESP32 como un punto de acceso con SSID y contraseña predefinidos.
- Habilitar un servidor web para capturar credenciales Wi-Fi.
- Validar las credenciales ingresadas por el usuario y almacenarlas en EEPROM.
- Notificar al sistema sobre el éxito o fallo del proceso de configuración.
- Reiniciar el dispositivo tras completar la configuración.

---

### Funciones Principales

#### **`initializeWiFiConfigMode`**
- **Propósito:** Configura el ESP32 como punto de acceso y prepara el servidor web.
- **Detalles:**
  - Crea un punto de acceso con SSID `MICA-Gateway` y contraseña `12345678`.
  - Inicia un servidor web en el puerto 80.
  - Configura rutas para:
    - Mostrar la página web para ingresar credenciales (`/`).
    - Guardar las credenciales ingresadas (`/save`).
  - Notifica al sistema sobre el inicio o fallo del modo de configuración.
- **Errores Gestionados:**
  - Fallo al iniciar el AP.
  - Fallo en el escaneo de redes Wi-Fi disponibles.

#### **`deactivateWiFiConfigMode`**
- **Propósito:** Desactiva el punto de acceso y detiene el servidor web.
- **Detalles:**
  - Finaliza el servidor web.
  - Desactiva el AP configurado.
  - Reinicia el dispositivo para aplicar los cambios.
  - Notifica al sistema sobre la desactivación del modo de configuración.

#### **`wifiConfigModeTask`**
- **Propósito:** Monitorea el estado del sistema y mantiene activo el servidor mientras el sistema está en `SYSTEM_STATE_CONFIG_MODE`.
- **Detalles:**
  - Revisa periódicamente si el estado sigue siendo `SYSTEM_STATE_CONFIG_MODE`.
  - Suspende la tarea si el estado cambia.

#### **`generateWiFiOptions`**
- **Propósito:** Genera una lista de redes Wi-Fi disponibles en formato HTML.
- **Detalles:**
  - Realiza un escaneo de redes Wi-Fi.
  - Devuelve opciones HTML con los nombres de las redes detectadas.
- **Errores Gestionados:**
  - Caso en el que no se detectan redes Wi-Fi.

---

### Notificaciones Emitidas
- **`EVENT_WIFI_CONFIG_STARTED`**: Inicio exitoso del modo de configuración.
- **`EVENT_WIFI_CONFIG_FAILED`**: Fallo al iniciar el AP, guardar credenciales o escanear redes.
- **`EVENT_WIFI_CONFIG_SAVED`**: Credenciales guardadas exitosamente en EEPROM.
- **`EVENT_WIFI_CONFIG_STOPPED`**: Desactivación del modo de configuración.

---

### Requisitos y Limitaciones
- **Dependencias:**
  - `system_state.h`: Para manejar los estados y notificaciones.
  - `eeprom_config.h`: Para guardar credenciales en EEPROM.
- **Limitaciones:**
  - La contraseña del AP es fija y no se puede cambiar en esta implementación.
  - El reinicio del dispositivo es obligatorio tras guardar credenciales.
  - No implementa mecanismos de recuperación ante fallos del servidor web.

---

### Consideraciones
1. **Logs:** Registra mensajes detallados para facilitar el diagnóstico de problemas durante el modo de configuración.
2. **Interfaz Web:** La página web tiene una interfaz básica, pero funcional, que podría mejorarse para ofrecer más opciones o un diseño responsivo.
3. **Reinicio Obligatorio:** Aunque el reinicio garantiza la aplicación de los cambios, puede no ser ideal en entornos críticos. Podría explorarse una solución para evitar reinicios innecesarios.


### **3. LoRa Receiver**
Manages communication with LoRa sensors. Currently, it simulates LoRa data by reading from a serial port.

- **Files:**
  - `lora_receiver.h`
  - `lora_receiver.cpp`

- **Responsibilities:**
  - Receive data packets from sensors (simulated using Serial2).
  - Validate data integrity.
  - Store valid data in a LORA  queue for processing.

#### **Key Functions:**
- `initializeLoRaReceiver`
  - Configures the LoRa hardware (simulated) and creates a queue for storing incoming data.
- `loRaReceiverTask`
  - A FreeRTOS task to handle incoming LoRa data and send the register to the LORA queue.

- **Activation:**
  - Always active.
  - Not controlled by the central state management task.

---

### **4. MQTT Handler**
Handles communication with the backend via MQTT.

- **Files:**
  - `mqtt_handler.h`
  - `mqtt_handler.cpp`

- **Responsibilities:**
  - Connect to the MQTT broker.
  - Publish sensor data from the LORA Queue to relevant topics.
  - Handle acknowledgments and reconnections.
  - Optionally handle incoming MQTT messages via a callback.

#### **Key Functions:**
- `initializeMQTTHandler`
  - Configures the MQTT client and assigns a callback for incoming messages.
- `mqttPublishTask`
  - A FreeRTOS task to manage MQTT communication publising the data on the LORA queue on the relevant topic.
- `mqttMessageCallback`
  - Handles incoming MQTT messages.

- **Activation:**
  - Active only when the system state is `SYSTEM_STATE_MQTT`.
  - Suspended by the **central state management task** in all other states.

---

### **5. LED Manager**
Manages visual feedback through LEDs based on the current system state.

- **Files:**
  - `led_manager.h`
  - `led_manager.cpp`

- **Responsibilities:**
  - Reflect the current system state via distinct LED patterns:
    - **Connecting (`SYSTEM_STATE_CONNECTING`):** Red LED blinks slowly.
    - **Connected (`SYSTEM_STATE_CONNECTED`):** Green LED blinks slow.
    - **MQTT Error (`SYSTEM_STATE_MQTT`):** Green LED stays on.
    - **Error (`SYSTEM_STATE_ERROR`):** Red LED stays on.
    - **Configuration Mode (`SYSTEM_STATE_CONFIG_MODE`):** Green LED blinks fast.
  - Continuously monitor the system state and adjust LEDs dynamically.

#### **Key Functions:**
- `initializeLedManager`
  - Configures LED pins as outputs and sets initial states.
- `ledTask`
  - A FreeRTOS task that monitors the system state and adjusts LED patterns accordingly.

- **Activation:**
  - Always active.
  - Not controlled by the central state management task.

---

### **6. Módulo: System State Management**

#### **Descripción**
El módulo de gestión de estados del sistema es responsable de:
- Monitorear y gestionar los cambios en el estado global del sistema.
- Coordinar las acciones necesarias según el estado actual.
- Integrar notificaciones y revisiones periódicas para asegurar que el sistema mantenga un estado coherente.
- Activar o desactivar tareas asociadas a cada estado del sistema.
- en este modulo se declaran todas la variables, semaforos y colas globale


- **Files:**
  - `system_state.h`
  - `system_state.cpp`

---

#### **Arquitectura**
El módulo se compone de las siguientes funciones principales:

##### **1. `handleStateTransitions()`**
- **Propósito:** Gestionar los cambios en el estado global del sistema.
- **Lógica:**
  1. **Revisión de Notificaciones:** Espera notificaciones en la cola para cambios de estado.
     - Ejemplo de notificaciones:
       - `EVENT_BUTTON_LONG_PRESS`: Cambiar a `SYSTEM_STATE_CONFIG_MODE`.
       - `EVENT_WIFI_CONNECTED`: Cambiar a `SYSTEM_STATE_CONNECTED`.
       - `EVENT_MQTT_CONNECTED`: Mantener `SYSTEM_STATE_CONNECTED`.
  2. **Revisión Periódica:** Si no hay notificaciones en el tiempo definido, revisa las condiciones actuales del sistema:
     - Estado de conexión Wi-Fi.
     - Estado de conexión MQTT.
     - Otros indicadores relevantes.
  3. Determina si el estado debe cambiar según los parámetros revisados.

##### **2. `handleStateActions()`**
- **Propósito:** Ejecutar las acciones asociadas al estado global actual.
- **Lógica:**
  - Activa o desactiva tareas según el estado del sistema:
    - `SYSTEM_STATE_CONNECTING`: Activa Wi-Fi y MQTT, desactiva LoRa.
    - `SYSTEM_STATE_CONNECTED`: Mantiene Wi-Fi y MQTT activos, habilita LoRa.
    - `SYSTEM_STATE_MQTT_ERROR`: Mantiene Wi-Fi activo, intenta reconectar MQTT.
    - `SYSTEM_STATE_CONFIG_MODE`: Activa el servidor web y el punto de acceso, desactiva LoRa.
    - `SYSTEM_STATE_ERROR`: Suspende todas las tareas no esenciales.

##### **3. `stateManagementTask()`**
- **Propósito:** Ejecutar la lógica completa de gestión del estado.
- **Lógica:**
  - Llama a `handleStateTransitions()` para gestionar los cambios de estado.
  - Llama a `handleStateActions()` para ejecutar las tareas asociadas al estado actual.

---

#### **Estado de Actividad**
- La tarea `stateManagementTask` está **siempre activa** para garantizar la correcta supervisión y coordinación de los estados.




## Módulo: LoRa Receiver

### Descripción
El módulo **LoRa Receiver** simula la recepción de datos desde un dispositivo LoRa utilizando el puerto serie. Los datos recibidos se validan y se almacenan en una cola de FreeRTOS para ser procesados por otros módulos.

### Archivos Asociados
- `lora_receiver.h`
- `lora_receiver.cpp`

---

### Responsabilidades
- Configurar el puerto serie simulado para recibir datos LoRa.
- Leer y validar datos desde el puerto serie.
- Almacenar los datos recibidos en una cola de FreeRTOS.
- Notificar al sistema sobre los eventos relacionados con la recepción de datos.

---

### Funciones Principales

#### **`initializeLoRaReceiver`**
- **Propósito:** Configura el puerto serie para la simulación de LoRa y crea una cola para almacenar los datos recibidos.
- **Detalles:**
  - Configura `Serial2` a 9600 baudios.
  - Crea una cola (`loraQueue`) con un tamaño máximo de 10 elementos y 128 bytes por elemento.
  - Registra en los logs el estado de inicialización.
  - Notifica errores al sistema en caso de fallos en la inicialización del puerto o la cola.
- **Errores Gestionados:**
  - Fallo en la inicialización del puerto serie.
  - Fallo al crear la cola.

#### **`loraReceiverTask`**
- **Propósito:** Lee datos desde el puerto serie y los envía a la cola de FreeRTOS.
- **Detalles:**
  - Lee datos del puerto serie (`Serial2`) hasta encontrar un carácter de nueva línea (`\n`).
  - Valida que los datos no estén vacíos antes de enviarlos a la cola.
  - En caso de éxito, envía los datos a la cola y notifica con `EVENT_LORA_DATA_RECEIVED`.
  - Si la cola está llena, descarta los datos y notifica con `EVENT_LORA_QUEUE_FULL`.
  - Registra en los logs el contenido recibido o cualquier error relacionado.
- **Errores Gestionados:**
  - Datos vacíos recibidos desde LoRa.
  - Cola llena al intentar almacenar datos.

---

### Notificaciones Emitidas
- **`EVENT_LORA_DATA_RECEIVED`**: Datos recibidos correctamente y almacenados en la cola.
- **`EVENT_LORA_QUEUE_FULL`**: La cola está llena y los datos se descartaron.
- **`EVENT_LORA_ERROR`**: Error en la inicialización del puerto o la cola.

---

### Requisitos y Limitaciones
- **Dependencias:**
  - `system_state.h`: Para manejar los estados y notificaciones.
- **Limitaciones:**
  - El puerto serie utilizado (`Serial2`) debe estar disponible y correctamente configurado en el hardware.
  - Los datos están limitados a un tamaño de 128 bytes por mensaje.
  - La cola puede almacenar un máximo de 10 mensajes simultáneamente.

---

### Consideraciones
1. **Logs:** Los mensajes de depuración son útiles para monitorear el flujo de datos y diagnosticar problemas en tiempo real.
2. **Simulación:** Este módulo utiliza un puerto serie para simular LoRa, pero podría reemplazarse con un controlador LoRa real en futuras implementaciones.
3. **Optimización:** El tamaño de la cola y los mensajes podría ajustarse para adaptarse a diferentes aplicaciones.
---

## Módulo: MQTT Handler

### Descripción
El módulo **MQTT Handler** gestiona la comunicación con un broker MQTT. Publica datos recibidos desde la cola LoRa en un tópico específico y maneja mensajes entrantes a través de un callback.

### Archivos Asociados
- `mqtt_handler.h`
- `mqtt_handler.cpp`

---

### Responsabilidades
- Conectar al broker MQTT y mantener la conexión activa.
- Publicar datos desde la cola LoRa en un tópico MQTT.
- Manejar mensajes entrantes a través de un callback.
- Notificar al sistema sobre cambios en el estado de la conexión MQTT.

---

### Funciones Principales

#### **`initializeMQTTHandler`**
- **Propósito:** Configura el cliente MQTT, asigna el servidor del broker y asocia el callback para mensajes entrantes.
- **Detalles:**
  - Asigna el servidor MQTT (`MQTT_BROKER`) y el puerto (`MQTT_PORT`).
  - Registra un callback (`mqttMessageCallback`) para manejar mensajes entrantes.
  - Registra en los logs el estado de inicialización.

#### **`connectToMQTT`**
- **Propósito:** Intenta conectar al broker MQTT con un máximo de 5 reintentos.
- **Detalles:**
  - Registra en los logs cada intento de conexión.
  - Notifica `EVENT_MQTT_CONNECTED` al conectar exitosamente.
  - Notifica `EVENT_MQTT_DISCONNECTED` tras fallar todos los intentos.
  - Incluye un retardo de 5 segundos entre intentos.

#### **`mqttMessageCallback`**
- **Propósito:** Procesa mensajes entrantes desde el broker MQTT.
- **Detalles:**
  - Registra en los logs el contenido del mensaje recibido.
  - No realiza procesamiento adicional en la implementación actual.

#### **`mqttPublishTask`**
- **Propósito:** Publica datos desde la cola LoRa en el tópico MQTT.
- **Detalles:**
  - Verifica si el sistema está en estado `SYSTEM_STATE_CONNECTED_MQTT`.
  - Si no está conectado al broker, intenta reconectar.
  - Consume datos de la cola LoRa (`loraQueue`) y los publica en el tópico `MQTT_TOPIC`.
  - Notifica `EVENT_MQTT_DISCONNECTED` si ocurre un fallo al publicar.
  - Cambia el estado del sistema a `SYSTEM_STATE_ERROR` en caso de errores graves.

---

### Notificaciones Emitidas
- **`EVENT_MQTT_CONNECTED`**: Conexión exitosa al broker MQTT.
- **`EVENT_MQTT_DISCONNECTED`**: Desconexión del broker o fallo al publicar un mensaje.

---

### Requisitos y Limitaciones
- **Dependencias:**
  - `system_state.h`: Para manejar los estados y notificaciones.
  - `lora_receiver.h`: Para acceder a los datos en la cola LoRa.
- **Limitaciones:**
  - La conexión se intenta un máximo de 5 veces antes de notificar desconexión.
  - Solo publica datos en el tópico `gateway/lora/data`.
  - El tamaño del mensaje está limitado a 128 bytes (buffer LoRa).

---

### Consideraciones
1. **Logs:** Los mensajes detallados facilitan la depuración y el monitoreo de las publicaciones y la conexión.
2. **Resiliencia:** Implementa reintentos para conectar al broker, pero no maneja reintentos automáticos para publicar mensajes fallidos.
3. **Configurabilidad:** En futuras versiones, podría añadirse soporte para múltiples tópicos o configuración dinámica del broker MQTT.

---

## Módulo: LED Manager

### Descripción
El módulo **LED Manager** proporciona retroalimentación visual sobre el estado actual del sistema mediante LEDs. Cada estado del sistema está asociado con un patrón o comportamiento específico en los LEDs.

### Archivos Asociados
- `led_manager.h`
- `led_manager.cpp`

---

### Responsabilidades
- Inicializar los pines GPIO asociados a los LEDs.
- Controlar el comportamiento de los LEDs según el estado del sistema.
- Registrar en los logs los cambios de estado y su representación en los LEDs.

---

### Funciones Principales

#### **`initializeLedManager`**
- **Propósito:** Configura los pines GPIO para los LEDs y los inicializa en estado apagado.
- **Detalles:**
  - Configura `GREEN_LED_PIN` y `RED_LED_PIN` como salidas.
  - Apaga ambos LEDs al inicio.
  - Registra un mensaje en los logs indicando que el módulo ha sido inicializado.

#### **`ledTask`**
- **Propósito:** Monitorea el estado del sistema y ajusta el comportamiento de los LEDs en consecuencia.
- **Detalles:**
  - Obtiene el estado actual del sistema con `getSystemState`.
  - Ajusta los LEDs según el estado:
    - `SYSTEM_STATE_CONNECTING`: LED rojo parpadea lentamente.
    - `SYSTEM_STATE_CONNECTED_WIFI`: LED verde parpadea lentamente.
    - `SYSTEM_STATE_CONNECTED_MQTT`: LED verde encendido fijo.
    - `SYSTEM_STATE_ERROR`: LED rojo encendido fijo.
    - `SYSTEM_STATE_CONFIG_MODE`: LED verde parpadea rápidamente.
    - **Estado desconocido:** Todos los LEDs apagados.
  - Incluye retrasos (`vTaskDelay`) para reducir el consumo de CPU.
  - Registra mensajes de depuración sobre el comportamiento de los LEDs.

---

### Estados del Sistema y Comportamiento de los LEDs

| **Estado del Sistema**        | **LED Verde**               | **LED Rojo**                |
|-------------------------------|-----------------------------|-----------------------------|
| `SYSTEM_STATE_CONNECTING`     | Apagado                    | Parpadea lentamente         |
| `SYSTEM_STATE_CONNECTED_WIFI` | Parpadea lentamente         | Apagado                    |
| `SYSTEM_STATE_CONNECTED_MQTT` | Encendido fijo              | Apagado                    |
| `SYSTEM_STATE_ERROR`          | Apagado                    | Encendido fijo             |
| `SYSTEM_STATE_CONFIG_MODE`    | Parpadea rápidamente        | Apagado                    |
| Estado desconocido            | Apagado                    | Apagado                    |

---

### Requisitos y Limitaciones
- **Dependencias:**
  - `system_state.h`: Para obtener el estado actual del sistema.
- **Limitaciones:**
  - Los pines GPIO para los LEDs están fijos en `GREEN_LED_PIN` (2) y `RED_LED_PIN` (4).
  - El comportamiento de los LEDs no es configurable sin modificar el código fuente.

---

### Consideraciones
1. **Logs:** Proporciona mensajes detallados que ayudan a identificar el estado actual y el comportamiento de los LEDs.
3. **Compatibilidad:** Los pines definidos para los LEDs deben estar libres y correctamente conectados al hardware.
3. **Optimización:** Se utiliza `vTaskDelay` para evitar consumo innecesario de CPU, lo que también asegura un comportamiento suave en los LEDs.

---

## **8️⃣ Button Manager Module**
### **📌 Descripción**
El módulo `Button Manager` se encarga de gestionar la detección de eventos del botón físico conectado al ESP32. Se basa en una **Interrupción (`ISR`)** para detectar cambios en el estado del botón y una **tarea (`buttonTask`)** en FreeRTOS para detectar **pulsaciones largas** y evitar problemas de rebote.

---

### **📌 Eventos Notificados a `system_state`**
| **Evento**                  | **Descripción** |
|-----------------------------|----------------|
| `EVENT_BUTTON_PRESSED`      | Notifica que el botón ha sido presionado. |
| `EVENT_BUTTON_RELEASED`     | Notifica que el botón ha sido liberado. |
| `EVENT_LONG_PRESS_BUTTON`   | Notifica que el botón ha sido presionado por más de 5 segundos. |

---

### **📌 Funciones Públicas**
| **Función**                     | **Descripción** |
|----------------------------------|----------------|
| `initializeButtonManager()`      | Inicializa el módulo, configura el pin del botón y activa la interrupción (`ISR`). |
| `buttonTask(void *pvParameters)` | Monitorea el estado del botón en FreeRTOS y detecta pulsaciones largas. |

---

### **📌 Implementación**
1. **Detección de eventos mediante interrupción (`ISR`)**  
   - Se configura una **interrupción externa** (`attachInterrupt()`) en el **GPIO del botón**.  
   - Cuando el botón cambia de estado (`LOW/HIGH`), la **ISR** envía una notificación a `system_state`.

2. **Procesamiento de pulsaciones largas**  
   - La tarea `buttonTask` monitorea continuamente el estado del botón.  
   - Si el botón **permanece presionado más de 5 segundos**, envía `EVENT_LONG_PRESS_BUTTON`.  
   - Se implementa **debounce** para evitar falsos positivos.

3. **Comunicación con `system_state`**  
   - `buttonTask` no modifica directamente el estado del sistema.  
   - En su lugar, **usa `notifySystemState(evento)`** para informar cambios.  
   - `system_state.cpp` maneja los cambios de estado y activación de tareas.