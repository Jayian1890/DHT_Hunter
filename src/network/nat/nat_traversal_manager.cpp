#include "dht_hunter/network/nat/nat_traversal_manager.hpp"
#include <sstream>

namespace dht_hunter::network::nat {

// Initialize static members
std::shared_ptr<NATTraversalManager> NATTraversalManager::s_instance = nullptr;
std::mutex NATTraversalManager::s_instanceMutex;

std::shared_ptr<NATTraversalManager> NATTraversalManager::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (!s_instance) {
        s_instance = std::shared_ptr<NATTraversalManager>(new NATTraversalManager());
    }
    return s_instance;
}

NATTraversalManager::NATTraversalManager() : m_initialized(false) {
    unified_event::logInfo("Network.NAT", "NAT traversal manager created");
}

NATTraversalManager::~NATTraversalManager() {
    shutdown();
}

bool NATTraversalManager::initialize() {
    if (m_initialized) {
        return true;
    }

    unified_event::logInfo("Network.NAT", "Initializing NAT traversal manager");

    // Initialize UPnP service
    auto upnpService = UPnPService::getInstance();
    if (upnpService->initialize()) {
        m_services.push_back(upnpService);
        unified_event::logInfo("Network.NAT", "UPnP service initialized");
    } else {
        unified_event::logWarning("Network.NAT", "Failed to initialize UPnP service");
    }

    // Initialize NAT-PMP service
    auto natpmpService = NATPMPService::getInstance();
    if (natpmpService->initialize()) {
        m_services.push_back(natpmpService);
        unified_event::logInfo("Network.NAT", "NAT-PMP service initialized");
    } else {
        unified_event::logWarning("Network.NAT", "Failed to initialize NAT-PMP service");
    }

    // Check if we have any services
    if (m_services.empty()) {
        unified_event::logWarning("Network.NAT", "No NAT traversal services available");
        return false;
    }

    // Set the active service
    m_activeService = m_services[0];
    unified_event::logInfo("Network.NAT", "Active NAT traversal service: " + m_activeService->getStatus());

    m_initialized = true;
    unified_event::logInfo("Network.NAT", "NAT traversal manager initialized");
    return true;
}

void NATTraversalManager::shutdown() {
    if (!m_initialized) {
        return;
    }

    unified_event::logInfo("Network.NAT", "Shutting down NAT traversal manager");

    // Shut down all services
    for (auto& service : m_services) {
        service->shutdown();
    }

    m_services.clear();
    m_activeService = nullptr;
    m_initialized = false;
    unified_event::logInfo("Network.NAT", "NAT traversal manager shut down");
}

bool NATTraversalManager::isInitialized() const {
    return m_initialized;
}

uint16_t NATTraversalManager::addPortMapping(uint16_t internalPort, uint16_t externalPort, const std::string& protocol, const std::string& description) {
    if (!m_initialized) {
        unified_event::logWarning("Network.NAT", "Cannot add port mapping - NAT traversal manager not initialized");
        return 0;
    }

    // Check if we already have this mapping
    std::string key = std::to_string(externalPort) + ":" + protocol;
    {
        std::lock_guard<std::mutex> lock(m_portMappingsMutex);
        if (m_portMappings.find(key) != m_portMappings.end()) {
            return m_portMappings[key].externalPort;
        }
    }

    // Try to add the mapping with each service
    for (auto& service : m_services) {
        uint16_t mappedPort = service->addPortMapping(internalPort, externalPort, protocol, description);
        if (mappedPort > 0) {
            // Add the mapping to our list
            PortMapping mapping;
            mapping.internalPort = internalPort;
            mapping.externalPort = mappedPort;
            mapping.protocol = protocol;
            mapping.description = description;
            mapping.service = service;

            {
                std::lock_guard<std::mutex> lock(m_portMappingsMutex);
                m_portMappings[std::to_string(mappedPort) + ":" + protocol] = mapping;
            }

            unified_event::logInfo("Network.NAT", "Added port mapping: " + std::to_string(mappedPort) + " (" + protocol + ") -> " + std::to_string(internalPort) + " (" + description + ")");
            return mappedPort;
        }
    }

    unified_event::logWarning("Network.NAT", "Failed to add port mapping: " + std::to_string(externalPort) + " (" + protocol + ") -> " + std::to_string(internalPort));
    return 0;
}

bool NATTraversalManager::removePortMapping(uint16_t externalPort, const std::string& protocol) {
    if (!m_initialized) {
        unified_event::logWarning("Network.NAT", "Cannot remove port mapping - NAT traversal manager not initialized");
        return false;
    }

    // Check if we have this mapping
    std::string key = std::to_string(externalPort) + ":" + protocol;
    std::shared_ptr<NATTraversalService> service;
    {
        std::lock_guard<std::mutex> lock(m_portMappingsMutex);
        auto it = m_portMappings.find(key);
        if (it != m_portMappings.end()) {
            service = it->second.service;
        } else {
            return false;
        }
    }

    // Remove the mapping
    bool success = service->removePortMapping(externalPort, protocol);
    if (success) {
        // Remove the mapping from our list
        {
            std::lock_guard<std::mutex> lock(m_portMappingsMutex);
            m_portMappings.erase(key);
        }

        unified_event::logInfo("Network.NAT", "Removed port mapping: " + std::to_string(externalPort) + " (" + protocol + ")");
        return true;
    } else {
        unified_event::logWarning("Network.NAT", "Failed to remove port mapping: " + std::to_string(externalPort) + " (" + protocol + ")");
        return false;
    }
}

std::string NATTraversalManager::getExternalIPAddress() {
    if (!m_initialized) {
        unified_event::logWarning("Network.NAT", "Cannot get external IP address - NAT traversal manager not initialized");
        return "";
    }

    // Try to get the external IP address from each service
    for (auto& service : m_services) {
        std::string ip = service->getExternalIPAddress();
        if (!ip.empty()) {
            return ip;
        }
    }

    unified_event::logWarning("Network.NAT", "Failed to get external IP address");
    return "";
}

std::string NATTraversalManager::getStatus() const {
    if (!m_initialized) {
        return "Not initialized";
    }

    std::stringstream ss;
    ss << "Initialized, " << m_services.size() << " services, " << m_portMappings.size() << " port mappings";
    if (m_activeService) {
        ss << ", Active service: " << m_activeService->getStatus();
    }
    return ss.str();
}

} // namespace dht_hunter::network::nat
