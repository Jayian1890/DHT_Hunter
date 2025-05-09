#pragma once

#include "dht_hunter/bencode/bencode.hpp"
#include "dht_hunter/types/info_hash.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"
#include "utils/utility.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <fstream>

namespace dht_hunter::bittorrent {

/**
 * @brief Metadata persistence
 */
class MetadataPersistence {
public:
    /**
     * @brief Gets the instance
     * @return The instance
     */
    static MetadataPersistence& getInstance() {
        static MetadataPersistence instance;
        return instance;
    }

    /**
     * @brief Saves metadata
     * @param infoHash The info hash
     * @param metadata The metadata
     * @return True if the metadata was saved, false otherwise
     */
    bool saveMetadata(const types::InfoHash& infoHash, std::shared_ptr<bencode::BencodeValue> metadata) {
        std::lock_guard<std::mutex> lock(m_metadataMutex);
        m_metadata[infoHash] = metadata;
        return saveToFile();
    }

    /**
     * @brief Gets metadata
     * @param infoHash The info hash
     * @return The metadata or nullptr if not found
     */
    std::shared_ptr<bencode::BencodeValue> getMetadata(const types::InfoHash& infoHash) {
        std::lock_guard<std::mutex> lock(m_metadataMutex);
        auto it = m_metadata.find(infoHash);
        if (it != m_metadata.end()) {
            return it->second;
        }
        return nullptr;
    }

    /**
     * @brief Gets all metadata
     * @return The metadata map
     */
    std::unordered_map<types::InfoHash, std::shared_ptr<bencode::BencodeValue>> getAllMetadata() {
        std::lock_guard<std::mutex> lock(m_metadataMutex);
        return m_metadata;
    }

    /**
     * @brief Loads metadata from file
     * @param filePath The file path
     * @return True if the metadata was loaded, false otherwise
     */
    bool loadFromFile(const std::string& filePath = "metadata.dat") {
        std::lock_guard<std::mutex> lock(m_metadataMutex);
        m_filePath = filePath;

        // Check if the file exists
        if (!utility::file::fileExists(filePath)) {
            unified_event::logInfo("BitTorrent.MetadataPersistence", "Metadata file does not exist: " + filePath);
            return false;
        }

        try {
            // Open the file
            std::ifstream file(filePath, std::ios::binary);
            if (!file.is_open()) {
                unified_event::logError("BitTorrent.MetadataPersistence", "Failed to open metadata file: " + filePath);
                return false;
            }

            // Read the file size
            file.seekg(0, std::ios::end);
            auto fileSize = file.tellg();
            file.seekg(0, std::ios::beg);

            // Read the file content
            std::vector<char> buffer(static_cast<size_t>(fileSize));
            file.read(buffer.data(), static_cast<std::streamsize>(fileSize));

            // Close the file
            file.close();

            // Decode the file content
            auto data = bencode::BencodeValue::decode(buffer.data(), static_cast<size_t>(buffer.size()));
            if (!data || !data->isDictionary()) {
                unified_event::logError("BitTorrent.MetadataPersistence", "Invalid metadata file format: " + filePath);
                return false;
            }

            // Clear the current metadata
            m_metadata.clear();

            // Get the metadata dictionary
            auto metadataDict = data->getDict("metadata");
            if (!metadataDict) {
                unified_event::logError("BitTorrent.MetadataPersistence", "No metadata found in file: " + filePath);
                return false;
            }

            // Iterate over the metadata dictionary
            for (const auto& [key, value] : *metadataDict) {
                // Convert the key to an info hash
                if (key.size() != 40) {
                    unified_event::logWarning("BitTorrent.MetadataPersistence", "Invalid info hash in metadata file: " + key);
                    continue;
                }

                types::InfoHash infoHash;
                try {
                    types::infoHashFromString(key, infoHash);
                } catch (const std::exception& e) {
                    unified_event::logWarning("BitTorrent.MetadataPersistence", "Failed to parse info hash in metadata file: " + key);
                    continue;
                }

                // Add the metadata to the map
                m_metadata[infoHash] = std::make_shared<bencode::BencodeValue>(*value);
            }

            unified_event::logInfo("BitTorrent.MetadataPersistence", "Successfully loaded metadata from " + filePath + " with " + std::to_string(m_metadata.size()) + " entries");
            return true;
        } catch (const std::exception& e) {
            unified_event::logError("BitTorrent.MetadataPersistence", "Failed to load metadata from file: " + filePath + ", error: " + e.what());
            return false;
        }
    }

private:
    /**
     * @brief Constructor
     */
    MetadataPersistence() : m_filePath("metadata.dat") {
        // Load metadata from file
        loadFromFile(m_filePath);
    }

    /**
     * @brief Saves metadata to file
     * @return True if the metadata was saved, false otherwise
     */
    bool saveToFile() {
        try {
            // Create a dictionary for the metadata
            auto data = std::make_shared<bencode::BencodeValue>();
            bencode::BencodeValue::Dictionary dict;
            data->setDict(dict);

            // Create a dictionary for the metadata
            bencode::BencodeValue::Dictionary metadataDict;

            // Add the metadata to the dictionary
            for (const auto& [infoHash, metadata] : m_metadata) {
                std::string infoHashStr = types::infoHashToString(infoHash);
                metadataDict[infoHashStr] = metadata;
            }

            // Add the metadata dictionary to the data
            data->setDict("metadata", metadataDict);

            // Encode the data
            auto encoded = data->encode();

            // Open the file
            std::ofstream file(m_filePath, std::ios::binary | std::ios::trunc);
            if (!file.is_open()) {
                unified_event::logError("BitTorrent.MetadataPersistence", "Failed to open metadata file for writing: " + m_filePath);
                return false;
            }

            // Write the data to the file
            file.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));

            // Close the file
            file.close();

            unified_event::logInfo("BitTorrent.MetadataPersistence", "Successfully saved metadata to " + m_filePath + " with " + std::to_string(m_metadata.size()) + " entries");
            return true;
        } catch (const std::exception& e) {
            unified_event::logError("BitTorrent.MetadataPersistence", "Failed to save metadata to file: " + m_filePath + ", error: " + e.what());
            return false;
        }
    }

    std::string m_filePath;
    std::unordered_map<types::InfoHash, std::shared_ptr<bencode::BencodeValue>> m_metadata;
    std::mutex m_metadataMutex;
};

} // namespace dht_hunter::bittorrent
