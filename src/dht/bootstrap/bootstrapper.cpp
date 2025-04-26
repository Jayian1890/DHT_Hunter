#include "dht_hunter/dht/bootstrap/bootstrapper.hpp"
#include "dht_hunter/dht/routing/routing_manager.hpp"
#include "dht_hunter/dht/routing/node_lookup.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include <random>
#include <netdb.h>
#include <arpa/inet.h>

namespace dht_hunter::dht {

Bootstrapper::Bootstrapper(const DHTConfig& config,
                         const NodeID& nodeID,
                         std::shared_ptr<RoutingManager> routingManager,
                         std::shared_ptr<NodeLookup> nodeLookup)
    : m_config(config),
      m_nodeID(nodeID),
      m_routingManager(routingManager),
      m_nodeLookup(nodeLookup),
      m_logger(event::Logger::forComponent("DHT.Bootstrapper")) {
    m_logger.info("Creating bootstrapper");
}

void Bootstrapper::bootstrap(std::function<void(bool)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_logger.info("Bootstrapping DHT node");
    
    // Check if we already have nodes in the routing table
    if (m_routingManager && m_routingManager->getNodeCount() > 0) {
        m_logger.info("Routing table already has {} nodes", m_routingManager->getNodeCount());
        
        // Perform a random lookup to refresh the routing table
        performRandomLookup(callback);
        return;
    }
    
    // Get the bootstrap nodes
    const auto& bootstrapNodes = m_config.getBootstrapNodes();
    
    if (bootstrapNodes.empty()) {
        m_logger.error("No bootstrap nodes configured");
        if (callback) {
            callback(false);
        }
        return;
    }
    
    // Resolve the bootstrap nodes
    std::vector<network::EndPoint> endpoints;
    
    for (const auto& node : bootstrapNodes) {
        auto resolvedEndpoints = resolveBootstrapNode(node);
        endpoints.insert(endpoints.end(), resolvedEndpoints.begin(), resolvedEndpoints.end());
    }
    
    if (endpoints.empty()) {
        m_logger.error("Failed to resolve any bootstrap nodes");
        if (callback) {
            callback(false);
        }
        return;
    }
    
    m_logger.info("Resolved {} bootstrap endpoints", endpoints.size());
    
    // Add the bootstrap nodes to the routing table
    for (const auto& endpoint : endpoints) {
        // Generate a random node ID for the bootstrap node
        // This will be replaced with the actual node ID when we receive a response
        auto node = std::make_shared<Node>(generateRandomNodeID(), endpoint);
        
        if (m_routingManager) {
            m_routingManager->addNode(node);
        }
    }
    
    // Perform a random lookup to find more nodes
    performRandomLookup(callback);
}

std::vector<network::EndPoint> Bootstrapper::resolveBootstrapNode(const std::string& node) {
    std::vector<network::EndPoint> endpoints;
    
    // Parse the node string
    std::string host;
    uint16_t port = DEFAULT_PORT;
    
    size_t colonPos = node.find(':');
    if (colonPos != std::string::npos) {
        host = node.substr(0, colonPos);
        port = static_cast<uint16_t>(std::stoi(node.substr(colonPos + 1)));
    } else {
        host = node;
    }
    
    // Resolve the host
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    
    int status = getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (status != 0) {
        m_logger.error("Failed to resolve host {}: {}", host, gai_strerror(status));
        return endpoints;
    }
    
    // Add the resolved addresses
    for (p = res; p != nullptr; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(p->ai_addr);
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, INET_ADDRSTRLEN);
            
            network::NetworkAddress address(ipStr);
            network::EndPoint endpoint(address, port);
            
            endpoints.push_back(endpoint);
        }
    }
    
    freeaddrinfo(res);
    
    return endpoints;
}

void Bootstrapper::performRandomLookup(std::function<void(bool)> callback) {
    // Generate a random node ID
    NodeID randomID = generateRandomNodeID();
    
    m_logger.info("Performing lookup for random node ID: {}", nodeIDToString(randomID));
    
    // Perform a lookup
    if (m_nodeLookup) {
        m_nodeLookup->lookup(randomID, [this, callback](const std::vector<std::shared_ptr<Node>>& nodes) {
            m_logger.info("Random lookup found {} nodes", nodes.size());
            
            // Add the nodes to the routing table
            for (const auto& node : nodes) {
                if (m_routingManager) {
                    m_routingManager->addNode(node);
                }
            }
            
            // Call the callback
            if (callback) {
                callback(!nodes.empty());
            }
        });
    } else {
        m_logger.error("No node lookup available");
        if (callback) {
            callback(false);
        }
    }
}

} // namespace dht_hunter::dht
