#pragma once

#include "dht_hunter/event/event.hpp"
#include "dht_hunter/event/event_handler.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <typeindex>

namespace dht_hunter::event {

/**
 * @class EventBus
 * @brief A simple event bus for publishing and subscribing to events.
 */
class EventBus {
public:
    /**
     * @brief Get the singleton instance of the event bus.
     * @return The singleton instance.
     */
    static EventBus& getInstance() {
        static EventBus instance;
        return instance;
    }

    /**
     * @brief Publish an event.
     * @param event The event to publish.
     */
    void publish(const std::shared_ptr<Event>& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        const auto& type = event->getType();
        
        // Call handlers for this specific event type
        auto it = m_handlers.find(type);
        if (it != m_handlers.end()) {
            for (const auto& handler : it->second) {
                handler->handle(event);
            }
        }
        
        // Call handlers for all events
        it = m_handlers.find("*");
        if (it != m_handlers.end()) {
            for (const auto& handler : it->second) {
                handler->handle(event);
            }
        }
    }

    /**
     * @brief Subscribe to events of a specific type.
     * @param type The event type to subscribe to, or "*" for all events.
     * @param handler The handler to call when an event of the specified type is published.
     */
    void subscribe(const std::string& type, std::shared_ptr<EventHandler> handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handlers[type].push_back(std::move(handler));
    }

private:
    /**
     * @brief Private constructor to enforce singleton pattern.
     */
    EventBus() = default;

    /**
     * @brief Private destructor.
     */
    ~EventBus() = default;

    /**
     * @brief Delete copy constructor.
     */
    EventBus(const EventBus&) = delete;

    /**
     * @brief Delete assignment operator.
     */
    EventBus& operator=(const EventBus&) = delete;

    std::mutex m_mutex;
    std::unordered_map<std::string, std::vector<std::shared_ptr<EventHandler>>> m_handlers;
};

} // namespace dht_hunter::event
