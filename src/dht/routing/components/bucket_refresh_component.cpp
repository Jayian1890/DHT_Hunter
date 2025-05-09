#include "dht_hunter/dht/routing/components/bucket_refresh_component.hpp"
#include <chrono>

namespace dht_hunter::dht::routing {

BucketRefreshComponent::BucketRefreshComponent(const DHTConfig& config,
                                             const NodeID& nodeID,
                                             std::shared_ptr<RoutingTable> routingTable,
                                             std::function<void(std::shared_ptr<Node>)> nodeAddCallback)
    : BaseRoutingComponent("BucketRefresh", config, nodeID, routingTable),
      m_nodeAddCallback(nodeAddCallback),
      m_refreshThreadRunning(false) {
}

BucketRefreshComponent::~BucketRefreshComponent() {
    onStop();
}

bool BucketRefreshComponent::onInitialize() {
    if (!m_nodeAddCallback) {
        unified_event::logError("DHT.Routing." + m_name, "Node add callback is null");
        return false;
    }

    return true;
}

bool BucketRefreshComponent::onStart() {
    std::lock_guard<std::mutex> lock(m_refreshMutex);

    if (m_refreshThreadRunning) {
        return true;
    }

    m_refreshThreadRunning = true;
    m_refreshThread = std::thread(&BucketRefreshComponent::checkAndRefreshBuckets, this);
    return true;
}

void BucketRefreshComponent::onStop() {
    {
        std::lock_guard<std::mutex> lock(m_refreshMutex);

        if (!m_refreshThreadRunning) {
            return;
        }

        m_refreshThreadRunning = false;
    }

    m_refreshCondition.notify_all();

    if (m_refreshThread.joinable()) {
        m_refreshThread.join();
    }
}

void BucketRefreshComponent::checkAndRefreshBuckets() {
    try {
        while (m_refreshThreadRunning) {
            // Wait for the refresh interval
            {
                std::unique_lock<std::mutex> lock(m_refreshMutex);
                // Use a default refresh interval of 15 minutes
                m_refreshCondition.wait_for(lock, std::chrono::minutes(15),
                                          [this] { return !m_refreshThreadRunning; });

                if (!m_refreshThreadRunning) {
                    break;
                }
            }

            // Check all buckets
            if (m_routingTable) {
                size_t bucketCount = m_routingTable->getBucketCount();

                // In the new implementation, we don't have direct methods to check if a bucket needs refreshing
                // or to refresh a specific bucket. Instead, we'll refresh all buckets periodically.

                // Get all nodes from the routing table
                auto nodes = m_routingTable->getAllNodes();

                // Log the refresh operation
                unified_event::logDebug("DHT.Routing." + m_name, "Refreshing routing table with " +
                                      std::to_string(nodes.size()) + " nodes");

                // For each node, trigger a find node operation to refresh the routing table
                for (const auto& node : nodes) {
                    // Add the node to the routing table (this will update its last seen time)
                    m_nodeAddCallback(node);
                }
            }
        }
    } catch (const std::exception& e) {
        unified_event::logError("DHT.Routing." + m_name, "Exception in bucket refresh thread: " + std::string(e.what()));
    } catch (...) {
        unified_event::logError("DHT.Routing." + m_name, "Unknown exception in bucket refresh thread");
    }
}

} // namespace dht_hunter::dht::routing
