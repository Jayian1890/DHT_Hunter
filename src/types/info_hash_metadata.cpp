#include "dht_hunter/types/info_hash_metadata.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>

namespace dht_hunter::types {

// Initialize static members
std::shared_ptr<InfoHashMetadataRegistry> InfoHashMetadataRegistry::s_instance = nullptr;
std::mutex InfoHashMetadataRegistry::s_instanceMutex;

InfoHashMetadata::InfoHashMetadata(const InfoHash& infoHash)
    : m_infoHash(infoHash) {
}

InfoHashMetadata::InfoHashMetadata(const InfoHash& infoHash, const std::string& name)
    : m_infoHash(infoHash), m_name(name) {
}

const InfoHash& InfoHashMetadata::getInfoHash() const {
    return m_infoHash;
}

void InfoHashMetadata::setInfoHash(const InfoHash& infoHash) {
    m_infoHash = infoHash;
}

const std::string& InfoHashMetadata::getName() const {
    return m_name;
}

void InfoHashMetadata::setName(const std::string& name) {
    m_name = name;
}

const std::vector<TorrentFile>& InfoHashMetadata::getFiles() const {
    return m_files;
}

void InfoHashMetadata::addFile(const TorrentFile& file) {
    m_files.push_back(file);
}

void InfoHashMetadata::setFiles(const std::vector<TorrentFile>& files) {
    m_files = files;
}

uint64_t InfoHashMetadata::getTotalSize() const {
    uint64_t totalSize = 0;
    for (const auto& file : m_files) {
        totalSize += file.size;
    }
    return totalSize;
}

std::vector<uint8_t> InfoHashMetadata::serialize() const {
    std::vector<uint8_t> result;

    // Reserve space for the info hash (20 bytes)
    result.reserve(20 + m_name.size() + 100 * m_files.size());

    // Write the info hash
    result.insert(result.end(), m_infoHash.begin(), m_infoHash.end());

    // Write the name length and name
    uint32_t nameLength = static_cast<uint32_t>(m_name.size());
    result.resize(result.size() + sizeof(nameLength));
    std::memcpy(result.data() + result.size() - sizeof(nameLength), &nameLength, sizeof(nameLength));
    result.insert(result.end(), m_name.begin(), m_name.end());

    // Write the number of files
    uint32_t fileCount = static_cast<uint32_t>(m_files.size());
    result.resize(result.size() + sizeof(fileCount));
    std::memcpy(result.data() + result.size() - sizeof(fileCount), &fileCount, sizeof(fileCount));

    // Write each file
    for (const auto& file : m_files) {
        // Write the path length and path
        uint32_t pathLength = static_cast<uint32_t>(file.path.size());
        result.resize(result.size() + sizeof(pathLength));
        std::memcpy(result.data() + result.size() - sizeof(pathLength), &pathLength, sizeof(pathLength));
        result.insert(result.end(), file.path.begin(), file.path.end());

        // Write the file size
        result.resize(result.size() + sizeof(file.size));
        std::memcpy(result.data() + result.size() - sizeof(file.size), &file.size, sizeof(file.size));
    }

    return result;
}

size_t InfoHashMetadata::deserialize(const std::vector<uint8_t>& data, size_t offset) {
    // Check if there's enough data for the info hash
    if (data.size() < offset + 20) {
        return 0;
    }

    // Read the info hash
    std::copy(data.begin() + static_cast<std::ptrdiff_t>(offset),
              data.begin() + static_cast<std::ptrdiff_t>(offset + 20),
              m_infoHash.begin());
    offset += 20;

    // Check if there's enough data for the name length
    if (data.size() < offset + sizeof(uint32_t)) {
        return 0;
    }

    // Read the name length
    uint32_t nameLength;
    std::memcpy(&nameLength, data.data() + offset, sizeof(nameLength));
    offset += sizeof(nameLength);

    // Check if there's enough data for the name
    if (data.size() < offset + nameLength) {
        return 0;
    }

    // Read the name
    m_name.assign(reinterpret_cast<const char*>(data.data() + offset), static_cast<size_t>(nameLength));
    offset += nameLength;

    // Check if there's enough data for the file count
    if (data.size() < offset + sizeof(uint32_t)) {
        return 0;
    }

    // Read the file count
    uint32_t fileCount;
    std::memcpy(&fileCount, data.data() + offset, sizeof(fileCount));
    offset += sizeof(fileCount);

    // Clear existing files
    m_files.clear();

    // Read each file
    for (uint32_t i = 0; i < fileCount; ++i) {
        // Check if there's enough data for the path length
        if (data.size() < offset + sizeof(uint32_t)) {
            return 0;
        }

        // Read the path length
        uint32_t pathLength;
        std::memcpy(&pathLength, data.data() + offset, sizeof(pathLength));
        offset += sizeof(pathLength);

        // Check if there's enough data for the path
        if (data.size() < offset + pathLength) {
            return 0;
        }

        // Read the path
        std::string path(reinterpret_cast<const char*>(data.data() + offset), static_cast<size_t>(pathLength));
        offset += pathLength;

        // Check if there's enough data for the file size
        if (data.size() < offset + sizeof(uint64_t)) {
            return 0;
        }

        // Read the file size
        uint64_t fileSize;
        std::memcpy(&fileSize, data.data() + offset, sizeof(fileSize));
        offset += sizeof(fileSize);

        // Add the file
        m_files.emplace_back(path, fileSize);
    }

    return offset;
}

InfoHashMetadataRegistry::InfoHashMetadataRegistry() {
}

InfoHashMetadataRegistry::~InfoHashMetadataRegistry() {
    // Clear the singleton instance
    try {
        utility::thread::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "InfoHashMetadataRegistry::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        std::cerr << "Error in Types.InfoHashMetadataRegistry: " << e.what() << std::endl;
    }
}

std::shared_ptr<InfoHashMetadataRegistry> InfoHashMetadataRegistry::getInstance() {
    try {
        return utility::thread::withLock(s_instanceMutex, []() {
            if (!s_instance) {
                s_instance = std::shared_ptr<InfoHashMetadataRegistry>(new InfoHashMetadataRegistry());
            }
            return s_instance;
        }, "InfoHashMetadataRegistry::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        std::cerr << "Error in Types.InfoHashMetadataRegistry: " << e.what() << std::endl;
        return nullptr;
    }
}

void InfoHashMetadataRegistry::registerMetadata(const InfoHashMetadata& metadata) {
    try {
        utility::thread::withLock(m_mutex, [this, &metadata]() {
            auto infoHash = metadata.getInfoHash();
            auto metadataPtr = std::make_shared<InfoHashMetadata>(metadata);
            m_metadata[infoHash] = metadataPtr;
        }, "InfoHashMetadataRegistry::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        std::cerr << "Error in Types.InfoHashMetadataRegistry: " << e.what() << std::endl;
    }
}

std::shared_ptr<InfoHashMetadata> InfoHashMetadataRegistry::getMetadata(const InfoHash& infoHash) const {
    try {
        return utility::thread::withLock(m_mutex, [this, &infoHash]() -> std::shared_ptr<InfoHashMetadata> {
            auto it = m_metadata.find(infoHash);
            if (it != m_metadata.end()) {
                return it->second;
            }
            return nullptr;
        }, "InfoHashMetadataRegistry::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        std::cerr << "Error in Types.InfoHashMetadataRegistry: " << e.what() << std::endl;
        return nullptr;
    }
}

std::vector<std::shared_ptr<InfoHashMetadata>> InfoHashMetadataRegistry::getAllMetadata() const {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            std::vector<std::shared_ptr<InfoHashMetadata>> result;
            result.reserve(m_metadata.size());
            for (const auto& [_, metadata] : m_metadata) {
                result.push_back(metadata);
            }
            return result;
        }, "InfoHashMetadataRegistry::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        std::cerr << "Error in Types.InfoHashMetadataRegistry: " << e.what() << std::endl;
        return {};
    }
}

std::vector<uint8_t> InfoHashMetadataRegistry::serializeAll() const {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            std::vector<uint8_t> result;

            // Get all metadata
            auto allMetadata = getAllMetadata();

            // Write the number of metadata entries
            uint32_t metadataCount = static_cast<uint32_t>(allMetadata.size());
            result.resize(sizeof(metadataCount));
            std::memcpy(result.data(), &metadataCount, sizeof(metadataCount));

            // Write each metadata entry
            for (const auto& metadata : allMetadata) {
                auto serialized = metadata->serialize();

                // Write the size of the serialized data
                uint32_t dataSize = static_cast<uint32_t>(serialized.size());
                size_t oldSize = result.size();
                result.resize(oldSize + sizeof(dataSize));
                std::memcpy(result.data() + oldSize, &dataSize, sizeof(dataSize));

                // Write the serialized data
                result.insert(result.end(), serialized.begin(), serialized.end());
            }

            return result;
        }, "InfoHashMetadataRegistry::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        std::cerr << "Error in Types.InfoHashMetadataRegistry: " << e.what() << std::endl;
        return {};
    }
}

bool InfoHashMetadataRegistry::deserializeAll(const std::vector<uint8_t>& data) {
    try {
        return utility::thread::withLock(m_mutex, [this, &data]() {
            // Clear existing metadata
            m_metadata.clear();

            // Check if there's enough data for the metadata count
            if (data.size() < sizeof(uint32_t)) {
                return false;
            }

            // Read the metadata count
            uint32_t metadataCount;
            std::memcpy(&metadataCount, data.data(), sizeof(metadataCount));
            size_t offset = sizeof(metadataCount);

            // Read each metadata entry
            for (uint32_t i = 0; i < metadataCount; ++i) {
                // Check if there's enough data for the data size
                if (data.size() < offset + sizeof(uint32_t)) {
                    return false;
                }

                // Read the data size
                uint32_t dataSize;
                std::memcpy(&dataSize, data.data() + offset, sizeof(dataSize));
                offset += sizeof(dataSize);

                // Check if there's enough data for the serialized metadata
                if (data.size() < offset + dataSize) {
                    return false;
                }

                // Create a new metadata object
                auto metadata = std::make_shared<InfoHashMetadata>();

                // Deserialize the metadata
                size_t bytesRead = metadata->deserialize(data, offset);
                if (bytesRead == 0) {
                    return false;
                }

                // Add the metadata to the registry
                m_metadata[metadata->getInfoHash()] = metadata;

                // Update the offset
                offset += dataSize;
            }

            return true;
        }, "InfoHashMetadataRegistry::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        std::cerr << "Error in Types.InfoHashMetadataRegistry: " << e.what() << std::endl;
        return false;
    }
}

} // namespace dht_hunter::types
