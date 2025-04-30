#include "dht_hunter/utility/metadata/metadata_utils.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"

namespace dht_hunter::utility::metadata {

bool MetadataUtils::setInfoHashName(const types::InfoHash& infoHash, const std::string& name) {
    try {
        // Get the metadata registry
        auto registry = types::InfoHashMetadataRegistry::getInstance();
        if (!registry) {
            unified_event::logError("Utility.MetadataUtils", "Failed to get metadata registry");
            return false;
        }

        // Get the metadata for this info hash
        auto metadata = registry->getMetadata(infoHash);
        if (!metadata) {
            // Create new metadata
            types::InfoHashMetadata newMetadata(infoHash, name);
            registry->registerMetadata(newMetadata);
        } else {
            // Update existing metadata
            metadata->setName(name);
            registry->registerMetadata(*metadata);
        }

        return true;
    } catch (const std::exception& e) {
        unified_event::logError("Utility.MetadataUtils", "Exception in setInfoHashName: " + std::string(e.what()));
        return false;
    }
}

std::string MetadataUtils::getInfoHashName(const types::InfoHash& infoHash) {
    try {
        // Get the metadata registry
        auto registry = types::InfoHashMetadataRegistry::getInstance();
        if (!registry) {
            unified_event::logError("Utility.MetadataUtils", "Failed to get metadata registry");
            return "";
        }

        // Get the metadata for this info hash
        auto metadata = registry->getMetadata(infoHash);
        if (!metadata) {
            return "";
        }

        return metadata->getName();
    } catch (const std::exception& e) {
        unified_event::logError("Utility.MetadataUtils", "Exception in getInfoHashName: " + std::string(e.what()));
        return "";
    }
}

bool MetadataUtils::addFileToInfoHash(const types::InfoHash& infoHash, const std::string& path, uint64_t size) {
    try {
        // Get the metadata registry
        auto registry = types::InfoHashMetadataRegistry::getInstance();
        if (!registry) {
            unified_event::logError("Utility.MetadataUtils", "Failed to get metadata registry");
            return false;
        }

        // Get the metadata for this info hash
        auto metadata = registry->getMetadata(infoHash);
        if (!metadata) {
            // Create new metadata
            types::InfoHashMetadata newMetadata(infoHash);
            newMetadata.addFile(types::TorrentFile(path, size));
            registry->registerMetadata(newMetadata);
        } else {
            // Add file to existing metadata
            metadata->addFile(types::TorrentFile(path, size));
            registry->registerMetadata(*metadata);
        }

        return true;
    } catch (const std::exception& e) {
        unified_event::logError("Utility.MetadataUtils", "Exception in addFileToInfoHash: " + std::string(e.what()));
        return false;
    }
}

std::vector<types::TorrentFile> MetadataUtils::getInfoHashFiles(const types::InfoHash& infoHash) {
    try {
        // Get the metadata registry
        auto registry = types::InfoHashMetadataRegistry::getInstance();
        if (!registry) {
            unified_event::logError("Utility.MetadataUtils", "Failed to get metadata registry");
            return {};
        }

        // Get the metadata for this info hash
        auto metadata = registry->getMetadata(infoHash);
        if (!metadata) {
            return {};
        }

        return metadata->getFiles();
    } catch (const std::exception& e) {
        unified_event::logError("Utility.MetadataUtils", "Exception in getInfoHashFiles: " + std::string(e.what()));
        return {};
    }
}

uint64_t MetadataUtils::getInfoHashTotalSize(const types::InfoHash& infoHash) {
    try {
        // Get the metadata registry
        auto registry = types::InfoHashMetadataRegistry::getInstance();
        if (!registry) {
            unified_event::logError("Utility.MetadataUtils", "Failed to get metadata registry");
            return 0;
        }

        // Get the metadata for this info hash
        auto metadata = registry->getMetadata(infoHash);
        if (!metadata) {
            return 0;
        }

        return metadata->getTotalSize();
    } catch (const std::exception& e) {
        unified_event::logError("Utility.MetadataUtils", "Exception in getInfoHashTotalSize: " + std::string(e.what()));
        return 0;
    }
}

std::shared_ptr<types::InfoHashMetadata> MetadataUtils::getInfoHashMetadata(const types::InfoHash& infoHash) {
    try {
        // Get the metadata registry
        auto registry = types::InfoHashMetadataRegistry::getInstance();
        if (!registry) {
            unified_event::logError("Utility.MetadataUtils", "Failed to get metadata registry");
            return nullptr;
        }

        return registry->getMetadata(infoHash);
    } catch (const std::exception& e) {
        unified_event::logError("Utility.MetadataUtils", "Exception in getInfoHashMetadata: " + std::string(e.what()));
        return nullptr;
    }
}

std::vector<std::shared_ptr<types::InfoHashMetadata>> MetadataUtils::getAllMetadata() {
    try {
        // Get the metadata registry
        auto registry = types::InfoHashMetadataRegistry::getInstance();
        if (!registry) {
            unified_event::logError("Utility.MetadataUtils", "Failed to get metadata registry");
            return {};
        }

        return registry->getAllMetadata();
    } catch (const std::exception& e) {
        unified_event::logError("Utility.MetadataUtils", "Exception in getAllMetadata: " + std::string(e.what()));
        return {};
    }
}

} // namespace dht_hunter::utility::metadata
