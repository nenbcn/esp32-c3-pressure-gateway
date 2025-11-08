#include "mqtt_handler.h"
#include "telemetry_manager.h"
#include "secrets.h"
#include "device_id.h"

#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>


const int MQTT_MAX_MESSAGE_SIZE = 8192;
const int JSON_BASE_SIZE = 300;          // Base size for JSON document
const int JSON_PULSE_ENTRY_SIZE = 200;   // Size per pulse entry in JSON

WiFiClientSecure net;
PubSubClient mqttClient(net);

// Device certificate selection based on MAC address
static String deviceCert;
static String devicePrivateKey;

// Internal: load cert and key from flash
bool loadDeviceCredentialsFromFlash() {
    Preferences prefs;
    prefs.begin("iot-secrets", true);
    deviceCert = prefs.getString("certificatePem", "");
    devicePrivateKey = prefs.getString("privateKey", "");
    prefs.end();
    Serial.println("DEVICE CERTIFICATE AWS: " + String(deviceCert.length()));
    Serial.println("DEVICE PRIVATE KEY AWS: " + String(devicePrivateKey.length()));
    return (deviceCert.length() > 0 && devicePrivateKey.length() > 0);
}

// Internal: request new cert and key from API
bool requestDeviceCredentialsFromAPI() {
    HTTPClient https;
    https.begin(IOT_API_ENDPOINT + String("/register-device"));
    https.addHeader("Content-Type", "application/json");
    https.addHeader("Authorization", IOT_API_KEY);
    String macAddress = getDeviceId();
    String body = "{\"deviceName\": \"" + macAddress + "\"}";
    int httpCode = https.POST(body);
    if (httpCode == 200) {
        String payload = https.getString();
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            deviceCert = doc["certificatePem"].as<String>();
            devicePrivateKey = doc["privateKey"].as<String>();
            Preferences prefs;
            prefs.begin("iot-secrets", false); // write mode
            prefs.putString("certificatePem", deviceCert);
            prefs.putString("privateKey", devicePrivateKey);
            prefs.end();
            Serial.println("[Provisioning] Device credentials provisioned and saved.");
            https.end();
            return true;
        } else {
            Serial.println("[Provisioning] JSON parse error.");
        }
    } else {
        Serial.println("[Provisioning] HTTP error during device registration: " + String(httpCode));
    }
    https.end();
    return false;
}


// Topics for MQTT communication
String PULSE_DATA_TOPIC;
String HEALTH_CHECK_TOPIC;
String OTA_TOPIC;


void initializeMQTTHandler() {
    String deviceId = getDeviceId();
    PULSE_DATA_TOPIC = "mica/dev/telemetry/gateway/" + deviceId + "/water-consumption";
    HEALTH_CHECK_TOPIC = "mica/dev/status/gateway/" + deviceId + "/healthcheck";
    net.setCACert(AWS_CERT_CA);
    net.setCertificate(deviceCert.c_str());
    net.setPrivateKey(devicePrivateKey.c_str());
    mqttClient.setServer(AWS_IOT_ENDPOINT, MQTT_PORT);
    mqttClient.setCallback(mqttMessageCallback);
    mqttClient.setBufferSize(MQTT_MAX_MESSAGE_SIZE);  // Set buffer size to handle larger messages
    Log::info("MQTT Handler initialized for AWS IoT with MAC: %s", deviceId.c_str());
}

/**
 * @brief Callback function for MQTT messages.
 * @param topic Topic of the received message.
 * @param payload Payload of the received message.
 * @param length Length of the payload.
 */
#include <ArduinoJson.h>
#include <Preferences.h>


void mqttMessageCallback(char* topic, byte* payload, unsigned int length) {
    String deviceId = getDeviceId();
    OTA_TOPIC = "mica/dev/command/gateway/" + deviceId + "/ota";
    Log::debug("Message received on topic %s:", topic);

    char message[length + 1];
    for (unsigned int i = 0; i < length; i++) {
        message[i] = (char)payload[i];
    }
    message[length] = '\0';
    Log::debug("Payload: %s", (const char *) message);

    if (strcmp(topic, OTA_TOPIC.c_str()) == 0) {
        Log::info("OTA update command received via dedicated topic.");
        StaticJsonDocument<2048> doc;
        DeserializationError err = deserializeJson(doc, message);
        if (err) {
            Log::error("Failed to parse OTA JSON: %s", err.c_str());
            return;
        }
        String jsonPretty;
        serializeJsonPretty(doc, jsonPretty);
        Log::debug("Full parsed JSON (pretty):\n%s", jsonPretty.c_str());
        const char* firmwareUrl = doc["firmwareUrl"];
        if (!firmwareUrl || strlen(firmwareUrl) == 0) {
            Log::error("No firmwareUrl in OTA message.");
            return;
        }
        Preferences preferences;
        preferences.begin("ota", false);
        preferences.putString("url", firmwareUrl);
        preferences.end();
        Log::info("Firmware URL length: %d", strlen(firmwareUrl));
        Log::info("Stored firmwareUrl to EEPROM: %s", firmwareUrl);
        notifySystemState(EVENT_OTA_UPDATE);
    }
}

bool connectMQTTClient(String deviceId) {
    initializeMQTTHandler();
    if (mqttClient.connect(deviceId.c_str())) {
        OTA_TOPIC = "mica/dev/command/gateway/" + deviceId + "/ota";
        if (mqttClient.subscribe(OTA_TOPIC.c_str())) {
            Log::info("Subscribed to OTA topic: %s", OTA_TOPIC.c_str());
        } else {
            Log::error("Failed to subscribe to OTA topic: %s", OTA_TOPIC.c_str());
        }

        return true;
    }

    return false;
}

/**
 * @brief Connects to the MQTT broker with the device ID.
 * @return true if the connection is successful, false otherwise.
 */
bool connectMQTT() {
    if (mqttClient.connected()) {
        Log::info("MQTT is already connected. Skipping connection attempt.");
        return true;
    }

    Log::warn("Attempting to connect to MQTT...");
    String deviceId = getDeviceId();
    int maxRetries = 3;
    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        Log::info("MQTT Connection Attempt %d/%d", attempt, maxRetries);
        if (connectMQTTClient(deviceId)) {
            Log::info("Successfully connected to MQTT with client ID: %s. Notifying EVENT_MQTT_CONNECTED.", deviceId.c_str());
            notifySystemState(EVENT_MQTT_CONNECTED);
            return true;
        }
        Log::warn("MQTT connection failed. Retrying in 2 seconds...");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    Log::error("Failed to connect to MQTT after %d attempts. Notifying EVENT_MQTT_DISCONNECTED.", maxRetries);
    notifySystemState(EVENT_MQTT_DISCONNECTED);
    return false;
}

/**
 * @brief Formats and publishes pulse sensor data to MQTT.
 * @param data Pointer to the processed pulse data.
 * @param count Number of data entries to publish.
 * @return true if publishing succeeds, false otherwise.
 */
bool publishPulseData(ProcessedData* data, uint8_t count) {
    if (!mqttClient.connected()) {
        Log::warn("Cannot publish pulse data: MQTT not connected");
        return false;
    }

    if (count == 0 || data == NULL) {
        Log::warn("No pulse data to publish");
        return false;
    }

    // Calculate JSON document size with proper margins for production
    const size_t capacity = JSON_BASE_SIZE + (count * JSON_PULSE_ENTRY_SIZE);
    
    DynamicJsonDocument doc(capacity);
    doc["sensor_id"] = getDeviceId();
    doc["sentTimestamp"] = getCurrentTime();
    char deviceSerial[18];
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(deviceSerial, sizeof(deviceSerial), "%02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    JsonArray pulses = doc.createNestedArray("pulses");
    for (uint8_t i = 0; i < count; ++i) {
        JsonObject pulse = pulses.createNestedObject();
        pulse["startTimestamp"] = data[i].startTimestamp;
        pulse["endTimestamp"] = data[i].endTimestamp;
        pulse["pulseCount"] = data[i].pulseCount;
        pulse["averagePeriod"] = data[i].averagePeriod;
    }
    
    // Check if we're approaching capacity limits
    size_t jsonSize = measureJson(doc);
    if (jsonSize > capacity * 0.9) {
        Log::warn("JSON document size (%d) approaching capacity (%d)", jsonSize, capacity);
    }

    String jsonString;
    serializeJson(doc, jsonString);
    Log::info(jsonString.c_str());
    Log::info("JSON size: %d bytes", jsonString.length());

    bool published = mqttClient.publish(PULSE_DATA_TOPIC.c_str(), jsonString.c_str());
    if (published) {
        Log::info("Published pulse data: %s", jsonString.c_str());
    } else {
        Log::error("Failed to publish pulse data to %s. MQTT State: %d", 
                  PULSE_DATA_TOPIC.c_str(), mqttClient.state());
    }
    return published;
}

void mqttConnectTask(void *pvParameters) {
    while (true) {
        if (loadDeviceCredentialsFromFlash()) {
            Log::info("Credentials loaded from flash.");
            notifySystemState(EVENT_MQTT_AWS_CREDENTIALS);
        } else {
            Log::info("No credentials found, requesting from API...");
            if (requestDeviceCredentialsFromAPI()) {
                Log::info("AWS Credentials obtained successfully!");
                notifySystemState(EVENT_MQTT_AWS_CREDENTIALS);
            } else {
                Log::info("Failed to obtain awscredentials");
            }
        }
        // Short delay to prevent task starvation
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void mqttPublishTask(void *pvParameters) {
    while (true) {
        SystemState currentState = getSystemState();
        bool mqttConnected = mqttClient.connected();

        mqttClient.loop();

        // Case 1: MQTT connected but system state not updated
        if (mqttConnected && currentState != SYSTEM_STATE_CONNECTED_MQTT) {
            Log::info("MQTT connected but state incorrect. Notifying EVENT_MQTT_CONNECTED.");
            notifySystemState(EVENT_MQTT_CONNECTED);
        }

        // Case 2: MQTT not connected, attempt reconnection if WiFi is active
        if (!mqttConnected) {
            if (WiFi.status() == WL_CONNECTED) {
                Log::info("WiFi active. Attempting MQTT connection...");
                connectMQTT();
            } else {
                Log::error("WiFi disconnected or inactive. Notifying EVENT_WIFI_DISCONNECTED.");
                notifySystemState(EVENT_WIFI_DISCONNECTED);
            }
        }

        // Short delay to prevent task starvation
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Publishes a health check message to MQTT.
 * @param uptime Device uptime in milliseconds
 * @return true if publishing succeeds, false otherwise.
 */
bool publishHealthCheck(uint64_t uptime) {
    if (!mqttClient.connected()) {
        Log::warn("Cannot publish health check: MQTT not connected");
        return false;
    }
    
    const size_t capacity = 256;
    DynamicJsonDocument doc(capacity);
    doc["sentTimestamp"] = getCurrentTime();
    doc["uptime"] = uptime;
    String jsonString;
    serializeJson(doc, jsonString);
    
    bool published = mqttClient.publish(HEALTH_CHECK_TOPIC.c_str(), jsonString.c_str());
    if (published) {
        Log::info("Health check published: %s", jsonString.c_str());
    } else {
        Log::error("Failed to publish health check");
    }
    return published;
}