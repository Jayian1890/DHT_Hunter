#include "dht_hunter/dht/core/dht_config.hpp"
#include <filesystem>

namespace dht_hunter::dht {

DHTConfig::DHTConfig()
    : m_port(DEFAULT_PORT),
      m_kBucketSize(K_BUCKET_SIZE),
      m_alpha(DEFAULT_LOOKUP_ALPHA),
      m_maxResults(DEFAULT_LOOKUP_MAX_RESULTS),
      m_routingTableSaveInterval(ROUTING_TABLE_SAVE_INTERVAL),
      m_routingTablePath(""), // Empty path - routing table persistence is handled by PersistenceManager
      m_tokenRotationInterval(DEFAULT_TOKEN_ROTATION_INTERVAL),
      m_bucketRefreshInterval(1), // Default to 1 minute
      m_configDir("config") {

    // Add default bootstrap nodes
    for (size_t i = 0; i < DEFAULT_BOOTSTRAP_NODES_COUNT; ++i) {
        m_bootstrapNodes.push_back(DEFAULT_BOOTSTRAP_NODES[i]);
    }
}

DHTConfig::DHTConfig(uint16_t port)
    : DHTConfig() {
    m_port = port;
}

DHTConfig::DHTConfig(uint16_t port, const std::string& configDir)
    : DHTConfig() {
    m_port = port;
    m_configDir = configDir;
}

uint16_t DHTConfig::getPort() const {
    return m_port;
}

void DHTConfig::setPort(uint16_t port) {
    m_port = port;
}

size_t DHTConfig::getKBucketSize() const {
    return m_kBucketSize;
}

void DHTConfig::setKBucketSize(size_t size) {
    m_kBucketSize = size;
}

size_t DHTConfig::getAlpha() const {
    return m_alpha;
}

void DHTConfig::setAlpha(size_t alpha) {
    m_alpha = alpha;
}

size_t DHTConfig::getMaxResults() const {
    return m_maxResults;
}

void DHTConfig::setMaxResults(size_t maxResults) {
    m_maxResults = maxResults;
}

const std::vector<std::string>& DHTConfig::getBootstrapNodes() const {
    return m_bootstrapNodes;
}

void DHTConfig::setBootstrapNodes(const std::vector<std::string>& nodes) {
    m_bootstrapNodes = nodes;
}

void DHTConfig::addBootstrapNode(const std::string& node) {
    m_bootstrapNodes.push_back(node);
}

int DHTConfig::getRoutingTableSaveInterval() const {
    return m_routingTableSaveInterval;
}

void DHTConfig::setRoutingTableSaveInterval(int interval) {
    m_routingTableSaveInterval = interval;
}

const std::string& DHTConfig::getRoutingTablePath() const {
    return m_routingTablePath;
}

void DHTConfig::setRoutingTablePath(const std::string& path) {
    m_routingTablePath = path;
}

int DHTConfig::getTokenRotationInterval() const {
    return m_tokenRotationInterval;
}

void DHTConfig::setTokenRotationInterval(int interval) {
    m_tokenRotationInterval = interval;
}

const std::string& DHTConfig::getConfigDir() const {
    return m_configDir;
}

void DHTConfig::setConfigDir(const std::string& configDir) {
    m_configDir = configDir;
}

int DHTConfig::getBucketRefreshInterval() const {
    return m_bucketRefreshInterval;
}

void DHTConfig::setBucketRefreshInterval(int interval) {
    m_bucketRefreshInterval = interval;
}

std::string DHTConfig::getFullPath(const std::string& relativePath) const {
    if (relativePath.empty()) {
        return m_configDir;
    }

    std::filesystem::path configPath(m_configDir);
    std::filesystem::path relPath(relativePath);

    // If the path is already absolute, return it as is
    if (relPath.is_absolute()) {
        return relativePath;
    }

    // Combine the config directory with the relative path
    return (configPath / relPath).string();
}

} // namespace dht_hunter::dht
