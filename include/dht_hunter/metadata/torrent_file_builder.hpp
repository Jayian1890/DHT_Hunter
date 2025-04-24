#pragma once

#include "dht_hunter/bencode/bencode.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/util/hash.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

namespace dht_hunter::metadata {

/**
 * @brief Class for building torrent files from raw metadata
 */
class TorrentFileBuilder {
public:
    /**
     * @brief Default constructor
     */
    TorrentFileBuilder();

    /**
     * @brief Constructs a torrent file builder from raw metadata
     * @param metadata The raw metadata
     * @param size The size of the metadata
     */
    TorrentFileBuilder(const std::vector<uint8_t>& metadata, uint32_t size);

    /**
     * @brief Sets the raw metadata
     * @param metadata The raw metadata
     * @param size The size of the metadata
     * @return True if the metadata was set successfully, false otherwise
     */
    bool setMetadata(const std::vector<uint8_t>& metadata, uint32_t size);

    /**
     * @brief Gets the info hash of the torrent
     * @return The info hash
     */
    std::array<uint8_t, 20> getInfoHash() const;

    /**
     * @brief Gets the name of the torrent
     * @return The name
     */
    std::string getName() const;

    /**
     * @brief Gets the total size of the torrent
     * @return The total size
     */
    uint64_t getTotalSize() const;

    /**
     * @brief Gets the piece length of the torrent
     * @return The piece length
     */
    uint32_t getPieceLength() const;

    /**
     * @brief Gets the number of pieces in the torrent
     * @return The number of pieces
     */
    uint32_t getPieceCount() const;

    /**
     * @brief Checks if the torrent is a multi-file torrent
     * @return True if the torrent is a multi-file torrent, false otherwise
     */
    bool isMultiFile() const;

    /**
     * @brief Gets the file paths and sizes for a multi-file torrent
     * @return A vector of pairs containing the file path and size
     */
    std::vector<std::pair<std::string, uint64_t>> getFiles() const;

    /**
     * @brief Builds a torrent file
     * @return The torrent file as a bencode dictionary
     */
    std::shared_ptr<bencode::BencodeValue> buildTorrentFile() const;

    /**
     * @brief Saves the torrent file to disk
     * @param filePath The path to save the torrent file to
     * @return True if the torrent file was saved successfully, false otherwise
     */
    bool saveTorrentFile(const std::string& filePath) const;

    /**
     * @brief Saves the torrent file to disk with a generated filename
     * @param directory The directory to save the torrent file to
     * @return The path to the saved torrent file, or empty string if failed
     */
    std::string saveTorrentFile(const std::filesystem::path& directory) const;

    /**
     * @brief Gets the torrent file as a string
     * @return The torrent file as a string
     */
    std::string getTorrentFileAsString() const;

    /**
     * @brief Sets the announce URL for the torrent
     * @param announceUrl The announce URL
     */
    void setAnnounceUrl(const std::string& announceUrl);

    /**
     * @brief Sets the announce list for the torrent
     * @param announceList The announce list
     */
    void setAnnounceList(const std::vector<std::vector<std::string>>& announceList);

    /**
     * @brief Sets the creation date for the torrent
     * @param creationDate The creation date (Unix timestamp)
     */
    void setCreationDate(int64_t creationDate);

    /**
     * @brief Sets the created by string for the torrent
     * @param createdBy The created by string
     */
    void setCreatedBy(const std::string& createdBy);

    /**
     * @brief Sets the comment for the torrent
     * @param comment The comment
     */
    void setComment(const std::string& comment);

    /**
     * @brief Sets the encoding for the torrent
     * @param encoding The encoding
     */
    void setEncoding(const std::string& encoding);

    /**
     * @brief Checks if the metadata is valid
     * @return True if the metadata is valid, false otherwise
     */
    bool isValid() const;

private:
    /**
     * @brief Parses the metadata
     * @return True if the metadata was parsed successfully, false otherwise
     */
    bool parseMetadata();

    /**
     * @brief Extracts the info dictionary from the metadata
     * @return True if the info dictionary was extracted successfully, false otherwise
     */
    bool extractInfoDictionary();

    /**
     * @brief Validates the info dictionary
     * @return True if the info dictionary is valid, false otherwise
     */
    bool validateInfoDictionary() const;

    std::vector<uint8_t> m_metadata;                                ///< The raw metadata
    uint32_t m_metadataSize;                                        ///< The size of the metadata
    std::shared_ptr<bencode::BencodeValue> m_metadataDict;          ///< The metadata as a bencode dictionary
    std::shared_ptr<bencode::BencodeValue> m_infoDict;              ///< The info dictionary
    std::array<uint8_t, 20> m_infoHash;                             ///< The info hash
    std::string m_name;                                             ///< The name of the torrent
    uint64_t m_totalSize;                                           ///< The total size of the torrent
    uint32_t m_pieceLength;                                         ///< The piece length
    uint32_t m_pieceCount;                                          ///< The number of pieces
    bool m_isMultiFile;                                             ///< Whether the torrent is a multi-file torrent
    std::vector<std::pair<std::string, uint64_t>> m_files;          ///< The files in the torrent (path, size)
    std::string m_announceUrl;                                      ///< The announce URL
    std::vector<std::vector<std::string>> m_announceList;           ///< The announce list
    int64_t m_creationDate;                                         ///< The creation date
    std::string m_createdBy;                                        ///< The created by string
    std::string m_comment;                                          ///< The comment
    std::string m_encoding;                                         ///< The encoding
    bool m_isValid;                                                 ///< Whether the metadata is valid
};

} // namespace dht_hunter::metadata
