#pragma once

#include "dht_hunter/network/nat/nat_traversal_service.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <cstdint>
#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <chrono>

namespace dht_hunter::network::nat {

/**
 * @brief NAT-PMP NAT traversal service
 *
 * This class provides NAT traversal using the NAT-PMP protocol.
 */
class NATPMPService : public NATTraversalService {
public:
    /**
     * @brief Gets the singleton instance
     * @return The singleton instance
     */
    static std::shared_ptr<NATPMPService> getInstance();

    /**
     * @brief Destructor
     */
    ~NATPMPService() override;

    /**
     * @brief Initializes the NAT-PMP service
     * @return True if initialization was successful, false otherwise
     */
    bool initialize() override;

    /**
     * @brief Shuts down the NAT-PMP service
     */
    void shutdown() override;

    /**
     * @brief Checks if the NAT-PMP service is initialized
     * @return True if initialized, false otherwise
     */
    bool isInitialized() const override;

    /**
     * @brief Adds a port mapping
     * @param internalPort The internal port
     * @param externalPort The external port (0 for automatic assignment)
     * @param protocol The protocol ("TCP" or "UDP")
     * @param description The description of the mapping
     * @return The external port that was mapped, or 0 if mapping failed
     */
    uint16_t addPortMapping(uint16_t internalPort, uint16_t externalPort, const std::string& protocol, const std::string& description) override;

    /**
     * @brief Removes a port mapping
     * @param externalPort The external port
     * @param protocol The protocol ("TCP" or "UDP")
     * @return True if removal was successful, false otherwise
     */
    bool removePortMapping(uint16_t externalPort, const std::string& protocol) override;

    /**
     * @brief Gets the external IP address
     * @return The external IP address, or empty string if not available
     */
    std::string getExternalIPAddress() override;

    /**
     * @brief Attempts to get the external IP address with retry logic
     * @return The external IP address, or empty string if all attempts failed
     */
    std::string tryGetExternalIPAddress();

    /**
     * @brief Gets the status of the NAT-PMP service
     * @return A string describing the status
     */
    std::string getStatus() const override;

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    NATPMPService();

    /**
     * @brief Discovers the gateway IP address
     * @return True if the gateway was discovered, false otherwise
     */
    bool discoverGateway();

    /**
     * @brief Tests if the gateway supports NAT-PMP
     * @return True if the gateway supports NAT-PMP, false otherwise
     */
    bool testGateway();

    /**
     * @brief Logs network interface information for diagnostic purposes
     */
    void logNetworkInterfaces();

    /**
     * @brief Sends a NAT-PMP request to the gateway
     * @param request The request data
     * @param requestSize The size of the request data
     * @param response The response data will be stored here
     * @param responseSize The size of the response data will be stored here
     * @return True if the request was successful, false otherwise
     */
    bool sendNATPMPRequest(const uint8_t* request, size_t requestSize, uint8_t* response, size_t& responseSize);

    /**
     * @brief Refreshes port mappings periodically
     */
    void refreshPortMappingsPeriodically();

    /**
     * @brief Represents a port mapping
     */
    struct PortMapping {
        uint16_t internalPort;
        uint16_t externalPort;
        std::string protocol;
        std::string description;
        std::chrono::steady_clock::time_point lastRefresh;
        uint32_t lifetime; // Lifetime in seconds
    };

    // Static instance for singleton pattern
    static std::shared_ptr<NATPMPService> s_instance;
    static std::mutex s_instanceMutex;

    // NAT-PMP state
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_externalIPFailed{false}; // Flag to indicate if external IP retrieval failed
    std::string m_externalIP;
    std::string m_gatewayIP;
    uint32_t m_epoch{0}; // NAT-PMP epoch time
    int m_socket{-1}; // Socket for NAT-PMP communication
    int m_externalIPRetryCount{0}; // Counter for external IP retrieval attempts

    // Port mappings
    std::unordered_map<std::string, PortMapping> m_portMappings; // Key: externalPort:protocol
    std::mutex m_portMappingsMutex;

    // Refresh thread
    std::thread m_refreshThread;
    std::atomic<bool> m_running{false};
    std::condition_variable m_refreshCondition;
    std::mutex m_refreshMutex;

    // Constants
    static constexpr int REFRESH_INTERVAL_SECONDS = 300; // 5 minutes
    static constexpr int DEFAULT_PORT_MAPPING_LIFETIME_SECONDS = 7200; // 2 hours
    static constexpr uint16_t NATPMP_PORT = 5351; // NAT-PMP port
    static constexpr uint8_t NATPMP_VERSION = 0; // NAT-PMP version
    static constexpr uint8_t NATPMP_OP_EXTERNAL_IP = 0; // Get external IP address
    static constexpr uint8_t NATPMP_OP_MAP_UDP = 1; // Map UDP port
    static constexpr uint8_t NATPMP_OP_MAP_TCP = 2; // Map TCP port
    static constexpr uint16_t NATPMP_RESULT_SUCCESS = 0; // Success result code
    static constexpr int NATPMP_TIMEOUT_SECONDS = 5; // Timeout for NAT-PMP requests
    static constexpr int MAX_EXTERNAL_IP_RETRY_ATTEMPTS = 3; // Maximum number of retry attempts for external IP
};

} // namespace dht_hunter::network::nat
