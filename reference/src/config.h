// config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <Log.h>
#include <stdint.h>  // For uint32_t type

// Access Point Constants
constexpr char AP_SSID[] = "MICA-Gateway";
constexpr char AP_PASSWORD[] = "12345678";

// OTA Constants
constexpr char firmwareUrl[] = "https://ota.mica.eco/firmware.bin";

// Data structures for pulse processing
typedef struct {
    uint64_t startTimestamp; // Event start time (Unix milliseconds)
    uint64_t endTimestamp;   // Event end time (Unix milliseconds)
    uint64_t pulseCount;     // Number of pulses detected
    uint64_t averagePeriod;  // Average period between pulses (ms)
} ProcessedData;

#endif