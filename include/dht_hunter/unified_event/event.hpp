#pragma once

#include "dht_hunter/unified_event/event_types_adapter.hpp"
#include <string>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <any>

namespace dht_hunter::unified_event {

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
    Event(EventType type, EventSeverity severity, const std::string& source);

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
    virtual std::string getName() const;

    /**
     * @brief Gets detailed information about the event
     * @return A string with detailed information about the event
     */
    virtual std::string getDetails() const {
        return "";
    }

    /**
     * @brief Gets a string representation of the event
     * @return A string representation of the event
     */
    virtual std::string toString() const;

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

/**
 * @brief Converts a string to an EventSeverity
 * @param severityStr The severity string
 * @return The corresponding EventSeverity, or EventSeverity::Info if not recognized
 */
EventSeverity stringToEventSeverity(const std::string& severityStr);

} // namespace dht_hunter::unified_event
