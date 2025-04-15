#pragma once

#include <string>
#include <memory>
#include <vector>
#include <chrono>

namespace dht_hunter::logforge {

/**
 * @enum LogLevel
 * @brief Defines the severity levels for logging.
 */
enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL,
    OFF
};

/**
 * @brief Converts a LogLevel to its string representation.
 * @param level The log level to convert.
 * @return String representation of the log level.
 */
inline std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:    return "TRACE";
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO";
        case LogLevel::WARNING:  return "WARNING";
        case LogLevel::ERROR:    return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        case LogLevel::OFF:      return "OFF";
        default:                 return "UNKNOWN";
    }
}

/**
 * @brief Converts a string to its corresponding LogLevel.
 * @param levelStr The string to convert.
 * @return The corresponding LogLevel, or LogLevel::INFO if not recognized.
 */
inline LogLevel stringToLogLevel(const std::string& levelStr) {
    if (levelStr == "TRACE")     return LogLevel::TRACE;
    if (levelStr == "DEBUG")     return LogLevel::DEBUG;
    if (levelStr == "INFO")      return LogLevel::INFO;
    if (levelStr == "WARNING")   return LogLevel::WARNING;
    if (levelStr == "ERROR")     return LogLevel::ERROR;
    if (levelStr == "CRITICAL")  return LogLevel::CRITICAL;
    if (levelStr == "OFF")       return LogLevel::OFF;
    return LogLevel::INFO; // Default
}

/**
 * @class LogSink
 * @brief Abstract base class for log sinks.
 * 
 * A log sink is a destination for log messages, such as console, file, etc.
 */
class LogSink {
public:
    virtual ~LogSink() = default;
    
    /**
     * @brief Writes a log message to the sink.
     * @param level The severity level of the message.
     * @param loggerName The name of the logger.
     * @param message The log message.
     * @param timestamp The timestamp of the log message.
     */
    virtual void write(LogLevel level, const std::string& loggerName, 
                      const std::string& message, 
                      const std::chrono::system_clock::time_point& timestamp) = 0;
                      
    /**
     * @brief Sets the minimum log level for this sink.
     * @param level The minimum log level.
     */
    void setLevel(LogLevel level) {
        m_level = level;
    }
    
    /**
     * @brief Gets the minimum log level for this sink.
     * @return The minimum log level.
     */
    LogLevel getLevel() const {
        return m_level;
    }
    
    /**
     * @brief Checks if a message with the given level should be logged.
     * @param level The level to check.
     * @return True if the message should be logged, false otherwise.
     */
    bool shouldLog(LogLevel level) const {
        return level >= m_level;
    }
    
protected:
    LogLevel m_level = LogLevel::INFO;
};

} // namespace dht_hunter::logforge
