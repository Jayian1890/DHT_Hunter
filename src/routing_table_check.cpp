#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>
#include <future>
#include "dht_hunter/dht/dht_node.hpp"
#include "dht_hunter/dht/routing_table.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

DEFINE_COMPONENT_LOGGER("Diagnostic", "RoutingTableCheck")

namespace dht_hunter::diagnostic {

void printRoutingTableStatus(const dht::RoutingTable& routingTable) {
    // Get all buckets
    const auto& buckets = routingTable.getBuckets();

    // Count total nodes
    size_t totalNodes = 0;
    for (const auto& bucket : buckets) {
        totalNodes += bucket.size();
    }

    getLogger()->info("Routing Table Status:");
    getLogger()->info("  Total Nodes: {}", totalNodes);

    // Count non-empty buckets
    size_t nonEmptyBuckets = 0;
    for (const auto& bucket : buckets) {
        if (bucket.size() > 0) {
            nonEmptyBuckets++;
        }
    }
    getLogger()->info("  Non-empty Buckets: {} / {}", nonEmptyBuckets, buckets.size());

    // Print bucket distribution
    getLogger()->info("  Bucket Distribution:");
    for (size_t i = 0; i < buckets.size(); i++) {
        if (buckets[i].size() > 0) {
            getLogger()->info("    Bucket {}: {} nodes", i, buckets[i].size());
        }
    }

    // Check for any full buckets
    size_t fullBuckets = 0;
    for (const auto& bucket : buckets) {
        if (bucket.isFull()) {
            fullBuckets++;
        }
    }
    getLogger()->info("  Full Buckets: {}", fullBuckets);

    // Analyze node distribution
    if (totalNodes > 0) {
        getLogger()->info("  Average Nodes per Non-empty Bucket: {:.2f}",
                     static_cast<double>(totalNodes) / nonEmptyBuckets);
    }
}

void checkRoutingTableHealth(std::shared_ptr<dht::DHTNode> dhtNode) {
    if (!dhtNode) {
        getLogger()->error("Invalid DHT node");
        return;
    }

    // Get the routing table
    const auto& routingTable = dhtNode->getRoutingTable();

    // Print routing table status
    printRoutingTableStatus(routingTable);

    // Perform a random node lookup to test routing table functionality
    getLogger()->info("Performing random node lookup to test routing table...");

    // Generate a random target ID
    dht::NodeID randomID = dht::generateRandomNodeID();

    // Create a promise to wait for the lookup to complete
    std::promise<bool> lookupPromise;
    auto lookupFuture = lookupPromise.get_future();

    // Start the lookup
    bool lookupStarted = dhtNode->findClosestNodes(randomID,
        [&lookupPromise](const std::vector<std::shared_ptr<dht::Node>>& nodes) {
            // Lookup completed
            getLogger()->info("Random node lookup completed, found {} nodes", nodes.size());

            // Print the first few nodes
            size_t nodesToPrint = std::min(nodes.size(), static_cast<size_t>(5));
            for (size_t i = 0; i < nodesToPrint; i++) {
                getLogger()->info("  Node {}: ID={}, Endpoint={}",
                             i, dht::nodeIDToString(nodes[i]->getID()),
                             nodes[i]->getEndpoint().toString());
            }

            // Set the promise
            lookupPromise.set_value(true);
        });

    if (!lookupStarted) {
        getLogger()->error("Failed to start random node lookup");
        return;
    }

    // Wait for the lookup to complete with a timeout
    auto status = lookupFuture.wait_for(std::chrono::seconds(10));
    if (status == std::future_status::timeout) {
        getLogger()->error("Random node lookup timed out");
    }

    getLogger()->info("Routing table check completed");
}

} // namespace dht_hunter::diagnostic
