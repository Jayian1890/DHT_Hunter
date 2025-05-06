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
 * @brief UPnP NAT traversal service
 *
 * This class provides NAT traversal using the UPnP protocol.
 */
class UPnPService : public NATTraversalService {
public:
    /**
     * @brief Gets the singleton instance
     * @return The singleton instance
     */
    static std::shared_ptr<UPnPService> getInstance();

    /**
     * @brief Destructor
     */
    ~UPnPService() override;

    /**
     * @brief Initializes the UPnP service
     * @return True if initialization was successful, false otherwise
     */
    bool initialize() override;

    /**
     * @brief Shuts down the UPnP service
     */
    void shutdown() override;

    /**
     * @brief Checks if the UPnP service is initialized
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
     * @brief Gets the status of the UPnP service
     * @return A string describing the status
     */
    std::string getStatus() const override;

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    UPnPService();

    /**
     * @brief Discovers UPnP devices on the network
     * @return True if devices were discovered, false otherwise
     */
    bool discoverDevices();

    /**
     * @brief Selects a UPnP device to use
     * @return True if a device was selected, false otherwise
     */
    bool selectDevice();

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
    };

    // Static instance for singleton pattern
    static std::shared_ptr<UPnPService> s_instance;
    static std::mutex s_instanceMutex;

    /**
     * @brief Gets the local IP address
     * @return True if successful, false otherwise
     */
    bool getLocalIPAddress();

    /**
     * @brief Downloads and parses a device description
     * @param url The URL of the device description
     * @return True if successful, false otherwise
     */
    bool parseDeviceDescription(const std::string& url);

    /**
     * @brief Sends a SOAP request to the control URL
     * @param soapAction The SOAP action
     * @param soapBody The SOAP request body
     * @param response The response will be stored here
     * @return True if successful, false otherwise
     */
    bool sendSOAPRequest(const std::string& soapAction, const std::string& soapBody, std::string& response);

    /**
     * @brief Parses a SOAP response
     * @param response The SOAP response
     * @param elementName The name of the element to extract
     * @return The value of the element, or empty string if not found
     */
    std::string parseSOAPResponse(const std::string& response, const std::string& elementName);

    /**
     * @brief Makes a relative URL absolute
     * @param baseURL The base URL
     * @param relativeURL The relative URL
     * @return The absolute URL
     */
    std::string makeAbsoluteURL(const std::string& baseURL, const std::string& relativeURL);

    /**
     * @brief Performs an HTTP GET request
     * @param url The URL to request
     * @param response The response will be stored here
     * @return True if successful, false otherwise
     */
    bool httpGet(const std::string& url, std::string& response);

    /**
     * @brief Performs an HTTP POST request
     * @param url The URL to request
     * @param contentType The content type
     * @param data The data to send
     * @param response The response will be stored here
     * @return True if successful, false otherwise
     */
    bool httpPost(const std::string& url, const std::string& contentType, const std::string& data, std::string& response);

    // UPnP state
    std::atomic<bool> m_initialized{false};
    std::string m_externalIP;
    std::string m_localIP;
    std::string m_controlURL;
    std::string m_serviceType;
    std::string m_deviceName;
    std::vector<std::string> m_deviceLocations;

    // Port mappings
    std::unordered_map<std::string, PortMapping> m_portMappings; // Key: externalPort:protocol
    std::mutex m_portMappingsMutex;

    // Refresh thread
    std::thread m_refreshThread;
    std::atomic<bool> m_running{false};
    std::condition_variable m_refreshCondition;
    std::mutex m_refreshMutex;

    // Constants
    static constexpr int REFRESH_INTERVAL_SECONDS = 600; // 10 minutes
    static constexpr int DISCOVERY_TIMEOUT_SECONDS = 10; // Increased from 5 to 10 seconds
    static constexpr int DISCOVERY_RETRY_COUNT = 1; // Number of discovery attempts
    static constexpr int DISCOVERY_RETRY_DELAY_MS = 1000; // Delay between discovery attempts
    static constexpr int MAX_PORT_MAPPING_LIFETIME_SECONDS = 86400; // 24 hours
};

} // namespace dht_hunter::network::nat
