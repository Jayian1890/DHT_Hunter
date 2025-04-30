#pragma once

#include "dht_hunter/types/info_hash_metadata.hpp"
#include <string>
#include <vector>
#include <memory>

namespace dht_hunter::utility::metadata {

/**
 * @brief Utility functions for working with InfoHash metadata
 */
class MetadataUtils {
public:
    /**
     * @brief Sets the name for an info hash
     * @param infoHash The info hash
     * @param name The name
     * @return True if successful, false otherwise
     */
    static bool setInfoHashName(const types::InfoHash& infoHash, const std::string& name);

    /**
     * @brief Gets the name for an info hash
     * @param infoHash The info hash
     * @return The name, or empty string if not found
     */
    static std::string getInfoHashName(const types::InfoHash& infoHash);

    /**
     * @brief Adds a file to an info hash
     * @param infoHash The info hash
     * @param path The file path
     * @param size The file size
     * @return True if successful, false otherwise
     */
    static bool addFileToInfoHash(const types::InfoHash& infoHash, const std::string& path, uint64_t size);

    /**
     * @brief Gets the files for an info hash
     * @param infoHash The info hash
     * @return The files, or empty vector if not found
     */
    static std::vector<types::TorrentFile> getInfoHashFiles(const types::InfoHash& infoHash);

    /**
     * @brief Gets the total size of all files for an info hash
     * @param infoHash The info hash
     * @return The total size in bytes, or 0 if not found
     */
    static uint64_t getInfoHashTotalSize(const types::InfoHash& infoHash);

    /**
     * @brief Gets the metadata for an info hash
     * @param infoHash The info hash
     * @return The metadata, or nullptr if not found
     */
    static std::shared_ptr<types::InfoHashMetadata> getInfoHashMetadata(const types::InfoHash& infoHash);

    /**
     * @brief Gets all metadata
     * @return All metadata
     */
    static std::vector<std::shared_ptr<types::InfoHashMetadata>> getAllMetadata();
};

} // namespace dht_hunter::utility::metadata
