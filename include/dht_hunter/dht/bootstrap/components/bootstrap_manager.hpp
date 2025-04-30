#pragma once

#include "dht_hunter/dht/bootstrap/components/base_bootstrap_component.hpp"
#include "dht_hunter/dht/routing/routing_manager.hpp"
#include "dht_hunter/dht/node_lookup/node_lookup.hpp"
#include <thread>
#include <future>
#include <random>

namespace dht_hunter::dht::bootstrap {

/**
 * @brief Manager for bootstrapping the DHT node
 * 
 * This component manages the bootstrap process, including resolving bootstrap nodes,
 * adding them to the routing table, and performing random lookups to find more nodes.
 */
class BootstrapManager : public BaseBootstrapComponent {
public:
    /**
     * @brief Gets the singleton instance
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param routingManager The routing manager
     * @param nodeLookup The node lookup
     * @return The singleton instance
     */
    static std::shared_ptr<BootstrapManager> getInstance(
        const DHTConfig& config,
        const NodeID& nodeID,
        std::shared_ptr<RoutingManager> routingManager,
        std::shared_ptr<NodeLookup> nodeLookup);

    /**
     * @brief Destructor
     */
    ~BootstrapManager() override;

protected:
    /**
     * @brief Initializes the component
     * @return True if the component was initialized successfully, false otherwise
     */
    bool onInitialize() override;

    /**
     * @brief Starts the component
     * @return True if the component was started successfully, false otherwise
     */
    bool onStart() override;

    /**
     * @brief Stops the component
     */
    void onStop() override;

    /**
     * @brief Bootstraps the DHT node
     * @param callback The callback to call when the bootstrap process is complete
     */
    void onBootstrap(std::function<void(bool)> callback) override;

private:
    /**
     * @brief Private constructor for singleton pattern
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param routingManager The routing manager
     * @param nodeLookup The node lookup
     */
    BootstrapManager(const DHTConfig& config,
                    const NodeID& nodeID,
                    std::shared_ptr<RoutingManager> routingManager,
                    std::shared_ptr<NodeLookup> nodeLookup);

    /**
     * @brief Resolves a bootstrap node
     * @param node The bootstrap node
     * @return The resolved endpoints
     */
    std::vector<EndPoint> resolveBootstrapNode(const std::string& node);

    /**
     * @brief Performs a node lookup for a random node ID
     * @param callback The callback to call when the lookup is complete
     */
    void performRandomLookup(std::function<void(bool)> callback);

    // Static instance for singleton pattern
    static std::shared_ptr<BootstrapManager> s_instance;
    static std::mutex s_instanceMutex;

    std::shared_ptr<RoutingManager> m_routingManager;
    std::shared_ptr<NodeLookup> m_nodeLookup;
    std::function<void(bool)> m_bootstrapCallback;  // Callback for bootstrap completion
};

} // namespace dht_hunter::dht::bootstrap
