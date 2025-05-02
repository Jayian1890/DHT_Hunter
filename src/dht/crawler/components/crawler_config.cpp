#include "dht_hunter/dht/crawler/components/crawler_config.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"

namespace dht_hunter::dht::crawler {

CrawlerConfig::CrawlerConfig()
    : m_parallelCrawls(10),
      m_refreshInterval(15),
      m_maxNodes(1000000),
      m_maxInfoHashes(1000000),
      m_autoStart(true) {
}

CrawlerConfig::CrawlerConfig(std::shared_ptr<utility::config::ConfigurationManager> configManager)
    : m_parallelCrawls(10),
      m_refreshInterval(15),
      m_maxNodes(1000000),
      m_maxInfoHashes(1000000),
      m_autoStart(true) {

    if (!configManager) {
        unified_event::logWarning("DHT.Crawler.Config", "Configuration manager is null, using default values");
        return;
    }

    // Load values from configuration
    m_parallelCrawls = static_cast<size_t>(configManager->getInt("crawler.parallelCrawls", 10));
    m_refreshInterval = static_cast<uint32_t>(configManager->getInt("crawler.refreshInterval", 15));
    m_maxNodes = static_cast<size_t>(configManager->getInt("crawler.maxNodes", 1000000));
    m_maxInfoHashes = static_cast<size_t>(configManager->getInt("crawler.maxInfoHashes", 1000000));
    m_autoStart = configManager->getBool("crawler.autoStart", true);

    unified_event::logInfo("DHT.Crawler.Config", "Loaded configuration: parallelCrawls=" + std::to_string(m_parallelCrawls) +
                          ", refreshInterval=" + std::to_string(m_refreshInterval) +
                          ", maxNodes=" + std::to_string(m_maxNodes) +
                          ", maxInfoHashes=" + std::to_string(m_maxInfoHashes) +
                          ", autoStart=" + (m_autoStart ? "true" : "false"));
}

size_t CrawlerConfig::getParallelCrawls() const {
    return m_parallelCrawls;
}

void CrawlerConfig::setParallelCrawls(size_t parallelCrawls) {
    m_parallelCrawls = parallelCrawls;
}

uint32_t CrawlerConfig::getRefreshInterval() const {
    return m_refreshInterval;
}

void CrawlerConfig::setRefreshInterval(uint32_t refreshInterval) {
    m_refreshInterval = refreshInterval;
}

size_t CrawlerConfig::getMaxNodes() const {
    return m_maxNodes;
}

void CrawlerConfig::setMaxNodes(size_t maxNodes) {
    m_maxNodes = maxNodes;
}

size_t CrawlerConfig::getMaxInfoHashes() const {
    return m_maxInfoHashes;
}

void CrawlerConfig::setMaxInfoHashes(size_t maxInfoHashes) {
    m_maxInfoHashes = maxInfoHashes;
}

bool CrawlerConfig::getAutoStart() const {
    return m_autoStart;
}

void CrawlerConfig::setAutoStart(bool autoStart) {
    m_autoStart = autoStart;
}

} // namespace dht_hunter::dht::crawler
