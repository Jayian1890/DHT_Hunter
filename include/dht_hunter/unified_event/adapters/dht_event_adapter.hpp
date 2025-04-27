#pragma once

#include "dht_hunter/unified_event/unified_event.hpp"
#include "dht_hunter/dht/types/dht_types.hpp"
#include "dht_hunter/dht/network/message.hpp"
#include "dht_hunter/network/network_address.hpp"
#include <memory>
#include <functional>
#include <string>

namespace dht_hunter::dht {

// Forward declarations
class Node;

namespace events {

// Forward declarations
enum class DHTEventType;
class DHTEvent;
class NodeDiscoveredEvent;
class NodeAddedEvent;
class MessageReceivedEvent;
class MessageSentEvent;
class PeerDiscoveredEvent;
class SystemErrorEvent;
class SystemStartedEvent;
class SystemStoppedEvent;

// Define the EventCallback type
using EventCallback = std::function<void(std::shared_ptr<DHTEvent>)>;

/**
 * @brief Adapter class for the DHT-specific event bus
 *
 * This adapter provides compatibility with the old DHT-specific event bus
 * while actually using the new unified event system under the hood.
 */
class EventBus {
public:
    /**
     * @brief Default constructor
     */
    EventBus() = default;
    /**
     * @brief Gets the singleton instance
     * @return The singleton instance
     */
    static std::shared_ptr<EventBus> getInstance() {
        static std::shared_ptr<EventBus> instance = std::make_shared<EventBus>();
        return instance;
    }

    /**
     * @brief Subscribes to an event type
     * @param eventType The event type to subscribe to
     * @param callback The callback to invoke when the event occurs
     * @return A subscription ID that can be used to unsubscribe
     */
    int subscribe(DHTEventType eventType, EventCallback callback);

    /**
     * @brief Subscribes to multiple event types
     * @param eventTypes The event types to subscribe to
     * @param callback The callback to invoke when any of the events occur
     * @return A vector of subscription IDs that can be used to unsubscribe
     */
    std::vector<int> subscribe(const std::vector<DHTEventType>& eventTypes, EventCallback callback);

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
    void publish(std::shared_ptr<DHTEvent> event);

private:

    /**
     * @brief Converts a DHT event type to a unified event type
     * @param eventType The DHT event type
     * @return The corresponding unified event type
     */
    unified_event::EventType convertEventType(DHTEventType eventType);

    /**
     * @brief Converts a DHT event to a unified event
     * @param event The DHT event
     * @return The corresponding unified event
     */
    std::shared_ptr<unified_event::Event> convertEvent(std::shared_ptr<DHTEvent> event);

    // Map of subscription IDs to unified event system subscription IDs
    std::unordered_map<int, int> m_subscriptionMap;

    // Next subscription ID
    int m_nextSubscriptionId = 1;

    // Mutex for thread safety
    mutable std::mutex m_mutex;
};

/**
 * @brief Base class for all DHT events
 */
class DHTEvent {
public:
    /**
     * @brief Constructor
     * @param type The event type
     */
    explicit DHTEvent(DHTEventType type) : m_type(type) {}

    /**
     * @brief Virtual destructor
     */
    virtual ~DHTEvent() = default;

    /**
     * @brief Gets the event type
     * @return The event type
     */
    DHTEventType getType() const { return m_type; }

protected:
    DHTEventType m_type;
};

/**
 * @brief Event types for the DHT event system
 */
enum class DHTEventType {
    NodeDiscovered,
    NodeAdded,
    MessageReceived,
    MessageSent,
    PeerDiscovered,
    SystemError,
    SystemStarted,
    SystemStopped
};

/**
 * @brief Event for node discovery
 */
class NodeDiscoveredEvent : public DHTEvent {
public:
    /**
     * @brief Constructor
     * @param node The discovered node
     */
    explicit NodeDiscoveredEvent(std::shared_ptr<Node> node)
        : DHTEvent(DHTEventType::NodeDiscovered), m_node(node) {}

    /**
     * @brief Gets the discovered node
     * @return The discovered node
     */
    std::shared_ptr<Node> getNode() const { return m_node; }

private:
    std::shared_ptr<Node> m_node;
};

/**
 * @brief Event for node addition to the routing table
 */
class NodeAddedEvent : public DHTEvent {
public:
    /**
     * @brief Constructor
     * @param node The added node
     * @param bucketIndex The bucket index where the node was added
     */
    NodeAddedEvent(std::shared_ptr<Node> node, size_t bucketIndex)
        : DHTEvent(DHTEventType::NodeAdded), m_node(node), m_bucketIndex(bucketIndex) {}

    /**
     * @brief Gets the added node
     * @return The added node
     */
    std::shared_ptr<Node> getNode() const { return m_node; }

    /**
     * @brief Gets the bucket index
     * @return The bucket index
     */
    size_t getBucketIndex() const { return m_bucketIndex; }

private:
    std::shared_ptr<Node> m_node;
    size_t m_bucketIndex;
};

/**
 * @brief Event for message reception
 */
class MessageReceivedEvent : public DHTEvent {
public:
    /**
     * @brief Constructor
     * @param message The received message
     * @param sender The sender address
     */
    MessageReceivedEvent(std::shared_ptr<Message> message, const network::NetworkAddress& sender)
        : DHTEvent(DHTEventType::MessageReceived), m_message(message), m_sender(sender) {}

    /**
     * @brief Gets the received message
     * @return The received message
     */
    std::shared_ptr<Message> getMessage() const { return m_message; }

    /**
     * @brief Gets the sender address
     * @return The sender address
     */
    const network::NetworkAddress& getSender() const { return m_sender; }

private:
    std::shared_ptr<Message> m_message;
    network::NetworkAddress m_sender;
};

/**
 * @brief Event for message sending
 */
class MessageSentEvent : public DHTEvent {
public:
    /**
     * @brief Constructor
     * @param message The sent message
     * @param recipient The recipient address
     */
    MessageSentEvent(std::shared_ptr<Message> message, const network::NetworkAddress& recipient)
        : DHTEvent(DHTEventType::MessageSent), m_message(message), m_recipient(recipient) {}

    /**
     * @brief Gets the sent message
     * @return The sent message
     */
    std::shared_ptr<Message> getMessage() const { return m_message; }

    /**
     * @brief Gets the recipient address
     * @return The recipient address
     */
    const network::NetworkAddress& getRecipient() const { return m_recipient; }

private:
    std::shared_ptr<Message> m_message;
    network::NetworkAddress m_recipient;
};

/**
 * @brief Event for peer discovery
 */
class PeerDiscoveredEvent : public DHTEvent {
public:
    /**
     * @brief Constructor
     * @param infoHash The info hash
     * @param peer The discovered peer
     */
    PeerDiscoveredEvent(const InfoHash& infoHash, const network::NetworkAddress& peer)
        : DHTEvent(DHTEventType::PeerDiscovered), m_infoHash(infoHash), m_peer(peer) {}

    /**
     * @brief Gets the info hash
     * @return The info hash
     */
    const InfoHash& getInfoHash() const { return m_infoHash; }

    /**
     * @brief Gets the discovered peer
     * @return The discovered peer
     */
    const network::NetworkAddress& getPeer() const { return m_peer; }

private:
    InfoHash m_infoHash;
    network::NetworkAddress m_peer;
};

/**
 * @brief Event for system errors
 */
class SystemErrorEvent : public DHTEvent {
public:
    /**
     * @brief Constructor
     * @param errorMessage The error message
     * @param errorCode The error code
     */
    SystemErrorEvent(const std::string& errorMessage, int errorCode = 0)
        : DHTEvent(DHTEventType::SystemError), m_errorMessage(errorMessage), m_errorCode(errorCode) {}

    /**
     * @brief Gets the error message
     * @return The error message
     */
    const std::string& getErrorMessage() const { return m_errorMessage; }

    /**
     * @brief Gets the error code
     * @return The error code
     */
    int getErrorCode() const { return m_errorCode; }

private:
    std::string m_errorMessage;
    int m_errorCode;
};

/**
 * @brief Event for system startup
 */
class SystemStartedEvent : public DHTEvent {
public:
    /**
     * @brief Constructor
     */
    SystemStartedEvent()
        : DHTEvent(DHTEventType::SystemStarted) {}
};

/**
 * @brief Event for system shutdown
 */
class SystemStoppedEvent : public DHTEvent {
public:
    /**
     * @brief Constructor
     */
    SystemStoppedEvent()
        : DHTEvent(DHTEventType::SystemStopped) {}
};

} // namespace events
} // namespace dht_hunter::dht
