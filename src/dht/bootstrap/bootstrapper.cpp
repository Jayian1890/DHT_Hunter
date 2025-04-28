#include "dht_hunter/dht/bootstrap/bootstrapper.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"
#include "dht_hunter/dht/routing/routing_manager.hpp"
#include "dht_hunter/dht/node_lookup/node_lookup.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include <random>
#include <netdb.h>
#include <arpa/inet.h>

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<Bootstrapper> Bootstrapper::s_instance = nullptr;
std::mutex Bootstrapper::s_instanceMutex;

std::shared_ptr<Bootstrapper> Bootstrapper::getInstance(
    const DHTConfig& config,
    const NodeID& nodeID,
    std::shared_ptr<RoutingManager> routingManager,
    std::shared_ptr<NodeLookup> nodeLookup) {

    try {
        return utility::thread::withLock(s_instanceMutex, [&]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<Bootstrapper>(new Bootstrapper(
                    config, nodeID, routingManager, nodeLookup));
            }
            return s_instance;
        }, "Bootstrapper::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Bootstrapper", e.what());
        return nullptr;
    }
}

Bootstrapper::Bootstrapper(const DHTConfig& config,
                         const NodeID& /*nodeID*/,
                         std::shared_ptr<RoutingManager> routingManager,
                         std::shared_ptr<NodeLookup> nodeLookup)
    : m_config(config),
      m_routingManager(routingManager),
      m_nodeLookup(nodeLookup) {
}

Bootstrapper::~Bootstrapper() {
    // Clear the singleton instance
    try {
        utility::thread::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "Bootstrapper::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Bootstrapper", e.what());
    }
}

void Bootstrapper::bootstrap(std::function<void(bool)> callback) {
    unified_event::logTrace("DHT.Bootstrapper", "TRACE: bootstrap() entry");
    // Store the callback for later use
    m_bootstrapCallback = callback;

    // Use a separate thread to perform the bootstrap process
    unified_event::logTrace("DHT.Bootstrapper", "TRACE: Starting bootstrap thread");
    std::thread bootstrapThread([this]() {
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: Bootstrap thread started");
        try {
            unified_event::logTrace("DHT.Bootstrapper", "TRACE: Checking if bootstrap is needed");
            bool shouldContinue = utility::thread::withLock(m_mutex, [this]() {
                // Always continue with bootstrap regardless of node count
                // This ensures we always try to connect to bootstrap nodes
                if (m_routingManager) {
                    unified_event::logTrace("DHT.Bootstrapper", "TRACE: Current node count: " + std::to_string(m_routingManager->getNodeCount()));
                }
                unified_event::logTrace("DHT.Bootstrapper", "TRACE: Continuing with bootstrap");
                return true; // Always continue with bootstrap
            }, "Bootstrapper::m_mutex");

            if (!shouldContinue) {
                unified_event::logTrace("DHT.Bootstrapper", "TRACE: Skipping bootstrap, returning");
                return;
            }
        } catch (const utility::thread::LockTimeoutException& e) {
            unified_event::logError("DHT.Bootstrapper", e.what());
            unified_event::logTrace("DHT.Bootstrapper", "TRACE: Lock timeout exception: " + std::string(e.what()));
            return;
        }
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: Performing random lookup");
        performRandomLookup(m_bootstrapCallback);

        // Get the bootstrap nodes
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: Getting bootstrap nodes from config");
        const auto& bootstrapNodes = m_config.getBootstrapNodes();
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: Got " + std::to_string(bootstrapNodes.size()) + " bootstrap nodes from config");

        if (bootstrapNodes.empty()) {
            unified_event::logTrace("DHT.Bootstrapper", "TRACE: No bootstrap nodes configured");

            if (m_bootstrapCallback) {
                unified_event::logTrace("DHT.Bootstrapper", "TRACE: Calling bootstrap callback with success=false");
                m_bootstrapCallback(false);
            }
            unified_event::logTrace("DHT.Bootstrapper", "TRACE: Returning from bootstrap thread");
            return;
        }

        // Resolve the bootstrap nodes
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: Resolving bootstrap nodes");
        std::vector<network::EndPoint> endpoints;

        for (const auto& node : bootstrapNodes) {
            unified_event::logTrace("DHT.Bootstrapper", "TRACE: Resolving bootstrap node: " + node);
            auto resolvedEndpoints = resolveBootstrapNode(node);
            unified_event::logTrace("DHT.Bootstrapper", "TRACE: Resolved " + std::to_string(resolvedEndpoints.size()) + " endpoints for node " + node);
            endpoints.insert(endpoints.end(), resolvedEndpoints.begin(), resolvedEndpoints.end());
        }
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: Resolved " + std::to_string(endpoints.size()) + " total endpoints");

        if (endpoints.empty()) {
            unified_event::logTrace("DHT.Bootstrapper", "TRACE: No endpoints resolved");

            if (m_bootstrapCallback) {
                unified_event::logTrace("DHT.Bootstrapper", "TRACE: Calling bootstrap callback with success=false");
                m_bootstrapCallback(false);
            }
            unified_event::logTrace("DHT.Bootstrapper", "TRACE: Returning from bootstrap thread");
            return;
        }


        // Add the bootstrap nodes to the routing table
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: Adding bootstrap nodes to routing table");
        for (const auto& endpoint : endpoints) {
            // Generate a random node ID for the bootstrap node
            // This will be replaced with the actual node ID when we receive a response
            unified_event::logTrace("DHT.Bootstrapper", "TRACE: Generating random node ID for bootstrap node " + endpoint.toString());
            auto node = std::make_shared<Node>(generateRandomNodeID(), endpoint);

            if (m_routingManager) {
                unified_event::logTrace("DHT.Bootstrapper", "TRACE: Adding node " + nodeIDToString(node->getID()) + " at " + endpoint.toString() + " to routing table");
                m_routingManager->addNode(node);
            } else {
                unified_event::logTrace("DHT.Bootstrapper", "TRACE: RoutingManager is null, cannot add node");
            }
        }
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: Added " + std::to_string(endpoints.size()) + " bootstrap nodes to routing table");

        // Perform a random lookup to find more nodes
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: Performing random lookup to find more nodes");
        performRandomLookup(m_bootstrapCallback);
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: Bootstrap thread completed");
    });

    // Detach the thread so it can run independently
    unified_event::logTrace("DHT.Bootstrapper", "TRACE: Detaching bootstrap thread");
    bootstrapThread.detach();

    // Return immediately, the callback will be called when the bootstrap process completes
    unified_event::logTrace("DHT.Bootstrapper", "TRACE: bootstrap() exit");
}

std::vector<network::EndPoint> Bootstrapper::resolveBootstrapNode(const std::string& node) {
    unified_event::logTrace("DHT.Bootstrapper", "TRACE: resolveBootstrapNode() entry for node: " + node);
    std::vector<network::EndPoint> endpoints;

    // Parse the node string
    std::string host;
    uint16_t port = DEFAULT_PORT;
    unified_event::logTrace("DHT.Bootstrapper", "TRACE: Default port set to " + std::to_string(DEFAULT_PORT));

    size_t colonPos = node.find(':');
    if (colonPos != std::string::npos) {
        host = node.substr(0, colonPos);
        port = static_cast<uint16_t>(std::stoi(node.substr(colonPos + 1)));
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: Parsed host: " + host + ", port: " + std::to_string(port));
    } else {
        host = node;
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: Using host: " + host + " with default port: " + std::to_string(port));
    }

    // Resolve the host
    unified_event::logTrace("DHT.Bootstrapper", "TRACE: Resolving host: " + host);
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int status = getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (status != 0) {
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: Failed to resolve host: " + host + ", error: " + std::string(gai_strerror(status)));
        return endpoints;
    }
    unified_event::logTrace("DHT.Bootstrapper", "TRACE: Successfully resolved host: " + host);

    // Add the resolved addresses
    unified_event::logTrace("DHT.Bootstrapper", "TRACE: Adding resolved addresses");
    for (p = res; p != nullptr; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(p->ai_addr);
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, INET_ADDRSTRLEN);

            unified_event::logTrace("DHT.Bootstrapper", "TRACE: Resolved IP: " + std::string(ipStr));
            network::NetworkAddress address(ipStr);
            network::EndPoint endpoint(address, port);

            unified_event::logTrace("DHT.Bootstrapper", "TRACE: Adding endpoint: " + endpoint.toString());
            endpoints.push_back(endpoint);
        }
    }
    unified_event::logTrace("DHT.Bootstrapper", "TRACE: Added " + std::to_string(endpoints.size()) + " endpoints");

    freeaddrinfo(res);

    unified_event::logTrace("DHT.Bootstrapper", "TRACE: resolveBootstrapNode() exit with " + std::to_string(endpoints.size()) + " endpoints");
    return endpoints;
}

void Bootstrapper::performRandomLookup(std::function<void(bool)> callback) {
    unified_event::logTrace("DHT.Bootstrapper", "TRACE: performRandomLookup() entry");
    if (m_nodeLookup) {
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: NodeLookup is available");
        NodeID randomID = generateRandomNodeID();
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: Generated random node ID: " + nodeIDToString(randomID));
        m_nodeLookup->lookup(randomID, [this, callback, randomID](const std::vector<std::shared_ptr<Node>>& nodes) {
            unified_event::logTrace("DHT.Bootstrapper", "TRACE: Random lookup for node ID " + nodeIDToString(randomID) + " completed with " + std::to_string(nodes.size()) + " nodes");

            // Add the nodes to the routing table
            unified_event::logTrace("DHT.Bootstrapper", "TRACE: Adding " + std::to_string(nodes.size()) + " nodes to routing table");
            for (const auto& node : nodes) {
                if (m_routingManager) {
                    unified_event::logTrace("DHT.Bootstrapper", "TRACE: Adding node " + nodeIDToString(node->getID()) + " at " + node->getEndpoint().toString() + " to routing table");
                    m_routingManager->addNode(node);
                } else {
                    unified_event::logTrace("DHT.Bootstrapper", "TRACE: RoutingManager is null, cannot add node");
                }
            }
            unified_event::logTrace("DHT.Bootstrapper", "TRACE: Added " + std::to_string(nodes.size()) + " nodes to routing table");

            // Call the callback
            if (callback) {
                unified_event::logTrace("DHT.Bootstrapper", "TRACE: Calling bootstrap callback with success=" + std::string(!nodes.empty() ? "true" : "false"));
                callback(!nodes.empty());
            } else {
                unified_event::logTrace("DHT.Bootstrapper", "TRACE: No callback provided");
            }
            unified_event::logTrace("DHT.Bootstrapper", "TRACE: Random lookup callback completed");
        });
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: Random lookup initiated");
    } else {
        unified_event::logTrace("DHT.Bootstrapper", "TRACE: NodeLookup is null");
        if (callback) {
            unified_event::logTrace("DHT.Bootstrapper", "TRACE: Calling bootstrap callback with success=false");
            callback(false);
        } else {
            unified_event::logTrace("DHT.Bootstrapper", "TRACE: No callback provided");
        }
    }
    unified_event::logTrace("DHT.Bootstrapper", "TRACE: performRandomLookup() exit");
}

} // namespace dht_hunter::dht
