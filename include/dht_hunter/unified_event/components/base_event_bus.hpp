#pragma once

#include "dht_hunter/unified_event/interfaces/event_bus_interface.hpp"
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>

namespace dht_hunter::unified_event {

/**
 * @brief Configuration for the event bus
 */
struct EventBusConfig {
    bool asyncProcessing = false;
    size_t eventQueueSize = 1000;
    size_t processingThreads = 1;
};

/**
 * @brief Subscription information
 */
struct Subscription {
    int id;
    EventType eventType;
    EventCallback callback;
};

/**
 * @brief Severity subscription information
 */
struct SeveritySubscription {
    int id;
    EventSeverity severity;
    EventCallback callback;
};

/**
 * @brief Base class for event bus implementations
 * 
 * This class provides common functionality for all event bus implementations.
 */
class BaseEventBus : public EventBusInterface {
public:
    /**
     * @brief Constructs a base event bus
     * @param config The event bus configuration
     */
    explicit BaseEventBus(const EventBusConfig& config);

    /**
     * @brief Destructor
     */
    ~BaseEventBus() override;

    /**
     * @brief Subscribes to an event type
     * @param eventType The event type to subscribe to
     * @param callback The callback to call when an event of this type is published
     * @return The subscription ID
     */
    int subscribe(EventType eventType, EventCallback callback) override;

    /**
     * @brief Subscribes to multiple event types
     * @param eventTypes The event types to subscribe to
     * @param callback The callback to call when an event of any of these types is published
     * @return The subscription IDs
     */
    std::vector<int> subscribe(const std::vector<EventType>& eventTypes, EventCallback callback) override;

    /**
     * @brief Subscribes to events with a specific severity
     * @param severity The severity to subscribe to
     * @param callback The callback to call when an event with this severity is published
     * @return The subscription ID
     */
    int subscribeToSeverity(EventSeverity severity, EventCallback callback) override;

    /**
     * @brief Unsubscribes from an event
     * @param subscriptionId The subscription ID to unsubscribe
     * @return True if the subscription was found and removed, false otherwise
     */
    bool unsubscribe(int subscriptionId) override;

    /**
     * @brief Publishes an event
     * @param event The event to publish
     */
    void publish(std::shared_ptr<Event> event) override;

    /**
     * @brief Adds an event processor
     * @param processor The processor to add
     * @return True if the processor was added successfully, false otherwise
     */
    bool addProcessor(std::shared_ptr<EventProcessor> processor) override;

    /**
     * @brief Removes an event processor
     * @param processorId The ID of the processor to remove
     * @return True if the processor was found and removed, false otherwise
     */
    bool removeProcessor(const std::string& processorId) override;

    /**
     * @brief Gets an event processor
     * @param processorId The ID of the processor to get
     * @return The processor, or nullptr if not found
     */
    std::shared_ptr<EventProcessor> getProcessor(const std::string& processorId) const override;

    /**
     * @brief Starts the event bus
     * @return True if the event bus was started successfully, false otherwise
     */
    bool start() override;

    /**
     * @brief Stops the event bus
     */
    void stop() override;

    /**
     * @brief Checks if the event bus is running
     * @return True if the event bus is running, false otherwise
     */
    bool isRunning() const override;

protected:
    /**
     * @brief Processes an event
     * @param event The event to process
     */
    void processEvent(std::shared_ptr<Event> event);

    /**
     * @brief Processes events from the queue
     */
    void processEvents();

    EventBusConfig m_config;
    
    // Subscriptions
    std::vector<Subscription> m_subscriptions;
    std::vector<SeveritySubscription> m_severitySubscriptions;
    int m_nextSubscriptionId;
    mutable std::mutex m_subscriptionsMutex;
    
    // Processors
    std::unordered_map<std::string, std::shared_ptr<EventProcessor>> m_processors;
    mutable std::mutex m_processorsMutex;
    
    // Async processing
    std::atomic<bool> m_running;
    std::vector<std::thread> m_processingThreads;
    std::queue<std::shared_ptr<Event>> m_eventQueue;
    std::mutex m_eventQueueMutex;
    std::condition_variable m_eventQueueCondition;
};

} // namespace dht_hunter::unified_event
