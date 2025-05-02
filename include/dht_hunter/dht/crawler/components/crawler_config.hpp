#pragma once

#include <cstdint>
#include <cstddef>
#include "dht_hunter/utility/config/configuration_manager.hpp"

namespace dht_hunter::dht::crawler {

/**
 * @brief Configuration for the crawler
 */
class CrawlerConfig {
public:
    /**
     * @brief Constructs a crawler configuration with default values
     */
    CrawlerConfig();

    /**
     * @brief Constructs a crawler configuration from the configuration manager
     * @param configManager The configuration manager
     */
    explicit CrawlerConfig(std::shared_ptr<utility::config::ConfigurationManager> configManager);

    /**
     * @brief Gets the number of parallel crawls
     * @return The number of parallel crawls
     */
    size_t getParallelCrawls() const;

    /**
     * @brief Sets the number of parallel crawls
     * @param parallelCrawls The number of parallel crawls
     */
    void setParallelCrawls(size_t parallelCrawls);

    /**
     * @brief Gets the refresh interval in seconds
     * @return The refresh interval in seconds
     */
    uint32_t getRefreshInterval() const;

    /**
     * @brief Sets the refresh interval in seconds
     * @param refreshInterval The refresh interval in seconds
     */
    void setRefreshInterval(uint32_t refreshInterval);

    /**
     * @brief Gets the maximum number of nodes to store
     * @return The maximum number of nodes to store
     */
    size_t getMaxNodes() const;

    /**
     * @brief Sets the maximum number of nodes to store
     * @param maxNodes The maximum number of nodes to store
     */
    void setMaxNodes(size_t maxNodes);

    /**
     * @brief Gets the maximum number of info hashes to track
     * @return The maximum number of info hashes to track
     */
    size_t getMaxInfoHashes() const;

    /**
     * @brief Sets the maximum number of info hashes to track
     * @param maxInfoHashes The maximum number of info hashes to track
     */
    void setMaxInfoHashes(size_t maxInfoHashes);

    /**
     * @brief Checks if the crawler should auto-start
     * @return True if the crawler should auto-start, false otherwise
     */
    bool getAutoStart() const;

    /**
     * @brief Sets whether the crawler should auto-start
     * @param autoStart Whether the crawler should auto-start
     */
    void setAutoStart(bool autoStart);

private:
    // How many nodes to crawl in parallel
    size_t m_parallelCrawls;

    // How often to refresh the crawler (in seconds)
    uint32_t m_refreshInterval;

    // Maximum number of nodes to store
    size_t m_maxNodes;

    // Maximum number of info hashes to track
    size_t m_maxInfoHashes;

    // Whether to automatically start crawling on initialization
    bool m_autoStart;
};

} // namespace dht_hunter::dht::crawler
