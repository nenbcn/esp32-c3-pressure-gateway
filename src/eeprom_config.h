// eeprom_config.h
#ifndef EEPROM_CONFIG_H
#define EEPROM_CONFIG_H

#include "includes.h"

// EEPROM Configuration Module
// Purpose:
// Handles storage and retrieval of Wi-Fi credentials in EEPROM.
// Provides functions for saving, loading, clearing, and validating credentials.

// Constants
#define EEPROM_SIZE 512         // Total EEPROM size
#define SSID_ADDR 0             // Address for SSID storage
#define PASS_ADDR 64            // Address for Password storage
#define FLAG_ADDR 128           // Address for validation flag
#define FLAG_VALID 0xA5         // Validation flag value
#define MAX_CRED_LENGTH 64      // Maximum length for SSID and Password

/**
 * @brief Initializes EEPROM and the associated mutex.
 * @return true if initialization is successful, false otherwise.
 */
bool eepromInitialize();

/**
 * @brief Validates if the EEPROM size is sufficient.
 * @return true if size is sufficient, false otherwise.
 */
bool validateEEPROMSize();

/**
 * @brief Saves Wi-Fi credentials to EEPROM.
 * @param ssid The SSID to save.
 * @param password The password to save.
 * @return true if credentials are saved successfully, false otherwise.
 */
bool saveCredentials(const String &ssid, const String &password);

/**
 * @brief Loads Wi-Fi credentials from EEPROM.
 * @param ssid Variable to store the loaded SSID.
 * @param password Variable to store the loaded password.
 * @return true if credentials are loaded successfully, false otherwise.
 */
bool loadCredentials(String &ssid, String &password);

/**
 * @brief Clears Wi-Fi credentials from EEPROM.
 */
void clearCredentials();

/**
 * @brief Prints the contents of EEPROM.
 */
void printEEPROMContents();

#endif