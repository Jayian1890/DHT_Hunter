# DHT Component Implementation Guide

This document provides detailed guidance for implementing the first few components of the DHT refactoring plan.

## First Component: DHTNodeConfig

The `DHTNodeConfig` component is a good starting point as it has no dependencies on other components and will be used by most other components.

### Step 1: Create the Header File

**File: include/dht_hunter/dht/core/dht_node_config.hpp**

```cpp
#pragma once

#include <string>
#include <cstdint>

namespace dht_hunter::dht {

/**
 * @brief Configuration for the DHT node
 */
class DHTNodeConfig {
public:
    /**
     * @brief Constructs a default configuration
     */
    DHTNodeConfig();

    /**
     * @brief Constructs a configuration with custom values
     * 
     * @param port Port to listen on
     * @param configDir Configuration directory
     */
    DHTNodeConfig(uint16_t port, const std::string& configDir);

    /**
     * @brief Validates the configuration
     * 
     * @return true if the configuration is valid, false otherwise
     */
    bool validate() const;

    /**
     * @brief Gets the port to listen on
     * 
     * @return The port
     */
    uint16_t getPort() const;

    /**
     * @brief Sets the port to listen on
     * 
     * @param port The port
     */
    void setPort(uint16_t port);

    /**
     * @brief Gets the configuration directory
     * 
     * @return The configuration directory
     */
    const std::string& getConfigDir() const;

    /**
     * @brief Sets the configuration directory
     * 
     * @param configDir The configuration directory
     */
    void setConfigDir(const std::string& configDir);

    /**
     * @brief Gets the maximum number of nodes in a k-bucket
     * 
     * @return The k-bucket size
     */
    size_t getKBucketSize() const;

    /**
     * @brief Sets the maximum number of nodes in a k-bucket
     * 
     * @param kBucketSize The k-bucket size
     */
    void setKBucketSize(size_t kBucketSize);

    /**
     * @brief Gets the alpha parameter for parallel lookups
     * 
     * @return The alpha parameter
     */
    size_t getLookupAlpha() const;

    /**
     * @brief Sets the alpha parameter for parallel lookups
     * 
     * @param lookupAlpha The alpha parameter
     */
    void setLookupAlpha(size_t lookupAlpha);

    /**
     * @brief Gets the maximum number of nodes to store in a lookup result
     * 
     * @return The maximum number of nodes
     */
    size_t getLookupMaxResults() const;

    /**
     * @brief Sets the maximum number of nodes to store in a lookup result
     * 
     * @param lookupMaxResults The maximum number of nodes
     */
    void setLookupMaxResults(size_t lookupMaxResults);

    /**
     * @brief Gets whether to save the routing table after each new node is added
     * 
     * @return true if the routing table should be saved, false otherwise
     */
    bool getSaveRoutingTableOnNewNode() const;

    /**
     * @brief Sets whether to save the routing table after each new node is added
     * 
     * @param saveRoutingTableOnNewNode Whether to save the routing table
     */
    void setSaveRoutingTableOnNewNode(bool saveRoutingTableOnNewNode);

    /**
     * @brief Gets whether to save transactions on shutdown
     * 
     * @return true if transactions should be saved, false otherwise
     */
    bool getSaveTransactionsOnShutdown() const;

    /**
     * @brief Sets whether to save transactions on shutdown
     * 
     * @param saveTransactionsOnShutdown Whether to save transactions
     */
    void setSaveTransactionsOnShutdown(bool saveTransactionsOnShutdown);

    /**
     * @brief Gets whether to load transactions on startup
     * 
     * @return true if transactions should be loaded, false otherwise
     */
    bool getLoadTransactionsOnStartup() const;

    /**
     * @brief Sets whether to load transactions on startup
     * 
     * @param loadTransactionsOnStartup Whether to load transactions
     */
    void setLoadTransactionsOnStartup(bool loadTransactionsOnStartup);

    /**
     * @brief Gets the path to the transactions file
     * 
     * @return The transactions file path
     */
    const std::string& getTransactionsPath() const;

    /**
     * @brief Sets the path to the transactions file
     * 
     * @param transactionsPath The transactions file path
     */
    void setTransactionsPath(const std::string& transactionsPath);

private:
    uint16_t m_port;
    std::string m_configDir;
    size_t m_kBucketSize;
    size_t m_lookupAlpha;
    size_t m_lookupMaxResults;
    bool m_saveRoutingTableOnNewNode;
    bool m_saveTransactionsOnShutdown;
    bool m_loadTransactionsOnStartup;
    std::string m_transactionsPath;
};

} // namespace dht_hunter::dht
```

### Step 2: Create the Implementation File

**File: src/dht/core/dht_node_config.cpp**

```cpp
#include "dht_hunter/dht/core/dht_node_config.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

DEFINE_COMPONENT_LOGGER("DHT", "NodeConfig")

namespace dht_hunter::dht {

DHTNodeConfig::DHTNodeConfig()
    : m_port(DEFAULT_PORT),
      m_configDir(DEFAULT_CONFIG_DIR),
      m_kBucketSize(DEFAULT_K_BUCKET_SIZE),
      m_lookupAlpha(DEFAULT_LOOKUP_ALPHA),
      m_lookupMaxResults(DEFAULT_LOOKUP_MAX_RESULTS),
      m_saveRoutingTableOnNewNode(DEFAULT_SAVE_ROUTING_TABLE_ON_NEW_NODE),
      m_saveTransactionsOnShutdown(DEFAULT_SAVE_TRANSACTIONS_ON_SHUTDOWN),
      m_loadTransactionsOnStartup(DEFAULT_LOAD_TRANSACTIONS_ON_STARTUP),
      m_transactionsPath("") {
}

DHTNodeConfig::DHTNodeConfig(uint16_t port, const std::string& configDir)
    : m_port(port),
      m_configDir(configDir),
      m_kBucketSize(DEFAULT_K_BUCKET_SIZE),
      m_lookupAlpha(DEFAULT_LOOKUP_ALPHA),
      m_lookupMaxResults(DEFAULT_LOOKUP_MAX_RESULTS),
      m_saveRoutingTableOnNewNode(DEFAULT_SAVE_ROUTING_TABLE_ON_NEW_NODE),
      m_saveTransactionsOnShutdown(DEFAULT_SAVE_TRANSACTIONS_ON_SHUTDOWN),
      m_loadTransactionsOnStartup(DEFAULT_LOAD_TRANSACTIONS_ON_STARTUP),
      m_transactionsPath("") {
}

bool DHTNodeConfig::validate() const {
    // Validate port
    if (m_port == 0) {
        getLogger()->error("Invalid port: {}", m_port);
        return false;
    }

    // Validate k-bucket size
    if (m_kBucketSize == 0) {
        getLogger()->error("Invalid k-bucket size: {}", m_kBucketSize);
        return false;
    }

    // Validate lookup alpha
    if (m_lookupAlpha == 0) {
        getLogger()->error("Invalid lookup alpha: {}", m_lookupAlpha);
        return false;
    }

    // Validate lookup max results
    if (m_lookupMaxResults == 0) {
        getLogger()->error("Invalid lookup max results: {}", m_lookupMaxResults);
        return false;
    }

    return true;
}

uint16_t DHTNodeConfig::getPort() const {
    return m_port;
}

void DHTNodeConfig::setPort(uint16_t port) {
    m_port = port;
}

const std::string& DHTNodeConfig::getConfigDir() const {
    return m_configDir;
}

void DHTNodeConfig::setConfigDir(const std::string& configDir) {
    m_configDir = configDir;
}

size_t DHTNodeConfig::getKBucketSize() const {
    return m_kBucketSize;
}

void DHTNodeConfig::setKBucketSize(size_t kBucketSize) {
    m_kBucketSize = kBucketSize;
}

size_t DHTNodeConfig::getLookupAlpha() const {
    return m_lookupAlpha;
}

void DHTNodeConfig::setLookupAlpha(size_t lookupAlpha) {
    m_lookupAlpha = lookupAlpha;
}

size_t DHTNodeConfig::getLookupMaxResults() const {
    return m_lookupMaxResults;
}

void DHTNodeConfig::setLookupMaxResults(size_t lookupMaxResults) {
    m_lookupMaxResults = lookupMaxResults;
}

bool DHTNodeConfig::getSaveRoutingTableOnNewNode() const {
    return m_saveRoutingTableOnNewNode;
}

void DHTNodeConfig::setSaveRoutingTableOnNewNode(bool saveRoutingTableOnNewNode) {
    m_saveRoutingTableOnNewNode = saveRoutingTableOnNewNode;
}

bool DHTNodeConfig::getSaveTransactionsOnShutdown() const {
    return m_saveTransactionsOnShutdown;
}

void DHTNodeConfig::setSaveTransactionsOnShutdown(bool saveTransactionsOnShutdown) {
    m_saveTransactionsOnShutdown = saveTransactionsOnShutdown;
}

bool DHTNodeConfig::getLoadTransactionsOnStartup() const {
    return m_loadTransactionsOnStartup;
}

void DHTNodeConfig::setLoadTransactionsOnStartup(bool loadTransactionsOnStartup) {
    m_loadTransactionsOnStartup = loadTransactionsOnStartup;
}

const std::string& DHTNodeConfig::getTransactionsPath() const {
    return m_transactionsPath;
}

void DHTNodeConfig::setTransactionsPath(const std::string& transactionsPath) {
    m_transactionsPath = transactionsPath;
}

} // namespace dht_hunter::dht
```

## Second Component: DHTConstants

The `DHTConstants` component will contain all the constants that were previously defined in `dht_node.hpp`.

### Step 1: Create the Header File

**File: include/dht_hunter/dht/core/dht_constants.hpp**

```cpp
#pragma once

#include <cstddef>
#include <cstdint>

namespace dht_hunter::dht {

/**
 * @brief Default port for DHT nodes
 */
constexpr uint16_t DEFAULT_PORT = 6881;

/**
 * @brief Maximum number of concurrent transactions
 */
constexpr size_t MAX_TRANSACTIONS = 1024;

/**
 * @brief Transaction timeout in seconds
 */
constexpr int TRANSACTION_TIMEOUT = 30;

/**
 * @brief Default alpha parameter for parallel lookups (number of concurrent requests)
 */
constexpr size_t DEFAULT_LOOKUP_ALPHA = 5;

/**
 * @brief Default maximum number of nodes to store in a lookup result
 */
constexpr size_t DEFAULT_LOOKUP_MAX_RESULTS = 16;

/**
 * @brief Maximum number of iterations for a node lookup
 */
constexpr size_t LOOKUP_MAX_ITERATIONS = 20;

/**
 * @brief Maximum number of peers to store per info hash
 */
constexpr size_t MAX_PEERS_PER_INFOHASH = 100;

/**
 * @brief Time-to-live for stored peers in seconds (30 minutes)
 */
constexpr int PEER_TTL = 1800;

/**
 * @brief Interval for cleaning up expired peers in seconds (5 minutes)
 */
constexpr int PEER_CLEANUP_INTERVAL = 300;

/**
 * @brief Interval for saving the routing table in seconds (10 minutes)
 */
constexpr int ROUTING_TABLE_SAVE_INTERVAL = 600;

/**
 * @brief Interval for saving transactions in seconds (5 minutes)
 */
constexpr int TRANSACTION_SAVE_INTERVAL = 300;

/**
 * @brief Default configuration directory
 */
constexpr char DEFAULT_CONFIG_DIR[] = "config";

/**
 * @brief Default setting for whether to save the routing table after each new node is added
 */
constexpr bool DEFAULT_SAVE_ROUTING_TABLE_ON_NEW_NODE = true;

/**
 * @brief Default setting for whether to save transactions on shutdown
 */
constexpr bool DEFAULT_SAVE_TRANSACTIONS_ON_SHUTDOWN = true;

/**
 * @brief Default setting for whether to load transactions on startup
 */
constexpr bool DEFAULT_LOAD_TRANSACTIONS_ON_STARTUP = true;

/**
 * @brief Default path for the routing table file
 */
constexpr char DEFAULT_ROUTING_TABLE_PATH[] = "config/routing_table.dat";

/**
 * @brief Default maximum number of nodes in a k-bucket
 */
constexpr size_t DEFAULT_K_BUCKET_SIZE = 16;

} // namespace dht_hunter::dht
```

## Third Component: DHTTokenManager

The `DHTTokenManager` component is responsible for generating and validating tokens for DHT operations.

### Step 1: Create the Header File

**File: include/dht_hunter/dht/storage/dht_token_manager.hpp**

```cpp
#pragma once

#include <string>
#include <chrono>
#include <mutex>
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/util/mutex_utils.hpp"

namespace dht_hunter::dht {

/**
 * @brief Manages token generation and validation for DHT operations
 */
class DHTTokenManager {
public:
    /**
     * @brief Constructs a token manager
     */
    DHTTokenManager();

    /**
     * @brief Destructor
     */
    ~DHTTokenManager() = default;

    /**
     * @brief Generates a token for a node
     * 
     * @param endpoint The node's endpoint
     * @return The token
     */
    std::string generateToken(const network::EndPoint& endpoint);

    /**
     * @brief Validates a token for a node
     * 
     * @param token The token
     * @param endpoint The node's endpoint
     * @return true if the token is valid, false otherwise
     */
    bool validateToken(const std::string& token, const network::EndPoint& endpoint);

    /**
     * @brief Rotates the secret
     * 
     * This should be called periodically to maintain security.
     */
    void rotateSecret();

    /**
     * @brief Checks if the secret needs to be rotated
     * 
     * @return true if the secret needs to be rotated, false otherwise
     */
    bool checkSecretRotation();

private:
    std::string m_secret;
    std::chrono::steady_clock::time_point m_secretLastChanged;
    std::string m_previousSecret;
    util::CheckedMutex m_secretMutex;
};

} // namespace dht_hunter::dht
```

### Step 2: Create the Implementation File

**File: src/dht/storage/dht_token_manager.cpp**

```cpp
#include "dht_hunter/dht/storage/dht_token_manager.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include <random>
#include <sstream>
#include <iomanip>

DEFINE_COMPONENT_LOGGER("DHT", "TokenManager")

namespace {
// Helper function to generate a random string of the specified length
std::string generateRandomString(size_t length) {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);

    std::string result(length, 0);
    for (size_t i = 0; i < length; ++i) {
        result[i] = charset[dist(rng)];
    }
    return result;
}
}

namespace dht_hunter::dht {

DHTTokenManager::DHTTokenManager()
    : m_secret(generateRandomString(20)),
      m_secretLastChanged(std::chrono::steady_clock::now()),
      m_previousSecret("") {
    getLogger()->debug("Token manager initialized with new secret");
}

std::string DHTTokenManager::generateToken(const network::EndPoint& endpoint) {
    std::lock_guard<util::CheckedMutex> lock(m_secretMutex);
    // Generate token based on endpoint and secret
    std::stringstream ss;
    ss << endpoint.toString() << m_secret;
    // Hash the token
    std::hash<std::string> hasher;
    size_t hash = hasher(ss.str());
    // Convert hash to string
    ss.str("");
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}

bool DHTTokenManager::validateToken(const std::string& token, const network::EndPoint& endpoint) {
    std::lock_guard<util::CheckedMutex> lock(m_secretMutex);
    // Generate token based on endpoint and current secret
    std::stringstream ss;
    ss << endpoint.toString() << m_secret;
    // Hash the token
    std::hash<std::string> hasher;
    size_t hash = hasher(ss.str());
    // Convert hash to string
    ss.str("");
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    // Check if token matches
    if (token == ss.str()) {
        return true;
    }
    // If not, try with previous secret
    ss.str("");
    ss << endpoint.toString() << m_previousSecret;
    hash = hasher(ss.str());
    ss.str("");
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return token == ss.str();
}

void DHTTokenManager::rotateSecret() {
    std::lock_guard<util::CheckedMutex> lock(m_secretMutex);
    m_previousSecret = m_secret;
    m_secret = generateRandomString(20);
    m_secretLastChanged = std::chrono::steady_clock::now();
    getLogger()->debug("Secret rotated");
}

bool DHTTokenManager::checkSecretRotation() {
    std::lock_guard<util::CheckedMutex> lock(m_secretMutex);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - m_secretLastChanged).count();
    return elapsed >= 10; // Rotate every 10 minutes
}

} // namespace dht_hunter::dht
```

## Updating CMakeLists.txt

You'll need to update the CMakeLists.txt file to include the new files:

**File: src/dht/CMakeLists.txt**

```cmake
# Define the DHT library
add_library(dht_hunter_dht
    # Core components
    core/dht_node.cpp
    core/dht_node_config.cpp
    
    # Network components
    network/dht_socket_manager.cpp
    network/dht_message_sender.cpp
    network/dht_message_handler.cpp
    
    # Routing components
    routing/dht_routing_manager.cpp
    routing/dht_node_lookup.cpp
    routing/dht_peer_lookup.cpp
    
    # Storage components
    storage/dht_peer_storage.cpp
    storage/dht_token_manager.cpp
    
    # Transaction components
    transactions/dht_transaction_manager.cpp
    transactions/dht_transaction_persistence.cpp
    
    # Persistence components
    persistence/dht_persistence_manager.cpp
    
    # Legacy files (to be refactored)
    types.cpp
    message.cpp
    query_message.cpp
    response_message.cpp
    error_message.cpp
    routing_table.cpp
    dht_node.cpp
    node_lookup.cpp
    get_peers_lookup.cpp
    peer_storage.cpp
    routing_table_persistence.cpp
    dht_node_persistence.cpp
    transaction_persistence.cpp
    dht_node_transaction_persistence.cpp
)

# Set include directories
target_include_directories(dht_hunter_dht
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_dht
    PUBLIC
    cxx_std_17
)

# Link with other libraries
target_link_libraries(dht_hunter_dht
    PUBLIC
    dht_hunter_logforge
    dht_hunter_bencode
    dht_hunter_network
)
```

## Next Steps

After implementing these initial components, you should:

1. Create the directory structure
2. Implement the components one by one
3. Update the DHTNode class to use the new components
4. Write unit tests for each component
5. Update the progress tracker

Remember to follow the incremental approach outlined in the refactoring plan to minimize risk and ensure that the system remains functional throughout the process.
