#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <filesystem>

namespace dht_hunter::storage {

/**
 * @brief Class for storing and retrieving torrent metadata
 * 
 * This class provides a file-based storage system for torrent metadata.
 * It uses a sharded directory structure to efficiently store and retrieve
 * metadata by info hash.
 */
class MetadataStorage {
public:
    /**
     * @brief Constructor
     * @param basePath Base directory for storing metadata
     * @param shardingLevel Number of directory levels for sharding (default: 2)
     */
    explicit MetadataStorage(const std::filesystem::path& basePath, int shardingLevel = 2);

    /**
     * @brief Destructor
     */
    ~MetadataStorage();

    /**
     * @brief Add metadata to storage
     * @param infoHash Info hash of the torrent
     * @param data Raw metadata bytes
     * @param size Size of the metadata
     * @return True if successful, false otherwise
     */
    bool addMetadata(const std::array<uint8_t, 20>& infoHash, const uint8_t* data, uint32_t size);

    /**
     * @brief Get metadata from storage
     * @param infoHash Info hash of the torrent
     * @return Pair of data and size if found, empty optional otherwise
     */
    std::optional<std::pair<std::vector<uint8_t>, uint32_t>> getMetadata(const std::array<uint8_t, 20>& infoHash);

    /**
     * @brief Check if metadata exists
     * @param infoHash Info hash of the torrent
     * @return True if metadata exists, false otherwise
     */
    bool exists(const std::array<uint8_t, 20>& infoHash);

    /**
     * @brief Remove metadata from storage
     * @param infoHash Info hash of the torrent
     * @return True if successful, false otherwise
     */
    bool removeMetadata(const std::array<uint8_t, 20>& infoHash);

    /**
     * @brief Get the number of metadata entries
     * @return Number of metadata entries
     */
    size_t count() const;

    /**
     * @brief Get the base path
     * @return Base path
     */
    std::filesystem::path getBasePath() const;

private:
    /**
     * @brief Convert info hash to hex string
     * @param infoHash Info hash
     * @return Hex string
     */
    std::string infoHashToHex(const std::array<uint8_t, 20>& infoHash) const;

    /**
     * @brief Get the path for a metadata file
     * @param infoHash Info hash
     * @return Path to the metadata file
     */
    std::filesystem::path getMetadataPath(const std::array<uint8_t, 20>& infoHash) const;

    /**
     * @brief Create directory structure for an info hash
     * @param infoHash Info hash
     * @return True if successful, false otherwise
     */
    bool createDirectoryStructure(const std::array<uint8_t, 20>& infoHash);

    /**
     * @brief Update the index file
     * @param infoHash Info hash
     * @param operation "add" or "remove"
     * @return True if successful, false otherwise
     */
    bool updateIndex(const std::array<uint8_t, 20>& infoHash, const std::string& operation);

    /**
     * @brief Load the index file
     * @return True if successful, false otherwise
     */
    bool loadIndex();

    /**
     * @brief Save the index file
     * @return True if successful, false otherwise
     */
    bool saveIndex();

    std::filesystem::path m_basePath;
    int m_shardingLevel;
    std::vector<std::string> m_index;
    bool m_indexDirty;
};

} // namespace dht_hunter::storage
