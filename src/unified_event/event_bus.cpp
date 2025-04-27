#include "dht_hunter/unified_event/event_bus.hpp"
#include <iostream>

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
    : m_config(config),
      m_nextSubscriptionId(1),
      m_running(false) {
}

EventBus::~EventBus() {
    stop();
    
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance.get() == this) {
        s_instance.reset();
    }
}

int EventBus::subscribe(EventType eventType, EventCallback callback) {
    std::lock_guard<std::mutex> lock(m_subscriptionsMutex);
    
    int subscriptionId = m_nextSubscriptionId++;
    m_subscriptions.push_back({subscriptionId, eventType, callback});
    
    return subscriptionId;
}

std::vector<int> EventBus::subscribe(const std::vector<EventType>& eventTypes, EventCallback callback) {
    std::vector<int> subscriptionIds;
    subscriptionIds.reserve(eventTypes.size());
    
    for (const auto& eventType : eventTypes) {
        subscriptionIds.push_back(subscribe(eventType, callback));
    }
    
    return subscriptionIds;
}

int EventBus::subscribeToSeverity(EventSeverity severity, EventCallback callback) {
    std::lock_guard<std::mutex> lock(m_subscriptionsMutex);
    
    int subscriptionId = m_nextSubscriptionId++;
    m_severitySubscriptions.push_back({subscriptionId, severity, callback});
    
    return subscriptionId;
}

bool EventBus::unsubscribe(int subscriptionId) {
    std::lock_guard<std::mutex> lock(m_subscriptionsMutex);
    
    // Check regular subscriptions
    auto it = std::find_if(m_subscriptions.begin(), m_subscriptions.end(),
                          [subscriptionId](const Subscription& subscription) {
                              return subscription.id == subscriptionId;
                          });
    
    if (it != m_subscriptions.end()) {
        m_subscriptions.erase(it);
        return true;
    }
    
    // Check severity subscriptions
    auto severityIt = std::find_if(m_severitySubscriptions.begin(), m_severitySubscriptions.end(),
                                  [subscriptionId](const SeveritySubscription& subscription) {
                                      return subscription.id == subscriptionId;
                                  });
    
    if (severityIt != m_severitySubscriptions.end()) {
        m_severitySubscriptions.erase(severityIt);
        return true;
    }
    
    return false;
}

void EventBus::publish(std::shared_ptr<Event> event) {
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

bool EventBus::addProcessor(std::shared_ptr<EventProcessor> processor) {
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

bool EventBus::removeProcessor(const std::string& processorId) {
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

std::shared_ptr<EventProcessor> EventBus::getProcessor(const std::string& processorId) const {
    std::lock_guard<std::mutex> lock(m_processorsMutex);
    
    auto it = m_processors.find(processorId);
    if (it == m_processors.end()) {
        return nullptr;
    }
    
    return it->second;
}

bool EventBus::start() {
    if (m_running) {
        return true;
    }
    
    m_running = true;
    
    if (m_config.asyncProcessing) {
        // Start processing threads
        for (size_t i = 0; i < m_config.processingThreads; ++i) {
            m_processingThreads.emplace_back(&EventBus::processEvents, this);
        }
    }
    
    return true;
}

void EventBus::stop() {
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
    
    // Shutdown all processors
    std::lock_guard<std::mutex> lock(m_processorsMutex);
    for (auto& processor : m_processors) {
        processor.second->shutdown();
    }
}

bool EventBus::isRunning() const {
    return m_running;
}

void EventBus::processEvents() {
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

void EventBus::processEvent(std::shared_ptr<Event> event) {
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
    
    // Call the callbacks
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

} // namespace dht_hunter::unified_event
