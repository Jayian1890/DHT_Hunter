#pragma once

#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/util/mutex_utils.hpp"
#include "dht_hunter/util/thread_pool.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <future>

namespace dht_hunter::dht {

/**
 * @brief RAII guard for bootstrap operations
 *
 * This class provides a RAII pattern for managing the bootstrapping state.
 * It automatically sets the bootstrapping flag when constructed and clears
 * it when destroyed if the guard was successfully acquired.
 */
class BootstrapGuard {
public:
    /**
     * @brief Constructs a bootstrap guard
     * @param flag The atomic flag to manage
     */
    BootstrapGuard(std::atomic<bool>& flag) : m_flag(flag), m_acquired(false) {
        m_acquired = !m_flag.exchange(true);
    }

    /**
     * @brief Destructor
     */
    ~BootstrapGuard() {
        if (m_acquired) {
            m_flag.store(false);
        }
    }

    /**
     * @brief Checks if the guard was successfully acquired
     * @return True if the guard was acquired, false otherwise
     */
    bool acquired() const { return m_acquired; }

private:
    std::atomic<bool>& m_flag;
    bool m_acquired;
};

// Forward declarations
class DHTNode;
class DHTMessageSender;
class DHTMessageHandler;
class DHTTransactionManager;

/**
 * @brief Configuration for the DHT bootstrapper
 */
struct DHTBootstrapperConfig {
    /**
     * @brief List of bootstrap nodes in the format "host:port"
     */
    std::vector<std::string> bootstrapNodes = {
        // Standard bootstrap nodes
        "router.bittorrent.com:6881",
        "dht.transmissionbt.com:6881",
        "router.utorrent.com:6881",
        "router.bitcomet.com:6881",
        "dht.aelitis.com:6881",

        // Nodes with non-standard ports
        "dht.libtorrent.org:25401",
        "dht.anacrolix.link:6881",
        "router.silotis.us:6881"
    };

    /**
     * @brief Timeout for the entire bootstrap process in seconds
     */
    std::chrono::seconds bootstrapTimeout{30};

    /**
     * @brief Timeout for DNS resolution in seconds
     */
    std::chrono::seconds dnsResolutionTimeout{2};

    /**
     * @brief Timeout for individual bootstrap attempts in seconds
     */
    std::chrono::seconds bootstrapAttemptTimeout{5};

    /**
     * @brief Minimum number of successful bootstraps required
     */
    int minSuccessfulBootstraps{1};

    /**
     * @brief Maximum number of parallel bootstrap attempts
     */
    int maxParallelBootstraps{5};

    /**
     * @brief Maximum number of retries per endpoint
     */
    int maxRetries{3};

    /**
     * @brief Delay between retries in milliseconds
     */
    std::chrono::milliseconds retryDelay{500};

    /**
     * @brief Fallback IP addresses for common bootstrap nodes in case DNS resolution fails
     */
    std::unordered_map<std::string, std::vector<std::string>> fallbackIPs = {
        {"router.bittorrent.com", {"67.215.246.10", "67.215.246.11"}},
        {"dht.transmissionbt.com", {"212.129.33.59"}},
        {"router.utorrent.com", {"67.215.246.10"}},
        {"router.bitcomet.com", {"67.215.246.10"}},
        {"dht.aelitis.com", {"85.17.40.79"}}
    };
};

/**
 * @brief Result of a bootstrap operation
 */
struct BootstrapResult {
    /**
     * @brief Error codes for bootstrap operations
     */
    enum class ErrorCode {
        None,                 ///< No error
        DNSResolutionFailed, ///< Failed to resolve hostname
        ConnectionFailed,    ///< Failed to connect to bootstrap node
        Timeout,             ///< Timed out waiting for response
        InvalidResponse,     ///< Received invalid response
        Cancelled,           ///< Bootstrap process was cancelled
        InternalError        ///< Internal error occurred
    };

    /**
     * @brief Whether the bootstrap was successful overall
     */
    bool success{false};

    /**
     * @brief Number of successful bootstrap attempts
     */
    int successfulAttempts{0};

    /**
     * @brief Total number of bootstrap attempts
     */
    int totalAttempts{0};

    /**
     * @brief Duration of the bootstrap process
     */
    std::chrono::milliseconds duration{0};

    /**
     * @brief List of successfully bootstrapped endpoints
     */
    std::vector<network::EndPoint> successfulEndpoints;

    /**
     * @brief List of failed bootstrap endpoints
     */
    std::vector<network::EndPoint> failedEndpoints;

    /**
     * @brief Error code
     */
    ErrorCode errorCode{ErrorCode::None};

    /**
     * @brief Error message
     */
    std::string errorMessage;
};

/**
 * @brief Callback for bootstrap completion
 */
using BootstrapCallback = std::function<void(const BootstrapResult&)>;

/**
 * @brief Class responsible for bootstrapping a DHT node
 *
 * This class handles the process of bootstrapping a DHT node by connecting
 * to known bootstrap nodes. It includes DNS resolution, fallback mechanisms,
 * and parallel bootstrapping.
 */
class DHTBootstrapper {
public:
    /**
     * @brief Constructs a DHT bootstrapper
     * @param config The bootstrapper configuration
     */
    explicit DHTBootstrapper(const DHTBootstrapperConfig& config = DHTBootstrapperConfig());

    /**
     * @brief Destructor
     */
    ~DHTBootstrapper();

    /**
     * @brief Sets the configuration
     * @param config The bootstrapper configuration
     */
    void setConfig(const DHTBootstrapperConfig& config);

    /**
     * @brief Gets the configuration
     * @return The bootstrapper configuration
     */
    const DHTBootstrapperConfig& getConfig() const;

    /**
     * @brief Bootstraps a DHT node synchronously
     * @param node The DHT node to bootstrap
     * @return The result of the bootstrap operation
     *
     * This method attempts to bootstrap the provided DHT node by connecting to known bootstrap nodes.
     * It performs the following steps:
     * 1. Resolves the hostnames of the bootstrap nodes to IP addresses
     * 2. Attempts to connect to each resolved endpoint
     * 3. Sends a find_node query to each endpoint
     * 4. Processes the responses and updates the node's routing table
     *
     * The method returns a BootstrapResult structure containing detailed information about the
     * bootstrap process, including the number of successful attempts, the total number of attempts,
     * and the duration of the process.
     *
     * @note This method is thread-safe and can be called from multiple threads.
     * @note This method will block until the bootstrap process completes or times out.
     */
    BootstrapResult bootstrap(std::shared_ptr<DHTNode> node);

    /**
     * @brief Bootstraps a DHT node asynchronously
     * @param node The DHT node to bootstrap
     * @param callback The callback to call when bootstrapping is complete
     * @return A future that will contain the result of the bootstrap operation
     *
     * This method is similar to bootstrap(), but it performs the bootstrap process asynchronously
     * in a separate thread. It returns a future that will contain the result of the bootstrap
     * operation when it completes.
     *
     * If a callback is provided, it will be called with the result of the bootstrap operation
     * when it completes.
     *
     * @note This method is thread-safe and can be called from multiple threads.
     * @note This method returns immediately and does not block.
     */
    std::future<BootstrapResult> bootstrapAsync(std::shared_ptr<DHTNode> node, BootstrapCallback callback = nullptr);

    /**
     * @brief Bootstraps a DHT node using the provided components
     * @param nodeID The node ID
     * @param messageSender The message sender
     * @param messageHandler The message handler
     * @param transactionManager The transaction manager
     * @return The result of the bootstrap operation
     *
     * This method is similar to bootstrap(), but it allows you to provide the individual components
     * of a DHT node instead of a complete DHT node. This can be useful for testing or for bootstrapping
     * a custom DHT node implementation.
     *
     * @note This method is thread-safe and can be called from multiple threads.
     * @note This method will block until the bootstrap process completes or times out.
     */
    BootstrapResult bootstrapWithComponents(
        const NodeID& nodeID,
        std::shared_ptr<DHTMessageSender> messageSender,
        std::shared_ptr<DHTMessageHandler> messageHandler,
        std::shared_ptr<DHTTransactionManager> transactionManager);

    /**
     * @brief Cancels any ongoing bootstrap operations
     *
     * This method cancels any ongoing bootstrap operations. It sets the cancelled flag
     * and waits for the bootstrap process to complete.
     *
     * @note This method is thread-safe and can be called from multiple threads.
     * @note This method will block until the bootstrap process completes.
     */
    void cancel();

    /**
     * @brief Checks if bootstrapping is in progress
     * @return True if bootstrapping is in progress, false otherwise
     *
     * This method checks if a bootstrap operation is currently in progress.
     *
     * @note This method is thread-safe and can be called from multiple threads.
     */
    bool isBootstrapping() const;

private:
    /**
     * @brief Resolves a hostname to IP addresses
     * @param host The hostname to resolve
     * @param port The port to use
     * @return A vector of endpoints
     */
    std::vector<network::EndPoint> resolveHost(const std::string& host, uint16_t port);

    /**
     * @brief Attempts to bootstrap with a single endpoint
     * @param node The DHT node to bootstrap
     * @param endpoint The endpoint to bootstrap with
     * @param result The bootstrap result to update
     * @return True if the bootstrap was successful, false otherwise
     */
    bool bootstrapWithEndpoint(
        std::shared_ptr<DHTNode> node,
        const network::EndPoint& endpoint,
        BootstrapResult& result);

    /**
     * @brief Attempts to bootstrap with a single endpoint using the provided components
     * @param nodeID The node ID
     * @param messageSender The message sender
     * @param messageHandler The message handler
     * @param transactionManager The transaction manager
     * @param endpoint The endpoint to bootstrap with
     * @param result The bootstrap result to update
     * @return True if the bootstrap was successful, false otherwise
     */
    bool bootstrapWithEndpointUsingComponents(
        const NodeID& nodeID,
        std::shared_ptr<DHTMessageSender> messageSender,
        std::shared_ptr<DHTMessageHandler> messageHandler,
        std::shared_ptr<DHTTransactionManager> transactionManager,
        const network::EndPoint& endpoint,
        BootstrapResult& result);

    /**
     * @brief Parses a bootstrap node string (host:port)
     * @param bootstrapNode The bootstrap node string
     * @param host The host (output)
     * @param port The port (output)
     * @return True if parsing was successful, false otherwise
     */
    bool parseBootstrapNode(const std::string& bootstrapNode, std::string& host, uint16_t& port);

    /**
     * @brief Sends a find_node query to a bootstrap node
     * @param nodeID The node ID
     * @param messageSender The message sender
     * @param endpoint The endpoint to send to
     * @param targetID The target ID to find (usually our own node ID)
     * @return True if the query was sent successfully, false otherwise
     */
    bool sendFindNodeQuery(
        const NodeID& nodeID,
        std::shared_ptr<DHTMessageSender> messageSender,
        const network::EndPoint& endpoint,
        const NodeID& targetID);

    /**
     * @brief Waits for a response from a bootstrap node
     * @param transactionManager The transaction manager
     * @param transactionID The transaction ID to wait for
     * @param timeout The timeout
     * @return True if a response was received, false otherwise
     */
    bool waitForResponse(
        std::shared_ptr<DHTTransactionManager> transactionManager,
        const std::string& transactionID,
        std::chrono::seconds timeout);

    // Configuration
    DHTBootstrapperConfig m_config;

    // State
    std::atomic<bool> m_bootstrapping{false};
    std::atomic<bool> m_cancelled{false};
    mutable util::CheckedMutex m_mutex;

    // Thread pool for parallel operations
    std::shared_ptr<util::ThreadPool> m_threadPool;

    // Logger
    std::shared_ptr<logforge::LogForge> getLogger() const;

#ifdef TESTING
public:
    // Expose internal methods for testing
    std::vector<network::EndPoint> testResolveHost(const std::string& host, uint16_t port) {
        return resolveHost(host, port);
    }

    bool testBootstrapWithEndpoint(
        std::shared_ptr<DHTNode> node,
        const network::EndPoint& endpoint,
        BootstrapResult& result) {
        return bootstrapWithEndpoint(node, endpoint, result);
    }

    bool testBootstrapWithEndpointUsingComponents(
        const NodeID& nodeID,
        std::shared_ptr<DHTMessageSender> messageSender,
        std::shared_ptr<DHTMessageHandler> messageHandler,
        std::shared_ptr<DHTTransactionManager> transactionManager,
        const network::EndPoint& endpoint,
        BootstrapResult& result) {
        return bootstrapWithEndpointUsingComponents(nodeID, messageSender, messageHandler, transactionManager, endpoint, result);
    }

    bool testWaitForResponse(
        std::shared_ptr<DHTTransactionManager> transactionManager,
        const std::string& transactionID,
        std::chrono::seconds timeout) {
        return waitForResponse(transactionManager, transactionID, timeout);
    }
#endif
};

} // namespace dht_hunter::dht
