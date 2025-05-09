#pragma once

#include "dht_hunter/dht/extensions/dht_extension.hpp"
#include "utils/dht_core_utils.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace dht_hunter::dht::extensions {

/**
 * @brief Factory for creating DHT extensions
 *
 * This class is responsible for creating and managing DHT extensions.
 * It follows the singleton pattern to ensure only one instance exists.
 */
class ExtensionFactory {
public:
    /**
     * @brief Gets the singleton instance
     * @return The singleton instance
     */
    static std::shared_ptr<ExtensionFactory> getInstance();

    /**
     * @brief Destructor
     */
    ~ExtensionFactory();

    /**
     * @brief Creates a DHT extension
     * @param name The name of the extension
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param routingTable The routing table
     * @return The created extension, or nullptr if the extension could not be created
     */
    std::shared_ptr<DHTExtension> createExtension(
        const std::string& name,
        const DHTConfig& config,
        const NodeID& nodeID,
        std::shared_ptr<RoutingTable> routingTable);

    /**
     * @brief Gets all available extension names
     * @return A vector of extension names
     */
    std::vector<std::string> getAvailableExtensions() const;

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    ExtensionFactory();

    // Singleton instance
    static std::shared_ptr<ExtensionFactory> s_instance;
    static std::mutex s_instanceMutex;

    // Map of extension names to creation functions
    using ExtensionCreator = std::function<std::shared_ptr<DHTExtension>(
        const DHTConfig&, const NodeID&, std::shared_ptr<RoutingTable>)>;
    std::unordered_map<std::string, ExtensionCreator> m_extensionCreators;
};

} // namespace dht_hunter::dht::extensions
