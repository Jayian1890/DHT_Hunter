#include "dht_hunter/storage/metadata_storage.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

#include <iostream>
#include <random>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <thread>

DEFINE_COMPONENT_LOGGER("Storage.Test", "Test")

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
        "metadata_storage_test.log",
        true,
        false
    );

    getLogger()->info("Starting metadata storage test");

    // Create storage
    std::filesystem::path basePath = "storage_test";
    dht_hunter::storage::MetadataStorage storage(basePath);

    getLogger()->info("Created metadata storage at {}", basePath.string());

    // Test adding metadata
    const int numEntries = 100;
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
        bool success = storage.addMetadata(infoHash, data.data(), static_cast<uint32_t>(data.size()));

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
        auto result = storage.getMetadata(infoHashes[i]);

        if (result) {
            auto [data, size] = *result;
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

    // Test removing metadata
    getLogger()->info("Removing metadata entries");

    startTime = std::chrono::high_resolution_clock::now();

    successCount = 0;
    for (const auto& infoHash : infoHashes) {
        bool success = storage.removeMetadata(infoHash);

        if (success) {
            successCount++;
        } else {
            getLogger()->error("Failed to remove metadata for info hash: {}", infoHashToHex(infoHash));
        }
    }

    endTime = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    getLogger()->info("Removed {} metadata entries in {} ms", successCount, duration);
    getLogger()->info("Average time per entry: {} ms", static_cast<double>(duration) / successCount);

    // Verify all entries are removed
    for (const auto& infoHash : infoHashes) {
        if (storage.exists(infoHash)) {
            getLogger()->error("Metadata still exists for info hash: {}", infoHashToHex(infoHash));
        }
    }

    getLogger()->info("Metadata storage test completed");

    return 0;
}
