#pragma once

#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/event/logger.hpp"
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <mutex>
#include <future>

namespace dht_hunter::dht {

// Forward declarations
class RoutingManager;
class NodeLookup;

/**
 * @brief Bootstraps a DHT node
 */
class Bootstrapper {
public:
    /**
     * @brief Constructs a bootstrapper
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param routingManager The routing manager
     * @param nodeLookup The node lookup
     */
    Bootstrapper(const DHTConfig& config,
                const NodeID& nodeID,
                std::shared_ptr<RoutingManager> routingManager,
                std::shared_ptr<NodeLookup> nodeLookup);

    /**
     * @brief Destructor
     */
    ~Bootstrapper() = default;

    /**
     * @brief Bootstraps the DHT node
     * @param callback The callback to call when bootstrapping is complete
     */
    void bootstrap(std::function<void(bool)> callback);

private:
    /**
     * @brief Resolves a bootstrap node
     * @param node The bootstrap node
     * @return The resolved endpoints
     */
    std::vector<network::EndPoint> resolveBootstrapNode(const std::string& node);

    /**
     * @brief Performs a node lookup for a random node ID
     * @param callback The callback to call when the lookup is complete
     */
    void performRandomLookup(std::function<void(bool)> callback);

    DHTConfig m_config;
    std::shared_ptr<RoutingManager> m_routingManager;
    std::shared_ptr<NodeLookup> m_nodeLookup;
    std::mutex m_mutex;
    event::Logger m_logger;
};

} // namespace dht_hunter::dht
