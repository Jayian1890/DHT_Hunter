#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <any>
#include <vector>

namespace event_system {

/**
 * @brief Event type enumeration
 */
enum class EventType {
    SYSTEM,         // System events (startup, shutdown, etc.)
    NETWORK,        // Network events (connection, data received, etc.)
    DHT,            // DHT events (node discovered, lookup completed, etc.)
    BITTORRENT,     // BitTorrent events (peer discovered, metadata received, etc.)
    USER_INTERFACE, // User interface events (user action, UI update, etc.)
    LOG,            // Log events (debug, info, warning, error, etc.)
    CUSTOM          // Custom events
};

/**
 * @brief Event severity enumeration
 */
enum class EventSeverity {
    TRACE,      // Trace-level severity
    DEBUG,      // Debug-level severity
    INFO,       // Info-level severity
    WARNING,    // Warning-level severity
    ERROR,      // Error-level severity
    CRITICAL    // Critical-level severity
};

/**
 * @brief Base class for all events in the system
 */
class Event {
public:
    /**
     * @brief Constructor
     * @param type The event type
     * @param severity The event severity
     * @param source The source of the event (component name)
     */
    Event(EventType type, EventSeverity severity, const std::string& source)
        : m_type(type),
          m_severity(severity),
          m_source(source),
          m_timestamp(std::chrono::system_clock::now()) {}

    /**
     * @brief Virtual destructor
     */
    virtual ~Event() = default;

    /**
     * @brief Gets the event type
     * @return The event type
     */
    EventType getType() const { return m_type; }

    /**
     * @brief Gets the event severity
     * @return The event severity
     */
    EventSeverity getSeverity() const { return m_severity; }

    /**
     * @brief Gets the source of the event
     * @return The source of the event
     */
    const std::string& getSource() const { return m_source; }

    /**
     * @brief Gets the timestamp of the event
     * @return The timestamp of the event
     */
    const std::chrono::system_clock::time_point& getTimestamp() const { return m_timestamp; }

    /**
     * @brief Gets the event name
     * @return The event name
     */
    virtual std::string getName() const { return "Event"; }

    /**
     * @brief Gets detailed information about the event
     * @return A string with detailed information about the event
     */
    virtual std::string getDetails() const { return ""; }

    /**
     * @brief Gets a string representation of the event
     * @return A string representation of the event
     */
    virtual std::string toString() const {
        return getName() + " from " + m_source;
    }

    /**
     * @brief Sets a property on the event
     * @param key The property key
     * @param value The property value
     */
    template<typename T>
    void setProperty(const std::string& key, const T& value) {
        m_properties[key] = value;
    }

    /**
     * @brief Gets a property from the event
     * @param key The property key
     * @return The property value, or nullptr if not found
     */
    template<typename T>
    T* getProperty(const std::string& key) {
        auto it = m_properties.find(key);
        if (it != m_properties.end()) {
            try {
                return std::any_cast<T>(&it->second);
            } catch (const std::bad_any_cast&) {
                return nullptr;
            }
        }
        return nullptr;
    }

    /**
     * @brief Gets a property from the event (const version)
     * @param key The property key
     * @return The property value, or nullptr if not found
     */
    template<typename T>
    const T* getProperty(const std::string& key) const {
        auto it = m_properties.find(key);
        if (it != m_properties.end()) {
            try {
                return std::any_cast<T>(&it->second);
            } catch (const std::bad_any_cast&) {
                return nullptr;
            }
        }
        return nullptr;
    }

protected:
    EventType m_type;
    EventSeverity m_severity;
    std::string m_source;
    std::chrono::system_clock::time_point m_timestamp;
    std::unordered_map<std::string, std::any> m_properties;
};

// Type alias for event pointers
using EventPtr = std::shared_ptr<Event>;
// Type alias for event handler functions
using EventHandler = std::function<void(EventPtr)>;

} // namespace event_system
