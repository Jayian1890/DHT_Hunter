#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/network/network_address.hpp"

namespace dht_hunter::dht {

/**
 * @brief Configuration for a DHT node
 */
class DHTConfig {
public:
    /**
     * @brief Constructs a DHT configuration with default values
     */
    DHTConfig();

    /**
     * @brief Constructs a DHT configuration with a specific port
     * @param port The UDP port to use
     */
    explicit DHTConfig(uint16_t port);

    /**
     * @brief Gets the UDP port
     * @return The UDP port
     */
    uint16_t getPort() const;

    /**
     * @brief Sets the UDP port
     * @param port The UDP port
     */
    void setPort(uint16_t port);

    /**
     * @brief Gets the k-bucket size
     * @return The k-bucket size
     */
    size_t getKBucketSize() const;

    /**
     * @brief Sets the k-bucket size
     * @param size The k-bucket size
     */
    void setKBucketSize(size_t size);

    /**
     * @brief Gets the alpha value for parallel lookups
     * @return The alpha value
     */
    size_t getAlpha() const;

    /**
     * @brief Sets the alpha value for parallel lookups
     * @param alpha The alpha value
     */
    void setAlpha(size_t alpha);

    /**
     * @brief Gets the maximum number of results for lookups
     * @return The maximum number of results
     */
    size_t getMaxResults() const;

    /**
     * @brief Sets the maximum number of results for lookups
     * @param maxResults The maximum number of results
     */
    void setMaxResults(size_t maxResults);

    /**
     * @brief Gets the bootstrap nodes
     * @return The bootstrap nodes
     */
    const std::vector<std::string>& getBootstrapNodes() const;

    /**
     * @brief Sets the bootstrap nodes
     * @param nodes The bootstrap nodes
     */
    void setBootstrapNodes(const std::vector<std::string>& nodes);

    /**
     * @brief Adds a bootstrap node
     * @param node The bootstrap node
     */
    void addBootstrapNode(const std::string& node);

    /**
     * @brief Gets the routing table save interval
     * @return The routing table save interval in seconds
     */
    int getRoutingTableSaveInterval() const;

    /**
     * @brief Sets the routing table save interval
     * @param interval The routing table save interval in seconds
     */
    void setRoutingTableSaveInterval(int interval);

    /**
     * @brief Gets the routing table file path
     * @return The routing table file path
     */
    const std::string& getRoutingTablePath() const;

    /**
     * @brief Sets the routing table file path
     * @param path The routing table file path
     */
    void setRoutingTablePath(const std::string& path);

    /**
     * @brief Gets the token rotation interval
     * @return The token rotation interval in seconds
     */
    int getTokenRotationInterval() const;

    /**
     * @brief Sets the token rotation interval
     * @param interval The token rotation interval in seconds
     */
    void setTokenRotationInterval(int interval);

private:
    uint16_t m_port;
    size_t m_kBucketSize;
    size_t m_alpha;
    size_t m_maxResults;
    std::vector<std::string> m_bootstrapNodes;
    int m_routingTableSaveInterval;
    std::string m_routingTablePath;
    int m_tokenRotationInterval;
};

} // namespace dht_hunter::dht
