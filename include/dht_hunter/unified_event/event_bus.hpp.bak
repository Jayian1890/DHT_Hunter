#pragma once

#include "dht_hunter/unified_event/event.hpp"
#include "dht_hunter/unified_event/event_processor.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>

namespace dht_hunter::unified_event {

/**
 * @brief Callback type for event handlers
 */
using EventCallback = std::function<void(std::shared_ptr<Event>)>;

/**
 * @brief Configuration for the event bus
 */
struct EventBusConfig {
    bool asyncProcessing = false;
    size_t eventQueueSize = 1000;
    size_t processingThreads = 1;
};

/**
 * @brief Central event bus for the unified event system
 * 
 * The EventBus is responsible for:
 * - Publishing events
 * - Managing subscriptions
 * - Routing events to processors
 * - Handling asynchronous event processing
 */
class EventBus : public std::enable_shared_from_this<EventBus> {
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
    ~EventBus();
    
    /**
     * @brief Subscribes to an event type
     * @param eventType The event type to subscribe to
     * @param callback The callback to invoke when the event occurs
     * @return A subscription ID that can be used to unsubscribe
     */
    int subscribe(EventType eventType, EventCallback callback);
    
    /**
     * @brief Subscribes to multiple event types
     * @param eventTypes The event types to subscribe to
     * @param callback The callback to invoke when any of the events occur
     * @return A vector of subscription IDs that can be used to unsubscribe
     */
    std::vector<int> subscribe(const std::vector<EventType>& eventTypes, EventCallback callback);
    
    /**
     * @brief Subscribes to events with a specific severity
     * @param severity The severity to subscribe to
     * @param callback The callback to invoke when an event with the specified severity occurs
     * @return A subscription ID that can be used to unsubscribe
     */
    int subscribeToSeverity(EventSeverity severity, EventCallback callback);
    
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
    void publish(std::shared_ptr<Event> event);
    
    /**
     * @brief Adds an event processor
     * @param processor The processor to add
     * @return True if the processor was added successfully, false otherwise
     */
    bool addProcessor(std::shared_ptr<EventProcessor> processor);
    
    /**
     * @brief Removes an event processor
     * @param processorId The ID of the processor to remove
     * @return True if the processor was found and removed, false otherwise
     */
    bool removeProcessor(const std::string& processorId);
    
    /**
     * @brief Gets a processor by ID
     * @param processorId The ID of the processor to get
     * @return The processor, or nullptr if not found
     */
    std::shared_ptr<EventProcessor> getProcessor(const std::string& processorId) const;
    
    /**
     * @brief Starts the event bus
     * @return True if the event bus was started successfully, false otherwise
     */
    bool start();
    
    /**
     * @brief Stops the event bus
     */
    void stop();
    
    /**
     * @brief Checks if the event bus is running
     * @return True if the event bus is running, false otherwise
     */
    bool isRunning() const;
    
private:
    /**
     * @brief Private constructor for singleton pattern
     * @param config Configuration for the event bus
     */
    explicit EventBus(const EventBusConfig& config);
    
    /**
     * @brief Processes events asynchronously
     */
    void processEvents();
    
    /**
     * @brief Processes a single event
     * @param event The event to process
     */
    void processEvent(std::shared_ptr<Event> event);
    
    // Singleton instance
    static std::shared_ptr<EventBus> s_instance;
    static std::mutex s_instanceMutex;
    
    // Configuration
    EventBusConfig m_config;
    
    // Subscription data
    struct Subscription {
        int id;
        EventType eventType;
        EventCallback callback;
    };
    
    struct SeveritySubscription {
        int id;
        EventSeverity severity;
        EventCallback callback;
    };
    
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
