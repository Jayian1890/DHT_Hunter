#pragma once

#include "dht_hunter/network/nat/nat_traversal_service.hpp"
#include "dht_hunter/network/nat/upnp_service.hpp"
#include "dht_hunter/network/nat/natpmp_service.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <cstdint>
#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>

namespace dht_hunter::network::nat {

/**
 * @brief Manager for NAT traversal services
 * 
 * This class manages multiple NAT traversal services and provides a unified interface.
 */
class NATTraversalManager {
public:
    /**
     * @brief Gets the singleton instance
     * @return The singleton instance
     */
    static std::shared_ptr<NATTraversalManager> getInstance();

    /**
     * @brief Destructor
     */
    ~NATTraversalManager();

    /**
     * @brief Initializes the NAT traversal manager
     * @return True if initialization was successful, false otherwise
     */
    bool initialize();

    /**
     * @brief Shuts down the NAT traversal manager
     */
    void shutdown();

    /**
     * @brief Checks if the NAT traversal manager is initialized
     * @return True if initialized, false otherwise
     */
    bool isInitialized() const;

    /**
     * @brief Adds a port mapping
     * @param internalPort The internal port
     * @param externalPort The external port (0 for automatic assignment)
     * @param protocol The protocol ("TCP" or "UDP")
     * @param description The description of the mapping
     * @return The external port that was mapped, or 0 if mapping failed
     */
    uint16_t addPortMapping(uint16_t internalPort, uint16_t externalPort, const std::string& protocol, const std::string& description);

    /**
     * @brief Removes a port mapping
     * @param externalPort The external port
     * @param protocol The protocol ("TCP" or "UDP")
     * @return True if removal was successful, false otherwise
     */
    bool removePortMapping(uint16_t externalPort, const std::string& protocol);

    /**
     * @brief Gets the external IP address
     * @return The external IP address, or empty string if not available
     */
    std::string getExternalIPAddress();

    /**
     * @brief Gets the status of the NAT traversal manager
     * @return A string describing the status
     */
    std::string getStatus() const;

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    NATTraversalManager();

    // Static instance for singleton pattern
    static std::shared_ptr<NATTraversalManager> s_instance;
    static std::mutex s_instanceMutex;

    // NAT traversal services
    std::vector<std::shared_ptr<NATTraversalService>> m_services;
    std::shared_ptr<NATTraversalService> m_activeService;
    std::atomic<bool> m_initialized{false};

    // Port mappings
    struct PortMapping {
        uint16_t internalPort;
        uint16_t externalPort;
        std::string protocol;
        std::string description;
        std::shared_ptr<NATTraversalService> service;
    };
    std::unordered_map<std::string, PortMapping> m_portMappings; // Key: externalPort:protocol
    std::mutex m_portMappingsMutex;
};

} // namespace dht_hunter::network::nat
