#pragma once

#include "dht_hunter/unified_event/event.hpp"
#include <string>

namespace dht_hunter::unified_event {

/**
 * @brief Event for system startup
 */
class SystemStartedEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     */
    explicit SystemStartedEvent(const std::string& source)
        : Event(EventType::SystemStarted, EventSeverity::Debug, source) {
    }
};

/**
 * @brief Event for system shutdown
 */
class SystemStoppedEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     */
    explicit SystemStoppedEvent(const std::string& source)
        : Event(EventType::SystemStopped, EventSeverity::Debug, source) {
    }
};

/**
 * @brief Event for system errors
 */
class SystemErrorEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param errorMessage The error message
     * @param errorCode The error code
     */
    SystemErrorEvent(const std::string& source, const std::string& errorMessage, int errorCode = 0)
        : Event(EventType::SystemError, EventSeverity::Error, source) {
        setProperty("message", errorMessage);
        setProperty("errorCode", errorCode);
    }

    /**
     * @brief Gets the error message
     * @return The error message
     */
    std::string getErrorMessage() const {
        auto message = getProperty<std::string>("message");
        return message ? *message : "";
    }

    /**
     * @brief Gets the error code
     * @return The error code
     */
    int getErrorCode() const {
        auto errorCode = getProperty<int>("errorCode");
        return errorCode ? *errorCode : 0;
    }

    /**
     * @brief Gets a string representation of the event
     * @return A string representation of the event
     */
    std::string toString() const override {
        std::string baseString = Event::toString();
        return baseString + ": " + getErrorMessage() + " (code: " + std::to_string(getErrorCode()) + ")";
    }
};

} // namespace dht_hunter::unified_event
