#pragma once

#include "dht_hunter/event/event.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include <string>
#include <unordered_map>

namespace dht_hunter::event {

/**
 * @class LogEvent
 * @brief Event for log messages.
 */
class LogEvent : public Event {
public:
    /**
     * @brief Constructor.
     * @param component The component that generated the log message.
     * @param message The log message.
     * @param level The log level.
     */
    LogEvent(std::string component, std::string message, logforge::LogLevel level)
        : m_component(std::move(component)), m_message(std::move(message)), m_level(level) {}

    /**
     * @brief Get the type of the event.
     * @return The event type.
     */
    std::string getType() const override {
        return "LogEvent";
    }

    /**
     * @brief Get the component that generated the log message.
     * @return The component name.
     */
    const std::string& getComponent() const {
        return m_component;
    }

    /**
     * @brief Get the log message.
     * @return The log message.
     */
    const std::string& getMessage() const {
        return m_message;
    }

    /**
     * @brief Get the log level.
     * @return The log level.
     */
    logforge::LogLevel getLevel() const {
        return m_level;
    }

    /**
     * @brief Add context data to the log event.
     * @param key The context key.
     * @param value The context value.
     */
    void addContext(const std::string& key, const std::string& value) {
        m_context[key] = value;
    }

    /**
     * @brief Get the context data.
     * @return The context data.
     */
    const std::unordered_map<std::string, std::string>& getContext() const {
        return m_context;
    }

private:
    std::string m_component;
    std::string m_message;
    logforge::LogLevel m_level;
    std::unordered_map<std::string, std::string> m_context;
};

} // namespace dht_hunter::event
