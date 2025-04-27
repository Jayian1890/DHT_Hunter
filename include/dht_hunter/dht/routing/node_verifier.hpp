#pragma once

#include "dht_hunter/dht/types/dht_types.hpp"
#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <queue>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>

namespace dht_hunter::dht {

// Forward declarations
class TransactionManager;
class MessageSender;
class RoutingTable;

/**
 * @brief Verifies nodes before adding them to the routing table
 */
class NodeVerifier {
public:
    /**
     * @brief Constructs a node verifier
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param routingTable The routing table
     * @param transactionManager The transaction manager
     * @param messageSender The message sender
     */
    NodeVerifier(const DHTConfig& config,
                const NodeID& nodeID,
                std::shared_ptr<RoutingTable> routingTable,
                std::shared_ptr<TransactionManager> transactionManager,
                std::shared_ptr<MessageSender> messageSender,
                std::shared_ptr<unified_event::EventBus> eventBus);

    /**
     * @brief Destructor
     */
    ~NodeVerifier();

    /**
     * @brief Starts the node verifier
     * @return True if the node verifier was started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the node verifier
     */
    void stop();

    /**
     * @brief Checks if the node verifier is running
     * @return True if the node verifier is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Adds a node to the verification queue
     * @param node The node to verify
     * @param callback The callback to call when verification is complete
     * @param bucketIndex The index of the bucket where the node would be placed
     * @return True if the node was added to the queue, false otherwise
     */
    bool verifyNode(std::shared_ptr<Node> node, std::function<void(bool)> callback = nullptr, size_t bucketIndex = 0);

    /**
     * @brief Gets the number of nodes in the verification queue
     * @return The number of nodes in the verification queue
     */
    size_t getQueueSize() const;

private:
    /**
     * @brief Processes the verification queue
     */
    void processQueue();

    /**
     * @brief Pings a node to verify it
     * @param node The node to ping
     * @param callback The callback to call when verification is complete
     * @param bucketIndex The index of the bucket where the node would be placed
     */
    void pingNode(std::shared_ptr<Node> node, std::function<void(bool)> callback, size_t bucketIndex);

    /**
     * @brief Adds a verified node to the routing table
     * @param node The node to add
     * @return True if the node was added, false otherwise
     */
    bool addVerifiedNode(std::shared_ptr<Node> node);

    // Structure to hold a node and its verification information
    struct VerificationEntry {
        std::shared_ptr<Node> node;
        std::chrono::steady_clock::time_point queuedTime;
        std::function<void(bool)> callback;
        size_t bucketIndex;
    };

    DHTConfig m_config;
    NodeID m_nodeID;
    std::shared_ptr<RoutingTable> m_routingTable;
    std::shared_ptr<TransactionManager> m_transactionManager;
    std::shared_ptr<MessageSender> m_messageSender;
    std::shared_ptr<unified_event::EventBus> m_eventBus;
    std::atomic<bool> m_running;
    std::thread m_processingThread;
    std::queue<VerificationEntry> m_verificationQueue;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_recentlyVerified;
    mutable std::mutex m_mutex;    // Logger removed

    // Constants
    static constexpr std::chrono::seconds VERIFICATION_WAIT_TIME{5}; // Wait 5 seconds before pinging
    static constexpr std::chrono::hours RECENTLY_VERIFIED_EXPIRY{1}; // Remember verified nodes for 1 hour
};

} // namespace dht_hunter::dht
