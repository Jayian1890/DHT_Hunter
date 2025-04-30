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
                m_refreshCondition.wait_for(lock, std::chrono::minutes(m_config.getBucketRefreshInterval()),
                                          [this] { return !m_refreshThreadRunning; });

                if (!m_refreshThreadRunning) {
                    break;
                }
            }

            // Check all buckets
            if (m_routingTable) {
                size_t bucketCount = m_routingTable->getBucketCount();

                for (size_t i = 0; i < bucketCount; ++i) {
                    // Check if the bucket needs refreshing
                    if (m_routingTable->needsRefresh(i)) {
                        unified_event::logDebug("DHT.Routing." + m_name, "Refreshing bucket " + std::to_string(i));

                        // Refresh the bucket
                        m_routingTable->refreshBucket(i, [this](const std::vector<std::shared_ptr<Node>>& nodes) {
                            // Add the nodes to the routing table
                            for (const auto& node : nodes) {
                                m_nodeAddCallback(node);
                            }
                        });
                    }
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
