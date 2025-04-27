#pragma once

#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/types/dht_types.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"
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
 * @brief Bootstraps a DHT node (Singleton)
 */
class Bootstrapper {
public:
    /**
     * @brief Gets the singleton instance of the bootstrapper
     * @param config The DHT configuration (only used if instance doesn't exist yet)
     * @param nodeID The node ID (only used if instance doesn't exist yet)
     * @param routingManager The routing manager (only used if instance doesn't exist yet)
     * @param nodeLookup The node lookup (only used if instance doesn't exist yet)
     * @return The singleton instance
     */
    static std::shared_ptr<Bootstrapper> getInstance(
        const DHTConfig& config,
        const NodeID& nodeID,
        std::shared_ptr<RoutingManager> routingManager,
        std::shared_ptr<NodeLookup> nodeLookup);

    /**
     * @brief Destructor
     */
    ~Bootstrapper();

    /**
     * @brief Delete copy constructor and assignment operator
     */
    Bootstrapper(const Bootstrapper&) = delete;
    Bootstrapper& operator=(const Bootstrapper&) = delete;
    Bootstrapper(Bootstrapper&&) = delete;
    Bootstrapper& operator=(Bootstrapper&&) = delete;

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

    /**
     * @brief Private constructor for singleton pattern
     */
    Bootstrapper(const DHTConfig& config,
                const NodeID& nodeID,
                std::shared_ptr<RoutingManager> routingManager,
                std::shared_ptr<NodeLookup> nodeLookup);

    // Static instance for singleton pattern
    static std::shared_ptr<Bootstrapper> s_instance;
    static std::mutex s_instanceMutex;

    DHTConfig m_config;
    std::shared_ptr<RoutingManager> m_routingManager;
    std::shared_ptr<NodeLookup> m_nodeLookup;
    std::mutex m_mutex;    // Logger removed
    std::function<void(bool)> m_bootstrapCallback;  // Callback for bootstrap completion
};

} // namespace dht_hunter::dht
