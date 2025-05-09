#include "utils/event_utils.hpp"
#include <iostream>

namespace dht_hunter::utils::event {

//=============================================================================
// EventBus Implementation
//=============================================================================

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

EventBus::EventBus() : m_nextSubscriptionId(1), m_running(false) {
}

EventBus::~EventBus() {
    stop();
}

void EventBus::publish(const std::shared_ptr<Event>& event) {
    if (!event) {
        return;
    }

    // Set the timestamp if not already set
    if (event->getTimestamp() == std::chrono::system_clock::time_point()) {
        event->setTimestamp(std::chrono::system_clock::now());
    }

    // Add the event to the queue
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_eventQueue.push(event);
    }

    // Notify the processing thread
    m_queueCondition.notify_one();
}

int EventBus::subscribe(const std::string& eventType, const std::shared_ptr<EventHandler>& handler) {
    if (!handler) {
        return -1;
    }

    std::lock_guard<std::mutex> lock(m_subscriptionsMutex);
    int subscriptionId = m_nextSubscriptionId++;
    m_subscriptions[subscriptionId] = {eventType, handler};
    return subscriptionId;
}

bool EventBus::unsubscribe(int subscriptionId) {
    std::lock_guard<std::mutex> lock(m_subscriptionsMutex);
    return m_subscriptions.erase(subscriptionId) > 0;
}

void EventBus::start() {
    if (m_running) {
        return;
    }

    m_running = true;
    m_processingThread = std::thread(&EventBus::processEvents, this);
}

void EventBus::stop() {
    if (!m_running) {
        return;
    }

    m_running = false;
    m_queueCondition.notify_all();
    if (m_processingThread.joinable()) {
        m_processingThread.join();
    }
}

bool EventBus::isRunning() const {
    return m_running;
}

void EventBus::processEvents() {
    while (m_running) {
        std::shared_ptr<Event> event;

        // Get the next event from the queue
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCondition.wait(lock, [this] { return !m_running || !m_eventQueue.empty(); });

            if (!m_running && m_eventQueue.empty()) {
                break;
            }

            if (!m_eventQueue.empty()) {
                event = m_eventQueue.front();
                m_eventQueue.pop();
            }
        }

        // Process the event
        if (event) {
            std::string eventType = event->getType();

            // Get a copy of the subscriptions to avoid holding the lock while calling handlers
            std::vector<std::shared_ptr<EventHandler>> handlers;
            {
                std::lock_guard<std::mutex> lock(m_subscriptionsMutex);
                for (const auto& pair : m_subscriptions) {
                    const auto& subscription = pair.second;
                    if (subscription.eventType == eventType || subscription.eventType == "*") {
                        handlers.push_back(subscription.handler);
                    }
                }
            }

            // Call the handlers
            for (const auto& handler : handlers) {
                try {
                    handler->handleEvent(event);
                } catch (const std::exception& e) {
                    std::cerr << "Exception in event handler: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "Unknown exception in event handler" << std::endl;
                }
            }
        }
    }
}

//=============================================================================
// EventFactory Implementation
//=============================================================================

// Initialize static members
std::shared_ptr<EventFactory> EventFactory::s_instance = nullptr;
std::mutex EventFactory::s_instanceMutex;

std::shared_ptr<EventFactory> EventFactory::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (!s_instance) {
        s_instance = std::shared_ptr<EventFactory>(new EventFactory());
    }
    return s_instance;
}

void EventFactory::registerEventCreator(const std::string& eventType, std::function<std::shared_ptr<Event>()> creator) {
    std::lock_guard<std::mutex> lock(m_creatorsMutex);
    m_creators[eventType] = creator;
}

std::shared_ptr<Event> EventFactory::createEvent(const std::string& eventType) {
    std::lock_guard<std::mutex> lock(m_creatorsMutex);
    auto it = m_creators.find(eventType);
    if (it != m_creators.end()) {
        return it->second();
    }
    return nullptr;
}

//=============================================================================
// EventProcessor Implementation
//=============================================================================

class EventProcessor::EventHandlerImpl : public EventHandler {
public:
    EventHandlerImpl(EventProcessor* processor) : m_processor(processor) {}

    void handleEvent(const std::shared_ptr<Event>& event) override {
        if (m_processor) {
            m_processor->processEvent(event);
        }
    }

    std::vector<std::string> getHandledEventTypes() const override {
        if (m_processor) {
            return m_processor->getProcessedEventTypes();
        }
        return {};
    }

private:
    EventProcessor* m_processor;
};

EventProcessor::EventProcessor() : m_running(false) {
    m_handler = std::make_shared<EventHandlerImpl>(this);
}

EventProcessor::~EventProcessor() {
    stop();
}

void EventProcessor::start() {
    if (m_running) {
        return;
    }

    m_running = true;
    subscribeToEvents();
}

void EventProcessor::stop() {
    if (!m_running) {
        return;
    }

    m_running = false;
    unsubscribeFromEvents();
}

bool EventProcessor::isRunning() const {
    return m_running;
}

void EventProcessor::subscribeToEvents() {
    auto eventBus = getEventBus();
    if (!eventBus) {
        return;
    }

    // Subscribe to all event types this processor can handle
    for (const auto& eventType : getProcessedEventTypes()) {
        int subscriptionId = eventBus->subscribe(eventType, m_handler);
        if (subscriptionId >= 0) {
            m_subscriptionIds.push_back(subscriptionId);
        }
    }
}

void EventProcessor::unsubscribeFromEvents() {
    auto eventBus = getEventBus();
    if (!eventBus) {
        return;
    }

    // Unsubscribe from all event types
    for (int subscriptionId : m_subscriptionIds) {
        eventBus->unsubscribe(subscriptionId);
    }
    m_subscriptionIds.clear();
}

std::shared_ptr<EventBus> EventProcessor::getEventBus() const {
    return EventBus::getInstance();
}

} // namespace dht_hunter::utils::event
