// telemetry_manager.cpp
#include "telemetry_manager.h"

#include <UtcClock.h>

#include "mqtt_handler.h"
#include "secrets.h"
#include "device_id.h"

// NTP Configuration for UTC standard time
const char* ntpServer = "pool.ntp.org";
const char* ntpBackupServer = "time.nist.gov";
const long gmtOffset_sec = 0;      // UTC+0 (no offset)
const int daylightOffset_sec = 0;  // No daylight saving time adjustment

static UtcClock utcClock(ntpServer, ntpBackupServer);

// Buffer management
static ProcessedData dataBuffer[BUFFER_SIZE];   // Buffer for processed data
static uint8_t bufferIndex = 0;                 // Current position in buffer
static SemaphoreHandle_t bufferMutex = NULL;    // Mutex for buffer access

// Buffer management constants
static const uint32_t BUFFER_SEND_INTERVAL = 10000;  // Send buffer every 10 second if not full
static uint32_t lastBufferSendTime = 0;             // Last time the buffer was sent

/**
 * @brief Initializes the telemetry manager
 * @return true if initialization is successful, false otherwise
 */
bool initializeTelemetryManager() {
    // Create mutex for buffer access
    bufferMutex = xSemaphoreCreateMutex();
    if (bufferMutex == NULL) {
        Log::error("Failed to create telemetry buffer mutex.");
        return false;
    }
    // Initialize NTP time with UTC timezone
    utcClock.init();
    lastBufferSendTime = millis();
    Log::info("Telemetry manager initialized successfully.");
    return true;
}

uint64_t getCurrentTime(uint64_t millisTimestamp) {
    return utcClock.getTime(millisTimestamp);
}

/**
 * @brief Adds processed pulse data to the telemetry buffer
 * @param data The processed data entry to add
 * @return true if data was added, false if buffer was full or mutex was not acquired
 */
bool addPulseDataToBuffer(const ProcessedData& data) {
    if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Log::error("Failed to acquire buffer mutex - timeout");
        return false;
    }
    
    bool result = false;
    if (bufferIndex < BUFFER_SIZE) {
        dataBuffer[bufferIndex] = data;
        bufferIndex++;
        
        Log::debug("Added pulse data to buffer [%d/%d].", bufferIndex, BUFFER_SIZE);
        result = true;
    } else {
        Log::warn("Buffer full (%d entries), unable to add new data.", BUFFER_SIZE);
    }
    
    xSemaphoreGive(bufferMutex);
    return result;
}

/**
 * @brief Get data from the buffer for transmission
 * @param buffer Pointer to array where data should be copied
 * @param size Number of entries copied
 * @return true if data was copied, false if buffer was empty or mutex couldn't be acquired
 */
static bool getBufferData(ProcessedData* buffer, uint8_t* size) {
    if (buffer == NULL || size == NULL) {
        Log::error("NULL parameters in getBufferData.");
        return false;
    }
    
    if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Log::error("Failed to acquire buffer mutex for reading - timeout");
        return false;
    }
    
    bool hasData = false;
    if (bufferIndex > 0) {
        // Copy data to output buffer
        memcpy(buffer, dataBuffer, bufferIndex * sizeof(ProcessedData));
        *size = bufferIndex;
        hasData = true;
    }
    
    xSemaphoreGive(bufferMutex);
    return hasData;
}

/**
 * @brief Clears the buffer after successful transmission
 * @return true if buffer was cleared successfully, false if mutex couldn't be acquired
 */
static bool clearBuffer() {
    if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Log::error("Failed to acquire buffer mutex for clearing - timeout");
        return false;
    }
    bufferIndex = 0;
    Log::debug("Buffer cleared after successful transmission");
    xSemaphoreGive(bufferMutex);
    return true;
}

/**
 * @brief Sends buffered pulse data via MQTT
 * @param reason Reason for sending (BUFFER_FULL or TIMEOUT)
 * @return true if data was sent successfully, false otherwise
 */
bool sendBufferedPulseData(SendReason reason) {
    ProcessedData outputBuffer[BUFFER_SIZE];
    uint8_t bufferSize = 0;
    
    if (getBufferData(outputBuffer, &bufferSize)) {
        const char* reasonStr = (reason == BUFFER_FULL) ? "buffer full" : "timeout";
        Log::info("Sending buffer: %d events, reason: %s", 
                   bufferSize, reasonStr);
        
        bool publishSuccess = publishPulseData(outputBuffer, bufferSize);
        if (publishSuccess) {
            if (!clearBuffer()) {
                Log::error("Failed to clear buffer after successful publish");
            }
            lastBufferSendTime = millis();
            return true;
        } else {
            Log::warn("MQTT publish failed, keeping data in buffer for retry");
            return false;
        }
    }

    return false;
}

class Sender {
    public:
        virtual bool send(uint32_t now) = 0;
};

class MqttSender : public Sender {
    public:
        bool send(uint32_t now) override {
            return publishHealthCheck((uint64_t) now);
        }
};

class HttpSender : public Sender {
        String url;
        String deviceId;
    public:
        HttpSender(String url, String deviceId) : url(url), deviceId(deviceId) {}

        bool send(uint32_t now) override {
            HTTPClient https;
            https.begin(url);
            https.addHeader("Content-Type", "application/json");
            https.addHeader("Authorization", IOT_API_KEY);
            int httpCode = https.POST(body(now));
            https.end();
            return httpCode < 300;
        }

    private:
        String body(uint32_t now) {
            const size_t capacity = 256;
            DynamicJsonDocument doc(capacity);
            doc["gatewayId"] = deviceId;
            doc["uptime"] = (uint64_t) now;
            String jsonString;
            serializeJson(doc, jsonString);
            return jsonString;
        }
};

/**
 * @class Health
 * @brief Manages periodic health checks for a device.
 *
 * The Health class tracks time-based intervals to determine when a device 
 * should send a health check.
 */
class Health {
        Sender *sender;
        uint32_t lastTime = 0;
        uint32_t interval;  
    public:
        /**
         * @brief Constructs a Health object with a given last check time and interval.
         * @param lastTime The last time (in milliseconds) a health check was sent.
         * @param interval The required interval (in milliseconds) between health checks.
         */
        Health(Sender *sender, uint32_t interval) : sender(sender), interval(interval) {}

        /**
         * @brief Determines if a health check is due based on the current time.
         * @param now The current time in milliseconds.
         * @return True if the elapsed time since the last check exceeds the interval.
         */
        bool isDue(uint32_t now) {
            uint32_t elaspsed = now - lastTime;
            return elaspsed > interval;
        }

        /**
         * @brief Sends a health check using the current timestamp.
         * 
         * This function sends the health check via an external publishHealthCheck
         * function and updates the lastTime if successful.
         * 
         * @param now The current time in milliseconds.
         * @return True if the health check was sent successfully, false otherwise.
         */
        bool send(uint32_t now) {
            bool success = sender->send(now);
            if (success) {
                lastTime = now;
            }
            return success;
        }
};

/**
 * @brief Checks if the buffer should be sent based on fullness or timeout
 * @param reason Pointer to store the reason for sending
 * @return true if buffer should be sent
 */
bool isBufferSendDue(SendReason* reason) {
    if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        Log::warn("Failed to acquire buffer mutex for checking - timeout");
        return false;
    }
    bool shouldSend = false;
    uint32_t now = millis();
    if (bufferIndex > 0 && (now - lastBufferSendTime > BUFFER_SEND_INTERVAL)) {
        *reason = TIMEOUT;
        shouldSend = true;
    }
    if (bufferIndex >= BUFFER_SIZE * 0.8) {
        *reason = BUFFER_FULL;
        shouldSend = true;
    }
    xSemaphoreGive(bufferMutex);
    return shouldSend;
}

/**
 * @brief FreeRTOS task to handle telemetry transmissions
 * Handles periodic health checks and buffer transmission management
 * @param pvParameters Task parameters (not used)
 */
void telemetryTask(void *pvParameters) {
    MqttSender mqttSender;
    String url = IOT_API_ENDPOINT + String("/healthcheck");
    String deviceId = getDeviceId();
    HttpSender httpSender(url, deviceId);

    Health mqttHealth(&mqttSender, HEALTHCHECK_INTERVAL);
    Health httpHealth(&httpSender, HEALTHCHECK_INTERVAL);

    mqttHealth.send(millis());
    httpHealth.send(millis());
    while (true) {
        uint32_t now = millis();
        SendReason reason;
        if (isBufferSendDue(&reason)) {
            sendBufferedPulseData(reason);
        }
        if (mqttHealth.isDue(now)) {
            mqttHealth.send(now);
        }
        if (httpHealth.isDue(now)) {
            httpHealth.send(now);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
