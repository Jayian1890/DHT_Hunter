#pragma once

#include "dht_hunter/unified_event/event.hpp"
#include "dht_hunter/unified_event/event_processor.hpp"
#include <functional>
#include <memory>
#include <vector>
#include <string>

namespace dht_hunter::unified_event {

/**
 * @brief Callback type for event handlers
 */
using EventCallback = std::function<void(std::shared_ptr<Event>)>;

/**
 * @brief Interface for the event bus
 * 
 * The EventBusInterface defines the common functionality for all event bus implementations.
 */
class EventBusInterface {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~EventBusInterface() = default;

    /**
     * @brief Subscribes to an event type
     * @param eventType The event type to subscribe to
     * @param callback The callback to call when an event of this type is published
     * @return The subscription ID
     */
    virtual int subscribe(EventType eventType, EventCallback callback) = 0;

    /**
     * @brief Subscribes to multiple event types
     * @param eventTypes The event types to subscribe to
     * @param callback The callback to call when an event of any of these types is published
     * @return The subscription IDs
     */
    virtual std::vector<int> subscribe(const std::vector<EventType>& eventTypes, EventCallback callback) = 0;

    /**
     * @brief Subscribes to events with a specific severity
     * @param severity The severity to subscribe to
     * @param callback The callback to call when an event with this severity is published
     * @return The subscription ID
     */
    virtual int subscribeToSeverity(EventSeverity severity, EventCallback callback) = 0;

    /**
     * @brief Unsubscribes from an event
     * @param subscriptionId The subscription ID to unsubscribe
     * @return True if the subscription was found and removed, false otherwise
     */
    virtual bool unsubscribe(int subscriptionId) = 0;

    /**
     * @brief Publishes an event
     * @param event The event to publish
     */
    virtual void publish(std::shared_ptr<Event> event) = 0;

    /**
     * @brief Adds an event processor
     * @param processor The processor to add
     * @return True if the processor was added successfully, false otherwise
     */
    virtual bool addProcessor(std::shared_ptr<EventProcessor> processor) = 0;

    /**
     * @brief Removes an event processor
     * @param processorId The ID of the processor to remove
     * @return True if the processor was found and removed, false otherwise
     */
    virtual bool removeProcessor(const std::string& processorId) = 0;

    /**
     * @brief Gets an event processor
     * @param processorId The ID of the processor to get
     * @return The processor, or nullptr if not found
     */
    virtual std::shared_ptr<EventProcessor> getProcessor(const std::string& processorId) const = 0;

    /**
     * @brief Starts the event bus
     * @return True if the event bus was started successfully, false otherwise
     */
    virtual bool start() = 0;

    /**
     * @brief Stops the event bus
     */
    virtual void stop() = 0;

    /**
     * @brief Checks if the event bus is running
     * @return True if the event bus is running, false otherwise
     */
    virtual bool isRunning() const = 0;
};

} // namespace dht_hunter::unified_event
