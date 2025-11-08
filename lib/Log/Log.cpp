#include "Log.h"
#include <cstdarg>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

static QueueHandle_t g_logMessageQueue = NULL;                // Queue for log messages

bool Log::init() {
    g_logMessageQueue = xQueueCreate(10, sizeof(LogMessage));
    if (g_logMessageQueue == NULL) {
        return false;
    }
    return true;
}

void Log::process(Print *print) {
    LogMessage logEntry;
    if (xQueueReceive(g_logMessageQueue, &logEntry, portMAX_DELAY) == pdPASS) {
        if (logEntry.level <= LOG_LEVEL) { // Filter by log level
            switch (logEntry.level) {
                case LOG_LEVEL_ERROR:   print->print("[ERROR]: ");   break;
                case LOG_LEVEL_WARNING: print->print("[WARNING]: "); break;
                case LOG_LEVEL_INFO:    print->print("[INFO]: ");    break;
                case LOG_LEVEL_DEBUG:   print->print("[DEBUG]: ");   break;
                default:                print->print("[UNKNOWN]: "); break;
            }
            print->println(logEntry.message);
        }
    }
}

void Log::log(LogLevel level, const char *format, ...) {
    if (g_logMessageQueue == NULL) return;

    LogMessage logEntry;
    logEntry.level = level;

    va_list args;
    va_start(args, format);
    vsnprintf(logEntry.message, sizeof(logEntry.message), format, args);
    va_end(args);

    xQueueSend(g_logMessageQueue, &logEntry, 0);
}
