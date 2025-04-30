#include "dht_hunter/unified_event/components/event_bus.hpp"

namespace dht_hunter::unified_event {

// Initialize static members
std::shared_ptr<EventBus> EventBus::s_instance = nullptr;
std::mutex EventBus::s_instanceMutex;

std::shared_ptr<EventBus> EventBus::getInstance(const EventBusConfig& config) {
    std::lock_guard<std::mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<EventBus>(new EventBus(config));
    }

    return s_instance;
}

EventBus::EventBus(const EventBusConfig& config)
    : BaseEventBus(config) {
}

EventBus::~EventBus() {
    stop();

    // Only reset the instance if we're the current instance
    // Use a try_lock to avoid deadlock if the mutex is already locked
    if (s_instanceMutex.try_lock()) {
        if (s_instance.get() == this) {
            s_instance.reset();
        }
        s_instanceMutex.unlock();
    }
}

} // namespace dht_hunter::unified_event
