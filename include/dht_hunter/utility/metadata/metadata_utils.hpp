#pragma once

/**
 * @file metadata_utils.hpp
 * @brief Legacy metadata utilities header that forwards to the new consolidated utilities
 *
 * This file provides backward compatibility for legacy code that still includes
 * the old metadata utilities header. It forwards to the new consolidated utilities.
 *
 * @deprecated Use utils/domain_utils.hpp instead
 */

#include "dht_hunter/utility/legacy_utils.hpp"

namespace dht_hunter::utility::metadata {

/**
 * @brief Utility functions for working with InfoHash metadata
 * @deprecated Use the free functions in dht_hunter::utility::metadata namespace instead
 */
class MetadataUtils {
public:
    /**
     * @brief Sets the name for an info hash
     * @param infoHash The info hash
     * @param name The name
     * @return True if successful, false otherwise
     */
    static bool setInfoHashName(const types::InfoHash& infoHash, const std::string& name) {
        return dht_hunter::utility::metadata::setInfoHashName(infoHash, name);
    }

    /**
     * @brief Gets the name for an info hash
     * @param infoHash The info hash
     * @return The name, or empty string if not found
     */
    static std::string getInfoHashName(const types::InfoHash& infoHash) {
        return dht_hunter::utility::metadata::getInfoHashName(infoHash);
    }

    /**
     * @brief Adds a file to an info hash
     * @param infoHash The info hash
     * @param path The file path
     * @param size The file size
     * @return True if successful, false otherwise
     */
    static bool addFileToInfoHash(const types::InfoHash& infoHash, const std::string& path, uint64_t size) {
        return dht_hunter::utility::metadata::addFileToInfoHash(infoHash, path, size);
    }

    /**
     * @brief Gets the files for an info hash
     * @param infoHash The info hash
     * @return The files, or empty vector if not found
     */
    static std::vector<types::TorrentFile> getInfoHashFiles(const types::InfoHash& infoHash) {
        return dht_hunter::utility::metadata::getInfoHashFiles(infoHash);
    }

    /**
     * @brief Gets the total size of all files for an info hash
     * @param infoHash The info hash
     * @return The total size in bytes, or 0 if not found
     */
    static uint64_t getInfoHashTotalSize(const types::InfoHash& infoHash) {
        return dht_hunter::utility::metadata::getInfoHashTotalSize(infoHash);
    }

    /**
     * @brief Gets the metadata for an info hash
     * @param infoHash The info hash
     * @return The metadata, or nullptr if not found
     */
    static std::shared_ptr<types::InfoHashMetadata> getInfoHashMetadata(const types::InfoHash& infoHash) {
        return dht_hunter::utility::metadata::getInfoHashMetadata(infoHash);
    }

    /**
     * @brief Gets all metadata
     * @return All metadata
     */
    static std::vector<std::shared_ptr<types::InfoHashMetadata>> getAllMetadata() {
        return dht_hunter::utility::metadata::getAllMetadata();
    }
};

} // namespace dht_hunter::utility::metadata
