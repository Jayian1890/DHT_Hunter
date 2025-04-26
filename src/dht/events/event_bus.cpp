#include "dht_hunter/dht/events/event_bus.hpp"

namespace dht_hunter::dht::events {

// Initialize static members
std::shared_ptr<EventBus> EventBus::s_instance = nullptr;
std::mutex EventBus::s_instanceMutex;

std::shared_ptr<EventBus> EventBus::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    
    if (!s_instance) {
        s_instance = std::shared_ptr<EventBus>(new EventBus());
    }
    
    return s_instance;
}

EventBus::EventBus()
    : m_nextSubscriptionId(1),
      m_logger(event::Logger::forComponent("DHT.EventBus")) {
}

EventBus::~EventBus() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance.get() == this) {
        s_instance.reset();
    }
}

int EventBus::subscribe(DHTEventType eventType, EventCallback callback) {
    std::lock_guard<std::mutex> lock(m_subscriptionsMutex);
    
    int subscriptionId = m_nextSubscriptionId++;
    m_subscriptions.push_back({subscriptionId, eventType, callback});
    
    m_logger.debug("Subscribed to event type {} with subscription ID {}", 
                  static_cast<int>(eventType), subscriptionId);
    
    return subscriptionId;
}

std::vector<int> EventBus::subscribe(const std::vector<DHTEventType>& eventTypes, EventCallback callback) {
    std::vector<int> subscriptionIds;
    subscriptionIds.reserve(eventTypes.size());
    
    for (const auto& eventType : eventTypes) {
        subscriptionIds.push_back(subscribe(eventType, callback));
    }
    
    return subscriptionIds;
}

bool EventBus::unsubscribe(int subscriptionId) {
    std::lock_guard<std::mutex> lock(m_subscriptionsMutex);
    
    auto it = std::find_if(m_subscriptions.begin(), m_subscriptions.end(),
                          [subscriptionId](const Subscription& subscription) {
                              return subscription.id == subscriptionId;
                          });
    
    if (it != m_subscriptions.end()) {
        m_logger.debug("Unsubscribed from event type {} with subscription ID {}", 
                      static_cast<int>(it->eventType), subscriptionId);
        
        m_subscriptions.erase(it);
        return true;
    }
    
    m_logger.warning("Failed to unsubscribe: subscription ID {} not found", subscriptionId);
    return false;
}

void EventBus::publish(std::shared_ptr<DHTEvent> event) {
    if (!event) {
        m_logger.error("Cannot publish null event");
        return;
    }
    
    DHTEventType eventType = event->getType();
    m_logger.debug("Publishing event of type {}", static_cast<int>(eventType));
    
    // Make a copy of the subscriptions to avoid holding the lock while calling callbacks
    std::vector<EventCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(m_subscriptionsMutex);
        
        for (const auto& subscription : m_subscriptions) {
            if (subscription.eventType == eventType) {
                callbacks.push_back(subscription.callback);
            }
        }
    }
    
    // Call the callbacks
    for (const auto& callback : callbacks) {
        try {
            callback(event);
        } catch (const std::exception& e) {
            m_logger.error("Exception in event callback: {}", e.what());
        } catch (...) {
            m_logger.error("Unknown exception in event callback");
        }
    }
}

} // namespace dht_hunter::dht::events
