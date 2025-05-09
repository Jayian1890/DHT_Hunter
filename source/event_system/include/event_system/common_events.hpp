#pragma once

#include "event_system/event.hpp"

namespace event_system {

/**
 * @brief System event class
 */
class SystemEvent : public Event {
public:
    /**
     * @brief System event type enumeration
     */
    enum class SystemEventType {
        STARTUP,        // System startup
        SHUTDOWN,       // System shutdown
        PAUSE,          // System pause
        RESUME,         // System resume
        CONFIG_CHANGED, // Configuration changed
        ERROR           // System error
    };

    /**
     * @brief Constructor
     * @param systemEventType The system event type
     * @param source The source of the event
     * @param severity The event severity
     */
    SystemEvent(SystemEventType systemEventType, const std::string& source, 
                EventSeverity severity = EventSeverity::INFO)
        : Event(EventType::SYSTEM, severity, source),
          m_systemEventType(systemEventType) {
        setProperty("systemEventType", m_systemEventType);
    }

    /**
     * @brief Gets the system event type
     * @return The system event type
     */
    SystemEventType getSystemEventType() const { return m_systemEventType; }

    /**
     * @brief Gets the event name
     * @return The event name
     */
    std::string getName() const override {
        switch (m_systemEventType) {
            case SystemEventType::STARTUP:        return "SystemStartup";
            case SystemEventType::SHUTDOWN:       return "SystemShutdown";
            case SystemEventType::PAUSE:          return "SystemPause";
            case SystemEventType::RESUME:         return "SystemResume";
            case SystemEventType::CONFIG_CHANGED: return "ConfigChanged";
            case SystemEventType::ERROR:          return "SystemError";
            default:                              return "SystemEvent";
        }
    }

private:
    SystemEventType m_systemEventType;
};

/**
 * @brief Log event class
 */
class LogEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param message The log message
     * @param source The source of the event
     * @param severity The event severity
     */
    LogEvent(const std::string& message, const std::string& source, 
             EventSeverity severity = EventSeverity::INFO)
        : Event(EventType::LOG, severity, source),
          m_message(message) {
        setProperty("message", m_message);
    }

    /**
     * @brief Gets the log message
     * @return The log message
     */
    const std::string& getMessage() const { return m_message; }

    /**
     * @brief Gets the event name
     * @return The event name
     */
    std::string getName() const override { return "LogEvent"; }

    /**
     * @brief Gets detailed information about the event
     * @return A string with detailed information about the event
     */
    std::string getDetails() const override { return m_message; }

private:
    std::string m_message;
};

/**
 * @brief Network event class
 */
class NetworkEvent : public Event {
public:
    /**
     * @brief Network event type enumeration
     */
    enum class NetworkEventType {
        CONNECTED,          // Connected to a remote host
        DISCONNECTED,       // Disconnected from a remote host
        DATA_RECEIVED,      // Data received from a remote host
        DATA_SENT,          // Data sent to a remote host
        CONNECTION_ERROR    // Connection error
    };

    /**
     * @brief Constructor
     * @param networkEventType The network event type
     * @param source The source of the event
     * @param severity The event severity
     */
    NetworkEvent(NetworkEventType networkEventType, const std::string& source, 
                 EventSeverity severity = EventSeverity::INFO)
        : Event(EventType::NETWORK, severity, source),
          m_networkEventType(networkEventType) {
        setProperty("networkEventType", m_networkEventType);
    }

    /**
     * @brief Gets the network event type
     * @return The network event type
     */
    NetworkEventType getNetworkEventType() const { return m_networkEventType; }

    /**
     * @brief Gets the event name
     * @return The event name
     */
    std::string getName() const override {
        switch (m_networkEventType) {
            case NetworkEventType::CONNECTED:       return "NetworkConnected";
            case NetworkEventType::DISCONNECTED:    return "NetworkDisconnected";
            case NetworkEventType::DATA_RECEIVED:   return "NetworkDataReceived";
            case NetworkEventType::DATA_SENT:       return "NetworkDataSent";
            case NetworkEventType::CONNECTION_ERROR: return "NetworkConnectionError";
            default:                               return "NetworkEvent";
        }
    }

private:
    NetworkEventType m_networkEventType;
};

} // namespace event_system
