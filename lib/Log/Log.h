#ifndef LOG_H
#define LOG_H

#include <utility>
#include <Print.h>

/**
 * @enum LogLevel
 * @brief Defines the severity levels for log messages.
 *
 * This enumeration provides a set of predefined logging levels, from the most severe (ERROR)
 * to the least severe (DEBUG). These levels are used to filter and categorize log messages.
 */
typedef enum {
    LOG_LEVEL_ERROR,   ///< An error that prevents the application from continuing.
    LOG_LEVEL_WARNING, ///< A potential issue that does not prevent the application from continuing.
    LOG_LEVEL_INFO,    ///< General information about the application's progress.
    LOG_LEVEL_DEBUG    ///< Detailed information for debugging purposes.
} LogLevel;

/**
 * @brief The compile-time log level.
 *
 * This constant determines the maximum logging level that will be compiled into the application.
 * Any log messages with a level higher than this value will be excluded from the final binary,
 * which helps to reduce code size and improve performance in production builds.
 */
constexpr LogLevel LOG_LEVEL = LOG_LEVEL_DEBUG;

/**
 * @struct LogMessage
 * @brief Represents a single log message.
 *
 * This structure holds the log level and the message content for a single log entry.
 */
typedef struct {
    LogLevel level;
    char message[128];
} LogMessage;

/**
 * @class Log
 * @brief A static class for logging messages.
 *
 * This class provides a simple, static interface for logging messages at various severity levels.
 * It uses variadic templates to support `printf`-style formatting.
 */
class Log {
    public:
        /**
         * @brief Initializes the logging system.
         * @return True if initialization was successful, false otherwise.
         */
        static bool init();

        /**
         * @brief Processes and prints pending log messages.
         * @param print A pointer to a `Print` object for outputting the logs.
         */
        static void process(Print *print);

        /**
         * @brief Logs an error message.
         * @tparam Args The types of the arguments for the format string.
         * @param format The format string (like in `printf`).
         * @param args The arguments to be formatted.
         */
        template<typename... Args>
        static void error(const char *format, Args&&... args) {
            log(LogLevel::LOG_LEVEL_ERROR, format, std::forward<Args>(args)...);
        }

        /**
         * @brief Logs a warning message.
         * @tparam Args The types of the arguments for the format string.
         * @param format The format string (like in `printf`).
         * @param args The arguments to be formatted.
         */
        template<typename... Args>
        static void warn(const char *format, Args&&... args) {
            log(LogLevel::LOG_LEVEL_WARNING, format, std::forward<Args>(args)...);
        }
        
        /**
         * @brief Logs an informational message.
         * @tparam Args The types of the arguments for the format string.
         * @param format The format string (like in `printf`).
         * @param args The arguments to be formatted.
         */
        template<typename... Args>
        static void info(const char *format, Args&&... args) {
            log(LogLevel::LOG_LEVEL_INFO, format, std::forward<Args>(args)...);
        }

        /**
         * @brief Logs a debug message.
         * @tparam Args The types of the arguments for the format string.
         * @param format The format string (like in `printf`).
         * @param args The arguments to be formatted.
         */
        template<typename... Args>
        static void debug(const char *format, Args&&... args) {
            log(LogLevel::LOG_LEVEL_DEBUG, format, std::forward<Args>(args)...);
        }
    
    private:
        /**
         * @brief The internal logging function.
         * @param level The severity level of the log message.
         * @param format The format string.
         * @param ... The variadic arguments.
         *
         * This is a private helper function used by the public logging methods to handle the
         * actual logging logic.
         */
        static void log(LogLevel level, const char *format, ...);
};

#endif
