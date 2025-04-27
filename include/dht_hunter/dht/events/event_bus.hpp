#pragma once

#include "dht_hunter/dht/events/dht_event.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace dht_hunter::dht::events {

/**
 * @brief Callback type for event handlers
 */
using EventCallback = std::function<void(std::shared_ptr<DHTEvent>)>;

/**
 * @brief Event bus for distributing events to subscribers
 * 
 * This class implements the publish-subscribe pattern for DHT events.
 * Components can subscribe to specific event types and will be notified
 * when those events occur.
 */
class EventBus {
public:
    /**
     * @brief Gets the singleton instance
     * @return The singleton instance
     */
    static std::shared_ptr<EventBus> getInstance();
    
    /**
     * @brief Destructor
     */
    ~EventBus();
    
    /**
     * @brief Subscribes to an event type
     * @param eventType The event type to subscribe to
     * @param callback The callback to invoke when the event occurs
     * @return A subscription ID that can be used to unsubscribe
     */
    int subscribe(DHTEventType eventType, EventCallback callback);
    
    /**
     * @brief Subscribes to multiple event types
     * @param eventTypes The event types to subscribe to
     * @param callback The callback to invoke when any of the events occur
     * @return A vector of subscription IDs that can be used to unsubscribe
     */
    std::vector<int> subscribe(const std::vector<DHTEventType>& eventTypes, EventCallback callback);
    
    /**
     * @brief Unsubscribes from an event
     * @param subscriptionId The subscription ID returned by subscribe
     * @return True if the subscription was found and removed, false otherwise
     */
    bool unsubscribe(int subscriptionId);
    
    /**
     * @brief Publishes an event
     * @param event The event to publish
     */
    void publish(std::shared_ptr<DHTEvent> event);
    
private:
    /**
     * @brief Private constructor for singleton pattern
     */
    EventBus();
    
    // Singleton instance
    static std::shared_ptr<EventBus> s_instance;
    static std::mutex s_instanceMutex;
    
    // Subscription data
    struct Subscription {
        int id;
        DHTEventType eventType;
        EventCallback callback;
    };
    
    std::vector<Subscription> m_subscriptions;
    int m_nextSubscriptionId;
    std::mutex m_subscriptionsMutex;
    
    // Logger
    event::Logger m_logger;
};

} // namespace dht_hunter::dht::events
