#include "dht_hunter/dht/network/bt_message_handler.hpp"
#include "dht_hunter/dht/routing/routing_manager.hpp"
#include "dht_hunter/network/end_point.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<BTMessageHandler> BTMessageHandler::s_instance = nullptr;
std::mutex BTMessageHandler::s_instanceMutex;

std::shared_ptr<BTMessageHandler> BTMessageHandler::getInstance(
    std::shared_ptr<RoutingManager> routingManager) {
    
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    
    if (!s_instance) {
        s_instance = std::shared_ptr<BTMessageHandler>(new BTMessageHandler(routingManager));
    } else if (routingManager && !s_instance->m_routingManager) {
        // Update the routing manager if it wasn't set before
        s_instance->m_routingManager = routingManager;
    }
    
    return s_instance;
}

BTMessageHandler::BTMessageHandler(std::shared_ptr<RoutingManager> routingManager)
    : m_routingManager(routingManager),
      m_logger(event::Logger::forComponent("DHT.BTMessageHandler")) {
}

BTMessageHandler::~BTMessageHandler() {
    // Clear the singleton instance
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance.get() == this) {
        s_instance.reset();
    }
}

bool BTMessageHandler::handlePortMessage(const network::NetworkAddress& peerAddress, const uint8_t* data, size_t length) {
    if (!m_routingManager) {
        m_logger.error("No routing manager available");
        return false;
    }
    
    if (length < 3) {
        m_logger.error("Invalid PORT message: too short");
        return false;
    }
    
    // The PORT message format is:
    // <len=0003><id=9><listen-port>
    // Where listen-port is a 2-byte big-endian integer
    
    // Extract the port from the message (big-endian)
    uint16_t port = (static_cast<uint16_t>(data[1]) << 8) | static_cast<uint16_t>(data[2]);
    
    m_logger.debug("Received PORT message from {}, DHT port: {}", peerAddress.toString(), port);
    
    // Create an endpoint for the peer's DHT node
    network::EndPoint endpoint(peerAddress, port);
    
    // Create a node with a random ID (will be updated when we communicate with it)
    auto node = std::make_shared<Node>(NodeID::random(), endpoint);
    
    // Add the node to the routing table
    // The routing manager will verify the node before adding it
    if (m_routingManager->addNode(node)) {
        m_logger.debug("Added DHT node from PORT message: {}:{}", peerAddress.toString(), port);
        return true;
    } else {
        m_logger.warning("Failed to add DHT node from PORT message: {}:{}", peerAddress.toString(), port);
        return false;
    }
}

} // namespace dht_hunter::dht
