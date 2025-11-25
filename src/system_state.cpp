// system_state.cpp
#include "system_state.h"
#include "wifi_connect.h"
#include "wifi_config_mode.h"
#include "mqtt_handler.h"
#include "led_manager.h"
#include "button_manager.h"
#include "ota_manager.h"
#include "pressure_reader.h"      // Replaces pin_receiver
#include "pressure_telemetry.h"   // Replaces telemetry_manager  
#include "message_formatter.h"    // JSON formatting and batching

// Internal Variables
volatile SystemState g_systemState = SYSTEM_STATE_CONNECTING; // Initial system state
SemaphoreHandle_t g_stateMutex = NULL;                 // Mutex to protect the system state (defined here, declared in data_types.h)
static TaskHandle_t g_stateManagerTaskHandle = NULL;          // Handle for the state management task
static SystemState lastLoggedState = SYSTEM_STATE_ERROR;      // Last logged state

// Task Handles
static TaskHandle_t g_wifiConnectTaskHandle = NULL;    // WiFi connection task
static TaskHandle_t g_wifiConfigTaskHandle = NULL;     // WiFi configuration mode task
static TaskHandle_t g_mqttConnectTaskHandle = NULL;    // MQTT connection task (AWS Credencials)
static TaskHandle_t g_mqttTaskHandle = NULL;           // MQTT task
static TaskHandle_t g_ledTaskHandle = NULL;            // LED management task
static TaskHandle_t g_buttonTaskHandle = NULL;         // Button management task
static TaskHandle_t g_otaTaskHandle = NULL;            // OTA update task
static TaskHandle_t g_pressureReaderTaskHandle = NULL;    // Pressure reader task
static TaskHandle_t g_pressureTelemetryTaskHandle = NULL;  // Pressure telemetry task
static TaskHandle_t g_messageFormatterTaskHandle = NULL;  // Message formatter task

void setOtaTaskHandle(TaskHandle_t handle) {
    g_otaTaskHandle = handle;
}

// Internal Function Declarations
static void stateManagementTask(void *pvParameters);   // Main state management task
static void handleStateTransitions();                  // Handles state transitions based on events
static void handleStateActions();                      // Executes actions corresponding to the current state
static bool initializeLogSystem();                     // Initializes the logging system
static void logTask(void *pvParameters);               // Task that processes log messages

//------------------------------------------------------------------------------
// System Initialization
//------------------------------------------------------------------------------
bool initializeSystemState() {
    // Create mutex to protect the system state
    g_stateMutex = xSemaphoreCreateMutex();
    if (g_stateMutex == NULL) {
        Log::error("Failed to create g_stateMutex.");
        return false;
    }

    if (!eepromInitialize()) {
        return false;
    }

    if (!initializeLogSystem()) {
        Log::error("Failed to initialize log system.");
        return false;
    }

    initializeLedManager();
    initializeButtonManager();

    if (!initializeWiFiConnection()) {
        return false;
    }

    if (!initializePressureReader()) {
        Log::error("Failed to initialize Pressure Reader.");
        return false;
    }

    if (!initializePressureTelemetry()) {
        Log::error("Failed to initialize Pressure Telemetry.");
        return false;
    }

    if (!initializeMessageFormatter()) {
        Log::error("Failed to initialize Message Formatter.");
        return false;
    }

    initializeOTAManager();

    // Create System Tasks with logs and verify their creation (using same stack sizes as mica-gateway)
    if (xTaskCreate(wifiConnectTask, "WiFi Connect Task", 4096, NULL, 2, &g_wifiConnectTaskHandle) != pdPASS) {
        Log::error("Failed to create WiFi Connect Task.");
        return false;
    }

    if (xTaskCreate(wifiConfigModeTask, "WiFi Config Mode Task", 4096, NULL, 2, &g_wifiConfigTaskHandle) != pdPASS) {
        Log::error("Failed to create WiFi Config Mode Task.");
        return false;
    }
    // CRITICAL: Suspend immediately - only active in CONFIG_MODE state
    vTaskSuspend(g_wifiConfigTaskHandle);

    if (xTaskCreate(mqttConnectTask, "MQTT Connect Task", 4096, NULL, 2, &g_mqttConnectTaskHandle) != pdPASS) {
        Log::error("Failed to create MQTT Connect Task.");
        return false;
    }
    // Suspend initially - will be resumed in CONFIG_MQTT state
    vTaskSuspend(g_mqttConnectTaskHandle);

    if (xTaskCreate(mqttPublishTask, "MQTT Task", 10000, NULL, 2, &g_mqttTaskHandle) != pdPASS) {
        Log::error("Failed to create MQTT Task.");
        return false;
    }
    // Suspend initially - will be resumed when MQTT is ready
    vTaskSuspend(g_mqttTaskHandle);

    // Create Pressure Reader Task (replaces Pin Receiver)
    if (xTaskCreate(pressureReaderTask, "Pressure Reader Task", PRESSURE_READER_STACK_SIZE, NULL, PRESSURE_READER_PRIORITY, &g_pressureReaderTaskHandle) != pdPASS) {
        Log::error("Failed to create Pressure Reader Task.");
        return false;
    }

    // Create Pressure Telemetry Task (replaces Telemetry Manager)
    if (xTaskCreate(pressureTelemetryTask, "Pressure Telemetry Task", PRESSURE_TELEMETRY_STACK_SIZE, NULL, PRESSURE_TELEMETRY_PRIORITY, &g_pressureTelemetryTaskHandle) != pdPASS) {
        Log::error("Failed to create Pressure Telemetry Task.");
        return false;
    }

    // Create Message Formatter Task (JSON formatting and batching)
    // Note: Task is created with normal priority but will be suspended immediately
    // to prevent execution before MQTT connection is established
    if (xTaskCreate(messageFormatterTask, "Message Formatter Task", MESSAGE_FORMATTER_STACK_SIZE, NULL, MESSAGE_FORMATTER_PRIORITY, &g_messageFormatterTaskHandle) != pdPASS) {
        Log::error("Failed to create Message Formatter Task.");
        return false;
    }
    // CRITICAL: Suspend immediately to prevent execution before MQTT is ready
    vTaskSuspend(g_messageFormatterTaskHandle);

    if (xTaskCreate(ledTask, "LED Task", 2048, NULL, 1, &g_ledTaskHandle) != pdPASS) {
        Log::error("Failed to create LED Task.");
        return false;
    }

    if (xTaskCreate(buttonTask, "Button Task", 2048, NULL, 1, &g_buttonTaskHandle) != pdPASS) {
        Log::error("Failed to create Button Task.");
        return false;
    }

    if (xTaskCreate(stateManagementTask, "State Management Task", 4096, NULL, 3, &g_stateManagerTaskHandle) != pdPASS) {
        Log::error("Failed to create State Management Task.");
        return false;
    }

    Log::info("System Initialization completed successfully.\n");
    return true;
}

// System State Management
void setSystemState(SystemState state) {
    if (xSemaphoreTake(g_stateMutex, portMAX_DELAY)) {
        g_systemState = state;
        Log::info("System state updated to: %d", state);
        xSemaphoreGive(g_stateMutex);
    }
}

SystemState getSystemState() {
    SystemState state;
    if (xSemaphoreTake(g_stateMutex, portMAX_DELAY)) {
        state = g_systemState;
        xSemaphoreGive(g_stateMutex);
    } else {
        Log::error("Failed to acquire mutex in getSystemState.");
        state = SYSTEM_STATE_ERROR;
    }
    return state;
}

// Log System
static bool initializeLogSystem() {
    if (!Log::init()) {
        Log::error("Failed to init log system.");
        return false;
    }

    if (xTaskCreate(logTask, "Log Task", 2048, NULL, 1, NULL) != pdPASS) {
        Log::error("Failed to create Log Task.");
        return false;
    }
    return true;
}

static void logTask(void *pvParameters) {
    while (true) {
        Log::process(&Serial);
    }
}

void logTaskStatus() {
    static char lastStatus[256] = ""; // Almacena el último estado de tareas para evitar duplicados
    char currentStatus[256]; // Estado actual de las tareas

    snprintf(currentStatus, sizeof(currentStatus),
        "WiFi Connect Task: %s\n"
        "WiFi Config Mode Task: %s\n"
        "MQTT Connect Task: %s\n"
        "MQTT Task: %s\n"
        "Pressure Reader Task: %s\n"
        "Pressure Telemetry Task: %s\n"
        "Message Formatter Task: %s\n"
        "LED Task: %s\n"
        "Button Task: %s\n",
        (eTaskGetState(g_wifiConnectTaskHandle) == eSuspended) ? "SUSPENDED" : "ACTIVE",
        (eTaskGetState(g_wifiConfigTaskHandle) == eSuspended) ? "SUSPENDED" : "ACTIVE",
        (eTaskGetState(g_mqttConnectTaskHandle) == eSuspended) ? "SUSPENDED" : "ACTIVE",
        (eTaskGetState(g_mqttTaskHandle) == eSuspended) ? "SUSPENDED" : "ACTIVE",
        (eTaskGetState(g_pressureReaderTaskHandle) == eSuspended) ? "SUSPENDED" : "ACTIVE",
        (eTaskGetState(g_pressureTelemetryTaskHandle) == eSuspended) ? "SUSPENDED" : "ACTIVE",
        (eTaskGetState(g_messageFormatterTaskHandle) == eSuspended) ? "SUSPENDED" : "ACTIVE",
        (g_ledTaskHandle ? "ACTIVE" : "ERROR (Not Created)"),
        (g_buttonTaskHandle ? "ACTIVE" : "ERROR (Not Created)")
    );

    // Solo imprimir si hay cambios en el estado de las tareas
    if (strcmp(currentStatus, lastStatus) != 0) {
        Serial.println("\n===== Task Status =====");
        Serial.println(currentStatus);
        strcpy(lastStatus, currentStatus); // Update the last registered state
    }
}

// Event Handling and Transitions
void notifySystemState(TaskNotificationEvent event) {
    if (g_stateManagerTaskHandle != NULL) {
        xTaskNotify(g_stateManagerTaskHandle, event, eSetBits);
    } else {
        Log::error("notifySystemState State Manager Task Handle is NULL.");
    }
}

/** @brief Waits to receive event notifications and returns the received event.
 * @param waitTime Time to wait for notifications in ticks.
 * @return TaskNotificationEvent The received event notification, or 0 if no notification was received.
 */
static TaskNotificationEvent receiveSystemStateNotification(TickType_t waitTime) {
    uint32_t receivedBits;
    if (xTaskNotifyWait(0, 0xFFFFFFFF, &receivedBits, waitTime) == pdPASS) {
        return (TaskNotificationEvent)receivedBits;
    }
    return (TaskNotificationEvent)0;
}

/** @brief Handles system state transitions when an event is received.
 */
static void handleStateTransitions() {
    TaskNotificationEvent event = receiveSystemStateNotification(pdMS_TO_TICKS(50));

    if (event == 0) return;

    // Handle long press button event globally, in any state
    if (event & EVENT_LONG_PRESS_BUTTON) {
        Log::info("Long press button event received. Transitioning to CONFIG_MODE.");
        setSystemState(SYSTEM_STATE_CONFIG_MODE);
        return; // Process this event exclusively
    }

    SystemState currentState = getSystemState();
    switch (currentState) {
        case SYSTEM_STATE_CONNECTING:
            if (event & EVENT_WIFI_CONNECTED) {
                Log::info("WiFi connected. Transitioning to CONNECTED_WIFI.");
                setSystemState(SYSTEM_STATE_CONFIG_MQTT);
            }
            if (event & EVENT_NO_PARAMETERS_EEPROM) {
                Log::warn("No WiFi parameters in EEPROM. Transitioning to CONFIG_MODE.");
                setSystemState(SYSTEM_STATE_CONFIG_MODE);
            }
            if (event & EVENT_WIFI_FAIL_CONNECT) {
                Log::error("WiFi connection failed. Trying again...");
            }
            break;

        case SYSTEM_STATE_CONFIG_MQTT:
            if (event & EVENT_MQTT_AWS_CREDENTIALS) {
                Log::info("AWS credentials accquired.");
                setSystemState(SYSTEM_STATE_CONNECTED_WIFI);
            }
            break;

        case SYSTEM_STATE_CONNECTED_WIFI:
            if (event & EVENT_MQTT_CONNECTED) {
                Log::info("MQTT connected. Transitioning to CONNECTED_MQTT.");
                setSystemState(SYSTEM_STATE_CONNECTED_MQTT);
            }
            break;

        case SYSTEM_STATE_CONNECTED_MQTT:
            if (event & EVENT_MQTT_DISCONNECTED) {
                Log::warn("MQTT disconnected. Downgrading to CONNECTED_WIFI.");
                setSystemState(SYSTEM_STATE_CONFIG_MQTT);
            }
            if (event & EVENT_WIFI_DISCONNECTED) {
                Log::warn("WiFi disconnected. Downgrading to CONNECTING.");
                setSystemState(SYSTEM_STATE_CONNECTING);
            }
            if (event & EVENT_OTA_UPDATE) {
                Log::info("OTA update event received. Transitioning to OTA_UPDATE state.");
                setSystemState(SYSTEM_STATE_OTA_UPDATE);
            }
            break;

        case SYSTEM_STATE_CONFIG_MODE:
            if (event & EVENT_WIFI_CONNECTED) {
                Log::info("Connected to wifi while in SYSTEM_STATE_CONFIG_MODE.");
                setSystemState(SYSTEM_STATE_CONFIG_MQTT);
            }
            // TODO: En este estado, podríamos manejar configuraciones adicionales
            break;

        case SYSTEM_STATE_ERROR:
            Log::error("Critical system error detected. Restarting device in 5 seconds...");
            break;

        default:
            Log::error("Unknown system state: %d", currentState);
            break;
    }

    // Handle pressure system events (logging only for now)
    if (event & EVENT_PRESSURE_QUEUE_FULL) {
        Log::warn("Pressure queue full event received. System may be overloaded.");
    }
    
    if (event & EVENT_I2C_ERROR_RECOVERY) {
        Log::warn("I2C error recovery event received. Bus was reset.");
    }
}

/** @brief Executes the actions associated with each system state.
 */
static void handleStateActions() {
    SystemState currentState = getSystemState();
   
    logTaskStatus(); // Print tasks status

    switch (currentState) {
        case SYSTEM_STATE_CONNECTING:
            if (g_wifiConnectTaskHandle) vTaskResume(g_wifiConnectTaskHandle);
            if (g_wifiConfigTaskHandle) vTaskSuspend(g_wifiConfigTaskHandle);
            if (g_mqttConnectTaskHandle) vTaskSuspend(g_mqttConnectTaskHandle);
            if (g_mqttTaskHandle) vTaskSuspend(g_mqttTaskHandle);
            if (g_pressureReaderTaskHandle) vTaskSuspend(g_pressureReaderTaskHandle);  // Suspend until WiFi connected
            if (g_buttonTaskHandle) vTaskSuspend(g_buttonTaskHandle);
            if (g_pressureTelemetryTaskHandle) vTaskSuspend(g_pressureTelemetryTaskHandle);
            if (g_messageFormatterTaskHandle) vTaskSuspend(g_messageFormatterTaskHandle);
            break;

        case SYSTEM_STATE_CONNECTED_WIFI:
            if (g_wifiConnectTaskHandle) vTaskResume(g_wifiConnectTaskHandle);
            if (g_wifiConfigTaskHandle) vTaskSuspend(g_wifiConfigTaskHandle);
            if (g_mqttConnectTaskHandle) vTaskSuspend(g_mqttConnectTaskHandle);
            if (g_mqttTaskHandle) vTaskResume(g_mqttTaskHandle);
            if (g_pressureReaderTaskHandle) vTaskResume(g_pressureReaderTaskHandle);
            if (g_buttonTaskHandle) vTaskSuspend(g_buttonTaskHandle);
            if (g_pressureTelemetryTaskHandle) vTaskResume(g_pressureTelemetryTaskHandle);  // Activar en WIFI
            if (g_messageFormatterTaskHandle) vTaskSuspend(g_messageFormatterTaskHandle);   // Suspender hasta MQTT
            break;

        case SYSTEM_STATE_CONFIG_MQTT:
            if (g_wifiConnectTaskHandle) vTaskResume(g_wifiConnectTaskHandle);
            if (g_wifiConfigTaskHandle) vTaskSuspend(g_wifiConfigTaskHandle);
            if (g_wifiConfigTaskHandle) vTaskSuspend(g_wifiConfigTaskHandle);
            if (g_mqttConnectTaskHandle) vTaskResume(g_mqttConnectTaskHandle);
            if (g_mqttTaskHandle) vTaskSuspend(g_mqttTaskHandle);
            if (g_pressureReaderTaskHandle) vTaskSuspend(g_pressureReaderTaskHandle);  // Suspend until MQTT ready
            if (g_buttonTaskHandle) vTaskSuspend(g_buttonTaskHandle);
            if (g_pressureTelemetryTaskHandle) vTaskSuspend(g_pressureTelemetryTaskHandle);
            if (g_messageFormatterTaskHandle) vTaskSuspend(g_messageFormatterTaskHandle);  // Suspender durante config MQTT
            break;

        case SYSTEM_STATE_CONNECTED_MQTT:
            if (g_wifiConnectTaskHandle) vTaskResume(g_wifiConnectTaskHandle);
            if (g_wifiConfigTaskHandle) vTaskSuspend(g_wifiConfigTaskHandle);
            if (g_mqttConnectTaskHandle) vTaskSuspend(g_mqttConnectTaskHandle);
            if (g_mqttTaskHandle) vTaskResume(g_mqttTaskHandle);
            if (g_pressureReaderTaskHandle) vTaskResume(g_pressureReaderTaskHandle);
            if (g_buttonTaskHandle) vTaskSuspend(g_buttonTaskHandle);
            if (g_pressureTelemetryTaskHandle) vTaskResume(g_pressureTelemetryTaskHandle);
            if (g_messageFormatterTaskHandle) vTaskResume(g_messageFormatterTaskHandle);
            break;

        case SYSTEM_STATE_CONFIG_MODE:
            if (g_wifiConnectTaskHandle) vTaskSuspend(g_wifiConnectTaskHandle);
            if (g_wifiConfigTaskHandle) vTaskResume(g_wifiConfigTaskHandle);
            if (g_mqttConnectTaskHandle) vTaskSuspend(g_mqttConnectTaskHandle);
            if (g_mqttTaskHandle) vTaskSuspend(g_mqttTaskHandle);
            if (g_pressureReaderTaskHandle) vTaskSuspend(g_pressureReaderTaskHandle);  // CRITICAL: Suspend in config mode
            if (g_buttonTaskHandle) vTaskSuspend(g_buttonTaskHandle);
            if (g_pressureTelemetryTaskHandle) vTaskSuspend(g_pressureTelemetryTaskHandle);
            if (g_messageFormatterTaskHandle) vTaskSuspend(g_messageFormatterTaskHandle);
            break;

        case SYSTEM_STATE_OTA_UPDATE:
            if (g_otaTaskHandle == NULL) {
                if (g_wifiConnectTaskHandle) vTaskSuspend(g_wifiConnectTaskHandle);
                if (g_mqttConnectTaskHandle) vTaskSuspend(g_mqttConnectTaskHandle);
                if (g_mqttTaskHandle) vTaskSuspend(g_mqttTaskHandle);
                if (g_pressureReaderTaskHandle) vTaskSuspend(g_pressureReaderTaskHandle); // Suspend during OTA
                if (g_buttonTaskHandle) vTaskSuspend(g_buttonTaskHandle);
                if (g_pressureTelemetryTaskHandle) vTaskSuspend(g_pressureTelemetryTaskHandle);

                if (xTaskCreate(otaTask, "OTA Task", 4096, NULL, 3, &g_otaTaskHandle) != pdPASS) {
                    Log::error("Failed to create OTA Task.");
                    setSystemState(SYSTEM_STATE_ERROR);
                }
            }
            break;

        case SYSTEM_STATE_ERROR:
            if (g_wifiConnectTaskHandle) vTaskSuspend(g_wifiConnectTaskHandle);
            if (g_wifiConfigTaskHandle) vTaskSuspend(g_wifiConfigTaskHandle);
            if (g_mqttConnectTaskHandle) vTaskSuspend(g_mqttConnectTaskHandle);
            if (g_mqttTaskHandle) vTaskSuspend(g_mqttTaskHandle);
            if (g_pressureReaderTaskHandle) vTaskSuspend(g_pressureReaderTaskHandle);
            if (g_buttonTaskHandle) vTaskSuspend(g_buttonTaskHandle);
            if (g_pressureTelemetryTaskHandle) vTaskSuspend(g_pressureTelemetryTaskHandle);

            vTaskDelay(pdMS_TO_TICKS(5000));
            ESP.restart();
            break;

        default:
            break;
    }
}

// ===================================================================================
// MAIN SYSTEM MANAGEMENT TASK
// ===================================================================================
/** @brief Main task that handles the system state.
 * @param pvParameters Parameters passed to the task (Not used).
 */
static void stateManagementTask(void *pvParameters) {
    while (true) {
        handleStateTransitions();
        handleStateActions();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
