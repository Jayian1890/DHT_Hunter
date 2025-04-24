#include "dht_hunter/metadata/torrent_file_builder.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace dht_hunter::metadata {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Metadata.TorrentFileBuilder");
}

TorrentFileBuilder::TorrentFileBuilder()
    : m_metadataSize(0),
      m_totalSize(0),
      m_pieceLength(0),
      m_pieceCount(0),
      m_isMultiFile(false),
      m_creationDate(0),
      m_isValid(false) {
    
    // Set default values
    m_createdBy = "DHT-Hunter";
    m_encoding = "UTF-8";
}

TorrentFileBuilder::TorrentFileBuilder(const std::vector<uint8_t>& metadata, uint32_t size)
    : m_metadata(metadata),
      m_metadataSize(size),
      m_totalSize(0),
      m_pieceLength(0),
      m_pieceCount(0),
      m_isMultiFile(false),
      m_creationDate(0),
      m_isValid(false) {
    
    // Set default values
    m_createdBy = "DHT-Hunter";
    m_encoding = "UTF-8";
    
    // Parse the metadata
    parseMetadata();
}

bool TorrentFileBuilder::setMetadata(const std::vector<uint8_t>& metadata, uint32_t size) {
    m_metadata = metadata;
    m_metadataSize = size;
    m_isValid = false;
    
    // Parse the metadata
    return parseMetadata();
}

std::array<uint8_t, 20> TorrentFileBuilder::getInfoHash() const {
    return m_infoHash;
}

std::string TorrentFileBuilder::getName() const {
    return m_name;
}

uint64_t TorrentFileBuilder::getTotalSize() const {
    return m_totalSize;
}

uint32_t TorrentFileBuilder::getPieceLength() const {
    return m_pieceLength;
}

uint32_t TorrentFileBuilder::getPieceCount() const {
    return m_pieceCount;
}

bool TorrentFileBuilder::isMultiFile() const {
    return m_isMultiFile;
}

std::vector<std::pair<std::string, uint64_t>> TorrentFileBuilder::getFiles() const {
    return m_files;
}

std::shared_ptr<bencode::BencodeValue> TorrentFileBuilder::buildTorrentFile() const {
    if (!m_isValid) {
        logger->error("Cannot build torrent file: Metadata is not valid");
        return nullptr;
    }
    
    // Create the torrent file dictionary
    auto torrentDict = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());
    
    // Add the info dictionary
    torrentDict->setDictionary("info", m_infoDict->getDictionary());
    
    // Add the announce URL if set
    if (!m_announceUrl.empty()) {
        torrentDict->setString("announce", m_announceUrl);
    }
    
    // Add the announce list if set
    if (!m_announceList.empty()) {
        auto announceList = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::List());
        
        for (const auto& tier : m_announceList) {
            auto tierList = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::List());
            
            for (const auto& tracker : tier) {
                tierList->addString(tracker);
            }
            
            announceList->add(tierList);
        }
        
        torrentDict->set("announce-list", announceList);
    }
    
    // Add the creation date if set
    if (m_creationDate > 0) {
        torrentDict->setInteger("creation date", m_creationDate);
    } else {
        // Use current time
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        torrentDict->setInteger("creation date", timestamp);
    }
    
    // Add the created by string
    torrentDict->setString("created by", m_createdBy);
    
    // Add the comment if set
    if (!m_comment.empty()) {
        torrentDict->setString("comment", m_comment);
    }
    
    // Add the encoding
    torrentDict->setString("encoding", m_encoding);
    
    return torrentDict;
}

bool TorrentFileBuilder::saveTorrentFile(const std::string& filePath) const {
    // Build the torrent file
    auto torrentFile = buildTorrentFile();
    if (!torrentFile) {
        return false;
    }
    
    // Encode the torrent file
    std::string encodedTorrent = bencode::BencodeEncoder::encode(*torrentFile);
    
    // Save the torrent file
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        logger->error("Failed to open file for writing: {}", filePath);
        return false;
    }
    
    file.write(encodedTorrent.data(), static_cast<std::streamsize>(encodedTorrent.size()));
    
    logger->info("Saved torrent file to {}", filePath);
    return true;
}

std::string TorrentFileBuilder::saveTorrentFile(const std::filesystem::path& directory) const {
    if (!m_isValid) {
        logger->error("Cannot save torrent file: Metadata is not valid");
        return "";
    }
    
    // Create the directory if it doesn't exist
    if (!std::filesystem::exists(directory)) {
        if (!std::filesystem::create_directories(directory)) {
            logger->error("Failed to create directory: {}", directory.string());
            return "";
        }
    }
    
    // Generate a filename based on the torrent name
    std::string filename = m_name;
    
    // Replace invalid characters in the filename
    std::replace_if(filename.begin(), filename.end(), 
                   [](char c) { return c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|'; }, 
                   '_');
    
    // Add the .torrent extension
    filename += ".torrent";
    
    // Create the full path
    std::filesystem::path filePath = directory / filename;
    
    // Save the torrent file
    if (!saveTorrentFile(filePath.string())) {
        return "";
    }
    
    return filePath.string();
}

std::string TorrentFileBuilder::getTorrentFileAsString() const {
    // Build the torrent file
    auto torrentFile = buildTorrentFile();
    if (!torrentFile) {
        return "";
    }
    
    // Encode the torrent file
    return bencode::BencodeEncoder::encode(*torrentFile);
}

void TorrentFileBuilder::setAnnounceUrl(const std::string& announceUrl) {
    m_announceUrl = announceUrl;
}

void TorrentFileBuilder::setAnnounceList(const std::vector<std::vector<std::string>>& announceList) {
    m_announceList = announceList;
}

void TorrentFileBuilder::setCreationDate(int64_t creationDate) {
    m_creationDate = creationDate;
}

void TorrentFileBuilder::setCreatedBy(const std::string& createdBy) {
    m_createdBy = createdBy;
}

void TorrentFileBuilder::setComment(const std::string& comment) {
    m_comment = comment;
}

void TorrentFileBuilder::setEncoding(const std::string& encoding) {
    m_encoding = encoding;
}

bool TorrentFileBuilder::isValid() const {
    return m_isValid;
}

bool TorrentFileBuilder::parseMetadata() {
    // Reset state
    m_metadataDict = nullptr;
    m_infoDict = nullptr;
    m_name = "";
    m_totalSize = 0;
    m_pieceLength = 0;
    m_pieceCount = 0;
    m_isMultiFile = false;
    m_files.clear();
    m_isValid = false;
    
    // Check if we have metadata
    if (m_metadata.empty() || m_metadataSize == 0) {
        logger->error("No metadata to parse");
        return false;
    }
    
    // Decode the metadata
    try {
        std::string metadataStr(reinterpret_cast<const char*>(m_metadata.data()), m_metadataSize);
        m_metadataDict = bencode::BencodeDecoder::decode(metadataStr);
    } catch (const bencode::BencodeException& e) {
        logger->error("Failed to decode metadata: {}", e.what());
        return false;
    }
    
    // Check if the metadata is a dictionary
    if (!m_metadataDict || !m_metadataDict->isDictionary()) {
        logger->error("Metadata is not a dictionary");
        return false;
    }
    
    // Extract the info dictionary
    if (!extractInfoDictionary()) {
        return false;
    }
    
    // Validate the info dictionary
    if (!validateInfoDictionary()) {
        return false;
    }
    
    // Calculate the info hash
    try {
        // Encode the info dictionary
        std::string encodedInfo = bencode::BencodeEncoder::encode(*m_infoDict);
        
        // Calculate the SHA-1 hash
        m_infoHash = util::sha1(encodedInfo);
        
        logger->debug("Calculated info hash: {}", util::bytesToHex(m_infoHash.data(), m_infoHash.size()));
    } catch (const std::exception& e) {
        logger->error("Failed to calculate info hash: {}", e.what());
        return false;
    }
    
    // Set the valid flag
    m_isValid = true;
    
    return true;
}

bool TorrentFileBuilder::extractInfoDictionary() {
    // Get the info dictionary
    auto infoDict = m_metadataDict->getDictionary("info");
    if (infoDict.empty()) {
        logger->error("Metadata does not contain an info dictionary");
        return false;
    }
    
    // Create a new BencodeValue for the info dictionary
    m_infoDict = std::make_shared<bencode::BencodeValue>(infoDict);
    
    // Get the name
    auto name = m_infoDict->getString("name");
    if (!name) {
        logger->error("Info dictionary does not contain a name");
        return false;
    }
    
    m_name = *name;
    
    // Get the piece length
    auto pieceLength = m_infoDict->getInteger("piece length");
    if (!pieceLength) {
        logger->error("Info dictionary does not contain a piece length");
        return false;
    }
    
    m_pieceLength = static_cast<uint32_t>(*pieceLength);
    
    // Get the pieces
    auto pieces = m_infoDict->getString("pieces");
    if (!pieces) {
        logger->error("Info dictionary does not contain pieces");
        return false;
    }
    
    // Calculate the number of pieces
    m_pieceCount = static_cast<uint32_t>(pieces->size() / 20);
    
    // Check if this is a multi-file torrent
    auto files = m_infoDict->getList("files");
    if (files) {
        // This is a multi-file torrent
        m_isMultiFile = true;
        
        // Process the files
        for (const auto& fileValue : *files) {
            // Get the file dictionary
            auto fileDict = fileValue->getDictionary();
            
            // Get the file length
            auto length = fileValue->getInteger("length");
            if (!length) {
                logger->error("File dictionary does not contain a length");
                return false;
            }
            
            // Get the file path
            auto pathList = fileValue->getList("path");
            if (!pathList) {
                logger->error("File dictionary does not contain a path");
                return false;
            }
            
            // Build the file path
            std::string filePath;
            for (const auto& pathPart : *pathList) {
                if (pathPart->isString()) {
                    if (!filePath.empty()) {
                        filePath += "/";
                    }
                    filePath += pathPart->getString();
                }
            }
            
            // Add the file to the list
            m_files.emplace_back(filePath, static_cast<uint64_t>(*length));
            
            // Add to the total size
            m_totalSize += static_cast<uint64_t>(*length);
        }
    } else {
        // This is a single-file torrent
        m_isMultiFile = false;
        
        // Get the file length
        auto length = m_infoDict->getInteger("length");
        if (!length) {
            logger->error("Info dictionary does not contain a length");
            return false;
        }
        
        // Add the file to the list
        m_files.emplace_back(m_name, static_cast<uint64_t>(*length));
        
        // Set the total size
        m_totalSize = static_cast<uint64_t>(*length);
    }
    
    return true;
}

bool TorrentFileBuilder::validateInfoDictionary() const {
    // Check if we have an info dictionary
    if (!m_infoDict) {
        logger->error("No info dictionary to validate");
        return false;
    }
    
    // Check if the info dictionary has a name
    if (!m_infoDict->getString("name")) {
        logger->error("Info dictionary does not contain a name");
        return false;
    }
    
    // Check if the info dictionary has a piece length
    if (!m_infoDict->getInteger("piece length")) {
        logger->error("Info dictionary does not contain a piece length");
        return false;
    }
    
    // Check if the info dictionary has pieces
    if (!m_infoDict->getString("pieces")) {
        logger->error("Info dictionary does not contain pieces");
        return false;
    }
    
    // Check if this is a multi-file torrent
    if (m_isMultiFile) {
        // Check if the info dictionary has files
        auto files = m_infoDict->getList("files");
        if (!files) {
            logger->error("Multi-file torrent does not contain files");
            return false;
        }
        
        // Check each file
        for (const auto& fileValue : *files) {
            // Check if the file has a length
            if (!fileValue->getInteger("length")) {
                logger->error("File does not contain a length");
                return false;
            }
            
            // Check if the file has a path
            auto pathList = fileValue->getList("path");
            if (!pathList) {
                logger->error("File does not contain a path");
                return false;
            }
            
            // Check if the path is valid
            if (pathList->empty()) {
                logger->error("File path is empty");
                return false;
            }
        }
    } else {
        // Check if the info dictionary has a length
        if (!m_infoDict->getInteger("length")) {
            logger->error("Single-file torrent does not contain a length");
            return false;
        }
    }
    
    return true;
}

} // namespace dht_hunter::metadata
