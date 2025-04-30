#include "dht_hunter/dht/extensions/factory/extension_factory.hpp"
#include "dht_hunter/dht/extensions/implementations/mainline_dht.hpp"
#include "dht_hunter/dht/extensions/implementations/kademlia_dht.hpp"
#include "dht_hunter/dht/extensions/implementations/azureus_dht.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"

namespace dht_hunter::dht::extensions {

// Initialize static members
std::shared_ptr<ExtensionFactory> ExtensionFactory::s_instance = nullptr;
std::mutex ExtensionFactory::s_instanceMutex;

std::shared_ptr<ExtensionFactory> ExtensionFactory::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<ExtensionFactory>(new ExtensionFactory());
    }

    return s_instance;
}

ExtensionFactory::ExtensionFactory() {
    // Register extension creators
    m_extensionCreators["mainline"] = [](const DHTConfig& config, const NodeID& nodeID, std::shared_ptr<RoutingTable> routingTable) {
        return std::make_shared<MainlineDHT>(config, nodeID, routingTable);
    };

    m_extensionCreators["kademlia"] = [](const DHTConfig& config, const NodeID& nodeID, std::shared_ptr<RoutingTable> routingTable) {
        return std::make_shared<KademliaDHT>(config, nodeID, routingTable);
    };

    m_extensionCreators["azureus"] = [](const DHTConfig& config, const NodeID& nodeID, std::shared_ptr<RoutingTable> routingTable) {
        return std::make_shared<AzureusDHT>(config, nodeID, routingTable);
    };
}

ExtensionFactory::~ExtensionFactory() {
    // Clear the singleton instance
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance.get() == this) {
        s_instance.reset();
    }
}

std::shared_ptr<DHTExtension> ExtensionFactory::createExtension(
    const std::string& name,
    const DHTConfig& config,
    const NodeID& nodeID,
    std::shared_ptr<RoutingTable> routingTable) {
    
    // Find the extension creator
    auto it = m_extensionCreators.find(name);
    if (it == m_extensionCreators.end()) {
        unified_event::logError("DHT.ExtensionFactory", "Unknown extension: " + name);
        return nullptr;
    }

    // Create the extension
    try {
        return it->second(config, nodeID, routingTable);
    } catch (const std::exception& e) {
        unified_event::logError("DHT.ExtensionFactory", "Failed to create extension " + name + ": " + e.what());
        return nullptr;
    }
}

std::vector<std::string> ExtensionFactory::getAvailableExtensions() const {
    std::vector<std::string> extensions;
    for (const auto& [name, _] : m_extensionCreators) {
        extensions.push_back(name);
    }
    return extensions;
}

} // namespace dht_hunter::dht::extensions
