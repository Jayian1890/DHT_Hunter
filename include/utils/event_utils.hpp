#pragma once

/**
 * @file event_utils.hpp
 * @brief Consolidated event system utilities
 *
 * This file contains consolidated event system components including:
 * - Event system core components
 * - Event adapters
 * - Event processors
 */

// Standard library includes
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <map>
#include <unordered_map>
#include <queue>
#include <any>
#include <typeindex>
#include <typeinfo>
#include <chrono>
#include <condition_variable>

// Project includes
#include "utils/common_utils.hpp"
#include "utils/system_utils.hpp"

namespace dht_hunter::utils::event {

//=============================================================================
// Event Base Class
//=============================================================================

/**
 * @class Event
 * @brief Base class for all events
 */
class Event {
public:
    /**
     * @brief Default constructor
     */
    Event() = default;

    /**
     * @brief Virtual destructor
     */
    virtual ~Event() = default;

    /**
     * @brief Get the event type
     * @return The event type
     */
    virtual std::string getType() const = 0;

    /**
     * @brief Get the event timestamp
     * @return The event timestamp
     */
    std::chrono::system_clock::time_point getTimestamp() const {
        return m_timestamp;
    }

    /**
     * @brief Set the event timestamp
     * @param timestamp The event timestamp
     */
    void setTimestamp(const std::chrono::system_clock::time_point& timestamp) {
        m_timestamp = timestamp;
    }

    /**
     * @brief Get the event ID
     * @return The event ID
     */
    const std::string& getID() const {
        return m_id;
    }

    /**
     * @brief Set the event ID
     * @param id The event ID
     */
    void setID(const std::string& id) {
        m_id = id;
    }

private:
    std::chrono::system_clock::time_point m_timestamp = std::chrono::system_clock::now();
    std::string m_id;
};

//=============================================================================
// Event Handler
//=============================================================================

/**
 * @class EventHandler
 * @brief Base class for event handlers
 */
class EventHandler {
public:
    /**
     * @brief Default constructor
     */
    EventHandler() = default;

    /**
     * @brief Virtual destructor
     */
    virtual ~EventHandler() = default;

    /**
     * @brief Handle an event
     * @param event The event to handle
     */
    virtual void handleEvent(const std::shared_ptr<Event>& event) = 0;

    /**
     * @brief Get the event types this handler can handle
     * @return The event types
     */
    virtual std::vector<std::string> getHandledEventTypes() const = 0;
};

//=============================================================================
// Event Bus
//=============================================================================

/**
 * @class EventBus
 * @brief Central event bus for publishing and subscribing to events
 */
class EventBus {
public:
    /**
     * @brief Get the singleton instance
     * @return The singleton instance
     */
    static std::shared_ptr<EventBus> getInstance();

    /**
     * @brief Destructor
     */
    ~EventBus();

    /**
     * @brief Publish an event
     * @param event The event to publish
     */
    void publish(const std::shared_ptr<Event>& event);

    /**
     * @brief Subscribe to an event type
     * @param eventType The event type to subscribe to
     * @param handler The handler to call when an event of this type is published
     * @return A subscription ID that can be used to unsubscribe
     */
    int subscribe(const std::string& eventType, const std::shared_ptr<EventHandler>& handler);

    /**
     * @brief Unsubscribe from an event type
     * @param subscriptionId The subscription ID returned by subscribe
     * @return True if the subscription was found and removed, false otherwise
     */
    bool unsubscribe(int subscriptionId);

    /**
     * @brief Start the event processing thread
     */
    void start();

    /**
     * @brief Stop the event processing thread
     */
    void stop();

    /**
     * @brief Check if the event bus is running
     * @return True if the event bus is running, false otherwise
     */
    bool isRunning() const;

private:
    /**
     * @brief Constructor
     */
    EventBus();

    /**
     * @brief Event processing thread function
     */
    void processEvents();

    // Singleton instance
    static std::shared_ptr<EventBus> s_instance;
    static std::mutex s_instanceMutex;

    // Event queue
    std::queue<std::shared_ptr<Event>> m_eventQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;

    // Subscribers
    struct Subscription {
        std::string eventType;
        std::shared_ptr<EventHandler> handler;
    };
    std::unordered_map<int, Subscription> m_subscriptions;
    std::mutex m_subscriptionsMutex;
    int m_nextSubscriptionId;

    // Thread control
    std::atomic<bool> m_running;
    std::thread m_processingThread;
};

//=============================================================================
// Event Factory
//=============================================================================

/**
 * @class EventFactory
 * @brief Factory for creating events
 */
class EventFactory {
public:
    /**
     * @brief Get the singleton instance
     * @return The singleton instance
     */
    static std::shared_ptr<EventFactory> getInstance();

    /**
     * @brief Destructor
     */
    ~EventFactory() = default;

    /**
     * @brief Register an event creator function
     * @param eventType The event type
     * @param creator The creator function
     */
    void registerEventCreator(const std::string& eventType, std::function<std::shared_ptr<Event>()> creator);

    /**
     * @brief Create an event
     * @param eventType The event type
     * @return The created event, or nullptr if the event type is not registered
     */
    std::shared_ptr<Event> createEvent(const std::string& eventType);

private:
    /**
     * @brief Constructor
     */
    EventFactory() = default;

    // Singleton instance
    static std::shared_ptr<EventFactory> s_instance;
    static std::mutex s_instanceMutex;

    // Event creators
    std::unordered_map<std::string, std::function<std::shared_ptr<Event>()>> m_creators;
    std::mutex m_creatorsMutex;
};

//=============================================================================
// Event Processor
//=============================================================================

/**
 * @class EventProcessor
 * @brief Base class for event processors
 */
class EventProcessor {
public:
    /**
     * @brief Default constructor
     */
    EventProcessor();

    /**
     * @brief Virtual destructor
     */
    virtual ~EventProcessor();

    /**
     * @brief Start the processor
     */
    virtual void start();

    /**
     * @brief Stop the processor
     */
    virtual void stop();

    /**
     * @brief Check if the processor is running
     * @return True if the processor is running, false otherwise
     */
    bool isRunning() const;

protected:
    /**
     * @brief Process an event
     * @param event The event to process
     */
    virtual void processEvent(const std::shared_ptr<Event>& event) = 0;

    /**
     * @brief Get the event types this processor can handle
     * @return The event types
     */
    virtual std::vector<std::string> getProcessedEventTypes() const = 0;

    /**
     * @brief Subscribe to events
     */
    void subscribeToEvents();

    /**
     * @brief Unsubscribe from events
     */
    void unsubscribeFromEvents();

    /**
     * @brief Get the event bus
     * @return The event bus
     */
    std::shared_ptr<EventBus> getEventBus() const;

private:
    class EventHandlerImpl;
    std::shared_ptr<EventHandlerImpl> m_handler;
    std::vector<int> m_subscriptionIds;
    std::atomic<bool> m_running;
};

} // namespace dht_hunter::utils::event
