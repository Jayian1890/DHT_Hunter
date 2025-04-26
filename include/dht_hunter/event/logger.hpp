#pragma once

#include "dht_hunter/event/event_bus.hpp"
#include "dht_hunter/event/log_event.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include <string>
#include <memory>

namespace dht_hunter::event {

/**
 * @class Logger
 * @brief A facade for logging that uses the event system.
 */
class Logger {
public:
    /**
     * @brief Create a logger for a specific component.
     * @param component The component name.
     * @return A logger for the specified component.
     */
    static Logger forComponent(const std::string& component) {
        return Logger(component);
    }

    /**
     * @brief Log a message at the TRACE level.
     * @param message The message to log.
     */
    void trace(const std::string& message) {
        log(logforge::LogLevel::TRACE, message);
    }

    /**
     * @brief Log a message at the DEBUG level.
     * @param message The message to log.
     */
    void debug(const std::string& message) {
        log(logforge::LogLevel::DEBUG, message);
    }

    /**
     * @brief Log a message at the INFO level.
     * @param message The message to log.
     */
    void info(const std::string& message) {
        log(logforge::LogLevel::INFO, message);
    }

    /**
     * @brief Log a message at the WARNING level.
     * @param message The message to log.
     */
    void warning(const std::string& message) {
        log(logforge::LogLevel::WARNING, message);
    }

    /**
     * @brief Log a message at the ERROR level.
     * @param message The message to log.
     */
    void error(const std::string& message) {
        log(logforge::LogLevel::ERROR, message);
    }

    /**
     * @brief Log a message at the CRITICAL level.
     * @param message The message to log.
     */
    void critical(const std::string& message) {
        log(logforge::LogLevel::CRITICAL, message);
    }

    /**
     * @brief Add context data to the logger.
     * @param key The context key.
     * @param value The context value.
     */
    void addContext(const std::string& key, const std::string& value) {
        m_context[key] = value;
    }

private:
    /**
     * @brief Constructor.
     * @param component The component name.
     */
    explicit Logger(std::string component) : m_component(std::move(component)) {}

    /**
     * @brief Log a message at the specified level.
     * @param level The log level.
     * @param message The message to log.
     */
    void log(logforge::LogLevel level, const std::string& message) {
        auto event = std::make_shared<LogEvent>(m_component, message, level);

        // Add context data
        for (const auto& [key, value] : m_context) {
            event->addContext(key, value);
        }

        // Publish the event
        EventBus::getInstance().publish(event);
    }

    std::string m_component;
    std::unordered_map<std::string, std::string> m_context;
};

} // namespace dht_hunter::event
