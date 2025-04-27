#include "dht_hunter/unified_event/unified_event.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace dht_hunter::unified_event;

// Example component that publishes events
class ExamplePublisher {
public:
    ExamplePublisher(const std::string& name)
        : m_name(name),
          m_eventBus(EventBus::getInstance()) {
    }
    
    void start() {
        // Publish a system started event
        auto startedEvent = std::make_shared<SystemStartedEvent>(m_name);
        m_eventBus->publish(startedEvent);
        
        logInfo(m_name, "Publisher started");
    }
    
    void publishEvents() {
        // Publish some log events
        logDebug(m_name, "This is a debug message");
        logInfo(m_name, "This is an info message");
        logWarning(m_name, "This is a warning message");
        
        // Publish a custom event
        auto errorEvent = std::make_shared<SystemErrorEvent>(m_name, "Something went wrong", 123);
        m_eventBus->publish(errorEvent);
    }
    
    void stop() {
        // Publish a system stopped event
        auto stoppedEvent = std::make_shared<SystemStoppedEvent>(m_name);
        m_eventBus->publish(stoppedEvent);
        
        logInfo(m_name, "Publisher stopped");
    }
    
private:
    std::string m_name;
    std::shared_ptr<EventBus> m_eventBus;
};

// Example component that subscribes to events
class ExampleSubscriber {
public:
    ExampleSubscriber(const std::string& name)
        : m_name(name),
          m_eventBus(EventBus::getInstance()) {
    }
    
    void start() {
        // Subscribe to system events
        m_subscriptionIds.push_back(
            m_eventBus->subscribe(EventType::SystemStarted,
                [this](std::shared_ptr<Event> event) {
                    handleSystemStartedEvent(event);
                }
            )
        );
        
        m_subscriptionIds.push_back(
            m_eventBus->subscribe(EventType::SystemStopped,
                [this](std::shared_ptr<Event> event) {
                    handleSystemStoppedEvent(event);
                }
            )
        );
        
        m_subscriptionIds.push_back(
            m_eventBus->subscribe(EventType::SystemError,
                [this](std::shared_ptr<Event> event) {
                    handleSystemErrorEvent(event);
                }
            )
        );
        
        // Subscribe to all events with error severity
        m_subscriptionIds.push_back(
            m_eventBus->subscribeToSeverity(EventSeverity::Error,
                [this](std::shared_ptr<Event> event) {
                    handleErrorSeverityEvent(event);
                }
            )
        );
        
        logInfo(m_name, "Subscriber started");
    }
    
    void stop() {
        // Unsubscribe from all events
        for (int subscriptionId : m_subscriptionIds) {
            m_eventBus->unsubscribe(subscriptionId);
        }
        m_subscriptionIds.clear();
        
        logInfo(m_name, "Subscriber stopped");
    }
    
private:
    void handleSystemStartedEvent(std::shared_ptr<Event> event) {
        logInfo(m_name, "Received system started event from " + event->getSource());
    }
    
    void handleSystemStoppedEvent(std::shared_ptr<Event> event) {
        logInfo(m_name, "Received system stopped event from " + event->getSource());
    }
    
    void handleSystemErrorEvent(std::shared_ptr<Event> event) {
        auto errorEvent = std::dynamic_pointer_cast<SystemErrorEvent>(event);
        if (errorEvent) {
            logError(m_name, "Received system error event from " + event->getSource() +
                    ": " + errorEvent->getErrorMessage() +
                    " (code: " + std::to_string(errorEvent->getErrorCode()) + ")");
        }
    }
    
    void handleErrorSeverityEvent(std::shared_ptr<Event> event) {
        logWarning(m_name, "Received error severity event: " + event->getName() + " from " + event->getSource());
    }
    
    std::string m_name;
    std::shared_ptr<EventBus> m_eventBus;
    std::vector<int> m_subscriptionIds;
};

int main() {
    // Initialize the unified event system
    if (!initializeEventSystem(true, true, true, false)) {
        std::cerr << "Failed to initialize the unified event system" << std::endl;
        return 1;
    }
    
    // Create a publisher and a subscriber
    ExamplePublisher publisher("Publisher");
    ExampleSubscriber subscriber("Subscriber");
    
    // Start the components
    subscriber.start();
    publisher.start();
    
    // Publish some events
    publisher.publishEvents();
    
    // Wait a bit to allow events to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Get statistics
    auto statisticsProcessor = getStatisticsProcessor();
    if (statisticsProcessor) {
        std::cout << "\nEvent Statistics:\n" << statisticsProcessor->getStatisticsAsJson() << std::endl;
    }
    
    // Stop the components
    publisher.stop();
    subscriber.stop();
    
    // Shutdown the unified event system
    shutdownEventSystem();
    
    return 0;
}
