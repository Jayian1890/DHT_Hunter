#include "dht_hunter/storage/metadata_storage.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <chrono>

namespace dht_hunter::storage {

DEFINE_COMPONENT_LOGGER("Storage.MetadataStorage", "Storage")

MetadataStorage::MetadataStorage(const std::filesystem::path& basePath, int shardingLevel)
    : m_basePath(basePath), m_shardingLevel(shardingLevel), m_indexDirty(false) {

    // Create base directory if it doesn't exist
    if (!std::filesystem::exists(m_basePath)) {
        getLogger()->info("Creating base directory: {}", m_basePath.string());
        std::filesystem::create_directories(m_basePath);
    }

    // Create index directory
    std::filesystem::path indexDir = m_basePath / "index";
    if (!std::filesystem::exists(indexDir)) {
        getLogger()->info("Creating index directory: {}", indexDir.string());
        std::filesystem::create_directories(indexDir);
    }

    // Load index
    loadIndex();

    getLogger()->info("Metadata storage initialized at {}", m_basePath.string());
    getLogger()->info("Using sharding level: {}", m_shardingLevel);
    getLogger()->info("Loaded {} entries from index", m_index.size());
}

MetadataStorage::~MetadataStorage() {
    // Save index if dirty
    if (m_indexDirty) {
        saveIndex();
    }
}

bool MetadataStorage::addMetadata(const std::array<uint8_t, 20>& infoHash, const uint8_t* data, uint32_t size) {
    // Check if metadata already exists
    if (exists(infoHash)) {
        getLogger()->debug("Metadata already exists for info hash: {}", infoHashToHex(infoHash));
        return true;
    }

    // Create directory structure
    if (!createDirectoryStructure(infoHash)) {
        getLogger()->error("Failed to create directory structure for info hash: {}", infoHashToHex(infoHash));
        return false;
    }

    // Get metadata path
    std::filesystem::path metadataPath = getMetadataPath(infoHash);

    // Write metadata to file
    try {
        std::ofstream file(metadataPath, std::ios::binary);
        if (!file) {
            getLogger()->error("Failed to open file for writing: {}", metadataPath.string());
            return false;
        }

        // Write size as header (4 bytes)
        file.write(reinterpret_cast<const char*>(&size), sizeof(size));

        // Write data
        file.write(reinterpret_cast<const char*>(data), size);

        if (!file) {
            getLogger()->error("Failed to write metadata to file: {}", metadataPath.string());
            return false;
        }

        file.close();
    } catch (const std::exception& e) {
        getLogger()->error("Exception while writing metadata: {}", e.what());
        return false;
    }

    // Update index
    if (!updateIndex(infoHash, "add")) {
        getLogger()->error("Failed to update index for info hash: {}", infoHashToHex(infoHash));
        // Try to remove the file
        std::filesystem::remove(metadataPath);
        return false;
    }

    getLogger()->debug("Added metadata for info hash: {}", infoHashToHex(infoHash));
    return true;
}

std::optional<std::pair<std::vector<uint8_t>, uint32_t>> MetadataStorage::getMetadata(const std::array<uint8_t, 20>& infoHash) {
    // Check if metadata exists
    if (!exists(infoHash)) {
        getLogger()->debug("Metadata does not exist for info hash: {}", infoHashToHex(infoHash));
        return std::nullopt;
    }

    // Get metadata path
    std::filesystem::path metadataPath = getMetadataPath(infoHash);

    // Read metadata from file
    try {
        std::ifstream file(metadataPath, std::ios::binary);
        if (!file) {
            getLogger()->error("Failed to open file for reading: {}", metadataPath.string());
            return std::nullopt;
        }

        // Read size from header (4 bytes)
        uint32_t size;
        file.read(reinterpret_cast<char*>(&size), sizeof(size));

        if (!file) {
            getLogger()->error("Failed to read size from file: {}", metadataPath.string());
            return std::nullopt;
        }

        // Read data
        std::vector<uint8_t> data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);

        if (!file) {
            getLogger()->error("Failed to read data from file: {}", metadataPath.string());
            return std::nullopt;
        }

        file.close();

        getLogger()->debug("Retrieved metadata for info hash: {}", infoHashToHex(infoHash));
        return std::make_pair(std::move(data), size);
    } catch (const std::exception& e) {
        getLogger()->error("Exception while reading metadata: {}", e.what());
        return std::nullopt;
    }
}

bool MetadataStorage::exists(const std::array<uint8_t, 20>& infoHash) {
    std::filesystem::path metadataPath = getMetadataPath(infoHash);
    return std::filesystem::exists(metadataPath);
}

bool MetadataStorage::removeMetadata(const std::array<uint8_t, 20>& infoHash) {
    // Check if metadata exists
    if (!exists(infoHash)) {
        getLogger()->debug("Metadata does not exist for info hash: {}", infoHashToHex(infoHash));
        return true;
    }

    // Get metadata path
    std::filesystem::path metadataPath = getMetadataPath(infoHash);

    // Remove file
    try {
        if (!std::filesystem::remove(metadataPath)) {
            getLogger()->error("Failed to remove file: {}", metadataPath.string());
            return false;
        }
    } catch (const std::exception& e) {
        getLogger()->error("Exception while removing metadata: {}", e.what());
        return false;
    }

    // Update index
    if (!updateIndex(infoHash, "remove")) {
        getLogger()->error("Failed to update index for info hash: {}", infoHashToHex(infoHash));
        return false;
    }

    getLogger()->debug("Removed metadata for info hash: {}", infoHashToHex(infoHash));
    return true;
}

size_t MetadataStorage::count() const {
    return m_index.size();
}

std::filesystem::path MetadataStorage::getBasePath() const {
    return m_basePath;
}

std::string MetadataStorage::infoHashToHex(const std::array<uint8_t, 20>& infoHash) const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (auto byte : infoHash) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

std::filesystem::path MetadataStorage::getMetadataPath(const std::array<uint8_t, 20>& infoHash) const {
    std::string hexHash = infoHashToHex(infoHash);

    std::filesystem::path path = m_basePath / "data";

    // Create sharded path
    for (size_t i = 0; i < static_cast<size_t>(m_shardingLevel) && i * 2 < hexHash.length(); ++i) {
        path /= hexHash.substr(i * 2, 2);
    }

    // Add the full hash as the filename
    path /= hexHash;

    return path;
}

bool MetadataStorage::createDirectoryStructure(const std::array<uint8_t, 20>& infoHash) {
    std::string hexHash = infoHashToHex(infoHash);

    std::filesystem::path path = m_basePath / "data";

    try {
        // Create data directory if it doesn't exist
        if (!std::filesystem::exists(path)) {
            std::filesystem::create_directories(path);
        }

        // Create sharded directories
        for (size_t i = 0; i < static_cast<size_t>(m_shardingLevel) && i * 2 < hexHash.length(); ++i) {
            path /= hexHash.substr(i * 2, 2);
            if (!std::filesystem::exists(path)) {
                std::filesystem::create_directories(path);
            }
        }

        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception while creating directory structure: {}", e.what());
        return false;
    }
}

bool MetadataStorage::updateIndex(const std::array<uint8_t, 20>& infoHash, const std::string& operation) {
    std::string hexHash = infoHashToHex(infoHash);

    if (operation == "add") {
        // Check if already in index
        if (std::find(m_index.begin(), m_index.end(), hexHash) == m_index.end()) {
            m_index.push_back(hexHash);
            m_indexDirty = true;
        }
    } else if (operation == "remove") {
        // Remove from index
        auto it = std::find(m_index.begin(), m_index.end(), hexHash);
        if (it != m_index.end()) {
            m_index.erase(it);
            m_indexDirty = true;
        }
    }

    // Save index periodically (every 100 operations)
    if (m_indexDirty && (m_index.size() % 100 == 0)) {
        return saveIndex();
    }

    return true;
}

bool MetadataStorage::loadIndex() {
    std::filesystem::path indexPath = m_basePath / "index" / "metadata.idx";

    if (!std::filesystem::exists(indexPath)) {
        getLogger()->info("Index file does not exist: {}", indexPath.string());
        return true;
    }

    try {
        std::ifstream file(indexPath);
        if (!file) {
            getLogger()->error("Failed to open index file for reading: {}", indexPath.string());
            return false;
        }

        m_index.clear();
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                m_index.push_back(line);
            }
        }

        file.close();
        m_indexDirty = false;

        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception while loading index: {}", e.what());
        return false;
    }
}

bool MetadataStorage::saveIndex() {
    std::filesystem::path indexPath = m_basePath / "index" / "metadata.idx";
    std::filesystem::path tempPath = indexPath.string() + ".tmp";

    try {
        // Write to temporary file first
        std::ofstream file(tempPath);
        if (!file) {
            getLogger()->error("Failed to open temporary index file for writing: {}", tempPath.string());
            return false;
        }

        for (const auto& hash : m_index) {
            file << hash << std::endl;
        }

        file.close();

        // Rename temporary file to actual file
        std::filesystem::rename(tempPath, indexPath);

        m_indexDirty = false;
        getLogger()->debug("Saved index with {} entries", m_index.size());

        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception while saving index: {}", e.what());
        return false;
    }
}

} // namespace dht_hunter::storage
