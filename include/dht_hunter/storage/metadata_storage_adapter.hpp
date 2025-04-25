#pragma once

#include "dht_hunter/metadata/metadata_storage.hpp"
#include "dht_hunter/storage/metadata_storage.hpp"
#include "dht_hunter/metadata/torrent_file.hpp"
#include "dht_hunter/metadata/torrent_file_builder.hpp"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <filesystem>

namespace dht_hunter::storage {

/**
 * @brief Adapter class that implements the metadata::MetadataStorage interface
 * using the new storage::MetadataStorage implementation
 */
class MetadataStorageAdapter : public metadata::MetadataStorage {
public:
    /**
     * @brief Constructor
     * @param config The configuration for the metadata storage
     */
    explicit MetadataStorageAdapter(const metadata::MetadataStorageConfig& config);

    /**
     * @brief Destructor
     */
    ~MetadataStorageAdapter();

    /**
     * @brief Initializes the storage
     * @return True if initialized successfully, false otherwise
     */
    bool initialize();

    /**
     * @brief Adds metadata to the storage
     * @param infoHash The info hash of the torrent
     * @param metadata The metadata
     * @param size The size of the metadata
     * @return True if the metadata was added successfully, false otherwise
     */
    bool addMetadata(
        const std::array<uint8_t, 20>& infoHash,
        const std::vector<uint8_t>& metadata,
        uint32_t size);

    /**
     * @brief Gets metadata from the storage
     * @param infoHash The info hash of the torrent
     * @return A pair containing the metadata and its size, or empty if not found
     */
    std::pair<std::vector<uint8_t>, uint32_t> getMetadata(const std::array<uint8_t, 20>& infoHash) const;

    /**
     * @brief Gets a torrent file from the storage
     * @param infoHash The info hash of the torrent
     * @return A shared pointer to the torrent file, or nullptr if not found
     */
    std::shared_ptr<metadata::TorrentFile> getTorrentFile(const std::array<uint8_t, 20>& infoHash);

    /**
     * @brief Checks if the storage contains metadata for a torrent
     * @param infoHash The info hash of the torrent
     * @return True if the storage contains metadata for the torrent, false otherwise
     */
    bool hasMetadata(const std::array<uint8_t, 20>& infoHash) const;

    /**
     * @brief Gets the number of metadata items in the storage
     * @return The number of metadata items in the storage
     */
    size_t getMetadataCount() const;

    /**
     * @brief Sets a callback to be called when metadata is added
     * @param callback The callback
     */
    void setMetadataAddedCallback(metadata::MetadataStorageCallback callback);

    /**
     * @brief Sets a callback to be called when metadata is removed
     * @param callback The callback
     */
    void setMetadataRemovedCallback(metadata::MetadataStorageCallback callback);

private:
    /**
     * @brief Gets the path to the torrent file for a torrent
     * @param infoHash The info hash of the torrent
     * @return The path to the torrent file
     */
    std::filesystem::path getTorrentFilePath(const std::array<uint8_t, 20>& infoHash) const;

    /**
     * @brief Creates a torrent file from metadata
     * @param infoHash The info hash of the torrent
     * @param metadata The metadata
     * @param size The size of the metadata
     * @return True if created successfully, false otherwise
     */
    bool createTorrentFile(
        const std::array<uint8_t, 20>& infoHash,
        const std::vector<uint8_t>& metadata,
        uint32_t size);

    /**
     * @brief Loads a torrent file from disk
     * @param path The path to the torrent file
     * @return A shared pointer to the torrent file, or nullptr if not found
     */
    std::shared_ptr<metadata::TorrentFile> loadTorrentFromFile(const std::filesystem::path& path) const;

    /**
     * @brief Converts an info hash to a hex string
     * @param infoHash The info hash
     * @return The hex string
     */
    std::string infoHashToString(const std::array<uint8_t, 20>& infoHash) const;

    metadata::MetadataStorageConfig m_config;                       ///< Configuration
    std::unique_ptr<storage::MetadataStorage> m_storage;            ///< The underlying storage
    mutable std::mutex m_mutex;                                     ///< Mutex for thread safety
    std::unordered_map<std::string, std::shared_ptr<metadata::TorrentFile>> m_torrentFiles; ///< Cache of torrent files
    metadata::MetadataStorageCallback m_metadataAddedCallback;      ///< Callback for when metadata is added
    metadata::MetadataStorageCallback m_metadataRemovedCallback;    ///< Callback for when metadata is removed
};

} // namespace dht_hunter::storage
