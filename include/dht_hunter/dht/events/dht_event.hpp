#pragma once

#include "dht_hunter/dht/types/dht_types.hpp"
#include "dht_hunter/dht/network/message.hpp"
#include "dht_hunter/network/network_address.hpp"
#include <string>
#include <memory>
#include <variant>
#include <vector>

namespace dht_hunter::dht::events {

/**
 * @brief Event types for the DHT system
 */
enum class DHTEventType {
    // Node events
    NodeDiscovered,
    NodeAdded,
    NodeRemoved,
    NodeUpdated,

    // Routing table events
    BucketRefreshed,
    BucketSplit,
    RoutingTableSaved,
    RoutingTableLoaded,

    // Message events
    MessageSent,
    MessageReceived,
    MessageError,

    // Lookup events
    LookupStarted,
    LookupProgress,
    LookupCompleted,
    LookupFailed,

    // Peer events
    PeerDiscovered,
    PeerAnnounced,

    // System events
    SystemStarted,
    SystemStopped,
    SystemError
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
     * @brief Destructor
     */
    virtual ~DHTEvent() = default;

    /**
     * @brief Gets the event type
     * @return The event type
     */
    DHTEventType getType() const { return m_type; }

private:
    DHTEventType m_type;
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
     * @param sender The sender endpoint
     */
    MessageReceivedEvent(std::shared_ptr<Message> message, const network::NetworkAddress& sender)
        : DHTEvent(DHTEventType::MessageReceived), m_message(message), m_sender(sender) {}

    /**
     * @brief Gets the received message
     * @return The received message
     */
    std::shared_ptr<Message> getMessage() const { return m_message; }

    /**
     * @brief Gets the sender endpoint
     * @return The sender endpoint
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
     * @param recipient The recipient endpoint
     */
    MessageSentEvent(std::shared_ptr<Message> message, const network::NetworkAddress& recipient)
        : DHTEvent(DHTEventType::MessageSent), m_message(message), m_recipient(recipient) {}

    /**
     * @brief Gets the sent message
     * @return The sent message
     */
    std::shared_ptr<Message> getMessage() const { return m_message; }

    /**
     * @brief Gets the recipient endpoint
     * @return The recipient endpoint
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
 * @brief Event for system started
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
 * @brief Event for system stopped
 */
class SystemStoppedEvent : public DHTEvent {
public:
    /**
     * @brief Constructor
     */
    SystemStoppedEvent()
        : DHTEvent(DHTEventType::SystemStopped) {}
};

} // namespace dht_hunter::dht::events
