#pragma once

#include "dht_hunter/metadata/torrent_file.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <filesystem>
#include <functional>

namespace dht_hunter::metadata {

/**
 * @brief Configuration for the metadata storage
 */
struct MetadataStorageConfig {
    std::string storageDirectory = "metadata";      ///< Directory to store metadata
    bool persistMetadata = true;                    ///< Whether to persist metadata to disk
    bool createTorrentFiles = true;                 ///< Whether to create torrent files
    std::string torrentFilesDirectory = "torrents"; ///< Directory to save torrent files
    uint32_t maxMetadataItems = 100000;             ///< Maximum number of metadata items to store
    bool deduplicateMetadata = true;                ///< Whether to deduplicate metadata
};

/**
 * @brief Callback for metadata storage events
 * @param infoHash The info hash of the torrent
 * @param metadata The metadata
 * @param size The size of the metadata
 */
using MetadataStorageCallback = std::function<void(
    const std::array<uint8_t, 20>& infoHash,
    const std::vector<uint8_t>& metadata,
    uint32_t size)>;

/**
 * @brief Class for storing metadata for torrents
 */
class MetadataStorage {
public:
    /**
     * @brief Constructs a metadata storage
     * @param config The configuration
     */
    explicit MetadataStorage(const MetadataStorageConfig& config = MetadataStorageConfig());
    
    /**
     * @brief Destructor
     */
    ~MetadataStorage();
    
    /**
     * @brief Initializes the metadata storage
     * @return True if initialized successfully, false otherwise
     */
    bool initialize();
    
    /**
     * @brief Shuts down the metadata storage
     */
    void shutdown();
    
    /**
     * @brief Checks if the metadata storage is initialized
     * @return True if initialized, false otherwise
     */
    bool isInitialized() const;
    
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
    std::shared_ptr<TorrentFile> getTorrentFile(const std::array<uint8_t, 20>& infoHash) const;
    
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
     * @brief Gets all info hashes in the storage
     * @return A vector of all info hashes in the storage
     */
    std::vector<std::array<uint8_t, 20>> getAllInfoHashes() const;
    
    /**
     * @brief Gets all torrent files in the storage
     * @return A vector of all torrent files in the storage
     */
    std::vector<std::shared_ptr<TorrentFile>> getAllTorrentFiles() const;
    
    /**
     * @brief Removes metadata from the storage
     * @param infoHash The info hash of the torrent
     * @return True if the metadata was removed, false if it wasn't found
     */
    bool removeMetadata(const std::array<uint8_t, 20>& infoHash);
    
    /**
     * @brief Clears all metadata from the storage
     */
    void clearMetadata();
    
    /**
     * @brief Sets the callback for metadata storage events
     * @param callback The callback function
     */
    void setCallback(MetadataStorageCallback callback);
    
    /**
     * @brief Gets the configuration
     * @return The configuration
     */
    MetadataStorageConfig getConfig() const;
    
    /**
     * @brief Sets the configuration
     * @param config The configuration
     */
    void setConfig(const MetadataStorageConfig& config);
    
    /**
     * @brief Saves all metadata to disk
     * @return True if saved successfully, false otherwise
     */
    bool saveAllMetadata();
    
    /**
     * @brief Loads all metadata from disk
     * @return True if loaded successfully, false otherwise
     */
    bool loadAllMetadata();
    
private:
    /**
     * @brief Structure for a metadata item
     */
    struct MetadataItem {
        std::array<uint8_t, 20> infoHash;  ///< Info hash of the torrent
        std::vector<uint8_t> metadata;     ///< Raw metadata
        uint32_t size;                     ///< Size of the metadata
        std::string name;                  ///< Name of the torrent
        uint64_t totalSize;                ///< Total size of the torrent
        bool isMultiFile;                  ///< Whether the torrent is a multi-file torrent
        std::chrono::system_clock::time_point addedTime; ///< Time when the metadata was added
    };
    
    /**
     * @brief Gets the path to the metadata file for a torrent
     * @param infoHash The info hash of the torrent
     * @return The path to the metadata file
     */
    std::filesystem::path getMetadataFilePath(const std::array<uint8_t, 20>& infoHash) const;
    
    /**
     * @brief Gets the path to the torrent file for a torrent
     * @param infoHash The info hash of the torrent
     * @return The path to the torrent file
     */
    std::filesystem::path getTorrentFilePath(const std::array<uint8_t, 20>& infoHash) const;
    
    /**
     * @brief Saves metadata to disk
     * @param infoHash The info hash of the torrent
     * @return True if saved successfully, false otherwise
     */
    bool saveMetadata(const std::array<uint8_t, 20>& infoHash);
    
    /**
     * @brief Loads metadata from disk
     * @param infoHash The info hash of the torrent
     * @return True if loaded successfully, false otherwise
     */
    bool loadMetadata(const std::array<uint8_t, 20>& infoHash);
    
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
     * @brief Extracts metadata information
     * @param metadata The metadata
     * @param size The size of the metadata
     * @param name Output parameter for the name
     * @param totalSize Output parameter for the total size
     * @param isMultiFile Output parameter for whether the torrent is a multi-file torrent
     * @return True if extracted successfully, false otherwise
     */
    bool extractMetadataInfo(
        const std::vector<uint8_t>& metadata,
        uint32_t size,
        std::string& name,
        uint64_t& totalSize,
        bool& isMultiFile);
    
    MetadataStorageConfig m_config;                                 ///< Configuration
    std::atomic<bool> m_initialized{false};                         ///< Whether the storage is initialized
    mutable std::mutex m_mutex;                                     ///< Mutex for thread safety
    std::unordered_map<std::string, MetadataItem> m_metadata;       ///< Metadata by info hash (hex string)
    MetadataStorageCallback m_callback;                             ///< Callback for metadata storage events
};

} // namespace dht_hunter::metadata
