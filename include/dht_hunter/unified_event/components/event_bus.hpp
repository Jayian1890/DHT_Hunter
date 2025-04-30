#pragma once

#include "dht_hunter/unified_event/components/base_event_bus.hpp"
#include <memory>
#include <mutex>

namespace dht_hunter::unified_event {

/**
 * @brief Central event bus for the unified event system
 * 
 * The EventBus is responsible for:
 * - Publishing events
 * - Managing subscriptions
 * - Routing events to processors
 * - Handling asynchronous event processing
 */
class EventBus : public BaseEventBus, public std::enable_shared_from_this<EventBus> {
public:
    /**
     * @brief Gets the singleton instance
     * @param config Optional configuration for the event bus
     * @return The singleton instance
     */
    static std::shared_ptr<EventBus> getInstance(const EventBusConfig& config = EventBusConfig());

    /**
     * @brief Destructor
     */
    ~EventBus() override;

private:
    /**
     * @brief Private constructor for singleton pattern
     * @param config The event bus configuration
     */
    explicit EventBus(const EventBusConfig& config);

    // Static instance for singleton pattern
    static std::shared_ptr<EventBus> s_instance;
    static std::mutex s_instanceMutex;
};

} // namespace dht_hunter::unified_event
