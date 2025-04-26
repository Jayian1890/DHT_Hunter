#pragma once

#include <string>
#include <chrono>
#include <unordered_map>

namespace dht_hunter::event {

/**
 * @class Event
 * @brief Base class for all events.
 */
class Event {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~Event() = default;

    /**
     * @brief Get the type of the event.
     * @return The event type.
     */
    virtual std::string getType() const = 0;

    /**
     * @brief Get the timestamp of the event.
     * @return The event timestamp.
     */
    std::chrono::system_clock::time_point getTimestamp() const {
        return m_timestamp;
    }

protected:
    /**
     * @brief Constructor.
     */
    Event() : m_timestamp(std::chrono::system_clock::now()) {}

private:
    std::chrono::system_clock::time_point m_timestamp;
};

} // namespace dht_hunter::event
