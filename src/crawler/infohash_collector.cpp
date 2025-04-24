#include "dht_hunter/crawler/infohash_collector.hpp"
#include "dht_hunter/util/hash.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace dht_hunter::crawler {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Crawler.InfoHashCollector");
}

InfoHashCollector::InfoHashCollector(const InfoHashCollectorConfig& config)
    : m_config(config) {
}

InfoHashCollector::~InfoHashCollector() {
    stop();
}

bool InfoHashCollector::start() {
    if (m_running) {
        logger->warning("InfoHashCollector is already running");
        return false;
    }

    m_running = true;

    // Start the processing thread
    m_processingThread = std::thread(&InfoHashCollector::processQueue, this);

    // Start the periodic save thread if a save path is set
    if (!m_savePath.empty()) {
        m_saveThread = std::thread(&InfoHashCollector::periodicSave, this);
    }

    logger->info("InfoHashCollector started");
    return true;
}

void InfoHashCollector::stop() {
    if (!m_running) {
        return;
    }

    m_running = false;

    // Wait for the processing thread to finish
    if (m_processingThread.joinable()) {
        m_processingThread.join();
    }

    // Wait for the save thread to finish
    if (m_saveThread.joinable()) {
        m_saveThread.join();
    }

    logger->info("InfoHashCollector stopped");
}

bool InfoHashCollector::isRunning() const {
    return m_running;
}

bool InfoHashCollector::addInfoHash(const dht_hunter::dht::InfoHash& infoHash) {
    if (!m_running) {
        logger->warning("Cannot add infohash: InfoHashCollector is not running");
        return false;
    }

    // Check if the queue is full
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.size() >= m_config.maxQueueSize) {
            logger->warning("Infohash queue is full, dropping infohash");
            return false;
        }

        // Add to the queue
        m_queue.push(infoHash);
    }

    return true;
}

size_t InfoHashCollector::getInfoHashCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_infoHashes.size();
}

std::vector<dht_hunter::dht::InfoHash> InfoHashCollector::getInfoHashes() const {
    std::vector<dht_hunter::dht::InfoHash> result;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        result.reserve(m_infoHashes.size());
        
        for (const auto& hexStr : m_infoHashes) {
            auto bytes = dht_hunter::util::hexToBytes(hexStr);
            dht_hunter::dht::InfoHash infoHash;
            std::copy(bytes.begin(), bytes.end(), infoHash.begin());
            result.push_back(infoHash);
        }
    }
    
    return result;
}

std::vector<dht_hunter::dht::InfoHash> InfoHashCollector::getInfoHashBatch(size_t count) const {
    std::vector<dht_hunter::dht::InfoHash> result;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t batchSize = std::min(count, m_infoHashes.size());
        result.reserve(batchSize);
        
        auto it = m_infoHashes.begin();
        for (size_t i = 0; i < batchSize && it != m_infoHashes.end(); ++i, ++it) {
            auto bytes = dht_hunter::util::hexToBytes(*it);
            dht_hunter::dht::InfoHash infoHash;
            std::copy(bytes.begin(), bytes.end(), infoHash.begin());
            result.push_back(infoHash);
        }
    }
    
    return result;
}

void InfoHashCollector::clearInfoHashes() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_infoHashes.clear();
    
    // Reset counters
    m_totalProcessed = 0;
    m_totalAdded = 0;
    m_totalDuplicates = 0;
    m_totalInvalid = 0;
    
    logger->info("Cleared all infohashes");
}

bool InfoHashCollector::saveInfoHashes(const std::string& filePath) const {
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        logger->error("Failed to open file for writing: {}", filePath);
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Write the number of infohashes
        uint32_t count = static_cast<uint32_t>(m_infoHashes.size());
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));
        
        // Write each infohash
        for (const auto& hexStr : m_infoHashes) {
            auto bytes = dht_hunter::util::hexToBytes(hexStr);
            file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }
    }
    
    logger->info("Saved {} infohashes to {}", m_infoHashes.size(), filePath);
    return true;
}

bool InfoHashCollector::loadInfoHashes(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        logger->error("Failed to open file for reading: {}", filePath);
        return false;
    }
    
    // Read the number of infohashes
    uint32_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    
    if (!file) {
        logger->error("Failed to read infohash count from file: {}", filePath);
        return false;
    }
    
    // Read each infohash
    std::unordered_set<std::string> loadedInfoHashes;
    for (uint32_t i = 0; i < count; ++i) {
        dht_hunter::dht::InfoHash infoHash;
        file.read(reinterpret_cast<char*>(infoHash.data()), infoHash.size());
        
        if (!file && !file.eof()) {
            logger->error("Failed to read infohash from file: {}", filePath);
            return false;
        }
        
        // Convert to hex string and add to set
        std::string hexStr = dht_hunter::util::bytesToHex(infoHash.data(), infoHash.size());
        loadedInfoHashes.insert(hexStr);
    }
    
    // Replace the current set with the loaded set
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_infoHashes = std::move(loadedInfoHashes);
    }
    
    logger->info("Loaded {} infohashes from {}", count, filePath);
    return true;
}

void InfoHashCollector::setNewInfoHashCallback(InfoHashCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_newInfoHashCallback = callback;
}

InfoHashCollectorConfig InfoHashCollector::getConfig() const {
    return m_config;
}

void InfoHashCollector::setConfig(const InfoHashCollectorConfig& config) {
    m_config = config;
}

void InfoHashCollector::processQueue() {
    logger->debug("InfoHash processing thread started");
    
    while (m_running) {
        dht_hunter::dht::InfoHash infoHash;
        bool hasInfoHash = false;
        
        // Get an infohash from the queue
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_queue.empty()) {
                infoHash = m_queue.front();
                m_queue.pop();
                hasInfoHash = true;
            }
        }
        
        if (hasInfoHash) {
            m_totalProcessed++;
            
            // Check if the infohash is valid
            if (m_config.filterInvalidInfoHashes && !isValidInfoHash(infoHash)) {
                m_totalInvalid++;
                continue;
            }
            
            // Convert to hex string for storage
            std::string hexStr = dht_hunter::util::bytesToHex(infoHash.data(), infoHash.size());
            
            bool added = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                
                // Check if we need to deduplicate
                if (m_config.deduplicateInfoHashes) {
                    if (m_infoHashes.find(hexStr) != m_infoHashes.end()) {
                        m_totalDuplicates++;
                        continue;
                    }
                }
                
                // Check if we have reached the maximum number of infohashes
                if (m_infoHashes.size() >= m_config.maxStoredInfoHashes) {
                    logger->warning("Maximum number of infohashes reached, dropping new infohash");
                    continue;
                }
                
                // Add the infohash
                m_infoHashes.insert(hexStr);
                added = true;
                m_totalAdded++;
                
                // Call the callback if set
                if (m_newInfoHashCallback) {
                    m_newInfoHashCallback(infoHash);
                }
            }
            
            if (added) {
                logger->debug("Added new infohash: {}", hexStr);
            }
        } else {
            // No infohash in the queue, sleep for a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    logger->debug("InfoHash processing thread stopped");
}

void InfoHashCollector::periodicSave() {
    logger->debug("Periodic save thread started");
    
    while (m_running) {
        // Sleep for the save interval
        for (int i = 0; i < m_config.saveInterval.count() && m_running; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (!m_running) {
            break;
        }
        
        // Save the infohashes
        if (!m_savePath.empty()) {
            saveInfoHashes(m_savePath);
        }
    }
    
    logger->debug("Periodic save thread stopped");
}

bool InfoHashCollector::isValidInfoHash(const dht_hunter::dht::InfoHash& infoHash) const {
    // Check if the infohash is all zeros
    bool allZeros = true;
    for (auto byte : infoHash) {
        if (byte != 0) {
            allZeros = false;
            break;
        }
    }
    
    if (allZeros) {
        return false;
    }
    
    // Add more validation checks as needed
    
    return true;
}

} // namespace dht_hunter::crawler
