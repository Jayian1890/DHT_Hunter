#include "dht_hunter/metadata/metadata_storage.hpp"
#include "dht_hunter/metadata/torrent_file_builder.hpp"
#include "dht_hunter/util/hash.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace dht_hunter::metadata {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Metadata.Storage");
}

MetadataStorage::MetadataStorage(const MetadataStorageConfig& config)
    : m_config(config) {
}

MetadataStorage::~MetadataStorage() {
    shutdown();
}

bool MetadataStorage::initialize() {
    if (m_initialized) {
        logger->warning("Metadata storage already initialized");
        return true;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Create the storage directory if it doesn't exist
    if (m_config.persistMetadata) {
        try {
            std::filesystem::path storageDir(m_config.storageDirectory);
            if (!std::filesystem::exists(storageDir)) {
                if (!std::filesystem::create_directories(storageDir)) {
                    logger->error("Failed to create storage directory: {}", m_config.storageDirectory);
                    return false;
                }
            }
        } catch (const std::exception& e) {
            logger->error("Exception while creating storage directory: {}", e.what());
            return false;
        }
    }

    // Create the torrent files directory if it doesn't exist
    if (m_config.createTorrentFiles) {
        try {
            std::filesystem::path torrentDir(m_config.torrentFilesDirectory);
            if (!std::filesystem::exists(torrentDir)) {
                if (!std::filesystem::create_directories(torrentDir)) {
                    logger->error("Failed to create torrent files directory: {}", m_config.torrentFilesDirectory);
                    return false;
                }
            }
        } catch (const std::exception& e) {
            logger->error("Exception while creating torrent files directory: {}", e.what());
            return false;
        }
    }

    // Load all metadata from disk
    if (m_config.persistMetadata) {
        loadAllMetadata();
    }

    m_initialized = true;
    logger->info("Metadata storage initialized with {} items", m_metadata.size());
    return true;
}

void MetadataStorage::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Save all metadata to disk
    if (m_config.persistMetadata) {
        saveAllMetadata();
    }

    // Clear all metadata
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_metadata.clear();
    }

    m_initialized = false;
    logger->info("Metadata storage shut down");
}

bool MetadataStorage::isInitialized() const {
    return m_initialized;
}

bool MetadataStorage::addMetadata(
    const std::array<uint8_t, 20>& infoHash,
    const std::vector<uint8_t>& metadata,
    uint32_t size) {
    
    if (!m_initialized) {
        logger->error("Cannot add metadata: Metadata storage not initialized");
        return false;
    }

    if (metadata.empty() || size == 0) {
        logger->error("Cannot add metadata: Empty metadata");
        return false;
    }

    std::string infoHashStr = infoHashToString(infoHash);

    // Check if we already have this metadata
    if (m_config.deduplicateMetadata) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_metadata.find(infoHashStr) != m_metadata.end()) {
            logger->debug("Metadata already exists for info hash: {}", infoHashStr);
            return true;
        }
    }

    // Extract metadata information
    std::string name;
    uint64_t totalSize;
    bool isMultiFile;
    if (!extractMetadataInfo(metadata, size, name, totalSize, isMultiFile)) {
        logger->error("Failed to extract metadata information for info hash: {}", infoHashStr);
        return false;
    }

    // Create a new metadata item
    MetadataItem item;
    item.infoHash = infoHash;
    item.metadata = metadata;
    item.size = size;
    item.name = name;
    item.totalSize = totalSize;
    item.isMultiFile = isMultiFile;
    item.addedTime = std::chrono::system_clock::now();

    // Add to the storage
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Check if we need to remove some items to make room
        if (m_metadata.size() >= m_config.maxMetadataItems) {
            // Find the oldest item
            auto oldest = m_metadata.begin();
            for (auto it = m_metadata.begin(); it != m_metadata.end(); ++it) {
                if (it->second.addedTime < oldest->second.addedTime) {
                    oldest = it;
                }
            }

            // Remove the oldest item
            logger->debug("Removing oldest metadata item to make room: {}", oldest->first);
            m_metadata.erase(oldest);
        }

        // Add the new item
        m_metadata[infoHashStr] = item;
    }

    logger->debug("Added metadata for info hash: {}, name: {}, size: {}", infoHashStr, name, totalSize);

    // Save the metadata to disk
    if (m_config.persistMetadata) {
        saveMetadata(infoHash);
    }

    // Create a torrent file
    if (m_config.createTorrentFiles) {
        createTorrentFile(infoHash, metadata, size);
    }

    // Call the callback
    if (m_callback) {
        m_callback(infoHash, metadata, size);
    }

    return true;
}

std::pair<std::vector<uint8_t>, uint32_t> MetadataStorage::getMetadata(const std::array<uint8_t, 20>& infoHash) const {
    if (!m_initialized) {
        logger->error("Cannot get metadata: Metadata storage not initialized");
        return {};
    }

    std::string infoHashStr = infoHashToString(infoHash);

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_metadata.find(infoHashStr);
    if (it == m_metadata.end()) {
        return {};
    }

    return {it->second.metadata, it->second.size};
}

std::shared_ptr<TorrentFile> MetadataStorage::getTorrentFile(const std::array<uint8_t, 20>& infoHash) const {
    if (!m_initialized) {
        logger->error("Cannot get torrent file: Metadata storage not initialized");
        return nullptr;
    }

    // Check if the torrent file exists
    std::filesystem::path torrentFilePath = getTorrentFilePath(infoHash);
    if (!std::filesystem::exists(torrentFilePath)) {
        // Try to create the torrent file
        auto [metadata, size] = getMetadata(infoHash);
        if (metadata.empty() || size == 0) {
            return nullptr;
        }

        const_cast<MetadataStorage*>(this)->createTorrentFile(infoHash, metadata, size);
    }

    // Load the torrent file
    return loadTorrentFromFile(torrentFilePath);
}

bool MetadataStorage::hasMetadata(const std::array<uint8_t, 20>& infoHash) const {
    if (!m_initialized) {
        return false;
    }

    std::string infoHashStr = infoHashToString(infoHash);

    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metadata.find(infoHashStr) != m_metadata.end();
}

size_t MetadataStorage::getMetadataCount() const {
    if (!m_initialized) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metadata.size();
}

std::vector<std::array<uint8_t, 20>> MetadataStorage::getAllInfoHashes() const {
    if (!m_initialized) {
        return {};
    }

    std::vector<std::array<uint8_t, 20>> infoHashes;

    std::lock_guard<std::mutex> lock(m_mutex);
    infoHashes.reserve(m_metadata.size());

    for (const auto& [infoHashStr, item] : m_metadata) {
        infoHashes.push_back(item.infoHash);
    }

    return infoHashes;
}

std::vector<std::shared_ptr<TorrentFile>> MetadataStorage::getAllTorrentFiles() const {
    if (!m_initialized) {
        return {};
    }

    std::vector<std::shared_ptr<TorrentFile>> torrentFiles;

    std::lock_guard<std::mutex> lock(m_mutex);
    torrentFiles.reserve(m_metadata.size());

    for (const auto& [infoHashStr, item] : m_metadata) {
        auto torrentFile = getTorrentFile(item.infoHash);
        if (torrentFile) {
            torrentFiles.push_back(torrentFile);
        }
    }

    return torrentFiles;
}

bool MetadataStorage::removeMetadata(const std::array<uint8_t, 20>& infoHash) {
    if (!m_initialized) {
        logger->error("Cannot remove metadata: Metadata storage not initialized");
        return false;
    }

    std::string infoHashStr = infoHashToString(infoHash);

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_metadata.find(infoHashStr);
    if (it == m_metadata.end()) {
        return false;
    }

    // Remove the metadata
    m_metadata.erase(it);

    // Remove the metadata file
    if (m_config.persistMetadata) {
        try {
            std::filesystem::path metadataFilePath = getMetadataFilePath(infoHash);
            if (std::filesystem::exists(metadataFilePath)) {
                std::filesystem::remove(metadataFilePath);
            }
        } catch (const std::exception& e) {
            logger->error("Exception while removing metadata file: {}", e.what());
        }
    }

    // Remove the torrent file
    if (m_config.createTorrentFiles) {
        try {
            std::filesystem::path torrentFilePath = getTorrentFilePath(infoHash);
            if (std::filesystem::exists(torrentFilePath)) {
                std::filesystem::remove(torrentFilePath);
            }
        } catch (const std::exception& e) {
            logger->error("Exception while removing torrent file: {}", e.what());
        }
    }

    logger->debug("Removed metadata for info hash: {}", infoHashStr);
    return true;
}

void MetadataStorage::clearMetadata() {
    if (!m_initialized) {
        logger->error("Cannot clear metadata: Metadata storage not initialized");
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Clear the metadata
    m_metadata.clear();

    // Remove all metadata files
    if (m_config.persistMetadata) {
        try {
            std::filesystem::path storageDir(m_config.storageDirectory);
            if (std::filesystem::exists(storageDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(storageDir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".metadata") {
                        std::filesystem::remove(entry.path());
                    }
                }
            }
        } catch (const std::exception& e) {
            logger->error("Exception while removing metadata files: {}", e.what());
        }
    }

    // Remove all torrent files
    if (m_config.createTorrentFiles) {
        try {
            std::filesystem::path torrentDir(m_config.torrentFilesDirectory);
            if (std::filesystem::exists(torrentDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(torrentDir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".torrent") {
                        std::filesystem::remove(entry.path());
                    }
                }
            }
        } catch (const std::exception& e) {
            logger->error("Exception while removing torrent files: {}", e.what());
        }
    }

    logger->info("Cleared all metadata");
}

void MetadataStorage::setCallback(MetadataStorageCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = callback;
}

MetadataStorageConfig MetadataStorage::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void MetadataStorage::setConfig(const MetadataStorageConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

bool MetadataStorage::saveAllMetadata() {
    if (!m_initialized) {
        logger->error("Cannot save metadata: Metadata storage not initialized");
        return false;
    }

    if (!m_config.persistMetadata) {
        logger->warning("Persistence is disabled");
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    bool success = true;
    for (const auto& [infoHashStr, item] : m_metadata) {
        if (!saveMetadata(item.infoHash)) {
            success = false;
        }
    }

    logger->info("Saved {} metadata items", m_metadata.size());
    return success;
}

bool MetadataStorage::loadAllMetadata() {
    if (m_initialized) {
        logger->warning("Metadata storage already initialized");
    }

    if (!m_config.persistMetadata) {
        logger->warning("Persistence is disabled");
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        std::filesystem::path storageDir(m_config.storageDirectory);
        if (!std::filesystem::exists(storageDir)) {
            return true;
        }

        for (const auto& entry : std::filesystem::directory_iterator(storageDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".metadata") {
                // Extract the info hash from the filename
                std::string filename = entry.path().stem().string();
                if (filename.length() != 40) {
                    logger->warning("Invalid metadata filename: {}", entry.path().string());
                    continue;
                }

                // Convert the info hash string to bytes
                std::array<uint8_t, 20> infoHash;
                try {
                    auto bytes = util::hexToBytes(filename);
                    if (bytes.size() != 20) {
                        logger->warning("Invalid info hash in filename: {}", filename);
                        continue;
                    }
                    std::copy(bytes.begin(), bytes.end(), infoHash.begin());
                } catch (const std::exception& e) {
                    logger->warning("Failed to parse info hash from filename: {}", filename);
                    continue;
                }

                // Load the metadata
                if (!loadMetadata(infoHash)) {
                    logger->warning("Failed to load metadata from file: {}", entry.path().string());
                }
            }
        }

        logger->info("Loaded {} metadata items", m_metadata.size());
        return true;
    } catch (const std::exception& e) {
        logger->error("Exception while loading metadata: {}", e.what());
        return false;
    }
}

std::filesystem::path MetadataStorage::getMetadataFilePath(const std::array<uint8_t, 20>& infoHash) const {
    std::string infoHashStr = infoHashToString(infoHash);
    return std::filesystem::path(m_config.storageDirectory) / (infoHashStr + ".metadata");
}

std::filesystem::path MetadataStorage::getTorrentFilePath(const std::array<uint8_t, 20>& infoHash) const {
    std::string infoHashStr = infoHashToString(infoHash);
    return std::filesystem::path(m_config.torrentFilesDirectory) / (infoHashStr + ".torrent");
}

bool MetadataStorage::saveMetadata(const std::array<uint8_t, 20>& infoHash) {
    if (!m_config.persistMetadata) {
        return false;
    }

    std::string infoHashStr = infoHashToString(infoHash);

    // Find the metadata
    auto it = m_metadata.find(infoHashStr);
    if (it == m_metadata.end()) {
        logger->error("Cannot save metadata: Metadata not found for info hash: {}", infoHashStr);
        return false;
    }

    const auto& item = it->second;

    try {
        // Create the metadata file path
        std::filesystem::path metadataFilePath = getMetadataFilePath(infoHash);

        // Create the directory if it doesn't exist
        std::filesystem::path dir = metadataFilePath.parent_path();
        if (!std::filesystem::exists(dir)) {
            if (!std::filesystem::create_directories(dir)) {
                logger->error("Failed to create directory: {}", dir.string());
                return false;
            }
        }

        // Open the file
        std::ofstream file(metadataFilePath, std::ios::binary);
        if (!file) {
            logger->error("Failed to open file for writing: {}", metadataFilePath.string());
            return false;
        }

        // Write the metadata size
        file.write(reinterpret_cast<const char*>(&item.size), sizeof(item.size));

        // Write the metadata
        file.write(reinterpret_cast<const char*>(item.metadata.data()), static_cast<std::streamsize>(item.metadata.size()));

        logger->debug("Saved metadata to file: {}", metadataFilePath.string());
        return true;
    } catch (const std::exception& e) {
        logger->error("Exception while saving metadata: {}", e.what());
        return false;
    }
}

bool MetadataStorage::loadMetadata(const std::array<uint8_t, 20>& infoHash) {
    if (!m_config.persistMetadata) {
        return false;
    }

    std::string infoHashStr = infoHashToString(infoHash);

    try {
        // Create the metadata file path
        std::filesystem::path metadataFilePath = getMetadataFilePath(infoHash);

        // Check if the file exists
        if (!std::filesystem::exists(metadataFilePath)) {
            logger->debug("Metadata file does not exist: {}", metadataFilePath.string());
            return false;
        }

        // Open the file
        std::ifstream file(metadataFilePath, std::ios::binary);
        if (!file) {
            logger->error("Failed to open file for reading: {}", metadataFilePath.string());
            return false;
        }

        // Read the metadata size
        uint32_t size;
        file.read(reinterpret_cast<char*>(&size), sizeof(size));

        // Read the metadata
        std::vector<uint8_t> metadata(size);
        file.read(reinterpret_cast<char*>(metadata.data()), static_cast<std::streamsize>(size));

        // Check if we read the correct amount of data
        if (file.gcount() != static_cast<std::streamsize>(size)) {
            logger->error("Failed to read metadata from file: {}", metadataFilePath.string());
            return false;
        }

        // Extract metadata information
        std::string name;
        uint64_t totalSize;
        bool isMultiFile;
        if (!extractMetadataInfo(metadata, size, name, totalSize, isMultiFile)) {
            logger->error("Failed to extract metadata information from file: {}", metadataFilePath.string());
            return false;
        }

        // Create a new metadata item
        MetadataItem item;
        item.infoHash = infoHash;
        item.metadata = metadata;
        item.size = size;
        item.name = name;
        item.totalSize = totalSize;
        item.isMultiFile = isMultiFile;
        item.addedTime = std::chrono::system_clock::now();

        // Add to the storage
        m_metadata[infoHashStr] = item;

        logger->debug("Loaded metadata from file: {}", metadataFilePath.string());
        return true;
    } catch (const std::exception& e) {
        logger->error("Exception while loading metadata: {}", e.what());
        return false;
    }
}

bool MetadataStorage::createTorrentFile(
    const std::array<uint8_t, 20>& infoHash,
    const std::vector<uint8_t>& metadata,
    uint32_t size) {
    
    if (!m_config.createTorrentFiles) {
        return false;
    }

    std::string infoHashStr = infoHashToString(infoHash);

    try {
        // Create a torrent file builder
        TorrentFileBuilder builder(metadata, size);
        
        // Check if the builder is valid
        if (!builder.isValid()) {
            logger->error("Failed to create torrent file: Invalid metadata for info hash: {}", infoHashStr);
            return false;
        }
        
        // Create the torrent file path
        std::filesystem::path torrentFilePath = getTorrentFilePath(infoHash);
        
        // Create the directory if it doesn't exist
        std::filesystem::path dir = torrentFilePath.parent_path();
        if (!std::filesystem::exists(dir)) {
            if (!std::filesystem::create_directories(dir)) {
                logger->error("Failed to create directory: {}", dir.string());
                return false;
            }
        }
        
        // Save the torrent file
        if (!builder.saveTorrentFile(torrentFilePath.string())) {
            logger->error("Failed to save torrent file: {}", torrentFilePath.string());
            return false;
        }
        
        logger->debug("Created torrent file: {}", torrentFilePath.string());
        return true;
    } catch (const std::exception& e) {
        logger->error("Exception while creating torrent file: {}", e.what());
        return false;
    }
}

bool MetadataStorage::extractMetadataInfo(
    const std::vector<uint8_t>& metadata,
    uint32_t size,
    std::string& name,
    uint64_t& totalSize,
    bool& isMultiFile) {
    
    try {
        // Create a torrent file builder
        TorrentFileBuilder builder(metadata, size);
        
        // Check if the builder is valid
        if (!builder.isValid()) {
            logger->error("Failed to extract metadata information: Invalid metadata");
            return false;
        }
        
        // Get the metadata information
        name = builder.getName();
        totalSize = builder.getTotalSize();
        isMultiFile = builder.isMultiFile();
        
        return true;
    } catch (const std::exception& e) {
        logger->error("Exception while extracting metadata information: {}", e.what());
        return false;
    }
}

std::string MetadataStorage::infoHashToString(const std::array<uint8_t, 20>& infoHash) {
    return util::bytesToHex(infoHash.data(), infoHash.size());
}

} // namespace dht_hunter::metadata
