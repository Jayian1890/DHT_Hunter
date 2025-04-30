#include "dht_hunter/dht/crawler/components/crawler_config.hpp"

namespace dht_hunter::dht::crawler {

CrawlerConfig::CrawlerConfig()
    : m_parallelCrawls(10),
      m_refreshInterval(15),
      m_maxNodes(1000000),
      m_maxInfoHashes(1000000),
      m_autoStart(true) {
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
