#include "dht_hunter/dht/core/dht_node.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include "dht_hunter/dht/storage/peer_storage.hpp"

namespace dht_hunter::dht {

void DHTNode::subscribeToEvents() {
    m_logger.debug("Subscribing to DHT events");

    // Subscribe to node discovered events
    m_eventSubscriptionIds.push_back(
        m_eventBus->subscribe(unified_event::EventType::NodeDiscovered,
            [this](std::shared_ptr<unified_event::Event> event) {
                handleNodeDiscoveredEvent(event);
            }
        )
    );

    // Subscribe to peer discovered events
    m_eventSubscriptionIds.push_back(
        m_eventBus->subscribe(unified_event::EventType::PeerDiscovered,
            [this](std::shared_ptr<unified_event::Event> event) {
                handlePeerDiscoveredEvent(event);
            }
        )
    );

    // Subscribe to system error events
    m_eventSubscriptionIds.push_back(
        m_eventBus->subscribe(unified_event::EventType::SystemError,
            [this](std::shared_ptr<unified_event::Event> event) {
                handleSystemErrorEvent(event);
            }
        )
    );

    m_logger.debug("Subscribed to {} event types", m_eventSubscriptionIds.size());
}

void DHTNode::handleNodeDiscoveredEvent(std::shared_ptr<unified_event::Event> event) {
    auto nodeDiscoveredEvent = std::dynamic_pointer_cast<unified_event::NodeDiscoveredEvent>(event);
    if (!nodeDiscoveredEvent) {
        m_logger.error("Failed to cast event to NodeDiscoveredEvent");
        return;
    }

    auto node = nodeDiscoveredEvent->getNode();
    if (!node) {
        m_logger.error("Node in NodeDiscoveredEvent is null");
        return;
    }

    m_logger.debug("Node discovered: {}", node->getID().toString());

    // Add the node to the routing table
    if (m_routingManager) {
        m_routingManager->addNode(node);
    }
}

void DHTNode::handlePeerDiscoveredEvent(std::shared_ptr<unified_event::Event> event) {
    auto peerDiscoveredEvent = std::dynamic_pointer_cast<unified_event::PeerDiscoveredEvent>(event);
    if (!peerDiscoveredEvent) {
        m_logger.error("Failed to cast event to PeerDiscoveredEvent");
        return;
    }

    const auto& infoHash = peerDiscoveredEvent->getInfoHash();
    const auto& peer = peerDiscoveredEvent->getPeer();

    m_logger.debug("Peer discovered for info hash {}: {}", infoHashToString(infoHash), peer.toString());

    // Add the peer to the peer storage
    if (m_peerStorage) {
        // Create an endpoint from the network address
        network::EndPoint endpoint(peer, 0); // Use port 0 as it's not available in the event
        m_peerStorage->addPeer(infoHash, endpoint);
    }
}

void DHTNode::handleSystemErrorEvent(std::shared_ptr<unified_event::Event> event) {
    auto systemErrorEvent = std::dynamic_pointer_cast<unified_event::SystemErrorEvent>(event);
    if (!systemErrorEvent) {
        m_logger.error("Failed to cast event to SystemErrorEvent");
        return;
    }

    m_logger.error("System error: {} (code: {})",
                  systemErrorEvent->getErrorMessage(),
                  systemErrorEvent->getErrorCode());

    // Handle the error based on its severity
    // For now, just log it
}

} // namespace dht_hunter::dht
