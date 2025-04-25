#include "dht_hunter/storage/metadata_storage_adapter.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

#include <iostream>
#include <random>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <thread>
#include <filesystem>

DEFINE_COMPONENT_LOGGER("Storage.AdapterTest", "Test")

// Helper function to generate random data
std::vector<uint8_t> generateRandomData(size_t size) {
    std::vector<uint8_t> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (auto& byte : data) {
        byte = static_cast<uint8_t>(dis(gen));
    }
    
    return data;
}

// Helper function to generate random info hash
std::array<uint8_t, 20> generateRandomInfoHash() {
    std::array<uint8_t, 20> infoHash;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (auto& byte : infoHash) {
        byte = static_cast<uint8_t>(dis(gen));
    }
    
    return infoHash;
}

// Helper function to convert info hash to hex string
std::string infoHashToHex(const std::array<uint8_t, 20>& infoHash) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (auto byte : infoHash) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

int main() {
    // Initialize logging
    dht_hunter::logforge::LogForge::init(
        dht_hunter::logforge::LogLevel::DEBUG,
        dht_hunter::logforge::LogLevel::DEBUG,
        "metadata_storage_adapter_test.log",
        true,
        false
    );
    
    getLogger()->info("Starting metadata storage adapter test");
    
    // Create test directories
    std::filesystem::path testDir = "adapter_test";
    std::filesystem::path storageDir = testDir / "storage";
    std::filesystem::path torrentDir = testDir / "torrents";
    
    // Remove test directories if they exist
    if (std::filesystem::exists(testDir)) {
        std::filesystem::remove_all(testDir);
    }
    
    // Create test directories
    std::filesystem::create_directories(storageDir);
    std::filesystem::create_directories(torrentDir);
    
    // Create storage configuration
    dht_hunter::metadata::MetadataStorageConfig config;
    config.storageDirectory = storageDir.string();
    config.torrentFilesDirectory = torrentDir.string();
    config.persistMetadata = true;
    config.createTorrentFiles = true;
    config.maxMetadataItems = 1000;
    config.deduplicateMetadata = true;
    
    // Create storage adapter
    dht_hunter::storage::MetadataStorageAdapter adapter(config);
    
    // Initialize adapter
    if (!adapter.initialize()) {
        getLogger()->error("Failed to initialize adapter");
        return 1;
    }
    
    getLogger()->info("Adapter initialized");
    
    // Test adding metadata
    const int numEntries = 10;
    std::vector<std::array<uint8_t, 20>> infoHashes;
    std::vector<size_t> dataSizes;
    
    getLogger()->info("Adding {} random metadata entries", numEntries);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numEntries; ++i) {
        // Generate random info hash
        auto infoHash = generateRandomInfoHash();
        infoHashes.push_back(infoHash);
        
        // Generate random data (between 1KB and 100KB)
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dis(1024, 102400);
        size_t dataSize = dis(gen);
        dataSizes.push_back(dataSize);
        
        auto data = generateRandomData(dataSize);
        
        // Add metadata
        bool success = adapter.addMetadata(infoHash, data, static_cast<uint32_t>(data.size()));
        
        if (success) {
            getLogger()->debug("Added metadata for info hash: {}", infoHashToHex(infoHash));
        } else {
            getLogger()->error("Failed to add metadata for info hash: {}", infoHashToHex(infoHash));
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    getLogger()->info("Added {} metadata entries in {} ms", numEntries, duration);
    getLogger()->info("Average time per entry: {} ms", static_cast<double>(duration) / numEntries);
    
    // Test retrieving metadata
    getLogger()->info("Retrieving metadata entries");
    
    startTime = std::chrono::high_resolution_clock::now();
    
    int successCount = 0;
    for (size_t i = 0; i < infoHashes.size(); ++i) {
        auto [data, size] = adapter.getMetadata(infoHashes[i]);
        
        if (!data.empty() && size > 0) {
            if (size == dataSizes[i]) {
                successCount++;
            } else {
                getLogger()->error("Size mismatch for info hash: {}", infoHashToHex(infoHashes[i]));
            }
        } else {
            getLogger()->error("Failed to retrieve metadata for info hash: {}", infoHashToHex(infoHashes[i]));
        }
    }
    
    endTime = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    getLogger()->info("Retrieved {} metadata entries in {} ms", successCount, duration);
    getLogger()->info("Average time per entry: {} ms", static_cast<double>(duration) / successCount);
    
    // Test retrieving torrent files
    getLogger()->info("Retrieving torrent files");
    
    startTime = std::chrono::high_resolution_clock::now();
    
    successCount = 0;
    for (const auto& infoHash : infoHashes) {
        auto torrentFile = adapter.getTorrentFile(infoHash);
        
        if (torrentFile) {
            successCount++;
        } else {
            getLogger()->error("Failed to retrieve torrent file for info hash: {}", infoHashToHex(infoHash));
        }
    }
    
    endTime = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    getLogger()->info("Retrieved {} torrent files in {} ms", successCount, duration);
    getLogger()->info("Average time per file: {} ms", static_cast<double>(duration) / successCount);
    
    // Test metadata count
    size_t count = adapter.getMetadataCount();
    getLogger()->info("Metadata count: {}", count);
    
    // Test hasMetadata
    for (const auto& infoHash : infoHashes) {
        if (!adapter.hasMetadata(infoHash)) {
            getLogger()->error("Metadata not found for info hash: {}", infoHashToHex(infoHash));
        }
    }
    
    getLogger()->info("Metadata storage adapter test completed");
    
    return 0;
}
