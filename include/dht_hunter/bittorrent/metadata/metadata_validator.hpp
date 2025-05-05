#pragma once

#include "dht_hunter/bencode/bencode.hpp"
#include <memory>
#include <string>
#include <vector>

namespace dht_hunter::bittorrent {

/**
 * @brief Metadata validator
 */
class MetadataValidator {
public:
    /**
     * @brief Validates metadata
     * @param metadata The metadata to validate
     * @return True if the metadata is valid, false otherwise
     */
    static bool validate(std::shared_ptr<bencode::BencodeValue> metadata) {
        if (!metadata || !metadata->isDictionary()) {
            return false;
        }
        
        // Check for required fields
        auto name = metadata->getString("name");
        if (!name) {
            return false;
        }
        
        // Check for either length (single file) or files (multi-file)
        auto length = metadata->getInteger("length");
        auto files = metadata->getList("files");
        if (!length && !files) {
            return false;
        }
        
        // If it's a multi-file torrent, validate the files
        if (files) {
            for (const auto& file : *files) {
                if (!file->isDictionary()) {
                    return false;
                }
                
                auto filePath = file->getString("path");
                auto fileLength = file->getInteger("length");
                if (!filePath || !fileLength) {
                    return false;
                }
            }
        }
        
        // Check for piece length
        auto pieceLength = metadata->getInteger("piece length");
        if (!pieceLength) {
            return false;
        }
        
        return true;
    }
};

} // namespace dht_hunter::bittorrent
