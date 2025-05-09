#include "dht_hunter/dht/bootstrap/components/bootstrap_manager.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"
#include <netdb.h>
#include <arpa/inet.h>
#include <random>

// Constants
constexpr uint16_t DEFAULT_PORT = 6881;

namespace dht_hunter::dht::bootstrap {

// Initialize static members
std::shared_ptr<BootstrapManager> BootstrapManager::s_instance = nullptr;
std::mutex BootstrapManager::s_instanceMutex;

std::shared_ptr<BootstrapManager> BootstrapManager::getInstance(
    const DHTConfig& config,
    const NodeID& nodeID,
    std::shared_ptr<RoutingManager> routingManager,
    std::shared_ptr<NodeLookup> nodeLookup) {

    try {
        return utility::thread::withLock(s_instanceMutex, [&]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<BootstrapManager>(new BootstrapManager(
                    config, nodeID, routingManager, nodeLookup));
            }
            return s_instance;
        }, "BootstrapManager::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Bootstrap.BootstrapManager", e.what());
        return nullptr;
    }
}

BootstrapManager::BootstrapManager(const DHTConfig& config,
                                 const NodeID& nodeID,
                                 std::shared_ptr<RoutingManager> routingManager,
                                 std::shared_ptr<NodeLookup> nodeLookup)
    : BaseBootstrapComponent("BootstrapManager", config, nodeID),
      m_routingManager(routingManager),
      m_nodeLookup(nodeLookup) {
}

BootstrapManager::~BootstrapManager() {
    // Clear the singleton instance
    try {
        utility::thread::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "BootstrapManager::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Bootstrap.BootstrapManager", e.what());
    }
}

bool BootstrapManager::onInitialize() {
    if (!m_routingManager) {
        unified_event::logError("DHT.Bootstrap." + m_name, "Routing manager is null");
        return false;
    }

    if (!m_nodeLookup) {
        unified_event::logError("DHT.Bootstrap." + m_name, "Node lookup is null");
        return false;
    }

    return true;
}

bool BootstrapManager::onStart() {
    return true;
}

void BootstrapManager::onStop() {
    // Nothing to do
}

void BootstrapManager::onBootstrap(std::function<void(bool)> callback) {
    unified_event::logDebug("DHT.Bootstrap." + m_name, "Starting bootstrap process");

    // Store the callback for later use
    m_bootstrapCallback = callback;

    // Use a separate thread to perform the bootstrap process
    std::thread bootstrapThread([this]() {
        try {
            // Perform a random lookup to find nodes
            performRandomLookup([this](bool success) {
                if (success) {
                    unified_event::logDebug("DHT.Bootstrap." + m_name, "Random lookup successful");
                } else {
                    unified_event::logDebug("DHT.Bootstrap." + m_name, "Random lookup failed");
                }
            });

            // Get the bootstrap nodes
            const auto& bootstrapNodes = m_config.getBootstrapNodes();
            unified_event::logDebug("DHT.Bootstrap." + m_name, "Got " + std::to_string(bootstrapNodes.size()) + " bootstrap nodes from config");

            // Resolve the bootstrap nodes
            std::vector<EndPoint> endpoints;

            for (const auto& node : bootstrapNodes) {
                unified_event::logDebug("DHT.Bootstrap." + m_name, "Resolving bootstrap node: " + node);
                auto resolvedEndpoints = resolveBootstrapNode(node);
                unified_event::logDebug("DHT.Bootstrap." + m_name, "Resolved " + std::to_string(resolvedEndpoints.size()) + " endpoints for node " + node);
                endpoints.insert(endpoints.end(), resolvedEndpoints.begin(), resolvedEndpoints.end());
            }

            // Add the bootstrap nodes to the routing table
            unified_event::logDebug("DHT.Bootstrap." + m_name, "Adding bootstrap nodes to routing table");
            for (const auto& endpoint : endpoints) {
                // Generate a random node ID for the bootstrap node
                // This will be replaced with the actual node ID when we receive a response
                auto node = std::make_shared<Node>(generateRandomNodeID(), endpoint);

                if (m_routingManager) {
                    unified_event::logDebug("DHT.Bootstrap." + m_name, "Adding node " + nodeIDToString(node->getID()) + " at " + endpoint.toString() + " to routing table");
                    m_routingManager->addNode(node);
                } else {
                    unified_event::logError("DHT.Bootstrap." + m_name, "Routing manager is null, cannot add node");
                }
            }
            unified_event::logDebug("DHT.Bootstrap." + m_name, "Added " + std::to_string(endpoints.size()) + " bootstrap nodes to routing table");

            // Perform a random lookup to find more nodes
            performRandomLookup([this](bool success) {
                if (success) {
                    unified_event::logDebug("DHT.Bootstrap." + m_name, "Bootstrap successful");
                    if (m_bootstrapCallback) {
                        m_bootstrapCallback(true);
                    }
                } else {
                    unified_event::logError("DHT.Bootstrap." + m_name, "Bootstrap failed");
                    if (m_bootstrapCallback) {
                        m_bootstrapCallback(false);
                    }
                }
            });
        } catch (const std::exception& e) {
            unified_event::logError("DHT.Bootstrap." + m_name, "Exception in bootstrap thread: " + std::string(e.what()));
            if (m_bootstrapCallback) {
                m_bootstrapCallback(false);
            }
        } catch (...) {
            unified_event::logError("DHT.Bootstrap." + m_name, "Unknown exception in bootstrap thread");
            if (m_bootstrapCallback) {
                m_bootstrapCallback(false);
            }
        }
    });

    // Detach the thread so it can run independently
    bootstrapThread.detach();
}

std::vector<EndPoint> BootstrapManager::resolveBootstrapNode(const std::string& node) {
    std::vector<EndPoint> endpoints;

    // Parse the node string
    std::string host;
    uint16_t port = DEFAULT_PORT;

    // Check if the node string contains a port
    size_t colonPos = node.find(':');
    if (colonPos != std::string::npos) {
        host = node.substr(0, colonPos);
        try {
            port = static_cast<uint16_t>(std::stoi(node.substr(colonPos + 1)));
        } catch (const std::exception& e) {
            unified_event::logError("DHT.Bootstrap." + m_name, "Invalid port in bootstrap node: " + node);
            return endpoints;
        }
    } else {
        host = node;
    }

    // Resolve the host
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int status = getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (status != 0) {
        unified_event::logError("DHT.Bootstrap." + m_name, "Failed to resolve bootstrap node: " + node + ", error: " + gai_strerror(status));
        return endpoints;
    }

    // Add the resolved endpoints
    for (p = res; p != nullptr; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            // IPv4
            struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(p->ai_addr);
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, INET_ADDRSTRLEN);
            endpoints.push_back(EndPoint(network::NetworkAddress(ipStr), port));
        } else if (p->ai_family == AF_INET6) {
            // IPv6
            struct sockaddr_in6* ipv6 = reinterpret_cast<struct sockaddr_in6*>(p->ai_addr);
            char ipStr[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(ipv6->sin6_addr), ipStr, INET6_ADDRSTRLEN);
            endpoints.push_back(EndPoint(network::NetworkAddress(ipStr), port));
        }
    }

    freeaddrinfo(res);
    return endpoints;
}

void BootstrapManager::performRandomLookup(std::function<void(bool)> callback) {
    if (m_nodeLookup) {
        NodeID randomID = generateRandomNodeID();
        unified_event::logDebug("DHT.Bootstrap." + m_name, "Performing random lookup for node ID: " + nodeIDToString(randomID));
        m_nodeLookup->lookup(randomID, [this, callback, randomID](const std::vector<std::shared_ptr<Node>>& nodes) {
            unified_event::logDebug("DHT.Bootstrap." + m_name, "Random lookup for node ID " + nodeIDToString(randomID) + " completed with " + std::to_string(nodes.size()) + " nodes");

            // Add the nodes to the routing table
            for (const auto& node : nodes) {
                if (m_routingManager) {
                    unified_event::logDebug("DHT.Bootstrap." + m_name, "Adding node " + nodeIDToString(node->getID()) + " at " + node->getEndpoint().toString() + " to routing table");
                    m_routingManager->addNode(node);
                } else {
                    unified_event::logError("DHT.Bootstrap." + m_name, "Routing manager is null, cannot add node");
                }
            }

            // Call the callback
            if (callback) {
                callback(!nodes.empty());
            }
        });
    } else {
        unified_event::logError("DHT.Bootstrap." + m_name, "Node lookup is null, cannot perform random lookup");
        if (callback) {
            callback(false);
        }
    }
}

} // namespace dht_hunter::dht::bootstrap
