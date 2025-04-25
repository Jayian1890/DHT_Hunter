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
      m_transactionsPath(""),
      m_maxQueuedTransactions(DEFAULT_MAX_QUEUED_TRANSACTIONS) {
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
      m_transactionsPath(""),
      m_maxQueuedTransactions(DEFAULT_MAX_QUEUED_TRANSACTIONS) {
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

    // Validate max queued transactions
    if (m_maxQueuedTransactions == 0) {
        getLogger()->error("Invalid max queued transactions: {}", m_maxQueuedTransactions);
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

size_t DHTNodeConfig::getMaxQueuedTransactions() const {
    return m_maxQueuedTransactions;
}

void DHTNodeConfig::setMaxQueuedTransactions(size_t maxQueuedTransactions) {
    m_maxQueuedTransactions = maxQueuedTransactions;
}

std::string DHTNodeConfig::getRoutingTablePath() const {
    return m_configDir + "/routing_table.dat";
}

std::string DHTNodeConfig::getPeerCachePath() const {
    return m_configDir + "/peer_cache.dat";
}

} // namespace dht_hunter::dht
