#pragma once

#include "event_system/event.hpp"
#include <atomic>
#include <future>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <queue>
#include <condition_variable>
#include <functional>
#include <iostream>

namespace event_system {

/**
 * @brief Subscription information
 */
struct Subscription {
    int id;
    EventType eventType;
    EventHandler handler;
};

/**
 * @brief Event bus configuration
 */
struct EventBusConfig {
    bool asyncProcessing = true;
    size_t eventQueueSize = 1000;
};

/**
 * @brief Central event bus for the event system
 * 
 * The EventBus is responsible for:
 * - Publishing events
 * - Managing subscriptions
 * - Routing events to handlers
 * - Handling asynchronous event processing
 */
class EventBus {
public:
    /**
     * @brief Constructor
     * @param config The event bus configuration
     */
    explicit EventBus(const EventBusConfig& config = EventBusConfig())
        : m_config(config),
          m_nextSubscriptionId(1),
          m_running(false) {}

    /**
     * @brief Destructor
     */
    ~EventBus() {
        stop();
    }

    /**
     * @brief Subscribes to an event type
     * @param eventType The event type to subscribe to
     * @param handler The handler to call when an event of this type is published
     * @return The subscription ID
     */
    int subscribe(EventType eventType, EventHandler handler) {
        std::lock_guard<std::mutex> lock(m_subscriptionsMutex);

        int subscriptionId = m_nextSubscriptionId++;
        m_subscriptions.push_back({subscriptionId, eventType, handler});

        return subscriptionId;
    }

    /**
     * @brief Unsubscribes from an event
     * @param subscriptionId The subscription ID to unsubscribe
     * @return True if the subscription was found and removed, false otherwise
     */
    bool unsubscribe(int subscriptionId) {
        std::lock_guard<std::mutex> lock(m_subscriptionsMutex);

        for (auto it = m_subscriptions.begin(); it != m_subscriptions.end(); ++it) {
            if (it->id == subscriptionId) {
                m_subscriptions.erase(it);
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Publishes an event
     * @param event The event to publish
     * @return A future that will be fulfilled when the event is processed
     */
    std::future<void> publish(EventPtr event) {
        if (!event) {
            return std::async(std::launch::deferred, [](){});
        }

        if (m_config.asyncProcessing && m_running) {
            // Asynchronous processing
            std::lock_guard<std::mutex> lock(m_eventQueueMutex);

            // Check if the queue is full
            if (m_eventQueue.size() >= m_config.eventQueueSize) {
                // Queue is full, drop the oldest event
                m_eventQueue.pop();
            }

            m_eventQueue.push(event);
            m_eventQueueCondition.notify_one();

            // Return a future that will be fulfilled when the event is processed
            std::promise<void> promise;
            auto future = promise.get_future();
            m_eventPromises.push(std::move(promise));
            return future;
        } else {
            // Synchronous processing
            return std::async(std::launch::async, [this, event]() {
                processEvent(event);
            });
        }
    }

    /**
     * @brief Starts the event bus
     * @return True if the event bus was started successfully, false otherwise
     */
    bool start() {
        if (m_running) {
            return true;
        }

        m_running = true;

        if (m_config.asyncProcessing) {
            // Start the processing thread
            m_processingThread = std::async(std::launch::async, &EventBus::processEvents, this);
        }

        return true;
    }

    /**
     * @brief Stops the event bus
     */
    void stop() {
        if (!m_running) {
            return;
        }

        m_running = false;

        if (m_config.asyncProcessing) {
            // Notify the processing thread
            m_eventQueueCondition.notify_all();

            // Wait for the processing thread to finish
            if (m_processingThread.valid()) {
                m_processingThread.wait();
            }
        }

        // Clear the event queue
        std::lock_guard<std::mutex> lock(m_eventQueueMutex);
        while (!m_eventQueue.empty()) {
            m_eventQueue.pop();
        }
        
        // Fulfill any remaining promises
        while (!m_eventPromises.empty()) {
            auto& promise = m_eventPromises.front();
            try {
                promise.set_value();
            } catch (const std::future_error&) {
                // Promise might already be satisfied or broken
            }
            m_eventPromises.pop();
        }
    }

    /**
     * @brief Checks if the event bus is running
     * @return True if the event bus is running, false otherwise
     */
    bool isRunning() const {
        return m_running;
    }

private:
    /**
     * @brief Processes an event
     * @param event The event to process
     */
    void processEvent(EventPtr event) {
        // Notify subscribers
        std::vector<EventHandler> handlers;

        {
            std::lock_guard<std::mutex> lock(m_subscriptionsMutex);

            // Find subscribers for this event type
            for (const auto& subscription : m_subscriptions) {
                if (subscription.eventType == event->getType()) {
                    handlers.push_back(subscription.handler);
                }
            }
        }

        // Call the handlers outside the lock to avoid deadlocks
        for (const auto& handler : handlers) {
            try {
                handler(event);
            } catch (const std::exception& e) {
                std::cerr << "Exception in event handler: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown exception in event handler" << std::endl;
            }
        }
    }

    /**
     * @brief Processes events from the queue
     */
    void processEvents() {
        while (m_running) {
            EventPtr event;
            std::promise<void> promise;

            {
                std::unique_lock<std::mutex> lock(m_eventQueueMutex);

                // Wait for an event or stop signal
                m_eventQueueCondition.wait(lock, [this] {
                    return !m_running || !m_eventQueue.empty();
                });

                if (!m_running && m_eventQueue.empty()) {
                    break;
                }

                if (!m_eventQueue.empty()) {
                    event = m_eventQueue.front();
                    m_eventQueue.pop();
                    
                    if (!m_eventPromises.empty()) {
                        promise = std::move(m_eventPromises.front());
                        m_eventPromises.pop();
                    }
                }
            }

            if (event) {
                try {
                    processEvent(event);
                    
                    // Fulfill the promise
                    if (promise.valid()) {
                        promise.set_value();
                    }
                } catch (...) {
                    // Set exception on the promise
                    if (promise.valid()) {
                        try {
                            promise.set_exception(std::current_exception());
                        } catch (const std::future_error&) {
                            // Promise might already be satisfied or broken
                        }
                    }
                }
            }
        }
    }

    EventBusConfig m_config;
    
    // Subscriptions
    std::vector<Subscription> m_subscriptions;
    int m_nextSubscriptionId;
    mutable std::mutex m_subscriptionsMutex;
    
    // Async processing
    std::atomic<bool> m_running;
    std::future<void> m_processingThread;
    std::queue<EventPtr> m_eventQueue;
    std::queue<std::promise<void>> m_eventPromises;
    std::mutex m_eventQueueMutex;
    std::condition_variable m_eventQueueCondition;
};

// Global event bus instance
inline std::shared_ptr<EventBus> getEventBus() {
    static std::shared_ptr<EventBus> instance = std::make_shared<EventBus>();
    static std::once_flag flag;
    
    std::call_once(flag, [&]() {
        instance->start();
    });
    
    return instance;
}

} // namespace event_system
