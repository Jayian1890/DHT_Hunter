#pragma once

#include "dht_hunter/unified_event/event.hpp"
#include <string>

namespace dht_hunter::unified_event {

/**
 * @brief Event for custom messages
 */
class CustomEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param message The custom message
     * @param severity The event severity
     */
    CustomEvent(const std::string& source, const std::string& message, EventSeverity severity = EventSeverity::Info)
        : Event(EventType::Custom, severity, source) {
        setProperty("message", message);
    }
    
    /**
     * @brief Gets the custom message
     * @return The custom message
     */
    std::string getMessage() const {
        auto message = getProperty<std::string>("message");
        return message ? *message : "";
    }
    
    /**
     * @brief Gets the event name
     * @return The event name
     */
    std::string getName() const override {
        return getMessage();
    }
};

} // namespace dht_hunter::unified_event
