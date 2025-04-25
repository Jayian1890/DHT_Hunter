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

    /**
     * @brief Gets the maximum number of queued transactions
     * 
     * @return The maximum number of queued transactions
     */
    size_t getMaxQueuedTransactions() const;

    /**
     * @brief Sets the maximum number of queued transactions
     * 
     * @param maxQueuedTransactions The maximum number of queued transactions
     */
    void setMaxQueuedTransactions(size_t maxQueuedTransactions);

    /**
     * @brief Gets the routing table path
     * 
     * @return The routing table path
     */
    std::string getRoutingTablePath() const;

    /**
     * @brief Gets the peer cache path
     * 
     * @return The peer cache path
     */
    std::string getPeerCachePath() const;

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
    size_t m_maxQueuedTransactions;
};

} // namespace dht_hunter::dht
