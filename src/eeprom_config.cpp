// eeprom_config.cpp
#include "eeprom_config.h"

// Mutex to protect EEPROM access
SemaphoreHandle_t eepromMutex = NULL;

// Initialize EEPROM
bool eepromInitialize() {
    if (!validateEEPROMSize()) {
        return false;
    }

    if (!EEPROM.begin(EEPROM_SIZE)) {
        Log::error("Failed to initialize EEPROM.");
        return false;
    }

    eepromMutex = xSemaphoreCreateMutex();
    if (eepromMutex == NULL) {
        Log::error("Failed to create EEPROM mutex.");
        return false;
    }

    Serial.println("[INFO] EEPROM initialized successfully."); // LogMessage still not initialized
    return true;
}

// Validate EEPROM Size
bool validateEEPROMSize() {
    int requiredSize = FLAG_ADDR + 1; // Last used address

    if (EEPROM_SIZE < requiredSize) {
        Log::error("EEPROM_SIZE (%d) is insufficient. Required: %d\n", EEPROM_SIZE, requiredSize);
        return false;
    }

    Log::info("EEPROM_SIZE (%d) is sufficient. Required: %d\n", EEPROM_SIZE, requiredSize);
    return true;
}

// Save Credentials
bool saveCredentials(const String &ssid, const String &password) {
    if (eepromMutex == NULL) {
        Log::error("EEPROM mutex not initialized.");
        return false;
    }

    if (xSemaphoreTake(eepromMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        Log::info("Attempting to save credentials: SSID=%s, Password=%s", ssid.c_str(), password.c_str());

        if (ssid.length() > MAX_CRED_LENGTH || password.length() > MAX_CRED_LENGTH) {
            Log::error("Credentials exceed maximum length.");
            xSemaphoreGive(eepromMutex);
            return false;
        }

        EEPROM.write(FLAG_ADDR, FLAG_VALID);
        for (int i = 0; i < MAX_CRED_LENGTH; ++i) {
            EEPROM.write(SSID_ADDR + i, (i < ssid.length()) ? ssid[i] : 0);
            EEPROM.write(PASS_ADDR + i, (i < password.length()) ? password[i] : 0);
        }

        if (EEPROM.commit()) {
            Log::info("Credentials saved successfully in EEPROM.");
            xSemaphoreGive(eepromMutex);
            return true;
        } else {
            Log::error("Failed to commit changes to EEPROM.");
            xSemaphoreGive(eepromMutex);
            return false;
        }
    } else {
        Log::error("Could not acquire EEPROM mutex.");
        return false;
    }
}

bool loadCredentials(String &ssid, String &password) {
    if (eepromMutex == NULL) {
        Log::error("EEPROM mutex not initialized.");
        return false;
    }

    if (xSemaphoreTake(eepromMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (EEPROM.read(FLAG_ADDR) != FLAG_VALID) {
            Log::warn("No valid credentials found in EEPROM.");
            xSemaphoreGive(eepromMutex);
            return false;
        }

        char ssidBuffer[MAX_CRED_LENGTH + 1];
        char passBuffer[MAX_CRED_LENGTH + 1];
        for (int i = 0; i < MAX_CRED_LENGTH; ++i) {
            ssidBuffer[i] = EEPROM.read(SSID_ADDR + i);
            passBuffer[i] = EEPROM.read(PASS_ADDR + i);
        }
        ssidBuffer[MAX_CRED_LENGTH] = '\0';
        passBuffer[MAX_CRED_LENGTH] = '\0';

        ssid = String(ssidBuffer);
        password = String(passBuffer);

        Log::info("Loaded credentials: SSID=%s, Password=%s", ssid.c_str(), password.c_str());
        xSemaphoreGive(eepromMutex);
        return true;
    } else {
        Log::error("Could not acquire EEPROM mutex.");
        return false;
    }
}

// Clear Credentials
void clearCredentials() {
    if (eepromMutex == NULL) {
        Log::error("EEPROM mutex not initialized.");
        return;
    }

    if (xSemaphoreTake(eepromMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        EEPROM.write(FLAG_ADDR, 0xFF);
        for (int i = 0; i < MAX_CRED_LENGTH; ++i) {
            EEPROM.write(SSID_ADDR + i, 0);
            EEPROM.write(PASS_ADDR + i, 0);
        }
        EEPROM.commit();
        Log::info("Credentials cleared in EEPROM.");
        xSemaphoreGive(eepromMutex);
    } else {
        Log::error("Could not acquire EEPROM mutex.");
    }
}

// Print EEPROM Contents
void printEEPROMContents() {
    if (eepromMutex == NULL) {
        Log::error("EEPROM mutex not initialized.");
        return;
    }

    if (xSemaphoreTake(eepromMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        Log::info("EEPROM Contents:");
        Log::info("  FLAG_ADDR: %02X", EEPROM.read(FLAG_ADDR));

        char ssidBuffer[MAX_CRED_LENGTH + 1];
        for (int i = 0; i < MAX_CRED_LENGTH; ++i) {
            char c = EEPROM.read(SSID_ADDR + i);
            if (c == 0) break;
            ssidBuffer[i] = c;
        }
        ssidBuffer[MAX_CRED_LENGTH] = '\0';
        Log::info("  SSID: %s", ssidBuffer);

        char passBuffer[MAX_CRED_LENGTH + 1];
        for (int i = 0; i < MAX_CRED_LENGTH; ++i) {
            char c = EEPROM.read(PASS_ADDR + i);
            if (c == 0) break;
            passBuffer[i] = c;
        }
        passBuffer[MAX_CRED_LENGTH] = '\0';
        Log::info("\nPassword: %s", passBuffer);

        xSemaphoreGive(eepromMutex);
    } else {
        Log::error("Could not acquire EEPROM mutex.");
    }
}