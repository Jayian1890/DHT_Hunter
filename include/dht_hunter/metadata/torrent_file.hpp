#pragma once

#include "dht_hunter/bencode/bencode.hpp"
#include "dht_hunter/util/hash.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <filesystem>

namespace dht_hunter::metadata {

/**
 * @brief Represents a file in a torrent
 */
struct TorrentFileInfo {
    std::string path;           ///< Path of the file
    uint64_t length;            ///< Length of the file in bytes
    std::string md5sum;         ///< MD5 sum of the file (optional)
};

/**
 * @brief Represents a torrent file
 */
class TorrentFile {
public:
    /**
     * @brief Constructs an empty torrent file
     */
    TorrentFile();
    
    /**
     * @brief Constructs a torrent file from raw metadata
     * @param metadata The raw metadata
     * @param infoHash The info hash of the torrent
     */
    TorrentFile(const bencode::BencodeValue& metadata, const std::string& infoHash);
    
    /**
     * @brief Loads a torrent file from a file
     * @param filePath The path to the torrent file
     * @return True if the torrent file was loaded successfully, false otherwise
     */
    bool loadFromFile(const std::filesystem::path& filePath);
    
    /**
     * @brief Saves the torrent file to a file
     * @param filePath The path to save the torrent file to
     * @return True if the torrent file was saved successfully, false otherwise
     */
    bool saveToFile(const std::filesystem::path& filePath) const;
    
    /**
     * @brief Gets the info hash of the torrent
     * @return The info hash of the torrent
     */
    std::string getInfoHash() const;
    
    /**
     * @brief Gets the name of the torrent
     * @return The name of the torrent
     */
    std::string getName() const;
    
    /**
     * @brief Gets the total size of the torrent in bytes
     * @return The total size of the torrent in bytes
     */
    uint64_t getTotalSize() const;
    
    /**
     * @brief Gets the piece length of the torrent in bytes
     * @return The piece length of the torrent in bytes
     */
    uint64_t getPieceLength() const;
    
    /**
     * @brief Gets the pieces of the torrent
     * @return The pieces of the torrent (concatenated SHA-1 hashes)
     */
    std::string getPieces() const;
    
    /**
     * @brief Gets the number of pieces in the torrent
     * @return The number of pieces in the torrent
     */
    size_t getPieceCount() const;
    
    /**
     * @brief Gets the SHA-1 hash of a piece
     * @param index The index of the piece
     * @return The SHA-1 hash of the piece
     */
    std::string getPieceHash(size_t index) const;
    
    /**
     * @brief Checks if the torrent is a multi-file torrent
     * @return True if the torrent is a multi-file torrent, false otherwise
     */
    bool isMultiFile() const;
    
    /**
     * @brief Gets the files in the torrent
     * @return The files in the torrent
     */
    const std::vector<TorrentFileInfo>& getFiles() const;
    
    /**
     * @brief Gets the announce URL of the torrent
     * @return The announce URL of the torrent
     */
    std::string getAnnounce() const;
    
    /**
     * @brief Gets the announce list of the torrent
     * @return The announce list of the torrent
     */
    const std::vector<std::vector<std::string>>& getAnnounceList() const;
    
    /**
     * @brief Gets the creation date of the torrent
     * @return The creation date of the torrent (UNIX timestamp)
     */
    int64_t getCreationDate() const;
    
    /**
     * @brief Gets the comment of the torrent
     * @return The comment of the torrent
     */
    std::string getComment() const;
    
    /**
     * @brief Gets the created by of the torrent
     * @return The created by of the torrent
     */
    std::string getCreatedBy() const;
    
    /**
     * @brief Gets the encoding of the torrent
     * @return The encoding of the torrent
     */
    std::string getEncoding() const;
    
    /**
     * @brief Gets the private flag of the torrent
     * @return The private flag of the torrent
     */
    bool isPrivate() const;
    
    /**
     * @brief Sets the name of the torrent
     * @param name The name of the torrent
     */
    void setName(const std::string& name);
    
    /**
     * @brief Sets the piece length of the torrent
     * @param pieceLength The piece length of the torrent in bytes
     */
    void setPieceLength(uint64_t pieceLength);
    
    /**
     * @brief Sets the pieces of the torrent
     * @param pieces The pieces of the torrent (concatenated SHA-1 hashes)
     */
    void setPieces(const std::string& pieces);
    
    /**
     * @brief Adds a file to the torrent
     * @param path The path of the file
     * @param length The length of the file in bytes
     * @param md5sum The MD5 sum of the file (optional)
     */
    void addFile(const std::string& path, uint64_t length, const std::string& md5sum = "");
    
    /**
     * @brief Sets the single file length of the torrent
     * @param length The length of the file in bytes
     */
    void setLength(uint64_t length);
    
    /**
     * @brief Sets the single file MD5 sum of the torrent
     * @param md5sum The MD5 sum of the file
     */
    void setMd5sum(const std::string& md5sum);
    
    /**
     * @brief Sets the announce URL of the torrent
     * @param announce The announce URL of the torrent
     */
    void setAnnounce(const std::string& announce);
    
    /**
     * @brief Adds an announce URL to the torrent
     * @param announce The announce URL to add
     * @param tier The tier of the announce URL (0 = highest priority)
     */
    void addAnnounce(const std::string& announce, size_t tier = 0);
    
    /**
     * @brief Sets the creation date of the torrent
     * @param creationDate The creation date of the torrent (UNIX timestamp)
     */
    void setCreationDate(int64_t creationDate);
    
    /**
     * @brief Sets the comment of the torrent
     * @param comment The comment of the torrent
     */
    void setComment(const std::string& comment);
    
    /**
     * @brief Sets the created by of the torrent
     * @param createdBy The created by of the torrent
     */
    void setCreatedBy(const std::string& createdBy);
    
    /**
     * @brief Sets the encoding of the torrent
     * @param encoding The encoding of the torrent
     */
    void setEncoding(const std::string& encoding);
    
    /**
     * @brief Sets the private flag of the torrent
     * @param isPrivate The private flag of the torrent
     */
    void setPrivate(bool isPrivate);
    
    /**
     * @brief Gets the raw metadata of the torrent
     * @return The raw metadata of the torrent
     */
    bencode::BencodeValue getRawMetadata() const;
    
    /**
     * @brief Calculates the info hash of the torrent
     * @return The info hash of the torrent
     */
    std::string calculateInfoHash() const;
    
    /**
     * @brief Validates the torrent file
     * @return True if the torrent file is valid, false otherwise
     */
    bool validate() const;
    
    /**
     * @brief Gets a string representation of the torrent file
     * @return A string representation of the torrent file
     */
    std::string toString() const;
    
private:
    /**
     * @brief Parses the info dictionary from the metadata
     * @param info The info dictionary
     */
    void parseInfo(const bencode::BencodeValue& info);
    
    /**
     * @brief Parses the files list from the metadata
     * @param files The files list
     */
    void parseFiles(const bencode::BencodeValue& files);
    
    std::string m_infoHash;                                 ///< Info hash of the torrent
    std::string m_name;                                     ///< Name of the torrent
    uint64_t m_pieceLength;                                 ///< Piece length in bytes
    std::string m_pieces;                                   ///< Concatenated SHA-1 hashes of the pieces
    bool m_isMultiFile;                                     ///< Whether the torrent is a multi-file torrent
    std::vector<TorrentFileInfo> m_files;                   ///< Files in the torrent
    uint64_t m_length;                                      ///< Length of the single file in bytes
    std::string m_md5sum;                                   ///< MD5 sum of the single file
    std::string m_announce;                                 ///< Announce URL of the torrent
    std::vector<std::vector<std::string>> m_announceList;   ///< Announce list of the torrent
    int64_t m_creationDate;                                 ///< Creation date of the torrent (UNIX timestamp)
    std::string m_comment;                                  ///< Comment of the torrent
    std::string m_createdBy;                                ///< Created by of the torrent
    std::string m_encoding;                                 ///< Encoding of the torrent
    bool m_isPrivate;                                       ///< Whether the torrent is private
};

/**
 * @brief Creates a torrent file from raw metadata
 * @param metadata The raw metadata
 * @param infoHash The info hash of the torrent
 * @return A shared pointer to the torrent file
 */
std::shared_ptr<TorrentFile> createTorrentFromMetadata(const bencode::BencodeValue& metadata, const std::string& infoHash);

/**
 * @brief Loads a torrent file from a file
 * @param filePath The path to the torrent file
 * @return A shared pointer to the torrent file, or nullptr if the file could not be loaded
 */
std::shared_ptr<TorrentFile> loadTorrentFromFile(const std::filesystem::path& filePath);

} // namespace dht_hunter::metadata
