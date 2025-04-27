#pragma once

#include "dht_hunter/unified_event/event.hpp"
#include <string>

namespace dht_hunter::unified_event {

/**
 * @brief Event for log messages
 */
class LogMessageEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param severity The severity of the log message
     * @param message The log message
     */
    LogMessageEvent(const std::string& source, EventSeverity severity, const std::string& message)
        : Event(EventType::LogMessage, severity, source) {
        setProperty("message", message);
    }
    
    /**
     * @brief Gets the log message
     * @return The log message
     */
    std::string getMessage() const {
        auto message = getProperty<std::string>("message");
        return message ? *message : "";
    }
    
    /**
     * @brief Gets a string representation of the event
     * @return A string representation of the event
     */
    std::string toString() const override {
        std::string baseString = Event::toString();
        return baseString + ": " + getMessage();
    }
};

} // namespace dht_hunter::unified_event
