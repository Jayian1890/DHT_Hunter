#pragma once

#include "dht_hunter/unified_event/unified_event.hpp"
#include <string>
#include <memory>
#include <sstream>
#include <utility>

namespace dht_hunter::event {

/**
 * @brief Adapter class that provides compatibility with the old Logger interface
 *
 * This adapter allows existing code to continue using the old Logger interface
 * while actually using the new unified event system under the hood.
 */
class Logger {
public:
    /**
     * @brief Creates a logger for a specific component
     * @param componentName The name of the component
     * @return A logger instance for the component
     */
    static Logger forComponent(const std::string& componentName) {
        return Logger(componentName);
    }

    /**
     * @brief Constructor
     * @param componentName The name of the component
     */
    explicit Logger(const std::string& componentName)
        : m_componentName(componentName) {
    }

    /**
     * @brief Logs a debug message
     * @param format The format string
     * @param args The format arguments
     */
    template<typename... Args>
    void debug(const std::string& format, Args&&... args) const {
        log(unified_event::EventSeverity::Debug, format, std::forward<Args>(args)...);
    }

    /**
     * @brief Logs an info message
     * @param format The format string
     * @param args The format arguments
     */
    template<typename... Args>
    void info(const std::string& format, Args&&... args) const {
        log(unified_event::EventSeverity::Info, format, std::forward<Args>(args)...);
    }

    /**
     * @brief Logs a warning message
     * @param format The format string
     * @param args The format arguments
     */
    template<typename... Args>
    void warning(const std::string& format, Args&&... args) const {
        log(unified_event::EventSeverity::Warning, format, std::forward<Args>(args)...);
    }

    /**
     * @brief Logs an error message
     * @param format The format string
     * @param args The format arguments
     */
    template<typename... Args>
    void error(const std::string& format, Args&&... args) const {
        log(unified_event::EventSeverity::Error, format, std::forward<Args>(args)...);
    }

    /**
     * @brief Logs a critical message
     * @param format The format string
     * @param args The format arguments
     */
    template<typename... Args>
    void critical(const std::string& format, Args&&... args) const {
        log(unified_event::EventSeverity::Critical, format, std::forward<Args>(args)...);
    }

private:
    /**
     * @brief Logs a message with the specified severity
     * @param severity The severity level
     * @param format The format string
     * @param args The format arguments
     */
    template<typename... Args>
    void log(unified_event::EventSeverity severity, const std::string& format, Args&&... args) const {
        // Format the message
        std::string message = formatString(format, std::forward<Args>(args)...);

        // Log the message using the unified event system
        switch (severity) {
            case unified_event::EventSeverity::Debug:
                unified_event::logDebug(m_componentName, message);
                break;
            case unified_event::EventSeverity::Info:
                unified_event::logInfo(m_componentName, message);
                break;
            case unified_event::EventSeverity::Warning:
                unified_event::logWarning(m_componentName, message);
                break;
            case unified_event::EventSeverity::Error:
                unified_event::logError(m_componentName, message);
                break;
            case unified_event::EventSeverity::Critical:
                unified_event::logCritical(m_componentName, message);
                break;
        }
    }

    /**
     * @brief Formats a string with the specified arguments
     * @param format The format string
     * @param args The format arguments
     * @return The formatted string
     */
    template<typename... Args>
    std::string formatString(const std::string& format, Args&&... args) const {
        // Simple implementation that replaces {} with arguments
        // This is not a full fmt::format replacement, but it handles basic cases
        if constexpr (sizeof...(args) == 0) {
            return format;
        } else {
            return formatStringImpl(format, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Implementation of formatString for a single argument
     * @param format The format string
     * @param arg The argument
     * @return The formatted string
     */
    template<typename T>
    std::string formatStringImpl(const std::string& format, T&& arg) const {
        std::string result = format;
        size_t pos = result.find("{}");
        if (pos != std::string::npos) {
            std::stringstream ss;
            ss << arg;
            result.replace(pos, 2, ss.str());
        }
        return result;
    }

    /**
     * @brief Implementation of formatString for multiple arguments
     * @param format The format string
     * @param arg The first argument
     * @param args The remaining arguments
     * @return The formatted string
     */
    template<typename T, typename... Args>
    std::string formatStringImpl(const std::string& format, T&& arg, Args&&... args) const {
        std::string result = format;
        size_t pos = result.find("{}");
        if (pos != std::string::npos) {
            std::stringstream ss;
            ss << arg;
            result.replace(pos, 2, ss.str());
        }
        return formatStringImpl(result, std::forward<Args>(args)...);
    }

    std::string m_componentName;
};

} // namespace dht_hunter::event
