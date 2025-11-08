#include "UtcClock.h"

#include <Arduino.h>
#include <esp_sntp.h>

#include <Log.h>

#define SYNC_INTERVAL 1000 * 60 * 60

UtcClock::UtcClock(const char *ntpServerMain, const char *ntpServerBackup)
    : ntpServerMain(ntpServerMain), ntpServerBackup(ntpServerBackup) {}

void UtcClock::init() {
    sntp_set_sync_interval(SYNC_INTERVAL);
    configTime(
        gmtOffsetSeconds,
        gmtOffsetSeconds,
        ntpServerMain,
        ntpServerBackup
    );
}

uint64_t UtcClock::getTime(uint64_t millisTimestamp) {
    syncronize();
    uint64_t currentMillis = (millisTimestamp > 0) ? millisTimestamp : millis();

    if (isSyncronized) {
        return lastSyncUnixMs + (currentMillis - lastSyncMillis);
    }
    Log::warn("NTP not synchronized, using uptime estimate");
    return currentMillis;
}

bool UtcClock::hasLastSyncExpired() const {
    return millis() - lastSyncMillis > SYNC_INTERVAL || !isSyncronized;
}

void UtcClock::syncronize() {
    Log::debug("Time sync: %lu", millis());
    if (!hasLastSyncExpired()) {
        return;
    }

    if (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
        Log::debug("Could not sync, sntp not completed");
        return;
    }

    time_t newTime;
    time(&newTime);
    uint32_t syncMillis = millis();

    if (newTime < 1577836800) {  // January 1, 2020 timestamp
        Log::warn("Sync time validation failed");
        return;
    }

    if (!isSyncronized) {
        Log::debug("Initial sync");
        lastSyncMillis = syncMillis;
        lastSyncUnixMs = (uint64_t)newTime * 1000L;
        Log::debug("Sync value %llu", lastSyncUnixMs);
    } else {
        uint64_t expected = lastSyncUnixMs + (syncMillis - lastSyncMillis);
        uint64_t actual = (uint64_t)newTime * 1000L;
        sntp_sync_status_t sync_status = sntp_get_sync_status();
        Log::debug("Last sync %llu", lastSyncUnixMs);
        Log::debug("Elapsed %llu", syncMillis - lastSyncMillis);
        Log::debug("Expected %llu", expected);
        Log::debug("Actual %llu", actual);
        Log::debug("Drifted %lld ms", (int64_t)actual - (int64_t)expected);
        lastSyncMillis = syncMillis;
        lastSyncUnixMs = actual;
    }
    isSyncronized = true;
}
