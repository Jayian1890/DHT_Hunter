#include "dht_hunter/utility/network/network_utils.hpp"
#include "dht_hunter/utility/config/configuration_manager.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"

namespace dht_hunter::utility::network {

std::string getUserAgent() {
    auto configManager = config::ConfigurationManager::getInstance();
    if (!configManager) {
        unified_event::logWarning("Utility.Network", "Failed to get configuration manager, using default user agent");
        return "DHT-Hunter/1.0";
    }
    
    return configManager->getString("network.userAgent", "DHT-Hunter/1.0");
}

} // namespace dht_hunter::utility::network
