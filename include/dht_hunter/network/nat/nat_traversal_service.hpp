#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <functional>

namespace dht_hunter::network::nat {

/**
 * @brief Interface for NAT traversal services
 * 
 * This class provides an interface for NAT traversal services like UPnP and NAT-PMP.
 */
class NATTraversalService {
public:
    /**
     * @brief Destructor
     */
    virtual ~NATTraversalService() = default;

    /**
     * @brief Initializes the NAT traversal service
     * @return True if initialization was successful, false otherwise
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shuts down the NAT traversal service
     */
    virtual void shutdown() = 0;

    /**
     * @brief Checks if the NAT traversal service is initialized
     * @return True if initialized, false otherwise
     */
    virtual bool isInitialized() const = 0;

    /**
     * @brief Adds a port mapping
     * @param internalPort The internal port
     * @param externalPort The external port (0 for automatic assignment)
     * @param protocol The protocol ("TCP" or "UDP")
     * @param description The description of the mapping
     * @return The external port that was mapped, or 0 if mapping failed
     */
    virtual uint16_t addPortMapping(uint16_t internalPort, uint16_t externalPort, const std::string& protocol, const std::string& description) = 0;

    /**
     * @brief Removes a port mapping
     * @param externalPort The external port
     * @param protocol The protocol ("TCP" or "UDP")
     * @return True if removal was successful, false otherwise
     */
    virtual bool removePortMapping(uint16_t externalPort, const std::string& protocol) = 0;

    /**
     * @brief Gets the external IP address
     * @return The external IP address, or empty string if not available
     */
    virtual std::string getExternalIPAddress() = 0;

    /**
     * @brief Gets the status of the NAT traversal service
     * @return A string describing the status
     */
    virtual std::string getStatus() const = 0;
};

} // namespace dht_hunter::network::nat
