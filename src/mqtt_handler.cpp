/*
 * mqtt_handler.cpp
 * 
 * Simplified and generic MQTT handler for AWS IoT.
 * Reads pre-formatted messages from g_mqttQueue and publishes them.
 * Handles device provisioning and connection management.
 */

#include "mqtt_handler.h"
#include "secrets.h"
#include "device_id.h"
#include "system_state.h"
#include "pressure_telemetry.h"  // For g_mqttQueue and MqttMessage

#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// MQTT Configuration
const int MQTT_MAX_MESSAGE_SIZE = 8192;
const int MQTT_KEEPALIVE_SECONDS = 60;
const int MQTT_RECONNECT_INTERVAL_MS = 5000;

// WiFi and MQTT clients
WiFiClientSecure net;
PubSubClient mqttClient(net);

// Device credentials (loaded from flash or API)
static String deviceCert;
static String devicePrivateKey;
static bool credentialsLoaded = false;

// ============================================================================
// CREDENTIAL MANAGEMENT
// ============================================================================

/**
 * @brief Loads device credentials from flash memory.
 */
bool loadDeviceCredentialsFromFlash() {
    Preferences prefs;
    prefs.begin("iot-secrets", true);  // Read-only
    deviceCert = prefs.getString("certificatePem", "");
    devicePrivateKey = prefs.getString("privateKey", "");
    prefs.end();
    
    Log::info("[MQTT] Certificate length: %d, Key length: %d", 
              deviceCert.length(), devicePrivateKey.length());
    
    return (deviceCert.length() > 0 && devicePrivateKey.length() > 0);
}

/**
 * @brief Requests new device credentials from provisioning API.
 */
bool requestDeviceCredentialsFromAPI() {
    HTTPClient https;
    https.begin(IOT_API_ENDPOINT + String("/register-device"));
    https.addHeader("Content-Type", "application/json");
    https.addHeader("Authorization", IOT_API_KEY);
    
    String macAddress = getDeviceId();
    String body = "{\"deviceName\": \"" + macAddress + "\"}";
    
    Log::info("[MQTT] Requesting credentials from API for device: %s", macAddress.c_str());
    
    int httpCode = https.POST(body);
    if (httpCode == 200) {
        String payload = https.getString();
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            deviceCert = doc["certificatePem"].as<String>();
            devicePrivateKey = doc["privateKey"].as<String>();
            
            // Save to flash
            Preferences prefs;
            prefs.begin("iot-secrets", false);  // Write mode
            prefs.putString("certificatePem", deviceCert);
            prefs.putString("privateKey", devicePrivateKey);
            prefs.end();
            
            Log::info("[MQTT] Credentials provisioned and saved to flash");
            https.end();
            return true;
        } else {
            Log::error("[MQTT] JSON parse error: %s", error.c_str());
        }
    } else {
        Log::error("[MQTT] HTTP error during provisioning: %d", httpCode);
    }
    
    https.end();
    return false;
}

// ============================================================================
// MQTT CONNECTION
// ============================================================================

/**
 * @brief Initializes MQTT client with AWS IoT configuration.
 */
bool initializeMQTTHandler() {
    if (!credentialsLoaded) {
        Log::error("[MQTT] Cannot initialize: credentials not loaded");
        return false;
    }
    
    // Configure SSL/TLS
    net.setCACert(AWS_CERT_CA);
    net.setCertificate(deviceCert.c_str());
    net.setPrivateKey(devicePrivateKey.c_str());
    
    // Configure MQTT client
    mqttClient.setServer(AWS_IOT_ENDPOINT, MQTT_PORT);
    mqttClient.setKeepAlive(MQTT_KEEPALIVE_SECONDS);
    mqttClient.setBufferSize(MQTT_MAX_MESSAGE_SIZE);
    
    String deviceId = getDeviceId();
    Log::info("[MQTT] Handler initialized for device: %s", deviceId.c_str());
    Log::info("[MQTT] Endpoint: %s:%d", AWS_IOT_ENDPOINT, MQTT_PORT);
    
    return true;
}

/**
 * @brief Attempts to connect to MQTT broker.
 */
bool connectMQTT() {
    if (mqttClient.connected()) {
        return true;
    }
    
    if (!credentialsLoaded) {
        Log::error("[MQTT] Cannot connect: credentials not loaded");
        return false;
    }
    
    String deviceId = getDeviceId();
    Log::info("[MQTT] Attempting connection with client ID: %s", deviceId.c_str());
    
    if (mqttClient.connect(deviceId.c_str())) {
        Log::info("[MQTT] Connected successfully!");
        notifySystemState(EVENT_MQTT_CONNECTED);
        return true;
    } else {
        int state = mqttClient.state();
        Log::error("[MQTT] Connection failed, state: %d", state);
        // State codes: -4=timeout, -3=lost, -2=failed, -1=disconnected, 0=connected, 1-5=protocol errors
        return false;
    }
}

/**
 * @brief Publishes a generic MQTT message.
 */
bool publishMqttMessage(const MqttMessage* msg) {
    if (!mqttClient.connected()) {
        Log::warn("[MQTT] Cannot publish: not connected");
        return false;
    }
    
    bool published = mqttClient.publish(msg->topic, msg->payload, msg->qos == 1);
    
    if (published) {
        Log::info("[MQTT] ✓ Published to %s (%d bytes, QoS=%d)", 
                  msg->topic, strlen(msg->payload), msg->qos);
        Serial.println("[MQTT] Confirmed payload sent:");
        Serial.println(msg->payload);
        Serial.println();
    } else {
        Log::error("[MQTT] ✗ Failed to publish to %s", msg->topic);
    }
    
    return published;
}

/**
 * @brief Checks if MQTT is connected.
 */
bool isMqttConnected() {
    return mqttClient.connected();
}

// ============================================================================
// FREERTOS TASKS
// ============================================================================

/**
 * @brief Task to provision AWS IoT credentials.
 * Runs once at startup to load or request credentials.
 */
void mqttConnectTask(void *pvParameters) {
    (void)pvParameters;
    
    Log::info("[MQTT] Credential provisioning task started");
    
    while (true) {
        // First attempt: Load credentials from flash
        if (loadDeviceCredentialsFromFlash()) {
            Log::info("[MQTT] Credentials loaded from flash");
            credentialsLoaded = true;
            notifySystemState(EVENT_MQTT_AWS_CREDENTIALS);
        } else {
            Log::info("[MQTT] No credentials in flash, requesting from API...");
            
            // Wait for WiFi to be ready before requesting credentials
            while (WiFi.status() != WL_CONNECTED) {
                Log::debug("[MQTT] Waiting for WiFi...");
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            
            // Request credentials
            if (requestDeviceCredentialsFromAPI()) {
                Log::info("[MQTT] Credentials obtained from API");
                credentialsLoaded = true;
                notifySystemState(EVENT_MQTT_AWS_CREDENTIALS);
            } else {
                Log::error("[MQTT] Failed to obtain credentials");
                notifySystemState(EVENT_MQTT_DISCONNECTED);
            }
        }
        
        // Short delay to prevent task starvation (matching reference implementation)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Task to maintain MQTT connection and publish messages.
 * Reads from g_mqttQueue and publishes generic messages.
 */
void mqttPublishTask(void *pvParameters) {
    (void)pvParameters;
    
    Log::info("[MQTT] Publish task started");
    
    // Wait for credentials to be loaded
    while (!credentialsLoaded) {
        Log::debug("[MQTT] Waiting for credentials...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Initialize MQTT handler
    if (!initializeMQTTHandler()) {
        Log::error("[MQTT] Initialization failed, task terminating");
        vTaskDelete(NULL);
        return;
    }
    
    unsigned long lastReconnectAttempt = 0;
    
    Log::info("[MQTT] Publish task main loop started");
    
    while (1) {
        SystemState currentState = getSystemState();
        bool mqttConnected = mqttClient.connected();
        
        // Process MQTT loop (keepalive, etc)
        mqttClient.loop();
        
        // Case 1: MQTT connected but system state not updated
        if (mqttConnected && currentState != SYSTEM_STATE_CONNECTED_MQTT) {
            Log::info("[MQTT] MQTT connected but state incorrect. Notifying EVENT_MQTT_CONNECTED.");
            notifySystemState(EVENT_MQTT_CONNECTED);
        }
        
        // Case 2: MQTT not connected, attempt reconnection if WiFi is active
        if (!mqttConnected) {
            unsigned long now = millis();
            if (now - lastReconnectAttempt > MQTT_RECONNECT_INTERVAL_MS) {
                lastReconnectAttempt = now;
                
                // Check WiFi first
                if (WiFi.status() != WL_CONNECTED) {
                    Log::warn("[MQTT] WiFi disconnected, waiting...");
                    notifySystemState(EVENT_WIFI_DISCONNECTED);
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    continue;
                }
                
                // Attempt MQTT connection
                Log::info("[MQTT] WiFi active. Attempting MQTT connection...");
                connectMQTT();
            }
        } else {
            // MQTT is connected, check for messages in queue
            MqttMessage msg;
            if (xQueueReceive(g_mqttQueue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
                publishMqttMessage(&msg);
            }
        }
        
        // Small delay to prevent task starvation
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
