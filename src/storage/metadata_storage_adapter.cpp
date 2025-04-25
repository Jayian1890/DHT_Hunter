#include "dht_hunter/storage/metadata_storage_adapter.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace dht_hunter::storage {

DEFINE_COMPONENT_LOGGER("Storage.MetadataStorageAdapter", "Storage")

MetadataStorageAdapter::MetadataStorageAdapter(const metadata::MetadataStorageConfig& config)
    : m_config(config) {

    // Create the storage directory if it doesn't exist
    if (!std::filesystem::exists(m_config.storageDirectory)) {
        std::filesystem::create_directories(m_config.storageDirectory);
    }

    // Create the torrent files directory if it doesn't exist
    if (m_config.createTorrentFiles && !std::filesystem::exists(m_config.torrentFilesDirectory)) {
        std::filesystem::create_directories(m_config.torrentFilesDirectory);
    }

    // Create the storage with the configured directory
    m_storage = std::make_unique<storage::MetadataStorage>(m_config.storageDirectory);

    getLogger()->info("Metadata storage adapter initialized");
    getLogger()->info("  Storage directory: {}", m_config.storageDirectory);
    getLogger()->info("  Persist metadata: {}", m_config.persistMetadata ? "yes" : "no");
    getLogger()->info("  Create torrent files: {}", m_config.createTorrentFiles ? "yes" : "no");
    getLogger()->info("  Torrent files directory: {}", m_config.torrentFilesDirectory);
    getLogger()->info("  Max metadata items: {}", m_config.maxMetadataItems);
    getLogger()->info("  Deduplicate metadata: {}", m_config.deduplicateMetadata ? "yes" : "no");
}

MetadataStorageAdapter::~MetadataStorageAdapter() {
    getLogger()->info("Metadata storage adapter shutting down");
}

bool MetadataStorageAdapter::initialize() {
    getLogger()->info("Initializing metadata storage adapter");

    // The storage is already initialized in the constructor
    return true;
}

bool MetadataStorageAdapter::addMetadata(
    const std::array<uint8_t, 20>& infoHash,
    const std::vector<uint8_t>& metadata,
    uint32_t size) {

    // Check if we already have this metadata
    if (hasMetadata(infoHash)) {
        getLogger()->debug("Metadata already exists for info hash: {}", infoHashToString(infoHash));
        return true;
    }

    // Check if we've reached the maximum number of metadata items
    if (m_config.maxMetadataItems > 0 && getMetadataCount() >= m_config.maxMetadataItems) {
        getLogger()->warning("Maximum number of metadata items reached: {}", m_config.maxMetadataItems);
        return false;
    }

    // Add the metadata to the storage
    bool success = false;
    if (m_config.persistMetadata) {
        success = m_storage->addMetadata(infoHash, metadata.data(), size);
    } else {
        // If we're not persisting metadata, just pretend we added it
        success = true;
    }

    if (!success) {
        getLogger()->error("Failed to add metadata for info hash: {}", infoHashToString(infoHash));
        return false;
    }

    // Create a torrent file if configured to do so
    if (m_config.createTorrentFiles) {
        createTorrentFile(infoHash, metadata, size);
    }

    // Call the callback if set
    if (m_metadataAddedCallback) {
        try {
            m_metadataAddedCallback(infoHash, metadata, size);
        } catch (const std::exception& e) {
            getLogger()->error("Exception in metadata added callback: {}", e.what());
        }
    }

    getLogger()->info("Added metadata for info hash: {}", infoHashToString(infoHash));
    return true;
}

std::pair<std::vector<uint8_t>, uint32_t> MetadataStorageAdapter::getMetadata(const std::array<uint8_t, 20>& infoHash) const {
    // Check if we have this metadata
    if (!hasMetadata(infoHash)) {
        getLogger()->debug("Metadata does not exist for info hash: {}", infoHashToString(infoHash));
        return {std::vector<uint8_t>(), 0};
    }

    // Get the metadata from the storage
    auto result = m_storage->getMetadata(infoHash);
    if (!result) {
        getLogger()->error("Failed to get metadata for info hash: {}", infoHashToString(infoHash));
        return {std::vector<uint8_t>(), 0};
    }

    return *result;
}

std::shared_ptr<metadata::TorrentFile> MetadataStorageAdapter::getTorrentFile(const std::array<uint8_t, 20>& infoHash) {
    std::string infoHashStr = infoHashToString(infoHash);

    // Check if we have this torrent file in the cache
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_torrentFiles.find(infoHashStr);
        if (it != m_torrentFiles.end()) {
            return it->second;
        }
    }

    // Check if the torrent file exists on disk
    std::filesystem::path torrentFilePath = getTorrentFilePath(infoHash);
    if (std::filesystem::exists(torrentFilePath)) {
        auto torrentFile = loadTorrentFromFile(torrentFilePath);
        if (torrentFile) {
            // Add to cache
            std::lock_guard<std::mutex> lock(m_mutex);
            m_torrentFiles[infoHashStr] = torrentFile;
            return torrentFile;
        }
    }

    // If we don't have a torrent file but we have the metadata, create one
    if (hasMetadata(infoHash)) {
        auto [metadata, size] = getMetadata(infoHash);
        if (!metadata.empty() && size > 0) {
            createTorrentFile(infoHash, metadata, size);

            // Try loading again
            if (std::filesystem::exists(torrentFilePath)) {
                auto torrentFile = loadTorrentFromFile(torrentFilePath);
                if (torrentFile) {
                    // Add to cache
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_torrentFiles[infoHashStr] = torrentFile;
                    return torrentFile;
                }
            }
        }
    }

    getLogger()->debug("Torrent file not found for info hash: {}", infoHashStr);
    return nullptr;
}

bool MetadataStorageAdapter::hasMetadata(const std::array<uint8_t, 20>& infoHash) const {
    return m_storage->exists(infoHash);
}

size_t MetadataStorageAdapter::getMetadataCount() const {
    return m_storage->count();
}

void MetadataStorageAdapter::setMetadataAddedCallback(metadata::MetadataStorageCallback callback) {
    m_metadataAddedCallback = std::move(callback);
}

void MetadataStorageAdapter::setMetadataRemovedCallback(metadata::MetadataStorageCallback callback) {
    m_metadataRemovedCallback = std::move(callback);
}

std::filesystem::path MetadataStorageAdapter::getTorrentFilePath(const std::array<uint8_t, 20>& infoHash) const {
    std::string infoHashStr = infoHashToString(infoHash);
    return std::filesystem::path(m_config.torrentFilesDirectory) / (infoHashStr + ".torrent");
}

bool MetadataStorageAdapter::createTorrentFile(
    const std::array<uint8_t, 20>& infoHash,
    const std::vector<uint8_t>& metadata,
    uint32_t size) {

    // Create a torrent file builder
    metadata::TorrentFileBuilder builder(metadata, size);

    // Save the torrent file
    std::filesystem::path torrentFilePath = getTorrentFilePath(infoHash);
    bool success = builder.saveTorrentFile(torrentFilePath.string());

    if (!success) {
        getLogger()->error("Failed to create torrent file for info hash: {}", infoHashToString(infoHash));
        return false;
    }

    getLogger()->debug("Created torrent file for info hash: {}", infoHashToString(infoHash));
    return true;
}

std::shared_ptr<metadata::TorrentFile> MetadataStorageAdapter::loadTorrentFromFile(const std::filesystem::path& path) const {
    auto torrentFile = std::make_shared<metadata::TorrentFile>();
    if (!torrentFile->loadFromFile(path)) {
        getLogger()->error("Failed to load torrent file: {}", path.string());
        return nullptr;
    }

    return torrentFile;
}

std::string MetadataStorageAdapter::infoHashToString(const std::array<uint8_t, 20>& infoHash) const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (auto byte : infoHash) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

} // namespace dht_hunter::storage
