/**
 * @file domain_utils.cpp
 * @brief Implementation of domain-specific utility functions
 */

#include "utils/domain_utils.hpp"
#include "dht_hunter/utility/config/configuration_manager.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include "dht_hunter/types/info_hash_metadata.hpp"

#include <algorithm>
#include <sstream>

namespace dht_hunter::utility {

//-----------------------------------------------------------------------------
// Network utilities implementation
//-----------------------------------------------------------------------------
namespace network {

std::string getUserAgent() {
    auto configManager = config::ConfigurationManager::getInstance();
    if (!configManager) {
        unified_event::logWarning("Utility.Network", "Failed to get configuration manager, using default user agent");
        return "DHT-Hunter/1.0";
    }
    
    return configManager->getString("network.userAgent", "DHT-Hunter/1.0");
}

} // namespace network

//-----------------------------------------------------------------------------
// Node utilities implementation
//-----------------------------------------------------------------------------
namespace node {

std::vector<std::shared_ptr<types::Node>> sortNodesByDistance(
    const std::vector<std::shared_ptr<types::Node>>& nodes, 
    const types::NodeID& targetID) {

    std::vector<std::shared_ptr<types::Node>> sortedNodes = nodes;

    std::sort(sortedNodes.begin(), sortedNodes.end(),
        [&targetID](const std::shared_ptr<types::Node>& a, const std::shared_ptr<types::Node>& b) {
            types::NodeID distanceA = a->getID().distanceTo(targetID);
            types::NodeID distanceB = b->getID().distanceTo(targetID);
            return distanceA < distanceB;
        });

    return sortedNodes;
}

std::shared_ptr<types::Node> findNodeByEndpoint(
    const std::vector<std::shared_ptr<types::Node>>& nodes, 
    const types::EndPoint& endpoint) {

    auto it = std::find_if(nodes.begin(), nodes.end(),
        [&endpoint](const std::shared_ptr<types::Node>& node) {
            return node->getEndpoint() == endpoint;
        });

    return (it != nodes.end()) ? *it : nullptr;
}

std::shared_ptr<types::Node> findNodeByID(
    const std::vector<std::shared_ptr<types::Node>>& nodes, 
    const types::NodeID& nodeID) {

    auto it = std::find_if(nodes.begin(), nodes.end(),
        [&nodeID](const std::shared_ptr<types::Node>& node) {
            return node->getID() == nodeID;
        });

    return (it != nodes.end()) ? *it : nullptr;
}

std::string generateNodeLookupID(const types::NodeID& nodeID) {
    return "node_lookup_" + nodeID.toString();
}

std::string generatePeerLookupID(const types::InfoHash& infoHash) {
    std::stringstream ss;
    ss << "peer_lookup_";
    for (const auto& byte : infoHash) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return ss.str();
}

} // namespace node

//-----------------------------------------------------------------------------
// Metadata utilities implementation
//-----------------------------------------------------------------------------
namespace metadata {

bool setInfoHashName(const types::InfoHash& infoHash, const std::string& name) {
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
        unified_event::logError("Utility.MetadataUtils", "Exception while setting info hash name: " + std::string(e.what()));
        return false;
    }
}

std::string getInfoHashName(const types::InfoHash& infoHash) {
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
        unified_event::logError("Utility.MetadataUtils", "Exception while getting info hash name: " + std::string(e.what()));
        return "";
    }
}

bool addFileToInfoHash(const types::InfoHash& infoHash, const std::string& path, uint64_t size) {
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
        unified_event::logError("Utility.MetadataUtils", "Exception while adding file to info hash: " + std::string(e.what()));
        return false;
    }
}

std::vector<types::TorrentFile> getInfoHashFiles(const types::InfoHash& infoHash) {
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
        unified_event::logError("Utility.MetadataUtils", "Exception while getting info hash files: " + std::string(e.what()));
        return {};
    }
}

uint64_t getInfoHashTotalSize(const types::InfoHash& infoHash) {
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
        unified_event::logError("Utility.MetadataUtils", "Exception while getting info hash total size: " + std::string(e.what()));
        return 0;
    }
}

std::shared_ptr<types::InfoHashMetadata> getInfoHashMetadata(const types::InfoHash& infoHash) {
    try {
        // Get the metadata registry
        auto registry = types::InfoHashMetadataRegistry::getInstance();
        if (!registry) {
            unified_event::logError("Utility.MetadataUtils", "Failed to get metadata registry");
            return nullptr;
        }

        return registry->getMetadata(infoHash);
    } catch (const std::exception& e) {
        unified_event::logError("Utility.MetadataUtils", "Exception while getting info hash metadata: " + std::string(e.what()));
        return nullptr;
    }
}

std::vector<std::shared_ptr<types::InfoHashMetadata>> getAllMetadata() {
    try {
        // Get the metadata registry
        auto registry = types::InfoHashMetadataRegistry::getInstance();
        if (!registry) {
            unified_event::logError("Utility.MetadataUtils", "Failed to get metadata registry");
            return {};
        }

        return registry->getAllMetadata();
    } catch (const std::exception& e) {
        unified_event::logError("Utility.MetadataUtils", "Exception while getting all metadata: " + std::string(e.what()));
        return {};
    }
}

} // namespace metadata

} // namespace dht_hunter::utility
