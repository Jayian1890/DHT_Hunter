#pragma once

#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/logforge/logforge.hpp"
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

// Forward declarations
class DHTNode;

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
    bool acquired() const {
        return m_acquired;
    }

private:
    std::atomic<bool>& m_flag;
    bool m_acquired;
};

/**
 * @brief Configuration for the DHT bootstrapper
 */
struct DHTBootstrapperConfig {
    /**
     * @brief List of bootstrap nodes in the format "host:port"
     */
    std::vector<std::string> bootstrapNodes = {
        // BitTorrent nodes
        "router.bittorrent.com:6881",
        "router.utorrent.com:6881",
        
        // Transmission nodes
        "dht.transmissionbt.com:6881",
        
        // LibTorrent nodes
        "dht.libtorrent.org:25401",
        "router.libtorrent.com:25401",
        
        // Other reliable nodes
        "dht.anacrolix.link:6881",
        "dht.aelitis.com:6881",
        "router.silotis.us:6881",
        
        // Additional nodes
        "dht.bootkit.dev:6881",
        "dht.mwts.ru:6881",
        "dht.xirvik.com:6881",
        "dht.zoink.it:6881",
        "dht.leechers-paradise.org:6881"
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
    std::chrono::seconds bootstrapAttemptTimeout{15};

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
        // BitTorrent nodes
        {"router.bittorrent.com", {"67.215.246.10", "67.215.246.11", "208.83.20.20"}},
        {"router.utorrent.com", {"67.215.246.10", "67.215.246.11"}},
        
        // Transmission nodes
        {"dht.transmissionbt.com", {"212.129.33.59", "87.98.162.88"}},
        
        // LibTorrent nodes
        {"dht.libtorrent.org", {"67.215.246.10", "67.215.246.11"}},
        {"router.libtorrent.com", {"67.215.246.10", "67.215.246.11"}},
        
        // Other reliable nodes
        {"dht.aelitis.com", {"85.17.40.79", "77.247.178.152"}},
        {"dht.anacrolix.link", {"168.235.85.167", "116.203.226.223"}},
        {"router.silotis.us", {"192.99.2.144", "51.15.40.114"}},
        
        // Additional nodes
        {"dht.bootkit.dev", {"95.217.161.36"}},
        {"dht.mwts.ru", {"51.15.69.20"}},
        {"dht.xirvik.com", {"94.23.183.33"}},
        {"dht.zoink.it", {"195.154.107.103"}},
        {"dht.leechers-paradise.org", {"37.235.174.46"}}
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
     * @brief Whether the bootstrap operation was successful
     */
    bool success{false};

    /**
     * @brief Number of successful bootstrap attempts
     */
    int successfulAttempts{0};

    /**
     * @brief Number of total bootstrap attempts
     */
    int totalAttempts{0};

    /**
     * @brief Duration of the bootstrap operation
     */
    std::chrono::milliseconds duration{0};

    /**
     * @brief Endpoints that were successfully bootstrapped with
     */
    std::vector<network::EndPoint> successfulEndpoints;

    /**
     * @brief Endpoints that failed to bootstrap with
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
 *
 * The bootstrapping process works as follows:
 * 1. Resolve bootstrap node hostnames to IP addresses
 * 2. For each IP address, send a find_node query
 * 3. The message handler processes the response and routes it to the transaction manager
 * 4. The transaction manager calls the appropriate callback
 * 5. The callback processes the nodes in the response and adds them to the routing table
 *
 * The class uses the following components:
 * - DHTMessageSender: Sends DHT messages to other nodes
 * - DHTMessageHandler: Processes incoming DHT messages
 * - DHTTransactionManager: Manages DHT transactions and callbacks
 *
 * This class is implemented as a singleton to ensure only one instance exists.
 */
class DHTBootstrapper {
public:
    /**
     * @brief Gets the singleton instance of the DHT bootstrapper
     * @param config The bootstrapper configuration (only used if the instance doesn't exist yet)
     * @return The singleton instance
     */
    static DHTBootstrapper& getInstance(const DHTBootstrapperConfig& config = DHTBootstrapperConfig());
    
    /**
     * @brief Destructor
     */
    ~DHTBootstrapper();
    
    /**
     * @brief Delete copy constructor
     */
    DHTBootstrapper(const DHTBootstrapper&) = delete;
    
    /**
     * @brief Delete copy assignment operator
     */
    DHTBootstrapper& operator=(const DHTBootstrapper&) = delete;
    
    /**
     * @brief Delete move constructor
     */
    DHTBootstrapper(DHTBootstrapper&&) = delete;
    
    /**
     * @brief Delete move assignment operator
     */
    DHTBootstrapper& operator=(DHTBootstrapper&&) = delete;

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
     * @brief Bootstraps a DHT node
     * @param node The DHT node to bootstrap
     * @return The result of the bootstrap operation
     *
     * This method bootstraps a DHT node by connecting to known bootstrap nodes.
     * It resolves the bootstrap node hostnames to IP addresses, sends find_node
     * queries to each IP address, and processes the responses.
     *
     * @note This method is thread-safe and can be called from multiple threads.
     * @note This method will block until the bootstrap process completes.
     */
    BootstrapResult bootstrap(std::shared_ptr<DHTNode> node);

    /**
     * @brief Bootstraps a DHT node asynchronously
     * @param node The DHT node to bootstrap
     * @param callback The callback to call when the bootstrap process completes
     * @return A future that will contain the result of the bootstrap operation
     *
     * This method bootstraps a DHT node asynchronously by connecting to known bootstrap nodes.
     * It resolves the bootstrap node hostnames to IP addresses, sends find_node
     * queries to each IP address, and processes the responses.
     *
     * @note This method is thread-safe and can be called from multiple threads.
     * @note This method will return immediately and the bootstrap process will run in the background.
     */
    std::future<BootstrapResult> bootstrapAsync(std::shared_ptr<DHTNode> node, BootstrapCallback callback = nullptr);

    /**
     * @brief Cancels the bootstrap process
     *
     * This method cancels the bootstrap process if it is in progress.
     * It sets the cancelled flag and waits for the bootstrap process to complete.
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

    /**
     * @brief Gets the logger for this class
     * @return The logger
     */
    std::shared_ptr<logforge::LogForge> getLogger() const;

private:
    /**
     * @brief Private constructor to enforce singleton pattern
     * @param config The bootstrapper configuration
     */
    explicit DHTBootstrapper(const DHTBootstrapperConfig& config);

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
     * @brief Parses a bootstrap node string (host:port)
     * @param bootstrapNode The bootstrap node string
     * @param host The host (output)
     * @param port The port (output)
     * @return True if parsing was successful, false otherwise
     */
    bool parseBootstrapNode(const std::string& bootstrapNode, std::string& host, uint16_t& port);

    // Configuration
    DHTBootstrapperConfig m_config;

    // State
    std::atomic<bool> m_bootstrapping{false};
    std::atomic<bool> m_cancelled{false};
    mutable util::CheckedMutex m_mutex;

    // Thread pool for parallel operations
    std::shared_ptr<util::ThreadPool> m_threadPool;

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
#endif
};

} // namespace dht_hunter::dht
