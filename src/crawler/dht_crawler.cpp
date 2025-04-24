#include "dht_hunter/crawler/dht_crawler.hpp"
#include "dht_hunter/util/hash.hpp"

#include <algorithm>
#include <random>
#include <sstream>

namespace dht_hunter::crawler {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Crawler.DHTCrawler");
}

DHTCrawler::DHTCrawler(const DHTCrawlerConfig& config)
    : m_config(config),
      m_metadataFetchQueue() {
}

DHTCrawler::~DHTCrawler() {
    stop();
}

bool DHTCrawler::start() {
    if (m_running) {
        logger->warning("DHTCrawler is already running");
        return false;
    }

    // Initialize components
    if (!initializeDHTNode()) {
        logger->error("Failed to initialize DHT node");
        return false;
    }

    if (m_config.useInfoHashCollector && !initializeInfoHashCollector()) {
        logger->error("Failed to initialize info hash collector");
        m_dhtNode->stop();
        return false;
    }

    if (!initializeMetadataFetcher()) {
        logger->error("Failed to initialize metadata fetcher");
        if (m_infoHashCollector) {
            m_infoHashCollector->stop();
        }
        m_dhtNode->stop();
        return false;
    }

    if (m_config.useMetadataStorage && !initializeMetadataStorage()) {
        logger->error("Failed to initialize metadata storage");
        m_metadataFetcher->stop();
        if (m_infoHashCollector) {
            m_infoHashCollector->stop();
        }
        m_dhtNode->stop();
        return false;
    }

    // Set the running flag
    m_running = true;

    // Start threads
    m_lookupThread = std::thread(&DHTCrawler::lookupThread, this);
    m_metadataFetchThread = std::thread(&DHTCrawler::metadataFetchThread, this);
    m_statusThread = std::thread(&DHTCrawler::statusThread, this);

    logger->info("DHTCrawler started");
    return true;
}

void DHTCrawler::stop() {
    if (!m_running) {
        return;
    }

    // Set the running flag to false to stop threads
    m_running = false;

    // Wait for threads to finish
    if (m_lookupThread.joinable()) {
        m_lookupThread.join();
    }

    if (m_metadataFetchThread.joinable()) {
        m_metadataFetchThread.join();
    }

    if (m_statusThread.joinable()) {
        m_statusThread.join();
    }

    // Stop components
    if (m_metadataStorage) {
        m_metadataStorage->shutdown();
    }

    if (m_metadataFetcher) {
        m_metadataFetcher->stop();
    }

    if (m_infoHashCollector) {
        m_infoHashCollector->stop();
    }

    if (m_dhtNode) {
        m_dhtNode->stop();
    }

    logger->info("DHTCrawler stopped");
}

bool DHTCrawler::isRunning() const {
    return m_running;
}

std::shared_ptr<dht::DHTNode> DHTCrawler::getDHTNode() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_dhtNode;
}

std::shared_ptr<InfoHashCollector> DHTCrawler::getInfoHashCollector() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_infoHashCollector;
}

std::shared_ptr<metadata::MetadataFetcher> DHTCrawler::getMetadataFetcher() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metadataFetcher;
}

std::shared_ptr<metadata::MetadataStorage> DHTCrawler::getMetadataStorage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metadataStorage;
}

DHTCrawlerConfig DHTCrawler::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void DHTCrawler::setConfig(const DHTCrawlerConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

void DHTCrawler::setStatusCallback(CrawlerStatusCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statusCallback = callback;
}

uint64_t DHTCrawler::getInfoHashesDiscovered() const {
    return m_infoHashesDiscovered;
}

uint64_t DHTCrawler::getInfoHashesQueued() const {
    return m_infoHashesQueued;
}

uint64_t DHTCrawler::getMetadataFetched() const {
    return m_metadataFetched;
}

double DHTCrawler::getLookupRate() const {
    return m_lookupRate;
}

double DHTCrawler::getMetadataFetchRate() const {
    return m_metadataFetchRate;
}

bool DHTCrawler::initializeDHTNode() {
    // Create the DHT node
    m_dhtNode = std::make_shared<dht::DHTNode>(m_config.dhtPort);

    // Start the DHT node
    if (!m_dhtNode->start()) {
        logger->error("Failed to start DHT node");
        return false;
    }

    // Bootstrap the DHT node
    for (const auto& bootstrapNode : m_config.bootstrapNodes) {
        // Parse the bootstrap node string (host:port)
        std::string host;
        uint16_t port;

        size_t colonPos = bootstrapNode.find(':');
        if (colonPos != std::string::npos) {
            host = bootstrapNode.substr(0, colonPos);
            port = static_cast<uint16_t>(std::stoi(bootstrapNode.substr(colonPos + 1)));
        } else {
            host = bootstrapNode;
            port = 6881; // Default DHT port
        }

        // Resolve the host to an IP address
        auto addresses = network::NetworkAddress::resolve(host);
        if (addresses.empty()) {
            logger->warning("Failed to resolve bootstrap node: {}", host);
            continue;
        }

        // Create an endpoint for the bootstrap node
        network::EndPoint endpoint(addresses[0], port);

        // Bootstrap the DHT node
        if (!m_dhtNode->bootstrap(endpoint)) {
            logger->warning("Failed to bootstrap DHT node with {}", endpoint.toString());
        } else {
            logger->debug("Bootstrapped DHT node with {}", endpoint.toString());
        }
    }

    logger->info("DHT node initialized on port {}", m_dhtNode->getPort());
    return true;
}

bool DHTCrawler::initializeInfoHashCollector() {
    // Create the info hash collector
    m_infoHashCollector = std::make_shared<InfoHashCollector>(m_config.infoHashCollectorConfig);

    // Set the callback for new info hashes
    m_infoHashCollector->setNewInfoHashCallback(
        [this](const dht::InfoHash& infoHash) {
            handleNewInfoHash(infoHash);
        });

    // Start the info hash collector
    if (!m_infoHashCollector->start()) {
        logger->error("Failed to start info hash collector");
        return false;
    }

    // Set the info hash collector in the DHT node
    m_dhtNode->setInfoHashCollector(m_infoHashCollector);

    logger->info("Info hash collector initialized");
    return true;
}

bool DHTCrawler::initializeMetadataFetcher() {
    // Create the metadata fetcher
    m_metadataFetcher = std::make_shared<metadata::MetadataFetcher>(m_config.metadataFetcherConfig);

    // Start the metadata fetcher
    if (!m_metadataFetcher->start()) {
        logger->error("Failed to start metadata fetcher");
        return false;
    }

    logger->info("Metadata fetcher initialized");
    return true;
}

bool DHTCrawler::initializeMetadataStorage() {
    // Create the metadata storage
    m_metadataStorage = std::make_shared<metadata::MetadataStorage>(m_config.metadataStorageConfig);

    // Initialize the metadata storage
    if (!m_metadataStorage->initialize()) {
        logger->error("Failed to initialize metadata storage");
        return false;
    }

    logger->info("Metadata storage initialized with {} items", m_metadataStorage->getMetadataCount());
    return true;
}

void DHTCrawler::performRandomLookup() {
    // Generate a random node ID
    dht::NodeID targetID = dht::generateRandomNodeID();

    // Create a lookup request
    LookupRequest request;
    request.targetID = targetID;
    request.requestTime = std::chrono::steady_clock::now();

    // Add the lookup request to the queue
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lookupQueue.push(request);
    }

    // Update the lookup rate
    updateLookupRate();
}

void DHTCrawler::handleLookupCompletion(const dht::NodeID& targetID, const std::vector<std::shared_ptr<dht::Node>>& nodes) {
    // Find the lookup request
    std::string targetIDStr = infoHashToString(targetID);

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_activeLookups.find(targetIDStr);
    if (it == m_activeLookups.end()) {
        return;
    }

    // Mark the lookup as completed
    it->second.completed = true;
    it->second.endTime = std::chrono::steady_clock::now();

    // Remove from active lookups
    m_activeLookups.erase(it);

    // Log the result
    logger->debug("Lookup completed for target ID: {}, found {} nodes", targetIDStr, nodes.size());
}

void DHTCrawler::fetchMetadata(const dht::InfoHash& infoHash) {
    // Create a metadata fetch request
    MetadataFetchRequest request;
    request.infoHash = infoHash;
    request.requestTime = std::chrono::steady_clock::now();
    request.priority = 0; // Default priority

    // Add the metadata fetch request to the queue
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_metadataFetchQueue.push(request);
        m_infoHashesQueued++;
    }

    // Update the metadata fetch rate
    updateMetadataFetchRate();
}

void DHTCrawler::handleMetadataFetchCompletion(
    const dht::InfoHash& infoHash,
    const std::vector<uint8_t>& metadata,
    uint32_t size,
    bool success) {
    
    // Find the metadata fetch request
    std::string infoHashStr = infoHashToString(infoHash);

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_activeMetadataFetches.find(infoHashStr);
    if (it == m_activeMetadataFetches.end()) {
        return;
    }

    // Mark the metadata fetch as completed
    it->second.completed = true;
    it->second.endTime = std::chrono::steady_clock::now();

    // Remove from active metadata fetches
    m_activeMetadataFetches.erase(it);

    // Log the result
    if (success) {
        logger->debug("Metadata fetch completed for info hash: {}, size: {}", infoHashStr, size);
        m_metadataFetched++;
    } else {
        logger->debug("Metadata fetch failed for info hash: {}", infoHashStr);
    }
}

void DHTCrawler::handleNewInfoHash(const dht::InfoHash& infoHash) {
    std::string infoHashStr = infoHashToString(infoHash);

    // Check if we've already processed this info hash
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_processedInfoHashes.find(infoHashStr) != m_processedInfoHashes.end()) {
            return;
        }

        // Add to processed info hashes
        m_processedInfoHashes.insert(infoHashStr);
        m_infoHashesDiscovered++;
    }

    // Queue for metadata fetching
    fetchMetadata(infoHash);

    logger->debug("New info hash discovered: {}", infoHashStr);
}

void DHTCrawler::lookupThread() {
    logger->debug("Lookup thread started");

    while (m_running) {
        // Check if we can perform a lookup
        if (isLookupAllowed()) {
            // Check if we have any lookup requests in the queue
            LookupRequest request;
            bool hasRequest = false;

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_lookupQueue.empty()) {
                    request = m_lookupQueue.front();
                    m_lookupQueue.pop();
                    hasRequest = true;
                }
            }

            if (hasRequest) {
                // Start the lookup
                request.startTime = std::chrono::steady_clock::now();

                // Add to active lookups
                std::string targetIDStr = infoHashToString(request.targetID);
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_activeLookups[targetIDStr] = request;
                }

                // Perform the lookup
                m_dhtNode->findClosestNodes(request.targetID,
                    [this, targetID = request.targetID](const std::vector<std::shared_ptr<dht::Node>>& nodes) {
                        handleLookupCompletion(targetID, nodes);
                    });

                logger->debug("Started lookup for target ID: {}", targetIDStr);
            } else {
                // No lookup requests in the queue, generate a random one
                performRandomLookup();
            }
        }

        // Sleep for the lookup interval
        std::this_thread::sleep_for(m_config.lookupInterval);
    }

    logger->debug("Lookup thread stopped");
}

void DHTCrawler::metadataFetchThread() {
    logger->debug("Metadata fetch thread started");

    while (m_running) {
        // Check if we can perform a metadata fetch
        if (isMetadataFetchAllowed()) {
            // Check if we have any metadata fetch requests in the queue
            MetadataFetchRequest request;
            bool hasRequest = false;

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_metadataFetchQueue.empty()) {
                    request = m_metadataFetchQueue.top();
                    m_metadataFetchQueue.pop();
                    hasRequest = true;
                }
            }

            if (hasRequest) {
                // Start the metadata fetch
                request.startTime = std::chrono::steady_clock::now();

                // Add to active metadata fetches
                std::string infoHashStr = infoHashToString(request.infoHash);
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_activeMetadataFetches[infoHashStr] = request;
                }

                // Perform the metadata fetch
                m_metadataFetcher->fetchMetadata(request.infoHash,
                    [this, infoHash = request.infoHash](const std::array<uint8_t, 20>& infoHash,
                                                      const std::vector<uint8_t>& metadata,
                                                      uint32_t size,
                                                      bool success) {
                        handleMetadataFetchCompletion(infoHash, metadata, size, success);
                    });

                logger->debug("Started metadata fetch for info hash: {}", infoHashStr);
            }
        }

        // Sleep for the metadata fetch interval
        std::this_thread::sleep_for(m_config.metadataFetchInterval);
    }

    logger->debug("Metadata fetch thread stopped");
}

void DHTCrawler::statusThread() {
    logger->debug("Status thread started");

    while (m_running) {
        // Update statistics
        uint64_t infoHashesDiscovered = m_infoHashesDiscovered;
        uint64_t infoHashesQueued = m_infoHashesQueued;
        uint64_t metadataFetched = m_metadataFetched;
        double lookupRate = m_lookupRate;
        double metadataFetchRate = m_metadataFetchRate;

        // Log status
        logger->info("Status: {} info hashes discovered, {} queued, {} metadata fetched, lookup rate: {:.2f}/min, metadata fetch rate: {:.2f}/min",
                    infoHashesDiscovered, infoHashesQueued, metadataFetched, lookupRate, metadataFetchRate);

        // Call the status callback
        if (m_statusCallback) {
            m_statusCallback(infoHashesDiscovered, infoHashesQueued, metadataFetched, lookupRate, metadataFetchRate);
        }

        // Prioritize info hashes
        if (m_config.prioritizePopularInfoHashes) {
            prioritizeInfoHashes();
        }

        // Sleep for the status interval
        std::this_thread::sleep_for(m_config.statusInterval);
    }

    logger->debug("Status thread stopped");
}

void DHTCrawler::updateLookupRate() {
    auto now = std::chrono::steady_clock::now();

    // Add the current time to the lookup times
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lookupTimes.push_back(now);

        // Remove lookup times older than 1 minute
        auto oneMinuteAgo = now - std::chrono::minutes(1);
        m_lookupTimes.erase(
            std::remove_if(m_lookupTimes.begin(), m_lookupTimes.end(),
                          [oneMinuteAgo](const auto& time) { return time < oneMinuteAgo; }),
            m_lookupTimes.end());

        // Calculate the lookup rate
        m_lookupRate = static_cast<double>(m_lookupTimes.size());
    }
}

void DHTCrawler::updateMetadataFetchRate() {
    auto now = std::chrono::steady_clock::now();

    // Add the current time to the metadata fetch times
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_metadataFetchTimes.push_back(now);

        // Remove metadata fetch times older than 1 minute
        auto oneMinuteAgo = now - std::chrono::minutes(1);
        m_metadataFetchTimes.erase(
            std::remove_if(m_metadataFetchTimes.begin(), m_metadataFetchTimes.end(),
                          [oneMinuteAgo](const auto& time) { return time < oneMinuteAgo; }),
            m_metadataFetchTimes.end());

        // Calculate the metadata fetch rate
        m_metadataFetchRate = static_cast<double>(m_metadataFetchTimes.size());
    }
}

bool DHTCrawler::isLookupAllowed() {
    // Check if we have reached the maximum number of concurrent lookups
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_activeLookups.size() >= m_config.maxConcurrentLookups) {
            return false;
        }
    }

    // Check if we have reached the maximum lookup rate
    if (m_lookupRate >= m_config.maxLookupsPerMinute) {
        return false;
    }

    return true;
}

bool DHTCrawler::isMetadataFetchAllowed() {
    // Check if we have reached the maximum number of concurrent metadata fetches
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_activeMetadataFetches.size() >= m_config.maxConcurrentMetadataFetches) {
            return false;
        }
    }

    // Check if we have reached the maximum metadata fetch rate
    if (m_metadataFetchRate >= m_config.maxMetadataFetchesPerMinute) {
        return false;
    }

    return true;
}

void DHTCrawler::prioritizeInfoHashes() {
    // This is a simple implementation that prioritizes info hashes based on their age
    // More sophisticated prioritization could be implemented based on popularity, etc.

    std::lock_guard<std::mutex> lock(m_mutex);

    // Create a temporary queue
    std::priority_queue<MetadataFetchRequest> tempQueue;

    // Process all requests in the queue
    while (!m_metadataFetchQueue.empty()) {
        auto request = m_metadataFetchQueue.top();
        m_metadataFetchQueue.pop();

        // Calculate priority based on age (older requests get higher priority)
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - request.requestTime).count();
        request.priority = static_cast<uint32_t>(age);

        // Add to the temporary queue
        tempQueue.push(request);
    }

    // Swap the queues
    m_metadataFetchQueue = std::move(tempQueue);
}

std::string DHTCrawler::infoHashToString(const dht::InfoHash& infoHash) const {
    return util::bytesToHex(infoHash.data(), infoHash.size());
}

} // namespace dht_hunter::crawler
