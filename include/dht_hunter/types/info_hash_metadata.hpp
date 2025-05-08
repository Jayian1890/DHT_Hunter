#pragma once

#include "dht_hunter/types/info_hash.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace dht_hunter::types {

/**
 * @brief Represents a file in a torrent
 */
struct TorrentFile {
    std::string path;       ///< Path of the file
    uint64_t size;          ///< Size of the file in bytes

    TorrentFile() = default;
    TorrentFile(const std::string& path, uint64_t size) : path(path), size(size) {}
};

/**
 * @brief Metadata for an InfoHash
 */
class InfoHashMetadata {
public:
    /**
     * @brief Default constructor
     */
    InfoHashMetadata() = default;

    /**
     * @brief Constructor with info hash
     * @param infoHash The info hash
     */
    explicit InfoHashMetadata(const InfoHash& infoHash);

    /**
     * @brief Constructor with info hash and name
     * @param infoHash The info hash
     * @param name The name of the torrent
     */
    InfoHashMetadata(const InfoHash& infoHash, const std::string& name);

    /**
     * @brief Gets the info hash
     * @return The info hash
     */
    const InfoHash& getInfoHash() const;

    /**
     * @brief Sets the info hash
     * @param infoHash The info hash
     */
    void setInfoHash(const InfoHash& infoHash);

    /**
     * @brief Gets the name
     * @return The name
     */
    const std::string& getName() const;

    /**
     * @brief Sets the name
     * @param name The name
     */
    void setName(const std::string& name);

    /**
     * @brief Gets the files
     * @return The files
     */
    const std::vector<TorrentFile>& getFiles() const;

    /**
     * @brief Adds a file
     * @param file The file to add
     */
    void addFile(const TorrentFile& file);

    /**
     * @brief Sets the files
     * @param files The files
     */
    void setFiles(const std::vector<TorrentFile>& files);

    /**
     * @brief Gets the total size of all files
     * @return The total size in bytes
     */
    uint64_t getTotalSize() const;

    /**
     * @brief Serializes the metadata to a binary format
     * @return The serialized data
     */
    std::vector<uint8_t> serialize() const;

    /**
     * @brief Deserializes the metadata from a binary format
     * @param data The serialized data
     * @param offset The offset to start reading from
     * @return The number of bytes read, or 0 if deserialization failed
     */
    size_t deserialize(const std::vector<uint8_t>& data, size_t offset = 0);

private:
    InfoHash m_infoHash{};
    std::string m_name;
    std::vector<TorrentFile> m_files;
};

/**
 * @brief A registry for InfoHash metadata
 */
class InfoHashMetadataRegistry {
public:
    /**
     * @brief Gets the singleton instance
     * @return The singleton instance
     */
    static std::shared_ptr<InfoHashMetadataRegistry> getInstance();

    /**
     * @brief Destructor
     */
    ~InfoHashMetadataRegistry();

    /**
     * @brief Registers metadata for an info hash
     * @param metadata The metadata
     */
    void registerMetadata(const InfoHashMetadata& metadata);

    /**
     * @brief Gets metadata for an info hash
     * @param infoHash The info hash
     * @return The metadata, or nullptr if not found
     */
    std::shared_ptr<InfoHashMetadata> getMetadata(const InfoHash& infoHash) const;

    /**
     * @brief Gets all registered metadata
     * @return All registered metadata
     */
    std::vector<std::shared_ptr<InfoHashMetadata>> getAllMetadata() const;

    /**
     * @brief Serializes all metadata to a binary format
     * @return The serialized data
     */
    std::vector<uint8_t> serializeAll() const;

    /**
     * @brief Deserializes all metadata from a binary format
     * @param data The serialized data
     * @return True if deserialization was successful, false otherwise
     */
    bool deserializeAll(const std::vector<uint8_t>& data);

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    InfoHashMetadataRegistry();

    // Static instance for singleton pattern
    static std::shared_ptr<InfoHashMetadataRegistry> s_instance;
    static std::mutex s_instanceMutex;

    std::unordered_map<InfoHash, std::shared_ptr<InfoHashMetadata>> m_metadata;
    mutable std::mutex m_mutex;
};

} // namespace dht_hunter::types
