#include "dht_hunter/unified_event/components/base_event_bus.hpp"
#include <iostream>

namespace dht_hunter::unified_event {

BaseEventBus::BaseEventBus(const EventBusConfig& config)
    : m_config(config),
      m_nextSubscriptionId(1),
      m_running(false) {
}

BaseEventBus::~BaseEventBus() {
    stop();
}

int BaseEventBus::subscribe(EventType eventType, EventCallback callback) {
    std::lock_guard<std::mutex> lock(m_subscriptionsMutex);

    int subscriptionId = m_nextSubscriptionId++;
    m_subscriptions.push_back({subscriptionId, eventType, callback});

    return subscriptionId;
}

std::vector<int> BaseEventBus::subscribe(const std::vector<EventType>& eventTypes, EventCallback callback) {
    std::vector<int> subscriptionIds;
    subscriptionIds.reserve(eventTypes.size());

    for (const auto& eventType : eventTypes) {
        subscriptionIds.push_back(subscribe(eventType, callback));
    }

    return subscriptionIds;
}

int BaseEventBus::subscribeToSeverity(EventSeverity severity, EventCallback callback) {
    std::lock_guard<std::mutex> lock(m_subscriptionsMutex);

    int subscriptionId = m_nextSubscriptionId++;
    m_severitySubscriptions.push_back({subscriptionId, severity, callback});

    return subscriptionId;
}

bool BaseEventBus::unsubscribe(int subscriptionId) {
    std::lock_guard<std::mutex> lock(m_subscriptionsMutex);

    // Check type subscriptions
    for (auto it = m_subscriptions.begin(); it != m_subscriptions.end(); ++it) {
        if (it->id == subscriptionId) {
            m_subscriptions.erase(it);
            return true;
        }
    }

    // Check severity subscriptions
    for (auto it = m_severitySubscriptions.begin(); it != m_severitySubscriptions.end(); ++it) {
        if (it->id == subscriptionId) {
            m_severitySubscriptions.erase(it);
            return true;
        }
    }

    return false;
}

void BaseEventBus::publish(std::shared_ptr<Event> event) {
    if (!event) {
        return;
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
    } else {
        // Synchronous processing
        processEvent(event);
    }
}

bool BaseEventBus::addProcessor(std::shared_ptr<EventProcessor> processor) {
    if (!processor) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_processorsMutex);

    const std::string& processorId = processor->getId();
    if (m_processors.find(processorId) != m_processors.end()) {
        // Processor with this ID already exists
        return false;
    }

    // Initialize the processor
    if (!processor->initialize()) {
        return false;
    }

    m_processors[processorId] = processor;
    return true;
}

bool BaseEventBus::removeProcessor(const std::string& processorId) {
    std::lock_guard<std::mutex> lock(m_processorsMutex);

    auto it = m_processors.find(processorId);
    if (it == m_processors.end()) {
        return false;
    }

    // Shutdown the processor
    it->second->shutdown();

    m_processors.erase(it);
    return true;
}

std::shared_ptr<EventProcessor> BaseEventBus::getProcessor(const std::string& processorId) const {
    std::lock_guard<std::mutex> lock(m_processorsMutex);

    auto it = m_processors.find(processorId);
    if (it == m_processors.end()) {
        return nullptr;
    }

    return it->second;
}

bool BaseEventBus::start() {
    if (m_running) {
        return true;
    }

    m_running = true;

    if (m_config.asyncProcessing) {
        // Start processing threads
        for (size_t i = 0; i < m_config.processingThreads; ++i) {
            m_processingThreads.emplace_back(&BaseEventBus::processEvents, this);
        }
    }

    return true;
}

void BaseEventBus::stop() {
    if (!m_running) {
        return;
    }

    m_running = false;

    if (m_config.asyncProcessing) {
        // Notify all processing threads
        m_eventQueueCondition.notify_all();

        // Wait for all processing threads to finish
        for (auto& thread : m_processingThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        m_processingThreads.clear();
    }

    // Clear the event queue
    std::lock_guard<std::mutex> lock(m_eventQueueMutex);
    while (!m_eventQueue.empty()) {
        m_eventQueue.pop();
    }
}

bool BaseEventBus::isRunning() const {
    return m_running;
}

void BaseEventBus::processEvent(std::shared_ptr<Event> event) {
    // Process the event through processors
    {
        std::lock_guard<std::mutex> lock(m_processorsMutex);

        for (auto& processor : m_processors) {
            if (processor.second->shouldProcess(event)) {
                try {
                    processor.second->process(event);
                } catch (const std::exception& e) {
                    std::cerr << "Exception in processor " << processor.first << ": " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "Unknown exception in processor " << processor.first << std::endl;
                }
            }
        }
    }

    // Notify subscribers
    std::vector<EventCallback> typeCallbacks;
    std::vector<EventCallback> severityCallbacks;

    {
        std::lock_guard<std::mutex> lock(m_subscriptionsMutex);

        // Find subscribers for this event type
        for (const auto& subscription : m_subscriptions) {
            if (subscription.eventType == event->getType()) {
                typeCallbacks.push_back(subscription.callback);
            }
        }

        // Find subscribers for this event severity
        for (const auto& subscription : m_severitySubscriptions) {
            if (subscription.severity == event->getSeverity()) {
                severityCallbacks.push_back(subscription.callback);
            }
        }
    }

    // Call the callbacks outside the lock to avoid deadlocks
    for (const auto& callback : typeCallbacks) {
        try {
            callback(event);
        } catch (const std::exception& e) {
            std::cerr << "Exception in event callback: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown exception in event callback" << std::endl;
        }
    }

    for (const auto& callback : severityCallbacks) {
        try {
            callback(event);
        } catch (const std::exception& e) {
            std::cerr << "Exception in severity callback: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown exception in severity callback" << std::endl;
        }
    }
}

void BaseEventBus::processEvents() {
    while (m_running) {
        std::shared_ptr<Event> event;

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
            }
        }

        if (event) {
            processEvent(event);
        }
    }
}

} // namespace dht_hunter::unified_event
