#pragma once

#include "dht_hunter/dht/routing/components/base_routing_component.hpp"
#include <thread>
#include <condition_variable>
#include <functional>

namespace dht_hunter::dht::routing {

/**
 * @brief Component for refreshing routing table buckets
 * 
 * This component periodically checks and refreshes buckets in the routing table.
 */
class BucketRefreshComponent : public BaseRoutingComponent {
public:
    /**
     * @brief Constructs a bucket refresh component
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param routingTable The routing table
     * @param nodeAddCallback Callback to add nodes to the routing table
     */
    BucketRefreshComponent(const DHTConfig& config,
                          const NodeID& nodeID,
                          std::shared_ptr<RoutingTable> routingTable,
                          std::function<void(std::shared_ptr<Node>)> nodeAddCallback);

    /**
     * @brief Destructor
     */
    ~BucketRefreshComponent() override;

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

private:
    /**
     * @brief Checks and refreshes buckets that need refreshing
     */
    void checkAndRefreshBuckets();

    std::function<void(std::shared_ptr<Node>)> m_nodeAddCallback;
    std::thread m_refreshThread;
    std::condition_variable m_refreshCondition;
    std::mutex m_refreshMutex;
    std::atomic<bool> m_refreshThreadRunning;
};

} // namespace dht_hunter::dht::routing
