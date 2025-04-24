#include "dht_hunter/metadata/torrent_file.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/util/hash.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>

DEFINE_COMPONENT_LOGGER("Metadata", "TorrentFile")

namespace dht_hunter::metadata {

TorrentFile::TorrentFile()
    : m_pieceLength(0),
      m_isMultiFile(false),
      m_length(0),
      m_creationDate(-1),
      m_isPrivate(false) {
}

TorrentFile::TorrentFile(const bencode::BencodeValue& metadata, const std::string& infoHash)
    : m_infoHash(infoHash),
      m_pieceLength(0),
      m_isMultiFile(false),
      m_length(0),
      m_creationDate(-1),
      m_isPrivate(false) {

    // Check if the metadata is a dictionary
    if (metadata.getDictionary().empty()) {
        getLogger()->error("Metadata is not a dictionary");
        return;
    }

    // Get the info dictionary
    auto info = metadata.getDictionary().find("info");
    if (info == metadata.getDictionary().end() || !info->second || info->second->getDictionary().empty()) {
        getLogger()->error("Metadata does not contain an info dictionary");
        return;
    }

    // Parse the info dictionary
    parseInfo(*info->second);

    // Get the announce URL
    auto announce = metadata.getDictionary().find("announce");
    if (announce != metadata.getDictionary().end() && announce->second->isString()) {
        m_announce = announce->second->getString();
    }

    // Get the announce list
    auto announceList = metadata.getDictionary().find("announce-list");
    if (announceList != metadata.getDictionary().end() && announceList->second->isList()) {
        for (const auto& tier : announceList->second->getList()) {
            if (tier->isList()) {
                std::vector<std::string> tierList;
                for (const auto& url : tier->getList()) {
                    if (url->isString()) {
                        tierList.push_back(url->getString());
                    }
                }
                if (!tierList.empty()) {
                    m_announceList.push_back(tierList);
                }
            }
        }
    }

    // Get the creation date
    auto creationDate = metadata.getDictionary().find("creation date");
    if (creationDate != metadata.getDictionary().end() && creationDate->second->isInteger()) {
        m_creationDate = creationDate->second->getInteger();
    }

    // Get the comment
    auto comment = metadata.getDictionary().find("comment");
    if (comment != metadata.getDictionary().end() && comment->second->isString()) {
        m_comment = comment->second->getString();
    }

    // Get the created by
    auto createdBy = metadata.getDictionary().find("created by");
    if (createdBy != metadata.getDictionary().end() && createdBy->second->isString()) {
        m_createdBy = createdBy->second->getString();
    }

    // Get the encoding
    auto encoding = metadata.getDictionary().find("encoding");
    if (encoding != metadata.getDictionary().end() && encoding->second->isString()) {
        m_encoding = encoding->second->getString();
    }

    getLogger()->debug("Parsed torrent file: {}", m_name);
}

bool TorrentFile::loadFromFile(const std::filesystem::path& filePath) {
    try {
        // Open the file
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            getLogger()->error("Failed to open file: {}", filePath.string());
            return false;
        }

        // Read the file contents
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string contents = buffer.str();

        // Parse the bencode data
        bencode::BencodeDecoder decoder;
        auto result = decoder.decode(contents);
        if (!result) {
            getLogger()->error("Failed to parse torrent file");
            return false;
        }

        // Calculate the info hash
        auto info = result->getDictionary().find("info");
        if (info == result->getDictionary().end() || !info->second->isDictionary()) {
            getLogger()->error("Torrent file does not contain an info dictionary");
            return false;
        }

        std::string infoStr = bencode::BencodeEncoder::encode(*info->second);
        auto hash = util::sha1(infoStr);
        std::string infoHash = util::bytesToHex(hash.data(), hash.size());

        // Create a new torrent file from the metadata
        *this = TorrentFile(*result, infoHash);

        getLogger()->debug("Loaded torrent file: {}", filePath.string());
        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception while loading torrent file: {}", e.what());
        return false;
    }
}

bool TorrentFile::saveToFile(const std::filesystem::path& filePath) const {
    try {
        // Create the metadata
        auto metadata = getRawMetadata();

        // Encode the metadata
        std::string encoded = bencode::BencodeEncoder::encode(metadata);

        // Open the file
        std::ofstream file(filePath, std::ios::binary);
        if (!file) {
            getLogger()->error("Failed to open file for writing: {}", filePath.string());
            return false;
        }

        // Write the encoded metadata to the file
        file.write(encoded.c_str(), static_cast<std::streamsize>(encoded.size()));

        getLogger()->debug("Saved torrent file: {}", filePath.string());
        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception while saving torrent file: {}", e.what());
        return false;
    }
}

std::string TorrentFile::getInfoHash() const {
    return m_infoHash;
}

std::string TorrentFile::getName() const {
    return m_name;
}

uint64_t TorrentFile::getTotalSize() const {
    if (m_isMultiFile) {
        uint64_t totalSize = 0;
        for (const auto& file : m_files) {
            totalSize += file.length;
        }
        return totalSize;
    } else {
        return m_length;
    }
}

uint64_t TorrentFile::getPieceLength() const {
    return m_pieceLength;
}

std::string TorrentFile::getPieces() const {
    return m_pieces;
}

size_t TorrentFile::getPieceCount() const {
    if (m_pieceLength == 0) {
        return 0;
    }

    uint64_t totalSize = getTotalSize();
    return static_cast<size_t>((totalSize + m_pieceLength - 1) / m_pieceLength);
}

std::string TorrentFile::getPieceHash(size_t index) const {
    if (index >= getPieceCount()) {
        return "";
    }

    size_t hashSize = 20; // SHA-1 hash size
    size_t offset = index * hashSize;

    if (offset + hashSize > m_pieces.size()) {
        return "";
    }

    return m_pieces.substr(offset, hashSize);
}

bool TorrentFile::isMultiFile() const {
    return m_isMultiFile;
}

const std::vector<TorrentFileInfo>& TorrentFile::getFiles() const {
    return m_files;
}

std::string TorrentFile::getAnnounce() const {
    return m_announce;
}

const std::vector<std::vector<std::string>>& TorrentFile::getAnnounceList() const {
    return m_announceList;
}

int64_t TorrentFile::getCreationDate() const {
    return m_creationDate;
}

std::string TorrentFile::getComment() const {
    return m_comment;
}

std::string TorrentFile::getCreatedBy() const {
    return m_createdBy;
}

std::string TorrentFile::getEncoding() const {
    return m_encoding;
}

bool TorrentFile::isPrivate() const {
    return m_isPrivate;
}

void TorrentFile::setName(const std::string& name) {
    m_name = name;
}

void TorrentFile::setPieceLength(uint64_t pieceLength) {
    m_pieceLength = pieceLength;
}

void TorrentFile::setPieces(const std::string& pieces) {
    m_pieces = pieces;
}

void TorrentFile::addFile(const std::string& path, uint64_t length, const std::string& md5sum) {
    m_isMultiFile = true;

    TorrentFileInfo fileInfo;
    fileInfo.path = path;
    fileInfo.length = length;
    fileInfo.md5sum = md5sum;

    m_files.push_back(fileInfo);
}

void TorrentFile::setLength(uint64_t length) {
    m_isMultiFile = false;
    m_length = length;
}

void TorrentFile::setMd5sum(const std::string& md5sum) {
    m_md5sum = md5sum;
}

void TorrentFile::setAnnounce(const std::string& announce) {
    m_announce = announce;
}

void TorrentFile::addAnnounce(const std::string& announce, size_t tier) {
    // Ensure we have enough tiers
    while (m_announceList.size() <= tier) {
        m_announceList.push_back({});
    }

    // Add the announce URL to the specified tier
    m_announceList[tier].push_back(announce);
}

void TorrentFile::setCreationDate(int64_t creationDate) {
    m_creationDate = creationDate;
}

void TorrentFile::setComment(const std::string& comment) {
    m_comment = comment;
}

void TorrentFile::setCreatedBy(const std::string& createdBy) {
    m_createdBy = createdBy;
}

void TorrentFile::setEncoding(const std::string& encoding) {
    m_encoding = encoding;
}

void TorrentFile::setPrivate(bool isPrivate) {
    m_isPrivate = isPrivate;
}

bencode::BencodeValue TorrentFile::getRawMetadata() const {
    // Create a dictionary for the metadata
    bencode::BencodeValue metadata;

    // Add the announce URL
    if (!m_announce.empty()) {
        metadata.setString("announce", m_announce);
    }

    // Add the announce list
    if (!m_announceList.empty()) {
        auto announceList = std::make_shared<bencode::BencodeValue>();

        for (const auto& tier : m_announceList) {
            auto tierList = std::make_shared<bencode::BencodeValue>();

            for (const auto& url : tier) {
                tierList->add(std::make_shared<bencode::BencodeValue>(url));
            }

            announceList->add(tierList);
        }

        metadata.set("announce-list", announceList);
    }

    // Add the creation date
    if (m_creationDate >= 0) {
        metadata.setInteger("creation date", m_creationDate);
    }

    // Add the comment
    if (!m_comment.empty()) {
        metadata.setString("comment", m_comment);
    }

    // Add the created by
    if (!m_createdBy.empty()) {
        metadata.setString("created by", m_createdBy);
    }

    // Add the encoding
    if (!m_encoding.empty()) {
        metadata.setString("encoding", m_encoding);
    }

    // Create the info dictionary
    auto info = std::make_shared<bencode::BencodeValue>();

    // Add the name
    info->setString("name", m_name);

    // Add the piece length
    info->setInteger("piece length", static_cast<int64_t>(m_pieceLength));

    // Add the pieces
    info->setString("pieces", m_pieces);

    // Add the private flag
    if (m_isPrivate) {
        info->setInteger("private", 1);
    }

    // Add the files or length
    if (m_isMultiFile) {
        auto files = std::make_shared<bencode::BencodeValue>();

        for (const auto& file : m_files) {
            auto fileDict = std::make_shared<bencode::BencodeValue>();

            // Add the length
            fileDict->setInteger("length", static_cast<int64_t>(file.length));

            // Add the MD5 sum
            if (!file.md5sum.empty()) {
                fileDict->setString("md5sum", file.md5sum);
            }

            // Add the path
            auto path = std::make_shared<bencode::BencodeValue>();

            // Split the path by '/'
            std::string pathStr = file.path;
            std::replace(pathStr.begin(), pathStr.end(), '\\', '/');

            size_t pos = 0;
            std::string token;
            while ((pos = pathStr.find('/')) != std::string::npos) {
                token = pathStr.substr(0, pos);
                if (!token.empty()) {
                    path->add(std::make_shared<bencode::BencodeValue>(token));
                }
                pathStr.erase(0, pos + 1);
            }

            if (!pathStr.empty()) {
                path->add(std::make_shared<bencode::BencodeValue>(pathStr));
            }

            fileDict->set("path", path);

            files->add(fileDict);
        }

        info->set("files", files);
    } else {
        // Add the length
        info->setInteger("length", static_cast<int64_t>(m_length));

        // Add the MD5 sum
        if (!m_md5sum.empty()) {
            info->setString("md5sum", m_md5sum);
        }
    }

    // Add the info dictionary to the metadata
    metadata.set("info", info);

    return metadata;
}

std::string TorrentFile::calculateInfoHash() const {
    // Get the info dictionary
    auto metadata = getRawMetadata();
    auto info = metadata.getDictionary().find("info");
    if (info == metadata.getDictionary().end() || !info->second || info->second->getDictionary().empty()) {
        getLogger()->error("Metadata does not contain an info dictionary");
        return "";
    }

    // Encode the info dictionary
    std::string infoStr = bencode::BencodeEncoder::encode(*info->second);

    // Calculate the SHA-1 hash
    auto hash = util::sha1(infoStr);
    return util::bytesToHex(hash.data(), hash.size());
}

bool TorrentFile::validate() const {
    // Check if the name is set
    if (m_name.empty()) {
        getLogger()->error("Torrent name is not set");
        return false;
    }

    // Check if the piece length is set
    if (m_pieceLength == 0) {
        getLogger()->error("Piece length is not set");
        return false;
    }

    // Check if the pieces are set
    if (m_pieces.empty()) {
        getLogger()->error("Pieces are not set");
        return false;
    }

    // Check if the pieces length is a multiple of 20 (SHA-1 hash size)
    if (m_pieces.size() % 20 != 0) {
        getLogger()->error("Pieces length is not a multiple of 20");
        return false;
    }

    // Check if the files or length is set
    if (m_isMultiFile) {
        if (m_files.empty()) {
            getLogger()->error("No files in multi-file torrent");
            return false;
        }

        for (const auto& file : m_files) {
            if (file.path.empty()) {
                getLogger()->error("File path is empty");
                return false;
            }

            if (file.length == 0) {
                getLogger()->error("File length is 0");
                return false;
            }
        }
    } else {
        if (m_length == 0) {
            getLogger()->error("Length is not set");
            return false;
        }
    }

    return true;
}

std::string TorrentFile::toString() const {
    std::stringstream ss;

    ss << "Torrent File:" << std::endl;
    ss << "  Name: " << m_name << std::endl;
    ss << "  Info Hash: " << m_infoHash << std::endl;
    ss << "  Piece Length: " << m_pieceLength << " bytes" << std::endl;
    ss << "  Piece Count: " << getPieceCount() << std::endl;

    if (m_isMultiFile) {
        ss << "  Files: " << m_files.size() << std::endl;

        uint64_t totalSize = 0;
        for (const auto& file : m_files) {
            totalSize += file.length;
            ss << "    " << file.path << " (" << file.length << " bytes)" << std::endl;
        }

        ss << "  Total Size: " << totalSize << " bytes" << std::endl;
    } else {
        ss << "  Length: " << m_length << " bytes" << std::endl;
    }

    if (!m_announce.empty()) {
        ss << "  Announce: " << m_announce << std::endl;
    }

    if (!m_announceList.empty()) {
        ss << "  Announce List:" << std::endl;

        for (size_t i = 0; i < m_announceList.size(); ++i) {
            ss << "    Tier " << i << ":" << std::endl;

            for (const auto& url : m_announceList[i]) {
                ss << "      " << url << std::endl;
            }
        }
    }

    if (m_creationDate >= 0) {
        // Convert the UNIX timestamp to a human-readable date
        std::time_t time = static_cast<std::time_t>(m_creationDate);
        std::tm* tm = std::gmtime(&time);

        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S UTC", tm);

        ss << "  Creation Date: " << buffer << std::endl;
    }

    if (!m_comment.empty()) {
        ss << "  Comment: " << m_comment << std::endl;
    }

    if (!m_createdBy.empty()) {
        ss << "  Created By: " << m_createdBy << std::endl;
    }

    if (!m_encoding.empty()) {
        ss << "  Encoding: " << m_encoding << std::endl;
    }

    if (m_isPrivate) {
        ss << "  Private: Yes" << std::endl;
    }

    return ss.str();
}

void TorrentFile::parseInfo(const bencode::BencodeValue& info) {
    // Get the name
    auto name = info.getDictionary().find("name");
    if (name != info.getDictionary().end() && name->second && name->second->isString()) {
        m_name = name->second->getString();
    }

    // Get the piece length
    auto pieceLength = info.getDictionary().find("piece length");
    if (pieceLength != info.getDictionary().end() && pieceLength->second && pieceLength->second->isInteger()) {
        m_pieceLength = static_cast<uint64_t>(pieceLength->second->getInteger());
    }

    // Get the pieces
    auto pieces = info.getDictionary().find("pieces");
    if (pieces != info.getDictionary().end() && pieces->second && pieces->second->isString()) {
        m_pieces = pieces->second->getString();
    }

    // Get the private flag
    auto isPrivate = info.getDictionary().find("private");
    if (isPrivate != info.getDictionary().end() && isPrivate->second && isPrivate->second->isInteger()) {
        m_isPrivate = (isPrivate->second->getInteger() == 1);
    }

    // Check if this is a multi-file torrent
    auto files = info.getDictionary().find("files");
    if (files != info.getDictionary().end() && files->second && !files->second->getList().empty()) {
        m_isMultiFile = true;
        parseFiles(*files->second);
    } else {
        m_isMultiFile = false;

        // Get the length
        auto length = info.getDictionary().find("length");
        if (length != info.getDictionary().end() && length->second && length->second->isInteger()) {
            m_length = static_cast<uint64_t>(length->second->getInteger());
        }

        // Get the MD5 sum
        auto md5sum = info.getDictionary().find("md5sum");
        if (md5sum != info.getDictionary().end() && md5sum->second && md5sum->second->isString()) {
            m_md5sum = md5sum->second->getString();
        }
    }
}

void TorrentFile::parseFiles(const bencode::BencodeValue& files) {
    for (const auto& file : files.getList()) {
        if (!file || file->getDictionary().empty()) {
            continue;
        }

        TorrentFileInfo fileInfo;

        // Get the length
        auto length = file->getDictionary().find("length");
        if (length != file->getDictionary().end() && length->second && length->second->isInteger()) {
            fileInfo.length = static_cast<uint64_t>(length->second->getInteger());
        }

        // Get the MD5 sum
        auto md5sum = file->getDictionary().find("md5sum");
        if (md5sum != file->getDictionary().end() && md5sum->second && md5sum->second->isString()) {
            fileInfo.md5sum = md5sum->second->getString();
        }

        // Get the path
        auto path = file->getDictionary().find("path");
        if (path != file->getDictionary().end() && path->second && !path->second->getList().empty()) {
            std::string pathStr;

            for (const auto& pathPart : path->second->getList()) {
                if (pathPart && pathPart->isString()) {
                    if (!pathStr.empty()) {
                        pathStr += "/";
                    }
                    pathStr += pathPart->getString();
                }
            }

            fileInfo.path = pathStr;
        }

        if (!fileInfo.path.empty() && fileInfo.length > 0) {
            m_files.push_back(fileInfo);
        }
    }
}

std::shared_ptr<TorrentFile> createTorrentFromMetadata(const bencode::BencodeValue& metadata, const std::string& infoHash) {
    return std::make_shared<TorrentFile>(metadata, infoHash);
}

std::shared_ptr<TorrentFile> loadTorrentFromFile(const std::filesystem::path& filePath) {
    auto torrent = std::make_shared<TorrentFile>();
    if (torrent->loadFromFile(filePath)) {
        return torrent;
    }
    return nullptr;
}

} // namespace dht_hunter::metadata
