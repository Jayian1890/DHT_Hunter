#include "dht_hunter/unified_event/adapters/dht_event_adapter.hpp"

namespace dht_hunter::dht::events {

int EventBus::subscribe(DHTEventType eventType, EventCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Convert the DHT event type to a unified event type
    unified_event::EventType unifiedEventType = convertEventType(eventType);
    
    // Subscribe to the unified event system
    int unifiedSubscriptionId = unified_event::EventBus::getInstance()->subscribe(
        unifiedEventType,
        [callback](std::shared_ptr<unified_event::Event> event) {
            // TODO: Convert the unified event to a DHT event
            // For now, just ignore the event
        }
    );
    
    // Generate a new subscription ID for the DHT event system
    int subscriptionId = m_nextSubscriptionId++;
    
    // Store the mapping between the DHT subscription ID and the unified subscription ID
    m_subscriptionMap[subscriptionId] = unifiedSubscriptionId;
    
    return subscriptionId;
}

std::vector<int> EventBus::subscribe(const std::vector<DHTEventType>& eventTypes, EventCallback callback) {
    std::vector<int> subscriptionIds;
    subscriptionIds.reserve(eventTypes.size());
    
    for (const auto& eventType : eventTypes) {
        subscriptionIds.push_back(subscribe(eventType, callback));
    }
    
    return subscriptionIds;
}

bool EventBus::unsubscribe(int subscriptionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Find the unified subscription ID
    auto it = m_subscriptionMap.find(subscriptionId);
    if (it == m_subscriptionMap.end()) {
        return false;
    }
    
    // Unsubscribe from the unified event system
    bool result = unified_event::EventBus::getInstance()->unsubscribe(it->second);
    
    // Remove the mapping
    m_subscriptionMap.erase(it);
    
    return result;
}

void EventBus::publish(std::shared_ptr<DHTEvent> event) {
    if (!event) {
        return;
    }
    
    // Convert the DHT event to a unified event
    std::shared_ptr<unified_event::Event> unifiedEvent = convertEvent(event);
    
    // Publish the unified event
    if (unifiedEvent) {
        unified_event::EventBus::getInstance()->publish(unifiedEvent);
    }
}

unified_event::EventType EventBus::convertEventType(DHTEventType eventType) {
    switch (eventType) {
        case DHTEventType::NodeDiscovered:
            return unified_event::EventType::NodeDiscovered;
        case DHTEventType::NodeAdded:
            return unified_event::EventType::NodeAdded;
        case DHTEventType::MessageReceived:
            return unified_event::EventType::MessageReceived;
        case DHTEventType::MessageSent:
            return unified_event::EventType::MessageSent;
        case DHTEventType::PeerDiscovered:
            return unified_event::EventType::PeerDiscovered;
        case DHTEventType::SystemError:
            return unified_event::EventType::SystemError;
        case DHTEventType::SystemStarted:
            return unified_event::EventType::SystemStarted;
        case DHTEventType::SystemStopped:
            return unified_event::EventType::SystemStopped;
        default:
            return unified_event::EventType::Custom;
    }
}

std::shared_ptr<unified_event::Event> EventBus::convertEvent(std::shared_ptr<DHTEvent> event) {
    if (!event) {
        return nullptr;
    }
    
    // Convert the DHT event to a unified event based on its type
    switch (event->getType()) {
        case DHTEventType::NodeDiscovered: {
            auto nodeEvent = std::dynamic_pointer_cast<NodeDiscoveredEvent>(event);
            if (nodeEvent) {
                return std::make_shared<unified_event::NodeDiscoveredEvent>(
                    "DHT",
                    nodeEvent->getNode()
                );
            }
            break;
        }
        case DHTEventType::NodeAdded: {
            auto nodeEvent = std::dynamic_pointer_cast<NodeAddedEvent>(event);
            if (nodeEvent) {
                return std::make_shared<unified_event::NodeAddedEvent>(
                    "DHT",
                    nodeEvent->getNode(),
                    nodeEvent->getBucketIndex()
                );
            }
            break;
        }
        case DHTEventType::MessageReceived: {
            auto messageEvent = std::dynamic_pointer_cast<MessageReceivedEvent>(event);
            if (messageEvent) {
                return std::make_shared<unified_event::MessageReceivedEvent>(
                    "DHT",
                    messageEvent->getMessage(),
                    messageEvent->getSender()
                );
            }
            break;
        }
        case DHTEventType::MessageSent: {
            auto messageEvent = std::dynamic_pointer_cast<MessageSentEvent>(event);
            if (messageEvent) {
                return std::make_shared<unified_event::MessageSentEvent>(
                    "DHT",
                    messageEvent->getMessage(),
                    messageEvent->getRecipient()
                );
            }
            break;
        }
        case DHTEventType::PeerDiscovered: {
            auto peerEvent = std::dynamic_pointer_cast<PeerDiscoveredEvent>(event);
            if (peerEvent) {
                return std::make_shared<unified_event::PeerDiscoveredEvent>(
                    "DHT",
                    peerEvent->getInfoHash(),
                    peerEvent->getPeer()
                );
            }
            break;
        }
        case DHTEventType::SystemError: {
            auto errorEvent = std::dynamic_pointer_cast<SystemErrorEvent>(event);
            if (errorEvent) {
                return std::make_shared<unified_event::SystemErrorEvent>(
                    "DHT",
                    errorEvent->getErrorMessage(),
                    errorEvent->getErrorCode()
                );
            }
            break;
        }
        case DHTEventType::SystemStarted: {
            return std::make_shared<unified_event::SystemStartedEvent>("DHT");
        }
        case DHTEventType::SystemStopped: {
            return std::make_shared<unified_event::SystemStoppedEvent>("DHT");
        }
        default:
            break;
    }
    
    return nullptr;
}

} // namespace dht_hunter::dht::events
