#pragma once

#include "dht_hunter/dht/routing/components/base_routing_component.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include <thread>
#include <queue>
#include <unordered_map>
#include <chrono>
#include <functional>

namespace dht_hunter::dht::routing {

/**
 * @brief Component for verifying nodes before adding them to the routing table
 * 
 * This component verifies nodes by pinging them before they are added to the routing table.
 */
class NodeVerifierComponent : public BaseRoutingComponent {
public:
    /**
     * @brief Constructs a node verifier component
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param routingTable The routing table
     * @param transactionManager The transaction manager
     * @param messageSender The message sender
     */
    NodeVerifierComponent(const DHTConfig& config,
                         const NodeID& nodeID,
                         std::shared_ptr<RoutingTable> routingTable,
                         std::shared_ptr<TransactionManager> transactionManager,
                         std::shared_ptr<MessageSender> messageSender);

    /**
     * @brief Destructor
     */
    ~NodeVerifierComponent() override;

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

    std::shared_ptr<TransactionManager> m_transactionManager;
    std::shared_ptr<MessageSender> m_messageSender;
    std::thread m_processingThread;
    std::queue<VerificationEntry> m_verificationQueue;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_recentlyVerified;
    std::atomic<bool> m_processingThreadRunning;
    std::condition_variable m_processingCondition;
    std::mutex m_processingMutex;

    // Constants
    static constexpr std::chrono::seconds VERIFICATION_WAIT_TIME{5}; // Wait 5 seconds before pinging
    static constexpr std::chrono::hours RECENTLY_VERIFIED_EXPIRY{1}; // Remember verified nodes for 1 hour
};

} // namespace dht_hunter::dht::routing
